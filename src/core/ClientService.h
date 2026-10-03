#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "network/ConnectionState.h"   // 状态信号的参数类型
#include "protocol/Packet.h"           // 信号/槽参数需要完整类型

class QThread;
class NetworkWorker;
class ConnectionManager;
class ChatManager;

// Day4/Day5 门面（facade）：UI 唯一能看到的网络对象。
//
// 内部三件套全部活在同一张「网络线程」里：
//   - NetworkWorker     ：QTcpSocket + 收包拆帧（Day4）
//   - ConnectionManager ：心跳保活 + 断线自动重连（Day5）
//   - ChatManager       ：登录 / 私聊 / 群聊 / 用户列表的「消息语义」（Day5）
//
// UI 只认识本类：发请求用槽，收结果用信号。线程怎么开、谁在哪个线程，UI 完全不需要知道。
class ClientService : public QObject
{
    Q_OBJECT

public:
    explicit ClientService(QObject* parent = nullptr);
    ~ClientService() override;

public slots:
    // 出方向：连接 / 断开 / 发消息（都在 UI 线程被调用，内部排队投递到网络线程）
    void connectToServer(const QString& host, quint16 port, const QString& userName);
    void disconnectFromServer();
    void sendPrivate(const QString& to, const QString& text);
    void sendGroup(const QString& text);

signals:
    // 入方向：全部由网络线程驱动，跨线程队列投递回 UI 线程
    void stateChanged(ConnectionState state);            // 单一数据源：UI 状态只认这一处
    void errorOccurred(const QString& message);
    void reconnectAttempt(int attempt, int delayMs);     // 给日志用
    void userListChanged(const QStringList& users);
    void privateMessageReceived(const QString& from, const QString& text, qint64 ts);
    void groupMessageReceived(const QString& from, const QString& text, qint64 ts);
    void loginSucceeded(const QString& selfName);
    void loginFailed(const QString& reason);

private:
    void stopWorker();                   // 收尾：通知各对象 -> quit -> wait

    QThread*           m_thread = nullptr;   // 线程对象本身留在 UI 线程
    NetworkWorker*     m_worker = nullptr;   // 属于网络线程
    ConnectionManager* m_conn   = nullptr;   // 属于网络线程
    ChatManager*       m_chat   = nullptr;   // 属于网络线程
};
