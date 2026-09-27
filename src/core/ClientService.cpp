#include "ClientService.h"

#include <QMetaObject>
#include <QThread>

#include "network/NetworkWorker.h"

ClientService::ClientService(QObject* parent)
    : QObject(parent)
{
    m_thread = new QThread(this);        // 线程对象本身留在 UI 线程（它是「管理者」，不是「执行者」）
    m_worker = new NetworkWorker();      // 故意不给 parent：有 parent 就不能 moveToThread

    m_worker->moveToThread(m_thread);    // 从此 worker 的槽都在网络线程执行

    // 线程生命周期：起来了就让 worker 建 socket；线程结束了让 worker 自己删
    connect(m_thread, &QThread::started,  m_worker, &NetworkWorker::start);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // 工作线程 -> UI：跨线程，AutoConnection 实际走 QueuedConnection（参数会被拷贝后投递）
    connect(m_worker, &NetworkWorker::connected,      this, &ClientService::connected);
    connect(m_worker, &NetworkWorker::disconnected,   this, &ClientService::disconnected);
    connect(m_worker, &NetworkWorker::errorOccurred,  this, &ClientService::errorOccurred);
    connect(m_worker, &NetworkWorker::packetReceived, this, &ClientService::packetReceived);
    connect(m_worker, &NetworkWorker::stateChanged,   this, &ClientService::stateChanged);

    m_thread->start();
}

ClientService::~ClientService()
{
    stopWorker();
}

// ---------------------------------------------------------------- 出方向（都在 UI 线程被调用）

void ClientService::connectToServer(const QString& host, quint16 port)
{
    // 跨线程调用的正确姿势。写成 m_worker->connectToServer(host, port) 是普通函数调用，
    // 会留在 UI 线程执行 —— 那等于没跨线程。
    QMetaObject::invokeMethod(m_worker, "connectToServer", Qt::QueuedConnection,
                              Q_ARG(QString, host), Q_ARG(quint16, port));
}

void ClientService::disconnectFromServer()
{
    QMetaObject::invokeMethod(m_worker, "disconnectFromServer", Qt::QueuedConnection);
}

void ClientService::sendPacket(const proto::Packet& packet)
{
    // proto::Packet 是自定义类型：main() 里必须已经 qRegisterMetaType，
    // 否则运行期报 "Cannot queue arguments of type 'proto::Packet'"。
    QMetaObject::invokeMethod(m_worker, "sendPacket", Qt::QueuedConnection,
                              Q_ARG(proto::Packet, packet));
}

// ---------------------------------------------------------------- 内部

void ClientService::stopWorker()
{
    if (!m_worker)
        return;

    if (m_thread->isRunning()) {
        // 顺序很重要：① 让 worker 在自己的线程里收尾（BlockingQueuedConnection 会等它执行完）
        //            ② quit() 退出事件循环  ③ wait() 等线程真的结束
        // 少了 wait() 就会看到 "QThread: Destroyed while thread is still running"。
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
        m_thread->quit();
        if (!m_thread->wait(3000))
            qWarning("[ClientService] network thread did not finish within 3 s");
        // 线程结束后，worker 由 QThread::finished -> deleteLater 回收（Qt 在线程收尾时会处理延迟删除）
    } else {
        delete m_worker;                 // 线程压根没起来：自己收拾，别漏
    }

    m_worker = nullptr;                  // 之后绝不再用
}
