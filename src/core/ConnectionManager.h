#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "network/ConnectionState.h"
#include "protocol/Packet.h"

// Day5：心跳保活 + 断线自动重连（指数退避）。
//
// 它比较特别：既产包（心跳）也管状态，但同样**不碰 socket** —— 需要发东西就
// emit requestPacket()，需要重连就 emit reconnectRequested()，由 ClientService 接线到
// 网络线程里的 NetworkWorker。
//
// 注意：三个 QTimer 必须和本对象一样活在网络线程（Day4 的纪律）。本类由
// ClientService 在构造时 moveToThread，所以计时器全部在构造函数里 new 成自己的子对象，
// 启动 / 停止都在同一线程内完成。
class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject* parent = nullptr);

public slots:
    void start();                                  // 起心跳定时器（连上之后调）
    void stop();                                   // 停所有定时器
    void setState(ConnectionState s);
    void setEndpoint(const QString& host, quint16 port);   // 记住地址，重连要用
    void onConnected();                            // 传输层连上了：复位重试计数 + 开心跳
    void onHeartbeatAck();                         // 收到 HeartbeatAck -> 刷新时间戳
    void onTransportError(const QString& msg);     // 传输层断了 / 连不上 -> 进入重连

signals:
    void stateChanged(ConnectionState state);
    void reconnectAttempt(int attempt, int delayMs);
    void heartbeatTimedOut();
    void requestPacket(const proto::Packet& packet);
    void reconnectRequested(const QString& host, quint16 port);   // 请 ClientService 发起连接

private slots:
    void onHeartbeatTick();
    void onTimeoutCheck();
    void onReconnectTimer();

private:
    void scheduleReconnect();

    QTimer*       m_heartbeatTimer = nullptr;      // 5 秒一次
    QTimer*       m_timeoutTimer   = nullptr;      // 1 秒检查一次
    QTimer*       m_reconnectTimer = nullptr;      // 指数退避
    QElapsedTimer m_lastAck;                       // 单调时钟，别用 QDateTime
    ConnectionState m_state = ConnectionState::Disconnected;
    quint32       m_requestId = 0;
    int           m_attempt = 0;                   // 0,1,2,4,8,16 -> 上限 30s
    QString       m_host;                          // 重连目标
    quint16       m_port = 0;
};
