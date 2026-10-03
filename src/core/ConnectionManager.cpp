#include "ConnectionManager.h"

#include <QDebug>
#include <QJsonObject>

#include "protocol/MessageType.h"
#include "protocol/ProtocolCodec.h"

namespace {
// 阈值写成常量，别散落魔法数（Day5 坑清单：阈值拍脑袋 + 魔法数满天飞）
constexpr int kHeartbeatIntervalMs = 5000;     // 每 5 秒发一次 Heartbeat
constexpr int kTimeoutCheckMs      = 1000;     // 每 1 秒检查一次
constexpr int kAckTimeoutMs        = 15000;    // 15 秒没收到任何回包就判定死掉
constexpr int kBackoffBaseMs       = 1000;     // 退避基数：1s
constexpr int kBackoffMaxShift     = 5;        // 1,2,4,8,16,32 -> 封顶 30s
constexpr int kBackoffCapMs        = 30000;
} // namespace

ConnectionManager::ConnectionManager(QObject* parent)
    : QObject(parent)
{
    // 三个定时器都在这里 new（父子同线程），启动 / 停止也都在本线程内完成
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(kHeartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ConnectionManager::onHeartbeatTick);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setInterval(kTimeoutCheckMs);
    connect(m_timeoutTimer, &QTimer::timeout, this, &ConnectionManager::onTimeoutCheck);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ConnectionManager::onReconnectTimer);
}

// ---------------------------------------------------------------- 生命周期

void ConnectionManager::start()
{
    m_lastAck.start();                             // 单调时钟归零
    m_heartbeatTimer->start();
    m_timeoutTimer->start();
}

void ConnectionManager::stop()
{
    m_heartbeatTimer->stop();
    m_timeoutTimer->stop();
    m_reconnectTimer->stop();
}

void ConnectionManager::setState(ConnectionState s)
{
    if (s == m_state)
        return;

    // 回到「未连接」= 用户主动断开 / 收摊：定时器和退避计数全部清零
    if (s == ConnectionState::Disconnected) {
        stop();
        m_attempt = 0;
    }

    qInfo("[conn] state: %s -> %s", connectionStateName(m_state), connectionStateName(s));
    m_state = s;
    emit stateChanged(s);
}

void ConnectionManager::setEndpoint(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
}

void ConnectionManager::onConnected()
{
    m_attempt = 0;                                 // 成功后归零（D1 要看这条日志）
    qInfo("[conn] connected, attempt counter reset to 0");
    setState(ConnectionState::Connected);
    start();                                       // 心跳开始跑
}

// ---------------------------------------------------------------- 心跳

void ConnectionManager::onHeartbeatTick()
{
    // 只有「已连接」才发心跳：Reconnecting 期间定时器还没停，别让心跳去撞一个已断的 socket
    if (m_state != ConnectionState::Connected)
        return;

    emit requestPacket(ProtocolCodec::makeJson(proto::MessageType::Heartbeat,
                                               QJsonObject{}, ++m_requestId));
}

void ConnectionManager::onHeartbeatAck()
{
    m_lastAck.restart();                           // 只要收到回包就刷新
}

void ConnectionManager::onTimeoutCheck()
{
    if (m_state != ConnectionState::Connected)
        return;
    if (m_lastAck.elapsed() <= kAckTimeoutMs)
        return;

    qWarning("[conn] heartbeat timed out (%lld ms without ack)",
             static_cast<long long>(m_lastAck.elapsed()));
    emit heartbeatTimedOut();                      // ClientService 收到后会关掉 socket
    scheduleReconnect();
}

// ---------------------------------------------------------------- 重连（指数退避）

void ConnectionManager::onTransportError(const QString& msg)
{
    qInfo("[conn] transport error: %s", qUtf8Printable(msg));

    // 已经是「未连接」说明用户主动断开或者早就收摊了，不要自己爬起来
    if (m_state == ConnectionState::Disconnected)
        return;

    scheduleReconnect();
}

void ConnectionManager::scheduleReconnect()
{
    const int shift = qMin(m_attempt, kBackoffMaxShift);      // 1s 2s 4s 8s 16s 32s
    int delay = kBackoffBaseMs << shift;
    if (delay > kBackoffCapMs)
        delay = kBackoffCapMs;                               // 封顶 30s

    ++m_attempt;
    setState(ConnectionState::Reconnecting);
    qInfo("[conn] reconnect attempt %d in %d ms", m_attempt, delay);
    emit reconnectAttempt(m_attempt, delay);
    m_reconnectTimer->start(delay);
}

void ConnectionManager::onReconnectTimer()
{
    if (m_host.isEmpty()) {                        // 没记住地址就别瞎连
        setState(ConnectionState::Disconnected);
        return;
    }

    setState(ConnectionState::Connecting);
    qInfo("[conn] reconnecting to %s:%u ...", qUtf8Printable(m_host), unsigned(m_port));
    emit reconnectRequested(m_host, m_port);
}
