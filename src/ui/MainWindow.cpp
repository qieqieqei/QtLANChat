#include "MainWindow.h"
#include "core/ClientService.h"

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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();

    // Day4：先造出门面（构造内就起线程、moveToThread），再接线
    m_client = new ClientService(this);

    connectSignals();
    refreshUiState();                    // 初始：未连接，发送框禁用
    appendLog("INFO", "QtLANChat Day5 启动（聊天 + 心跳 + 自动重连）");
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------- UI 构建

void MainWindow::buildUi()
{
    setWindowTitle("QtLANChat - Day5");

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

    // Day5：消息输入框 + 发送按钮
    m_msgEdit = new QLineEdit();
    m_msgEdit->setPlaceholderText("输入消息，回车发送（双击右侧用户 = 私聊）");
    m_sendBtn = new QPushButton("Send");

    // objectName：方便 UI 自动化/测试脚本 findChild 定位（也便于排查）
    m_hostEdit->setObjectName("hostEdit");
    m_portSpin->setObjectName("portSpin");
    m_userEdit->setObjectName("userEdit");
    m_connectBtn->setObjectName("connectBtn");
    m_disconnectBtn->setObjectName("disconnectBtn");
    m_statusLabel->setObjectName("statusLabel");
    m_chatView->setObjectName("chatView");
    m_logView->setObjectName("logView");
    m_userList->setObjectName("userList");
    m_msgEdit->setObjectName("msgEdit");
    m_sendBtn->setObjectName("sendBtn");

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

    // 底部：输入框（拉伸）+ 发送按钮
    QHBoxLayout* bottomBar = new QHBoxLayout();
    bottomBar->addWidget(m_msgEdit, 1);
    bottomBar->addWidget(m_sendBtn);

    // 根布局
    QVBoxLayout* root = new QVBoxLayout(central);
    root->addLayout(topBar);
    root->addWidget(center, 1);
    root->addLayout(bottomBar);          // 插在聊天区与日志区之间
    root->addWidget(new QLabel("日志"));
    root->addWidget(m_logView, 1);

    // Tab 顺序：地址 -> 端口 -> 用户 -> Connect -> Disconnect -> 消息 -> Send
    setTabOrder(m_hostEdit, m_portSpin);
    setTabOrder(m_portSpin, m_userEdit);
    setTabOrder(m_userEdit, m_connectBtn);
    setTabOrder(m_connectBtn, m_disconnectBtn);
    setTabOrder(m_disconnectBtn, m_msgEdit);
    setTabOrder(m_msgEdit, m_sendBtn);

    resize(960, 640);
    setMinimumSize(720, 480);
}

// ---------------------------------------------------------------- 信号槽

