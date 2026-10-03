#pragma once

#include <QMetaType>

// Day5：连接状态第一次从 Day1 的 int 占位升级成真正的枚举。
// 跨线程信号必须注册 meta type，否则排队投递会被静默丢弃（Day5 坑清单第 8 条）：
//   main(): qRegisterMetaType<ConnectionState>("ConnectionState");
enum class ConnectionState {
    Disconnected,   // 未连接（含用户主动断开）
    Connecting,     // 正在连接
    Connected,      // 已连接（心跳生效中）
    Reconnecting    // 断线了，等退避定时器再次尝试
};

Q_DECLARE_METATYPE(ConnectionState)

// 状态的中文名：日志和界面共用一份，别在几个 switch 里各写一遍
inline const char* connectionStateName(ConnectionState s)
{
    switch (s) {
    case ConnectionState::Disconnected: return "未连接";
    case ConnectionState::Connecting:   return "连接中";
    case ConnectionState::Connected:    return "已连接";
    case ConnectionState::Reconnecting: return "重连中";
    }
    return "未知状态";
}
