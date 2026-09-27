#include "ReceiveBuffer.h"

void ReceiveBuffer::append(const QByteArray& data)
{
    m_buf += data;
}

int ReceiveBuffer::size() const
{
    return m_buf.size();
}

void ReceiveBuffer::clear()
{
    m_buf.clear();
}

bool ReceiveBuffer::takeNextPacket(proto::Packet* out, QString* error)
{
    if (out == nullptr)
        return false;

    // ① 连头都不够，继续等
    if (m_buf.size() < proto::kHeaderSize)
        return false;

    proto::PacketHeader h;

    // ② 垃圾流：magic/version 不对，丢弃并上报
    if (!proto::PacketHeader::fromBytes(m_buf, 0, &h)) {
        if (error != nullptr)
            *error = QStringLiteral("bad magic/version");
        m_buf.clear();
        return false;
    }

    // ③ 防爆内存：损坏或恶意的长度不尝试分配
    if (h.payloadLen > proto::kMaxPayload) {
        if (error != nullptr)
            *error = QStringLiteral("payload too large");
        m_buf.clear();
        return false;
    }

    const int total = proto::kHeaderSize + int(h.payloadLen);

    // ④ 包不完整，一个字节都不动
    if (m_buf.size() < total)
        return false;

    // ⑤ 只在这一刻消费缓冲
    out->header  = h;
    out->payload = m_buf.mid(proto::kHeaderSize, int(h.payloadLen));
    m_buf.remove(0, total);
    return true;
}
