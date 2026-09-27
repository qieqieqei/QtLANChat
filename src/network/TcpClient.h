#pragma once

#include <QObject>
#include <QString>
#include <QAbstractSocket>

#include "protocol/Packet.h"          // 信号参数需要完整类型
#include "protocol/ReceiveBuffer.h"   // 成员

class QTcpSocket;

// Day2：连 / 断 + 收发一行文本。
// Day3：入方向不再靠 '\n' 分帧，改用 ReceiveBuffer 从字节流里切出完整包；
//       出方向发的是整包（ProtocolCodec 负责 Packet -> bytes）。
class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject* parent = nullptr);
    ~TcpClient() override;

    bool isConnected() const;

public slots:
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void sendPacket(const proto::Packet& packet);        // Day3：整包出网

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void packetReceived(const proto::Packet& packet);    // Day3：已切好的完整包
    void stateChanged(int state);                        // 0=未连接 1=连接中 2=已连接（Day5 换成枚举）

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError code);

private:
    void emitState(int state);          // 状态去重：同一个状态只上报一次
    void handleDisconnected();          // 断开收口：清缓冲 + 状态归零 + 必要时发 disconnected()

    QTcpSocket*   m_socket = nullptr;
    ReceiveBuffer m_rx;                                 // Day3：字节流 -> 完整包
    int  m_lastState = 0;                               // 最近一次上报出去的状态
    bool m_connectEmitted = false;                      // connected() 是否已发（避免失败时发 disconnected()）
};
