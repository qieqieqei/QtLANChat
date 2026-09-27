#pragma once

#include <QObject>
#include <QString>

#include "protocol/Packet.h"     // 信号/槽参数需要完整类型

class QThread;
class NetworkWorker;

// Day4 门面（facade）：UI 唯一能看到的网络对象。
//
// 对外暴露的信号与 Day2 的 TcpClient 完全一致 —— 线程怎么开、socket 在哪个线程，
// UI 层完全不需要知道。它只负责：持有 QThread、把工作排队投递到网络线程、
// 把 worker 的信号原样转发回 UI 线程。
class ClientService : public QObject
{
    Q_OBJECT

public:
    explicit ClientService(QObject* parent = nullptr);
    ~ClientService() override;

public slots:
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void sendPacket(const proto::Packet& packet);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void packetReceived(const proto::Packet& packet);
    void stateChanged(int state);

private:
    void stopWorker();                   // 收尾：通知 worker -> quit -> wait

    QThread*       m_thread = nullptr;   // 线程对象本身留在 UI 线程
    NetworkWorker* m_worker = nullptr;   // 注意：它属于网络线程，UI 不能直接调它的方法
};
