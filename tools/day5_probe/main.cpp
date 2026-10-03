// tools/day5_probe/main.cpp —— Day5 验收探针（不是交付物，Day7 打包不要带）
// 自己拉起 chat_server，两个 ClientService 真实连上去，跑完 A/B/C/D/E 各项再退出。
#include <QCoreApplication>
#include <QDebug>
#include <QMetaType>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include "core/ClientService.h"
#include "network/ConnectionState.h"
#include "protocol/Packet.h"

namespace {

int g_fail = 0;

void check(bool ok, const QString& what)
{
    qInfo("%s  %s", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_fail;
}

struct Stats {
    int                   logins = 0;
    bool                  loginFailed = false;
    QString               failReason;
    QStringList           users;
    int                   priv = 0;
    QString               lastPrivText;
    int                   group = 0;
    int                   reconnects = 0;
    QStringList           states;          // 记录状态变化顺序（用字符串，便于打印）
};

const char* stateName(ConnectionState s)
{
    switch (s) {
    case ConnectionState::Disconnected: return "Disconnected";
    case ConnectionState::Connecting:   return "Connecting";
    case ConnectionState::Connected:    return "Connected";
    case ConnectionState::Reconnecting: return "Reconnecting";
    }
    return "?";
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qRegisterMetaType<proto::Packet>("proto::Packet");
    qRegisterMetaType<ConnectionState>("ConnectionState");

    const QString serverExe = QCoreApplication::applicationDirPath() + "/chat_server.exe";
    const quint16 port = 18888;

    QProcess server;
    auto startServer = [&]() {
        server.setProgram(serverExe);
        server.setArguments({QString::number(port)});
        server.start();
        if (!server.waitForStarted(3000)) {
            check(false, "chat_server 启动");
            return false;
        }
        return true;
    };

    // 唯一一份「会话密钥」：E2 用它验证引号/反斜杠/换行/中文/emoji 原样往返
    const QString special = QStringLiteral("quote\" back\\slash\nnewline 中文 😀 end");

    ClientService a, b, c;
    Stats sa, sb, sc;

    auto wire = [](ClientService& s, Stats& st, const char* tag) {
        QObject::connect(&s, &ClientService::loginSucceeded, [&st, tag](const QString& n) {
            st.logins++;
            qInfo("  [%s] loginSucceeded %s", tag, qPrintable(n));
        });
        QObject::connect(&s, &ClientService::loginFailed, [&st, tag](const QString& r) {
            st.loginFailed = true;
            st.failReason = r;
            qInfo("  [%s] loginFailed %s", tag, qPrintable(r));
        });
        QObject::connect(&s, &ClientService::userListChanged, [&st, tag](const QStringList& u) {
            st.users = u;
            qInfo("  [%s] userList [%s]", tag, qPrintable(u.join(", ")));
        });
        QObject::connect(&s, &ClientService::privateMessageReceived,
                         [&st, tag](const QString& f, const QString& t, qint64) {
            st.priv++;
            st.lastPrivText = t;
            qInfo("  [%s] private from %s", tag, qPrintable(f));
        });
        QObject::connect(&s, &ClientService::groupMessageReceived,
                         [&st, tag](const QString& f, const QString&, qint64) {
            st.group++;
            qInfo("  [%s] group from %s", tag, qPrintable(f));
        });
        QObject::connect(&s, &ClientService::stateChanged, [&st, tag](ConnectionState s) {
            st.states.append(stateName(s));
            qInfo("  [%s] state -> %s", tag, stateName(s));
        });
        QObject::connect(&s, &ClientService::reconnectAttempt, [&st, tag](int n, int ms) {
            st.reconnects++;
            qInfo("  [%s] reconnect #%d in %d ms", tag, n, ms);
        });
        QObject::connect(&s, &ClientService::errorOccurred, [tag](const QString& m) {
            qInfo("  [%s] error: %s", tag, qPrintable(m));
        });
    };
    wire(a, sa, "A/alice");
    wire(b, sb, "B/bob");
    wire(c, sc, "C/dup");

    if (!startServer())
        return 2;

    // ---- 时间线 ----
    QTimer::singleShot(600, [&]() {
        qInfo("-- connect A(alice) B(bob) C(alice 重名) --");
        a.connectToServer("127.0.0.1", port, "alice");
        b.connectToServer("127.0.0.1", port, "bob");
        c.connectToServer("127.0.0.1", port, "alice");      // E1
    });

    QTimer::singleShot(2200, [&]() {
        qInfo("-- B: 登录 / 用户列表 --");
        check(sa.logins >= 1, "B3 A 登录成功");
        check(sb.logins >= 1, "B3 B 登录成功");
        check(sc.loginFailed, "E1 重名 alice 被拒绝");
        check(sb.users.contains("alice") && sb.users.contains("bob"),
              "B3 B 收到用户列表 {alice,bob}");

        qInfo("-- 发消息（含 E2 特殊字符）--");
        a.sendPrivate("bob", special);                     // B1
        a.sendGroup(special);                              // B2
    });

    QTimer::singleShot(3200, [&]() {
        check(sb.priv >= 1, "B1 B 收到 A 的私聊");
        check(sb.lastPrivText == special, "E2 私聊文本原样往返（引号/反斜杠/换行/中文/emoji）");
        check(sb.group >= 1, "B2 B 收到群聊");
        check(sa.group == 0, "B2 群聊不回发给发送方自己");
        check(sc.group == 0, "未登录的连接收不到群聊（服务端只广播给已登录者）");
    });

    QTimer::singleShot(7500, [&]() {
        qInfo("-- C1: 连接存活 5s 以上（至少一次心跳/心跳应答）--");
        check(sa.reconnects == 0 && sb.reconnects == 0, "C1 有心跳保活，未误触发重连");
        check(!sa.states.isEmpty() && sa.states.back() == "Connected", "C1 A 仍是 Connected");

        qInfo("-- 杀掉服务器（模拟崩溃）--");
        server.kill();
        server.waitForFinished(3000);
    });

    QTimer::singleShot(10000, [&]() {
        qInfo("-- C2/C3: 自动重连 + 指数退避 --");
        check(sa.reconnects >= 1 && sb.reconnects >= 1, "C2 检测到掉线并安排重连");
        check(sa.states.contains("Reconnecting"), "C3 A 进入 Reconnecting 状态");
        qInfo("-- 重启服务器 --");
        startServer();
    });

    QTimer::singleShot(16500, [&]() {
        qInfo("-- C3: 重连成功 + 重新登录 --");
        check(sa.logins >= 2 && sb.logins >= 2, "C3 重连成功后自动重新 LOGIN");
        check(!sa.states.isEmpty() && sa.states.back() == "Connected", "C3 A 回到 Connected");
        check(sb.users.contains("alice") && sb.users.contains("bob"), "C3 重连后用户列表恢复");
        check(sc.logins == 0, "E1 被拒的客户端不会在重连后自动重登抢名字");
    });

    QTimer::singleShot(17000, [&]() {
        qInfo("-- 收尾：主动断开（C4 / D1 / D2）--");
        a.disconnectFromServer();
        b.disconnectFromServer();
        c.disconnectFromServer();
    });

    QTimer::singleShot(17800, [&]() { app.quit(); });

    const int rc = app.exec();      // a/b/c 在 main 结束时析构 -> ~ClientService 停线程（D1/D2）

    server.kill();
    server.waitForFinished(3000);

    qInfo("================ Day5 探针结束：失败 %d 项 ================", g_fail);
    return rc != 0 ? rc : (g_fail == 0 ? 0 : 1);
}
