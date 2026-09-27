#pragma once

#include <QMainWindow>
#include <QStringList>   // 信号参数用到 QStringList，必须完整可见

class QLineEdit;
class QSpinBox;
class QPushButton;
class QPlainTextEdit;
class QListWidget;
class QLabel;

class TcpClient;                 // Day2：网络层，UI 只连它的信号（不直接碰 QTcpSocket）

class MainWindow : public QMainWindow
{
    Q_OBJECT                     // 有它才会跑 moc，缺了信号槽就废

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    // 出方向：UI 只发信号，不碰网络。Day1 这些信号没有接收者也能编译
    void connectRequested(const QString& host, quint16 port, const QString& userName);
    void disconnectRequested();

public slots:
    // 入方向：将来由 ClientService 驱动 UI。Day1 用 int 占位，避免依赖还没写的枚举
    void onConnectionStateChanged(int state);   // TODO(Day5): 换成 ConnectionState + qRegisterMetaType
    void onChatMessageReceived(const QString& from, const QString& text, const QString& time);
    void onUserListChanged(const QStringList& users);
    void onLogMessage(const QString& level, const QString& text);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();

private:
    void buildUi();                       // 创建控件 + 布局
    void connectSignals();                // 集中做 connect
    void updateConnectEnabled();          // 输入变化时刷新按钮可用性
    void setConnectedUiState(bool connected);
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
    QLabel*         m_statusLabel   = nullptr;   // 状态提示（可选但对调试很有用）

    // Day2：真实的 TCP 客户端。所有权交给 Qt（parent=this），不要手动 delete
    TcpClient*      m_client        = nullptr;
};
