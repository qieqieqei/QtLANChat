#include "PacketHeader.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

namespace proto {

QByteArray PacketHeader::toBytes() const
{
    QByteArray out;
    out.reserve(kHeaderSize);

    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);          // 两端必须一致，写死
    ds << magic
       << version
       << quint8(type)                                // QDataStream 没有 MessageType 重载，必须显式转 quint8
       << flags
       << requestId
       << payloadLen
       << reserved;                                   // 2B 补齐 -> 总共 16B

    return out;
}

bool PacketHeader::fromBytes(const QByteArray& in, int offset, PacketHeader* out)
{
    if (out == nullptr)
        return false;
    if (offset < 0 || in.size() - offset < kHeaderSize)
        return false;                                 // 连头都不够：交给调用方继续等

    const QByteArray slice = in.mid(offset, kHeaderSize);
    QDataStream ds(slice);
    ds.setByteOrder(QDataStream::BigEndian);

    quint8 type = 0;
    ds >> out->magic >> out->version >> type >> out->flags
       >> out->requestId >> out->payloadLen >> out->reserved;

    if (ds.status() != QDataStream::Ok)
        return false;

    out->type = static_cast<MessageType>(type);

    // 这里只判结构和魔数/版本；payloadLen 的上限由 ReceiveBuffer 单独报错，
    // 好让「坏 magic」和「payload 超限」是两个可区分的错误。
    return out->magic == kMagic && out->version == kVersion;
}

} // namespace proto
