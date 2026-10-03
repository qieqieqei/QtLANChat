#include "ChatManager.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

#include "protocol/MessageType.h"
#include "protocol/ProtocolCodec.h"

using proto::MessageType;

ChatManager::ChatManager(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------- 出方向

void ChatManager::setUserName(const QString& name)
{
    m_self = name;
    m_users.clear();                       // 换用户 = 本地列表作废，等服务端快照
}

void ChatManager::login(const QString& userName)
{
    setUserName(userName);
    sendLogin();
}

void ChatManager::onServerConnected()
{
    if (m_self.isEmpty())
        return;                            // 还没有用户名：等 UI 先 setUserName
    m_users.clear();                       // 新会话，服务端 session 已重建，列表作废
    sendLogin();                           // 重连成功后必须重新 LOGIN，否则收不到任何消息
}

void ChatManager::sendLogin()
{
    if (m_self.isEmpty())
        return;
    QJsonObject obj;
    obj.insert(QStringLiteral("user"), m_self);
    emit requestPacket(ProtocolCodec::makeJson(MessageType::Login, obj, ++m_requestId));
}

void ChatManager::sendPrivate(const QString& to, const QString& text)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("to"), to);
    obj.insert(QStringLiteral("message"), text);
    obj.insert(QStringLiteral("timestamp"), double(QDateTime::currentMSecsSinceEpoch()));
    emit requestPacket(ProtocolCodec::makeJson(MessageType::ChatPrivate, obj, ++m_requestId));
}

void ChatManager::sendGroup(const QString& text)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("message"), text);
    obj.insert(QStringLiteral("timestamp"), double(QDateTime::currentMSecsSinceEpoch()));
    emit requestPacket(ProtocolCodec::makeJson(MessageType::ChatGroup, obj, ++m_requestId));
}

// ---------------------------------------------------------------- 入方向

void ChatManager::onPacketReceived(const proto::Packet& packet)
{
    QJsonObject obj;

    switch (packet.header.type) {
    case MessageType::LoginAck:
        if (!ProtocolCodec::decodeJson(packet, &obj)) {
            emit loginFailed(QStringLiteral("登录应答解析失败"));
            return;
        }
        if (obj.value(QStringLiteral("ok")).toBool(false)) {
            const QString name = obj.value(QStringLiteral("user")).toString(m_self);
            m_self = name;
            emit loginSucceeded(name);
        } else {
            // 登录被拒：清掉记住的名字，避免服务端重连后拿一个已被占用的名字反复重登
            m_self.clear();
            m_users.clear();
            emit loginFailed(obj.value(QStringLiteral("reason")).toString(QStringLiteral("未知原因")));
        }
        break;

    case MessageType::UserList:                       // 全量快照
        if (!ProtocolCodec::decodeJson(packet, &obj)) {
            qWarning() << "[chat] UserList payload 解析失败";
            return;
        }
        m_users = obj.value(QStringLiteral("users")).toVariant().toStringList();
        emitUserList();
        break;

    case MessageType::UserJoin: {                     // 增量：幂等，重复收到不会重复添加
        if (!ProtocolCodec::decodeJson(packet, &obj))
            return;
        const QString name = obj.value(QStringLiteral("user")).toString();
        if (!name.isEmpty() && !m_users.contains(name))
            m_users.append(name);
        emitUserList();
        break;
    }

    case MessageType::UserLeave: {                    // 增量：不在列表里就当没发生
        if (!ProtocolCodec::decodeJson(packet, &obj))
            return;
        const QString name = obj.value(QStringLiteral("user")).toString();
        m_users.removeAll(name);
        emitUserList();
        break;
    }

    case MessageType::ChatPrivate:
        if (!ProtocolCodec::decodeJson(packet, &obj)) {
            qWarning() << "[chat] ChatPrivate payload 解析失败";
            return;
        }
        emit privateMessageReceived(obj.value(QStringLiteral("from")).toString(),
                                   obj.value(QStringLiteral("message")).toString(),
                                   qint64(obj.value(QStringLiteral("timestamp")).toDouble()));
        break;

    case MessageType::ChatGroup:
        if (!ProtocolCodec::decodeJson(packet, &obj)) {
            qWarning() << "[chat] ChatGroup payload 解析失败";
            return;
        }
        emit groupMessageReceived(obj.value(QStringLiteral("from")).toString(),
                                  obj.value(QStringLiteral("message")).toString(),
                                  qint64(obj.value(QStringLiteral("timestamp")).toDouble()));
        break;

    case MessageType::HeartbeatAck:
        break;                                        // 心跳回包由 ConnectionManager 消费，这里不处理

    default:
        // 未知类型不要静默吞掉
        qWarning() << "[chat] unhandled packet type" << int(packet.header.type)
                   << "requestId" << packet.header.requestId;
        break;
    }
}

// ---------------------------------------------------------------- 内部

void ChatManager::emitUserList()
{
    emit userListChanged(m_users);
}
