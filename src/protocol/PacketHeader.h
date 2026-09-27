#pragma once

#include <QtGlobal>
#include <QByteArray>

#include "MessageType.h"

namespace proto {

// 16 字节定长头。字段顺序即线上顺序，全部 Big Endian。
//
//   offset  size  field
//   ------  ----  --------------------------------------------
//        0     2  magic      固定 0x4C43
//        2     1  version    协议版本
//        3     1  type       MessageType
//        4     2  flags      保留（以后做压缩/分片）
//        6     4  requestId  请求/关联 ID，响应原样带回
//       10     4  payloadLen 只算 payload，不含头
//       14     2  reserved   补齐到 16 字节（保证 payload 从偶数偏移 16 开始）
//   ------  ----  --------------------------------------------
//       16         header 总长（payload 紧跟其后）
//
// 注意：设计文档里写「2+1+1+2+4+4 = 16」，但六项实际相加是 14。
// 这里保留 16 字节头的约定（文档里 kHeaderSize=16 反复出现，且 parser 依赖它），
// 多出来的 2 字节用 reserved 显式占位，文档里六个字段的偏移保持不变。
struct PacketHeader {
    quint16     magic      = kMagic;         // 2B  固定 0x4C43
    quint8      version    = kVersion;       // 1B  协议版本
    MessageType type       = MessageType::Heartbeat;  // 1B  消息类型
    quint16     flags      = 0;              // 2B  保留（以后做压缩/分片）
    quint32     requestId  = 0;              // 4B  请求/关联 ID，响应原样带回
    quint32     payloadLen = 0;              // 4B  只算 payload，不含 16 字节头
    quint16     reserved   = 0;              // 2B  补齐到 16 字节，恒为 0

    // 全部 Big Endian，固定 16 字节
    QByteArray toBytes() const;
    static bool fromBytes(const QByteArray& in, int offset, PacketHeader* out);

    bool isValid() const
    {
        return magic == kMagic && version == kVersion && payloadLen <= kMaxPayload;
    }
};

} // namespace proto
