#pragma once

#include <QByteArray>
#include <QJsonObject>

#include "MessageType.h"
#include "Packet.h"

// 只负责「Packet <-> QByteArray」与 JSON 编解码。
// 这里不许 include 任何 QWidget/QMessageBox —— 纯字节转换，Day4 才能整体丢进工作线程。
class ProtocolCodec
{
public:
    static QByteArray encode(const proto::Packet& pkt);
    // 直接产出「可投递给 NetworkWorker::sendPacket 的 Packet」（ChatManager/ConnectionManager 用）
    static proto::Packet makeJson(proto::MessageType type, const QJsonObject& obj, quint32 requestId = 0);
    static QByteArray encodeJson(proto::MessageType type, const QJsonObject& obj, quint32 requestId = 0);
    static bool       decodeJson(const proto::Packet& pkt, QJsonObject* out);

    // 文件数据走裸二进制，不做 JSON。
    // payload 布局：[transferId 8B BE][offset 8B BE][chunk ...]
    static QByteArray encodeFileData(quint64 transferId, quint64 offset, const QByteArray& chunk);
    static bool       decodeFileData(const proto::Packet& pkt, quint64* transferId,
                                     quint64* offset, QByteArray* chunk);
};
