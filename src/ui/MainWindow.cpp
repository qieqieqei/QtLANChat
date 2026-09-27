#include "MainWindow.h"
#include "network/TcpClient.h"
#include "protocol/ProtocolCodec.h"

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QTime>
#include <QJsonObject>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();

    // Day2：先造出网络层，再接线（connectSignals() 里要引用 m_client）
    m_client = new TcpClient(this);

    connectSignals();
    setConnectedUiState(false);          // 初始：未连接
    appendLog("INFO", "QtLANChat Day3 启动（自定义协议）");
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------- UI 构建

void MainWindow::buildUi()
{
    setWindowTitle("QtLANChat - Day3");

    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    // 创建控件
    m_hostEdit = new QLineEdit("127.0.0.1");
    m_portSpin = new QSpinBox();
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8888);
    m_userEdit = new QLineEdit();
    m_userEdit->setPlaceholderText("用户名");
    m_connectBtn = new QPushButton("Connect");
    m_disconnectBtn = new QPushButton("Disconnect");
    m_statusLabel = new QLabel("未连接");
    m_chatView = new QPlainTextEdit();
    m_chatView->setReadOnly(true);
    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_userList = new QListWidget();

    // 顶部工具栏（全部用 Layout，禁止 setGeometry 硬定位）
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel("地址"));
    topBar->addWidget(m_hostEdit, 2);
    topBar->addWidget(new QLabel("端口"));
    topBar->addWidget(m_portSpin);
    topBar->addWidget(new QLabel("用户"));
    topBar->addWidget(m_userEdit, 1);
    topBar->addWidget(m_connectBtn);
    topBar->addWidget(m_disconnectBtn);
    topBar->addWidget(m_statusLabel);
    topBar->addStretch(1);

    // 右侧：在线用户列表
    QWidget* rightPane = new QWidget(central);
    QVBoxLayout* right = new QVBoxLayout(rightPane);
    right->setContentsMargins(0, 0, 0, 0);
    right->addWidget(new QLabel("在线用户"));
    right->addWidget(m_userList, 1);

    // 中间：聊天区 + 用户列表（可拖动分隔条）
    QSplitter* center = new QSplitter(Qt::Horizontal, central);
    center->addWidget(m_chatView);
    center->addWidget(rightPane);
    center->setStretchFactor(0, 3);
    center->setStretchFactor(1, 1);

    // 根布局
    QVBoxLayout* root = new QVBoxLayout(central);
    root->addLayout(topBar);
    root->addWidget(center, 1);
    root->addWidget(new QLabel("日志"));
    root->addWidget(m_logView, 1);

    // Tab 顺序：地址 -> 端口 -> 用户 -> Connect -> Disconnect
    setTabOrder(m_hostEdit, m_portSpin);
    setTabOrder(m_portSpin, m_userEdit);
    setTabOrder(m_userEdit, m_connectBtn);
    setTabOrder(m_connectBtn, m_disconnectBtn);

    resize(960, 640);
    setMinimumSize(720, 480);
}

// ---------------------------------------------------------------- 信号槽

void MainWindow::connectSignals()
{
    // 用函数指针语法，编译期检查签名，拼错直接编译失败（字符串式语法只在运行时报 warn）
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    // 回车快捷：任一输入框按回车 = 点 Connect
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);
    connect(m_userEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);

    // 输入变化 -> 刷新按钮可用性
    connect(m_hostEdit, &QLineEdit::textChanged, this, &MainWindow::updateConnectEnabled);
    connect(m_userEdit, &QLineEdit::textChanged, this, &MainWindow::updateConnectEnabled);

    // ---- Day2：UI 只发信号，真正干活的是 TcpClient ----
    connect(this,     &MainWindow::connectRequested,    m_client, &TcpClient::connectToServer);
    connect(this,     &MainWindow::disconnectRequested, m_client, &TcpClient::disconnectFromServer);

    // 入方向：状态 / 数据 / 错误，全部由网络层驱动 UI
    connect(m_client, &TcpClient::stateChanged,  this, &MainWindow::onConnectionStateChanged);
    connect(m_client, &TcpClient::packetReceived, this, &MainWindow::onPacketReceived);
    connect(m_client, &TcpClient::errorOccurred, this, [this](const QString& msg) {
        appendLog(QStringLiteral("ERROR"), msg);
    });
}

