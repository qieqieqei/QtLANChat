// tools/echo_server/main.cpp —— 只用来验证「客户端连没连上、数据通不通」
// 不属于客户端交付物，Day7 打包时不要带上
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const quint16 port = (argc > 1) ? quint16(QString::fromLocal8Bit(argv[1]).toUShort()) : 8888;

    QTcpServer server;
    if (!server.listen(QHostAddress::Any, port)) {          // 端口被占用时明确失败，别默默继续
        qWarning("listen failed on port %d: %s", int(port), qPrintable(server.errorString()));
        return 1;
    }
    qInfo("echo server listening on port %d", int(port));

    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server] {
        QTcpSocket* s = server.nextPendingConnection();
        qInfo("client connected: %s:%d", qPrintable(s->peerAddress().toString()), int(s->peerPort()));

        QObject::connect(s, &QTcpSocket::readyRead, s, [s] {
            const QByteArray data = s->readAll();          // Day2 先不管粘包，原样回显
            s->write(data);
        });
        QObject::connect(s, &QTcpSocket::disconnected, s, [] {
            qInfo("client disconnected");
        });
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    });

    return app.exec();
}
