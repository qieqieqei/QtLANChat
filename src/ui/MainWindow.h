#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "network/ConnectionState.h"   // 状态槽的参数类型

class QLineEdit;
class QSpinBox;
class QPushButton;
class QPlainTextEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;

class ClientService;            // Day4：网络线程的门面，UI 只连它的信号（不直接碰 QTcpSocket/线程）

class MainWindow : public QMainWindow
{
    Q_OBJECT                     // 有它才会跑 moc，缺了信号槽就废

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    // 出方向：UI 只发信号，不碰网络
    void connectRequested(const QString& host, quint16 port, const QString& userName);
    void disconnectRequested();
    void sendPrivateRequested(const QString& to, const QString& text);   // Day5
    void sendGroupRequested(const QString& text);                        // Day5

public slots:
    // 入方向：由 ClientService 驱动 UI
    void onConnectionStateChanged(ConnectionState state);                // Day5：int -> 枚举
    void onUserListChanged(const QStringList& users);
    void onPrivateMessageReceived(const QString& from, const QString& text, qint64 ts);
    void onGroupMessageReceived(const QString& from, const QString& text, qint64 ts);
    void onLoginSucceeded(const QString& selfName);
    void onLoginFailed(const QString& reason);
    void onReconnectAttempt(int attempt, int delayMs);
    void onLogMessage(const QString& level, const QString& text);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onUserDoubleClicked(QListWidgetItem* item);

private:
    void buildUi();                       // 创建控件 + 布局
    void connectSignals();                // 集中做 connect
    void refreshUiState();                // 状态/按钮可用性的统一入口
    void updateConnectEnabled();          // 输入或被状态变化时刷新 Connect 可用性
    void updateSendEnabled();             // 发送按钮是否可点，只由这里决定
    void appendLog(const QString& level, const QString& text);
    void appendChat(const QString& from, const QString& text, const QString& time);

    // 控件指针：所有权交给布局系统（addWidget 会自动 reparent），不要手动 delete
    QLineEdit*      m_hostEdit      = nullptr;   // 服务器地址
    QSpinBox*       m_portSpin      = nullptr;   // 端口
    QLineEdit*      m_userEdit      = nullptr;   // 用户名
    QPushButton*    m_connectBtn    = nullptr;   // Connect
    QPushButton*    m_disconnectBtn = nullptr;   // Disconnect
    QPlainTextEdit* m_chatView      = nullptr;   // 聊天显示区（只读）
    QListWidget*    m_userList      = nullptr;   // 用户列表
    QPlainTextEdit* m_logView       = nullptr;   // 日志区（只读）
    QLabel*         m_statusLabel   = nullptr;   // 状态提示
    QLineEdit*      m_msgEdit       = nullptr;   // Day5：消息输入框
    QPushButton*    m_sendBtn       = nullptr;   // Day5：发送按钮

    // Day4：网络对象整体搬进工作线程后，UI 唯一需要认识的就是这个门面
    ClientService*  m_client        = nullptr;

    // Day5：界面状态
    ConnectionState m_state         = ConnectionState::Disconnected;
    QString         m_selfName;                  // 登录成功后自己的名字（聊天区显示用）
    QString         m_targetUser;                // 私聊目标；空 = 发群聊
};
