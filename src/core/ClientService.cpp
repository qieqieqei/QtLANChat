#include "ClientService.h"

#include <QMetaObject>
#include <QThread>

#include "core/ChatManager.h"
#include "core/ConnectionManager.h"
#include "network/NetworkWorker.h"
#include "protocol/MessageType.h"

ClientService::ClientService(QObject* parent)
    : QObject(parent)
{
    m_thread = new QThread(this);        // 线程对象本身留在 UI 线程（它是「管理者」，不是「执行者」）
    m_worker = new NetworkWorker();      // 故意不给 parent：有 parent 就不能 moveToThread
    m_conn   = new ConnectionManager();  // 心跳 + 重连，必须和 socket 同线程（QTimer 纪律）
    m_chat   = new ChatManager();        // 消息语义，同样搬到网络线程

    // 线程归属由 moveToThread 决定，不是由谁 new 决定
    m_worker->moveToThread(m_thread);
    m_conn->moveToThread(m_thread);
    m_chat->moveToThread(m_thread);

    // 线程生命周期：起来了让 worker 建 socket；线程结束了让三个对象各自删
    connect(m_thread, &QThread::started,  m_worker, &NetworkWorker::start);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_conn,   &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_chat,   &QObject::deleteLater);

    // ---- 传输层 -> 连接管理：连上 / 掉线（非主动）都归它管 ----
    connect(m_worker, &NetworkWorker::connected,     m_conn, &ConnectionManager::onConnected);
    connect(m_worker, &NetworkWorker::transportLost, m_conn, &ConnectionManager::onTransportError);

    // ---- 传输层的包 -> ChatManager 解析；心跳回包单独喂给 ConnectionManager ----
    connect(m_worker, &NetworkWorker::packetReceived, m_chat, &ChatManager::onPacketReceived);
    connect(m_worker, &NetworkWorker::packetReceived, m_conn, [this](const proto::Packet& p) {
        if (p.header.type == proto::MessageType::HeartbeatAck)
            m_conn->onHeartbeatAck();
    });

    // ---- 连接管理 -> 传输层：重连 / 心跳超时收口 / 发心跳包 ----
    connect(m_conn, &ConnectionManager::reconnectRequested, m_worker, &NetworkWorker::connectToServer);
    connect(m_conn, &ConnectionManager::heartbeatTimedOut,  m_worker, &NetworkWorker::disconnectFromServer);
    connect(m_conn, &ConnectionManager::requestPacket,      m_worker, &NetworkWorker::sendPacket);

    // ---- 连上（含重连成功）-> 重新登录：服务端 session 已重建 ----
    connect(m_conn, &ConnectionManager::stateChanged, m_chat, [this](ConnectionState s) {
        if (s == ConnectionState::Connected)
            m_chat->onServerConnected();
    });

    // ---- 聊天 -> 传输层：ChatManager 只产包，发出去是 worker 的事 ----
    connect(m_chat, &ChatManager::requestPacket, m_worker, &NetworkWorker::sendPacket);

    // ---- 对 UI 的门面信号（单一数据源：状态只来自 ConnectionManager）----
    connect(m_conn,   &ConnectionManager::stateChanged,     this, &ClientService::stateChanged);
    connect(m_conn,   &ConnectionManager::reconnectAttempt, this, &ClientService::reconnectAttempt);
    connect(m_worker, &NetworkWorker::errorOccurred,        this, &ClientService::errorOccurred);
    connect(m_chat,   &ChatManager::userListChanged,        this, &ClientService::userListChanged);
    connect(m_chat,   &ChatManager::privateMessageReceived, this, &ClientService::privateMessageReceived);
    connect(m_chat,   &ChatManager::groupMessageReceived,   this, &ClientService::groupMessageReceived);
    connect(m_chat,   &ChatManager::loginSucceeded,         this, &ClientService::loginSucceeded);
    connect(m_chat,   &ChatManager::loginFailed,            this, &ClientService::loginFailed);

    m_thread->start();
}

ClientService::~ClientService()
{
    stopWorker();
}

// ---------------------------------------------------------------- 出方向（都在 UI 线程被调用）

void ClientService::connectToServer(const QString& host, quint16 port, const QString& userName)
{
    // 顺序有讲究（同一张网络线程的事件队列里，投递顺序 = 执行顺序）：
    //   ① 记住登录名  ② 记住重连要用的地址  ③ 逻辑状态置 Connecting  ④ 真正连
    QMetaObject::invokeMethod(m_chat, "setUserName", Qt::QueuedConnection,
                              Q_ARG(QString, userName));
    QMetaObject::invokeMethod(m_conn, "setEndpoint", Qt::QueuedConnection,
                              Q_ARG(QString, host), Q_ARG(quint16, port));
    QMetaObject::invokeMethod(m_conn, "setState", Qt::QueuedConnection,
                              Q_ARG(ConnectionState, ConnectionState::Connecting));
    QMetaObject::invokeMethod(m_worker, "connectToServer", Qt::QueuedConnection,
                              Q_ARG(QString, host), Q_ARG(quint16, port));
}

void ClientService::disconnectFromServer()
{
    // 先把逻辑状态归零（停心跳/重连定时器、复位退避计数），再让 worker 主动断开。
    // worker 主动断开不会发 transportLost，所以不会触发「自己重连自己」。
    QMetaObject::invokeMethod(m_conn, "setState", Qt::QueuedConnection,
                              Q_ARG(ConnectionState, ConnectionState::Disconnected));
    QMetaObject::invokeMethod(m_worker, "disconnectFromServer", Qt::QueuedConnection);
}

void ClientService::sendPrivate(const QString& to, const QString& text)
{
    QMetaObject::invokeMethod(m_chat, "sendPrivate", Qt::QueuedConnection,
                              Q_ARG(QString, to), Q_ARG(QString, text));
}

void ClientService::sendGroup(const QString& text)
{
    QMetaObject::invokeMethod(m_chat, "sendGroup", Qt::QueuedConnection,
                              Q_ARG(QString, text));
}

// ---------------------------------------------------------------- 内部

void ClientService::stopWorker()
{
    if (!m_worker)
        return;

    if (m_thread->isRunning()) {
        // 顺序很重要：① 让各对象在自己的线程里收尾（BlockingQueuedConnection 会等它执行完）
        //            ② quit() 退出事件循环  ③ wait() 等线程真的结束
        // 少了 wait() 就会看到 "QThread: Destroyed while thread is still running"。
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_conn,   "stop", Qt::BlockingQueuedConnection);
        m_thread->quit();
        if (!m_thread->wait(3000))
            qWarning("[ClientService] network thread did not finish within 3 s");
        // 线程结束后，三个对象由 QThread::finished -> deleteLater 回收（Qt 在线程收尾时会处理延迟删除）
    } else {
        delete m_worker;                 // 线程压根没起来：自己收拾，别漏
        delete m_conn;
        delete m_chat;
    }

    m_worker = nullptr;                  // 之后绝不再用
    m_conn   = nullptr;
    m_chat   = nullptr;
}