// ---------------------------------------------------------------- 交互逻辑

void MainWindow::onConnectClicked()
{
    const QString host = m_hostEdit->text().trimmed();
    const quint16 port = quint16(m_portSpin->value());
    const QString user = m_userEdit->text().trimmed();

    if (host.isEmpty() || user.isEmpty()) {
        appendLog("WARN", "地址或用户名不能为空");
        return;
    }

    appendLog("INFO", QString("连接请求 %1:%2 用户=%3").arg(host).arg(port).arg(user));
    emit connectRequested(host, port, user);   // 真正的连接由 TcpClient 发起
}

void MainWindow::onDisconnectClicked()
{
    appendLog("INFO", "请求断开");
    emit disconnectRequested();
    // 不断在这里改 UI 状态：等 TcpClient 的 stateChanged(0) 回来再改（单一数据源）
}

void MainWindow::updateConnectEnabled()
{
    const bool ok = !m_hostEdit->text().trimmed().isEmpty()
                    && !m_userEdit->text().trimmed().isEmpty();
    m_connectBtn->setEnabled(ok);
    // 已连接时不该再点 Connect（真实状态以后由 onConnectionStateChanged 决定）
}

void MainWindow::setConnectedUiState(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_hostEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    m_userEdit->setEnabled(!connected);
    m_statusLabel->setText(connected ? "已连接" : "未连接");
}

// ---------------------------------------------------------------- 入方向槽

void MainWindow::onConnectionStateChanged(int state)     // 将来由 ClientService 调用
{
    switch (state) {
    case 0:
        setConnectedUiState(false);
        appendLog("INFO", "已断开");
        break;
    case 1:
        appendLog("INFO", "连接中...");
        break;
    case 2:
        setConnectedUiState(true);
        appendLog("INFO", "已连接");
        break;
    default:
        appendLog("WARN", "未知状态");
        break;
    }
}

// Day3：协议层切好的完整包先到这里；Day5 会换成 ChatManager 分发
void MainWindow::onPacketReceived(const proto::Packet& packet)
{
    const proto::MessageType type = packet.header.type;

    // 聊天类消息的 payload 是紧凑 JSON：{"from":...,"text":...,"time":...}
    if (type == proto::MessageType::ChatPrivate || type == proto::MessageType::ChatGroup) {
        QJsonObject obj;
        if (ProtocolCodec::decodeJson(packet, &obj)) {
            appendChat(obj.value(QStringLiteral("from")).toString(QStringLiteral("server")),
                       obj.value(QStringLiteral("text")).toString(),
                       obj.value(QStringLiteral("time")).toString(
                           QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
            return;
        }
        appendLog(QStringLiteral("WARN"), QStringLiteral("聊天包 JSON 解析失败"));
        return;
    }

    // 其余类型：Day3 先只打日志，证明「字节流 -> 消息」打通了
    appendLog(QStringLiteral("INFO"),
              QStringLiteral("收到 packet type=0x%1 requestId=%2 payloadLen=%3")
                  .arg(int(type), 2, 16, QLatin1Char('0'))
                  .arg(packet.header.requestId)
                  .arg(packet.payload.size()));
}

void MainWindow::onChatMessageReceived(const QString& from, const QString& text, const QString& time)
{
    appendChat(from, text, time);
}

void MainWindow::onUserListChanged(const QStringList& users)
{
    m_userList->clear();
    m_userList->addItems(users);
}

void MainWindow::onLogMessage(const QString& level, const QString& text)
{
    appendLog(level, text);
}

// ---------------------------------------------------------------- 辅助

void MainWindow::appendLog(const QString& level, const QString& text)
{
    m_logView->appendPlainText(QString("[%1] %2 %3")
                                   .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                                   .arg(level, text));
}

void MainWindow::appendChat(const QString& from, const QString& text, const QString& time)
{
    m_chatView->appendPlainText(QString("[%1] %2: %3").arg(time, from, text));
}
