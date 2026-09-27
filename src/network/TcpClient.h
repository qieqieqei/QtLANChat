#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QAbstractSocket>

class QTcpSocket;

// Day2 只负责「连 / 断 / 收发一行文本」，重连是 Day5 的事
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
    void sendLine(const QString& line);                  // Day2 只发一行文本（内部补 '\n'）

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void lineReceived(const QString& line);              // 已按 '\n' 切好的整行
    void stateChanged(int state);                        // 0=未连接 1=连接中 2=已连接（Day5 换成枚举）

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError code);

private:
    void emitState(int state);          // 状态去重：同一个状态只上报一次
    void handleDisconnected();          // 断开收口：清缓冲 + 状态归零 + 必要时发 disconnected()

    QTcpSocket* m_socket = nullptr;
    QByteArray  m_rx;                                    // Day3 会换成 ReceiveBuffer，先占位
    int  m_lastState = 0;                                // 最近一次上报出去的状态
    bool m_connectEmitted = false;                       // connected() 是否已发（避免失败时发 disconnected()）
};
