#include "NetworkWorker.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDebug>
#include <QTcpSocket>
#include <QThread>

#include "protocol/ProtocolCodec.h"

NetworkWorker::NetworkWorker(QObject* parent)
    : QObject(parent)
{
    // 这里不要建 socket：此刻对象通常还在 UI 线程（还没 moveToThread）。
    // 真正的创建放在 start()，由 QThread::started 触发，保证父子同线程。
}

// ---------------------------------------------------------------- 生命周期

void NetworkWorker::start()
{
    if (m_socket)
        return;                                      // 幂等：线程重启时不重复建

    // 走到这里说明 QThread::started 已经把控制权交给网络线程，
    // 所以 socket 的「创建线程」= 「归属线程」，父子关系合法。
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected,    this, &NetworkWorker::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkWorker::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &NetworkWorker::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError code) {
        onSocketError(int(code));
    });

    // Day4 验收 A2：这两行打印出来的线程指针必须不同，否则 moveToThread 没生效
    qInfo("[worker] start thread=%p ui=%p",
          static_cast<void*>(QThread::currentThread()),
          static_cast<void*>(QCoreApplication::instance()
                                 ? QCoreApplication::instance()->thread()
                                 : nullptr));
}

void NetworkWorker::stop()
{
    // 由 ClientService 用 BlockingQueuedConnection 调进来 => 一定在网络线程内执行。
    // 只做收尾（abort + 丢半包），对象本身交给 ClientService：
    //   thread finished -> deleteLater，或线程没起来时直接 delete。
    if (m_socket)
        m_socket->abort();                           // abort 立刻断，不排队等发送队列排空

    m_rx.clear();
    m_lastState = 0;
    m_connectEmitted = false;
}

// ---------------------------------------------------------------- 出方向

void NetworkWorker::connectToServer(const QString& host, quint16 port)
{
    if (!m_socket)
        return;                                      // start() 还没跑：忽略

    // 连点 Connect 不能叠加多个连接尝试：先归零，再发起新的一次
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
        handleDisconnected();
    }

    emitState(1);                                    // 连接中
    m_socket->connectToHost(host, port);             // 异步：结果走 connected()/errorOccurred()
}

void NetworkWorker::disconnectFromServer()
{
    if (!m_socket)
        return;

    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        handleDisconnected();                        // 本来就没连：只把状态归零
        return;
    }

    m_socket->abort();
    handleDisconnected();                            // 兜底：abort() 在部分路径上不走 onDisconnected()
}

void NetworkWorker::sendPacket(const proto::Packet& packet)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("未连接，发送被丢弃"));
        return;
    }

    // 头 + payload 一次写出。粘不粘是接收端 ReceiveBuffer 的事。
    m_socket->write(ProtocolCodec::encode(packet));
}

// ---------------------------------------------------------------- 入方向（socket 信号）

void NetworkWorker::onConnected()
{
    m_connectEmitted = true;
    emitState(2);
    emit connected();
}

void NetworkWorker::onDisconnected()
{
    handleDisconnected();
}

void NetworkWorker::onReadyRead()
{
    m_rx.append(m_socket->readAll());                // 先全部收进缓冲，绝不当成「一条完整消息」

    // 一次 readyRead 里可能是 0 个、1 个或 N 个完整包
    for (;;) {
        proto::Packet pkt;
        QString err;
        if (!m_rx.takeNextPacket(&pkt, &err)) {
            if (!err.isEmpty()) {
                emit errorOccurred(err);             // magic 错 / 长度超限
                disconnectFromServer();              // 垃圾流：清缓冲 + 断开，别继续硬解
            }
            break;                                   // 头不够 / 包不完整：一个字节都不动，等下一次
        }
        emit packetReceived(pkt);                    // 跨线程投递到 ClientService（参数被拷贝）
    }
}

void NetworkWorker::onSocketError(int code)
{
    Q_UNUSED(code);                                  // 只需要文本；具体错误码留给上层判断时再说
    emit errorOccurred(m_socket->errorString());

    // 连接被拒 / DNS 失败 / 连接中被 RST：socket 已回到 UnconnectedState，把状态归零
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
        handleDisconnected();
}

// ---------------------------------------------------------------- 内部

void NetworkWorker::emitState(int state)
{
    if (state == m_lastState)
        return;                                      // 同一个状态不重复上报

    m_lastState = state;
    emit stateChanged(state);
}

void NetworkWorker::handleDisconnected()
{
    const bool hadConnect = m_connectEmitted;
    m_connectEmitted = false;
    m_rx.clear();                                    // 断开后残留的半包必须丢掉，否则会接到下一条连接上
    emitState(0);

    if (hadConnect)
        emit disconnected();
}
