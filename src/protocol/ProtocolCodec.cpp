#include "ProtocolCodec.h"

#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>

// ---------------------------------------------------------------- 组包

QByteArray ProtocolCodec::encode(const proto::Packet& pkt)
{
    QByteArray out;
    out.reserve(proto::kHeaderSize + pkt.payload.size());

    // 用 header.toBytes() 保证「头怎么写」只有一份实现（免得两处大小端不一致）
    out += pkt.header.toBytes();
    out += pkt.payload;
    return out;
}

QByteArray ProtocolCodec::encodeJson(proto::MessageType type, const QJsonObject& obj, quint32 requestId)
{
    // Compact！不要 Indented —— 缩进会多出几十字节，进协议就是灾难
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    proto::Packet pkt;
    pkt.header.magic      = proto::kMagic;
    pkt.header.version    = proto::kVersion;
    pkt.header.type       = type;
    pkt.header.flags      = 0;
    pkt.header.requestId  = requestId;
    pkt.header.payloadLen = quint32(payload.size());   // 只算 payload
    pkt.payload           = payload;

    return encode(pkt);
}

bool ProtocolCodec::decodeJson(const proto::Packet& pkt, QJsonObject* out)
{
    if (out == nullptr || pkt.payload.isEmpty())
        return false;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(pkt.payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;                                  // 只收对象，数组/标量都不认

    *out = doc.object();
    return true;
}

// ---------------------------------------------------------------- 文件数据（裸二进制）

QByteArray ProtocolCodec::encodeFileData(quint64 transferId, quint64 offset, const QByteArray& chunk)
{
    QByteArray payload;
    payload.reserve(16 + chunk.size());

    QDataStream ds(&payload, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << quint64(transferId) << quint64(offset);      // 16B 前缀
    payload += chunk;                                  // 后面是裸数据

    proto::Packet pkt;
    pkt.header.type       = proto::MessageType::FileData;
    pkt.header.payloadLen = quint32(payload.size());
    pkt.payload           = payload;

    return encode(pkt);
}

bool ProtocolCodec::decodeFileData(const proto::Packet& pkt, quint64* transferId,
                                   quint64* offset, QByteArray* chunk)
{
    if (transferId == nullptr || offset == nullptr || chunk == nullptr)
        return false;
    if (pkt.header.type != proto::MessageType::FileData)
        return false;
    if (pkt.payload.size() < 16)
        return false;                                  // 前缀都不够，别越界读

    QDataStream ds(pkt.payload);
    ds.setByteOrder(QDataStream::BigEndian);
    ds >> *transferId >> *offset;
    if (ds.status() != QDataStream::Ok)
        return false;

    *chunk = pkt.payload.mid(16);
    return true;
}
