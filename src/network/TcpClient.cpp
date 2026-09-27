#include "TcpClient.h"

#include <QTcpSocket>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
{
    // parent = this：socket 的生命周期跟着 TcpClient 走，不用手动 delete
    m_socket = new QTcpSocket(this);

    // 全部走信号。绝不在 UI 线程里等结果（waitForConnected() 会让界面卡死）
    connect(m_socket, &QTcpSocket::connected,          this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,       this, &TcpClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,          this, &TcpClient::onReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &TcpClient::onSocketError);
}

TcpClient::~TcpClient() = default;

bool TcpClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::connectToServer(const QString& host, quint16 port)
{
    // 连点 Connect 不能叠加多个连接尝试：先归零，再发起新的一次
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
        handleDisconnected();            // 幂等：不会重复发 disconnected()
    }

    emitState(1);                        // 连接中
    m_socket->connectToHost(host, port); // 异步！立刻返回，结果走 connected()/errorOccurred()
}

void TcpClient::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        handleDisconnected();            // 本来就没连：只把状态归零
        return;
    }

    // abort()：立即断开并丢弃未发送数据，不像 disconnectFromHost() 那样等待发送队列排空
    m_socket->abort();
    handleDisconnected();                // 兜底：abort() 在部分路径上不走 onDisconnected()
}

void TcpClient::sendLine(const QString& line)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("未连接，发送被丢弃"));
        return;
    }

    // toUtf8：中文按 UTF-8 出网（toLocal8Bit() 在 MSVC 下会写出 GBK，对端中文必乱码）
    // 末尾 '\n' 只是 Day2 的临时分帧手段，Day3 会换成 16 字节头 + payload
    m_socket->write(line.toUtf8() + '\n');
}

void TcpClient::onConnected()
{
    m_connectEmitted = true;
    emitState(2);
    emit connected();
}

void TcpClient::onDisconnected()
{
    handleDisconnected();
}

void TcpClient::onReadyRead()
{
    m_rx += m_socket->readAll();         // 先全部收进缓冲，绝不当成「一条完整消息」

    int idx = -1;
    while ((idx = m_rx.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_rx.left(idx));
        m_rx.remove(0, idx + 1);         // 残余留在 m_rx，等下一次 readyRead
        emit lineReceived(line);
    }
}

void TcpClient::onSocketError(QAbstractSocket::SocketError code)
{
    Q_UNUSED(code);                      // 只需要文本；具体错误码留给上层判断时再说
    emit errorOccurred(m_socket->errorString());

    // 连接被拒 / DNS 失败 / 连接中被 RST：socket 已回到 UnconnectedState，把状态归零
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
        handleDisconnected();
}

// ---------------------------------------------------------------- 内部

void TcpClient::emitState(int state)
{
    if (state == m_lastState)
        return;                          // 同一个状态不重复上报

    m_lastState = state;
    emit stateChanged(state);
}

void TcpClient::handleDisconnected()
{
    const bool hadConnect = m_connectEmitted;
    m_connectEmitted = false;
    m_rx.clear();                        // 断开后残留的半行必须丢掉，否则会接到下一条连接上
    emitState(0);

    if (hadConnect)
        emit disconnected();
}
