// tools/gui_probe/main.cpp —— Day5 GUI 取证（不是交付物）：真实 windows 平台跑一遍 UI 并截图
// 用法：先启动 chat_server.exe <port>，再运行本程序，产出 gui_day5.png
#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QMetaType>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QDebug>

#include "network/ConnectionState.h"
#include "protocol/Packet.h"
#include "ui/MainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    qRegisterMetaType<proto::Packet>("proto::Packet");
    qRegisterMetaType<ConnectionState>("ConnectionState");

    const quint16 port = (argc > 1) ? quint16(QString(argv[1]).toUShort()) : quint16(18888);

    MainWindow w;
    w.show();

    auto* host = w.findChild<QLineEdit*>("hostEdit");
    auto* spin = w.findChild<QSpinBox*>("portSpin");
    auto* user = w.findChild<QLineEdit*>("userEdit");
    auto* conn = w.findChild<QPushButton*>("connectBtn");
    auto* msg  = w.findChild<QLineEdit*>("msgEdit");
    auto* send = w.findChild<QPushButton*>("sendBtn");
    if (!host || !spin || !user || !conn || !msg || !send) {
        qWarning("gui_probe: 找不到控件（objectName 缺失？）");
        return 2;
    }

    host->setText("127.0.0.1");
    spin->setValue(port);
    user->setText("alice");

    QTimer::singleShot(400, [&]() {
        qInfo("gui_probe: 点击 Connect");
        conn->click();
    });
    QTimer::singleShot(1600, [&]() {
        qInfo("gui_probe: 输入并发送消息");
        msg->setText(QStringLiteral("你好，这是 Day5 的 GUI 取证 😀"));
        send->click();
    });
    QTimer::singleShot(2200, [&]() {
        const QString path = QDir::currentPath() + "/gui_day5.png";
        const bool ok = w.grab().save(path);
        qInfo("gui_probe: 截图 %s -> %s", ok ? "OK" : "FAIL", qPrintable(path));
        app.quit();
    });

    return app.exec();
}
