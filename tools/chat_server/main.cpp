// tools/chat_server/main.cpp —— Day5 联调服务器：支持登录 / 用户列表 / 私聊 / 群聊 / 心跳
// 不属于客户端交付物，Day7 打包时不要带上。
#include <QCoreApplication>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

#include "protocol/MessageType.h"
#include "protocol/ProtocolCodec.h"
#include "protocol/ReceiveBuffer.h"

namespace {

struct Client {
    QString       name;                 // 登录后的用户名；空 = 还没登录
    ReceiveBuffer rx;                   // 这个连接的粘包/拆包缓冲
};

quint16 argPort(int argc, char** argv, quint16 fallback)
{
    if (argc > 1) {
        bool ok = false;
        const uint p = QString::fromLocal8Bit(argv[1]).toUInt(&ok);
        if (ok && p > 0 && p <= 65535)
            return quint16(p);
    }
    return fallback;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const quint16 port = argPort(argc, argv, 8888);

    QTcpServer server;
    if (!server.listen(QHostAddress::Any, port)) {          // 端口被占用时明确失败，别默默继续
        qWarning("listen failed on port %d: %s", int(port), qPrintable(server.errorString()));
        return 1;
    }
    qInfo("chat server listening on port %d", int(port));

    QHash<QTcpSocket*, Client> clients;

    auto sendTo = [](QTcpSocket* s, const QByteArray& frame) {
        if (s && s->state() == QAbstractSocket::ConnectedState)
            s->write(frame);
    };

    // 广播用户列表（第一版：改动后统一发全量快照，客户端拿增量容易重复或漏）
    auto broadcastUserList = [&clients, &sendTo]() {
        QJsonArray arr;
        for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
            if (!it.value().name.isEmpty())
                arr.append(it.value().name);

        QJsonObject obj;
        obj.insert(QStringLiteral("users"), arr);
        const QByteArray frame = ProtocolCodec::encodeJson(proto::MessageType::UserList, obj);
        for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
            sendTo(it.key(), frame);
    };

    auto nameOwner = [&clients](const QString& name) -> QTcpSocket* {
        for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
            if (it.value().name == name)
                return it.key();
        return nullptr;
    };

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket* s = server.nextPendingConnection();
        clients.insert(s, Client{});
        qInfo("client connected: %s:%d",
              qPrintable(s->peerAddress().toString()), int(s->peerPort()));

        QObject::connect(s, &QTcpSocket::readyRead, s, [&clients, &broadcastUserList, &sendTo, s]() {
            Client& c = clients[s];
            c.rx.append(s->readAll());

            for (;;) {
                proto::Packet pkt;
                QString err;
                if (!c.rx.takeNextPacket(&pkt, &err)) {
                    if (!err.isEmpty()) {
                        qWarning("client %s protocol error: %s", qPrintable(c.name), qPrintable(err));
                        s->disconnectFromHost();
                    }
                    break;
                }

                QJsonObject obj;
                switch (pkt.header.type) {
                case proto::MessageType::Login: {
                    if (!ProtocolCodec::decodeJson(pkt, &obj)) {
                        s->write(ProtocolCodec::encodeJson(
                            proto::MessageType::LoginAck,
                            QJsonObject{{QStringLiteral("ok"), false},
                                        {QStringLiteral("reason"), QStringLiteral("Login payload 解析失败")}},
                            pkt.header.requestId));
                        break;
                    }
                    const QString name = obj.value(QStringLiteral("user")).toString().trimmed();
                    if (name.isEmpty()) {
                        s->write(ProtocolCodec::encodeJson(
                            proto::MessageType::LoginAck,
                            QJsonObject{{QStringLiteral("ok"), false},
                                        {QStringLiteral("reason"), QStringLiteral("用户名为空")}},
                            pkt.header.requestId));
                        break;
                    }
                    // E1：同名登录 → 拒绝后来者（不允许两个都成功）
                    QTcpSocket* other = nullptr;
                    for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
                        if (it.key() != s && it.value().name == name)
                            other = it.key();
                    if (other) {
                        qWarning("login rejected (duplicate name): %s", qPrintable(name));
                        s->write(ProtocolCodec::encodeJson(
                            proto::MessageType::LoginAck,
                            QJsonObject{{QStringLiteral("ok"), false},
                                        {QStringLiteral("reason"), QStringLiteral("用户名已被使用")}},
                            pkt.header.requestId));
                        break;
                    }

                    c.name = name;
                    qInfo("login ok: %s", qPrintable(name));
                    s->write(ProtocolCodec::encodeJson(
                        proto::MessageType::LoginAck,
                        QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("user"), name}},
                        pkt.header.requestId));
                    broadcastUserList();
                    break;
                }

                case proto::MessageType::Heartbeat:
                    // 原样带回 requestId，payload 空 JSON
                    s->write(ProtocolCodec::encodeJson(proto::MessageType::HeartbeatAck,
                                                       QJsonObject{}, pkt.header.requestId));
                    break;

                case proto::MessageType::ChatPrivate: {
                    if (!ProtocolCodec::decodeJson(pkt, &obj))
                        break;
                    const QString to = obj.value(QStringLiteral("to")).toString();
                    QJsonObject out{
                        {QStringLiteral("from"), c.name},
                        {QStringLiteral("message"), obj.value(QStringLiteral("message"))},
                        {QStringLiteral("timestamp"), obj.value(QStringLiteral("timestamp"))}};
                    QTcpSocket* target = nullptr;
                    for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
                        if (it.value().name == to)
                            target = it.key();
                    if (target)
                        sendTo(target, ProtocolCodec::encodeJson(proto::MessageType::ChatPrivate, out));
                    else
                        qWarning("private to offline user: %s", qPrintable(to));
                    break;
                }

                case proto::MessageType::ChatGroup: {
                    if (!ProtocolCodec::decodeJson(pkt, &obj))
                        break;
                    QJsonObject out{
                        {QStringLiteral("from"), c.name},
                        {QStringLiteral("message"), obj.value(QStringLiteral("message"))},
                        {QStringLiteral("timestamp"), obj.value(QStringLiteral("timestamp"))}};
                    const QByteArray frame = ProtocolCodec::encodeJson(proto::MessageType::ChatGroup, out);
                    for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
                        if (it.key() != s && !it.value().name.isEmpty())   // 只发给已登录的，未登录的连接不该收到业务消息
                            sendTo(it.key(), frame);
                    break;
                }

                default:
                    qWarning("unhandled packet type 0x%02x from %s",
                             int(pkt.header.type), qPrintable(c.name));
                    break;
                }
            }
        });

        QObject::connect(s, &QTcpSocket::disconnected, s, [&clients, &broadcastUserList, s]() {
            const QString name = clients.value(s).name;
            clients.remove(s);
            if (!name.isEmpty())
                qInfo("client disconnected: %s", qPrintable(name));
            s->deleteLater();
            broadcastUserList();
        });
    });

    return app.exec();
}
