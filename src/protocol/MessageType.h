#pragma once

#include <QtGlobal>

namespace proto {

constexpr quint16 kMagic      = 0x4C43;            // "LC"，Little Chat
constexpr quint8  kVersion    = 1;
constexpr int     kHeaderSize = 16;                // 2+1+1+2+4+4 (+2 保留) = 16
constexpr quint32 kMaxPayload = 8u * 1024 * 1024;  // 8 MB 上限，防止坏长度撑爆内存

enum class MessageType : quint8 {
    Login        = 0x01,
    LoginAck     = 0x02,
    ChatPrivate  = 0x10,
    ChatGroup    = 0x11,
    UserList     = 0x20,
    UserJoin     = 0x21,
    UserLeave    = 0x22,
    FileMeta     = 0x30,
    FileData     = 0x31,
    FileFinish   = 0x32,
    FileCancel   = 0x33,
    Heartbeat    = 0x40,
    HeartbeatAck = 0x41,
    Error        = 0x50,
};

} // namespace proto
