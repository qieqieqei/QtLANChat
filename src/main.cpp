#include <QApplication>
#include <QMetaType>

#include "network/ConnectionState.h"
#include "protocol/Packet.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Day4：proto::Packet 要跨线程（QueuedConnection）传递，必须早于任何跨线程使用先注册。
    // 不注册的话运行期会报：Cannot queue arguments of type 'proto::Packet'，槽根本不触发。
    qRegisterMetaType<proto::Packet>("proto::Packet");

    // Day5：状态枚举同样跨线程（网络线程 -> UI 线程），也要注册，否则状态信号被静默丢弃。
    qRegisterMetaType<ConnectionState>("ConnectionState");

    MainWindow w;
    w.show();
    return app.exec();
}