void MainWindow::connectSignals()
{
    // 用函数指针语法，编译期检查签名，拼错直接编译失败（字符串式语法只在运行时报 warn）
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_msgEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);
    connect(m_userList, &QListWidget::itemDoubleClicked, this, &MainWindow::onUserDoubleClicked);

    // 回车快捷：地址/用户输入框按回车 = 点 Connect
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);
    connect(m_userEdit, &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);

    // 输入变化 -> 刷新按钮可用性
    connect(m_hostEdit, &QLineEdit::textChanged, this, &MainWindow::updateConnectEnabled);
    connect(m_userEdit, &QLineEdit::textChanged, this, &MainWindow::updateConnectEnabled);

    // ---- Day4/Day5：UI 只发信号，真正干活的在网络线程 ----
    connect(this, &MainWindow::connectRequested,    m_client, &ClientService::connectToServer);
    connect(this, &MainWindow::disconnectRequested, m_client, &ClientService::disconnectFromServer);
    connect(this, &MainWindow::sendPrivateRequested, m_client, &ClientService::sendPrivate);
    connect(this, &MainWindow::sendGroupRequested,   m_client, &ClientService::sendGroup);

    // 入方向：状态 / 聊天 / 用户列表 / 错误，全部由网络层驱动 UI（跨线程 Queued）
    connect(m_client, &ClientService::stateChanged,   this, &MainWindow::onConnectionStateChanged);
    connect(m_client, &ClientService::userListChanged, this, &MainWindow::onUserListChanged);
    connect(m_client, &ClientService::privateMessageReceived, this, &MainWindow::onPrivateMessageReceived);
    connect(m_client, &ClientService::groupMessageReceived,   this, &MainWindow::onGroupMessageReceived);
    connect(m_client, &ClientService::loginSucceeded, this, &MainWindow::onLoginSucceeded);
    connect(m_client, &ClientService::loginFailed,    this, &MainWindow::onLoginFailed);
    connect(m_client, &ClientService::reconnectAttempt, this, &MainWindow::onReconnectAttempt);
    connect(m_client, &ClientService::errorOccurred, this, [this](const QString& msg) {
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
    emit connectRequested(host, port, user);   // 真正的连接由 ClientService 投递到网络线程
}

void MainWindow::onDisconnectClicked()
{
    appendLog("INFO", "请求断开");
    emit disconnectRequested();
    // 不在这里改 UI 状态：等 stateChanged(Disconnected) 回来再改（单一数据源）
}

void MainWindow::onSendClicked()
{
    const QString text = m_msgEdit->text().trimmed();
    if (text.isEmpty())
        return;

    if (m_state != ConnectionState::Connected) {
        appendLog("WARN", "未连接，无法发送");    // C4：不静默丢弃，给提示
        return;
    }

    const QString now = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString me  = m_selfName.isEmpty() ? QStringLiteral("我") : m_selfName;

    if (m_targetUser.isEmpty()) {
        emit sendGroupRequested(text);            // 群聊
        appendChat(me, text, now);                // B2：发送方也显示一次
    } else {
        emit sendPrivateRequested(m_targetUser, text);
        appendChat(me, QStringLiteral("-> %1: %2").arg(m_targetUser, text), now);   // B1：发送方显示自己发的
    }

    m_msgEdit->clear();
}

void MainWindow::onUserDoubleClicked(QListWidgetItem* item)
{
    if (!item)
        return;

    const QString name = item->text();
    if (name == m_targetUser) {
        m_targetUser.clear();                     // 再双击一次 = 取消私聊，回到群聊
        m_msgEdit->setPlaceholderText("输入消息，回车发送（双击右侧用户 = 私聊）");
        appendLog("INFO", "已切回群聊");
    } else {
        m_targetUser = name;
        m_msgEdit->setPlaceholderText(QString("私聊 -> %1（再双击一次取消）").arg(name));
        appendLog("INFO", QString("私聊目标：%1").arg(name));
    }
}

void MainWindow::updateConnectEnabled()
{
    const bool ok = m_state == ConnectionState::Disconnected
                    && !m_hostEdit->text().trimmed().isEmpty()
                    && !m_userEdit->text().trimmed().isEmpty();
    m_connectBtn->setEnabled(ok);
}

void MainWindow::updateSendEnabled()
{
    const bool ok = m_state == ConnectionState::Connected;
    m_msgEdit->setEnabled(ok);
    m_sendBtn->setEnabled(ok);
}

void MainWindow::refreshUiState()
{
    const bool offline = m_state == ConnectionState::Disconnected;
    m_hostEdit->setEnabled(offline);
    m_portSpin->setEnabled(offline);
    m_userEdit->setEnabled(offline);
    m_disconnectBtn->setEnabled(!offline);        // Connecting/Connected/Reconnecting 都能点断开
    updateConnectEnabled();
    updateSendEnabled();
}

// ---------------------------------------------------------------- 入方向槽

void MainWindow::onConnectionStateChanged(ConnectionState state)
{
    m_state = state;

    switch (state) {
    case ConnectionState::Disconnected:
        m_statusLabel->setText("未连接");
        appendLog("INFO", "已断开");
        break;
    case ConnectionState::Connecting:
        m_statusLabel->setText("连接中...");
        appendLog("INFO", "连接中...");
        break;
    case ConnectionState::Connected:
        m_statusLabel->setText("已连接");
        appendLog("INFO", "已连接");
        break;
    case ConnectionState::Reconnecting:
        m_statusLabel->setText("重连中...");
        appendLog("WARN", "连接断开，重连中...（输入与发送已禁用，窗口保持打开）");
        break;
    }

    refreshUiState();                             // 界面状态只在这一处改
}

void MainWindow::onUserListChanged(const QStringList& users)
{
    m_userList->clear();
    m_userList->addItems(users);

    // 目标用户下线了就切回群聊，否则会一直发到一个不存在的名字
    if (!m_targetUser.isEmpty() && !users.contains(m_targetUser)) {
        m_targetUser.clear();
        m_msgEdit->setPlaceholderText("输入消息，回车发送（双击右侧用户 = 私聊）");
    }
}

void MainWindow::onPrivateMessageReceived(const QString& from, const QString& text, qint64 ts)
{
    appendChat(from, text, QDateTime::fromMSecsSinceEpoch(ts).toString("HH:mm:ss"));
}

void MainWindow::onGroupMessageReceived(const QString& from, const QString& text, qint64 ts)
{
    appendChat(from, text, QDateTime::fromMSecsSinceEpoch(ts).toString("HH:mm:ss"));
}

void MainWindow::onLoginSucceeded(const QString& selfName)
{
    m_selfName = selfName;
    appendLog("INFO", QString("登录成功，当前用户：%1").arg(selfName));
}

void MainWindow::onLoginFailed(const QString& reason)
{
    appendLog("ERROR", QString("登录失败：%1").arg(reason));
}

void MainWindow::onReconnectAttempt(int attempt, int delayMs)
{
    appendLog("WARN", QString("第 %1 次重连，%2 ms 后重试").arg(attempt).arg(delayMs));
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
