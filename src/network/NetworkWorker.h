#pragma once

#include <QObject>
#include <QString>

#include "protocol/Packet.h"          // 信号/槽参数需要完整类型
#include "protocol/ReceiveBuffer.h"   // 成员

class QTcpSocket;

// Day4：住在「网络线程」里的干活的人。Day2/Day3 的 TcpClient 内容整体搬到这里。
//
// 纪律（Day4 的核心）：
//   - 本类所有槽都在网络线程执行，不许碰任何 UI 控件、不许弹 QMessageBox；
//   - 只通过信号把结果送出去，谁收到、收在哪个线程由连接类型决定；
//   - 线程归属由 moveToThread 决定，不是由谁 new 决定。
class NetworkWorker : public QObject
{
    Q_OBJECT

public:
    explicit NetworkWorker(QObject* parent = nullptr);

public slots:
    void start();                                     // 线程启动后调用：在这里建 socket、接线
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void sendPacket(const proto::Packet& packet);     // 自定义类型做跨线程参数 => 必须注册 meta type
    void stop();                                      // 收尾：abort + 清缓冲（对象由 ClientService 回收）

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void packetReceived(const proto::Packet& packet);
    void stateChanged(int state);                     // 0=未连接 1=连接中 2=已连接

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(int code);                     // 收 int：头文件里就不用引 QtNetwork

private:
    void emitState(int state);                        // 状态去重：同一个状态只上报一次
    void handleDisconnected();                        // 断开收口：清缓冲 + 状态归零 + 必要时发 disconnected()

    QTcpSocket*   m_socket = nullptr;
    ReceiveBuffer m_rx;                               // 字节流 -> 完整包
    int  m_lastState = 0;                             // 最近一次上报出去的状态
    bool m_connectEmitted = false;                    // connected() 是否已发（避免连接失败时发 disconnected()）
};
