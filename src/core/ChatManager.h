#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "protocol/Packet.h"

// Day5：只负责「消息语义」——登录、私聊、群聊、用户列表；**不碰 socket**。
// 它只产出 / 消费 proto::Packet：
//   出方向：emit requestPacket(pkt)  -> ClientService 交给网络线程里的 NetworkWorker 发出去
//   入方向：onPacketReceived(pkt)    <- 网络线程把收到的整包喂进来
// 这条线一破，Day6 的文件传输就会乱。
//
// 它和 ConnectionManager 一样活在网络线程（由 ClientService moveToThread）。
class ChatManager : public QObject
{
    Q_OBJECT

public:
    explicit ChatManager(QObject* parent = nullptr);

public slots:
    void setUserName(const QString& name);                      // 只记登录名（等连上交由 onServerConnected 发 LOGIN）
    void login(const QString& userName);                        // 记名字并立刻发 LOGIN（已连接时用）
    void onServerConnected();                                   // 传输层连上/重连成功 -> 用记住的名字重新 LOGIN
    void sendPrivate(const QString& to, const QString& text);   // -> ChatPrivate
    void sendGroup(const QString& text);                        // -> ChatGroup
    void onPacketReceived(const proto::Packet& packet);         // 解析并分发

signals:
    void requestPacket(const proto::Packet& packet);            // 交给 ClientService 发出去
    void userListChanged(const QStringList& users);
    void privateMessageReceived(const QString& from, const QString& text, qint64 ts);
    void groupMessageReceived(const QString& from, const QString& text, qint64 ts);
    void loginSucceeded(const QString& selfName);
    void loginFailed(const QString& reason);

private:
    void sendLogin();           // 统一出口：把 LOGIN 包发出去
    void emitUserList();        // 统一出口：列表变了就发全量
    QStringList m_users;        // 本地维护的用户列表（第一版：服务端只发全量快照）
    QString     m_self;         // 登录名（重连后要拿它重新登录）
    quint32     m_requestId = 0;
};
