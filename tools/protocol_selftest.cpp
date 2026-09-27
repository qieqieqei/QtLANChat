// tools/protocol_selftest.cpp —— Day3 一次性自测工具，不进最终交付物
//
// 用「把 encode 出来的字节按指定切片喂进 ReceiveBuffer」的方式验证粘包/拆包，
// 不依赖网络、可复现。8 个测试向量必须全部 PASS。
#include <QByteArray>
#include <QList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDataStream>
#include <QIODevice>
#include <cstdio>

#include "protocol/MessageType.h"
#include "protocol/PacketHeader.h"
#include "protocol/Packet.h"
#include "protocol/ReceiveBuffer.h"
#include "protocol/ProtocolCodec.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* what)
{
    if (ok) {
        ++g_pass;
        std::printf("  PASS  %s\n", what);
    } else {
        ++g_fail;
        std::printf("  FAIL  %s\n", what);
    }
}

struct FeedResult {
    QList<proto::Packet> got;
    QString              error;
    int                  leftInBuffer = 0;
};

// 按 cuts 逐片喂入；每喂一片就把能取出的完整包全部取出
static FeedResult feedCuts(const QByteArray& stream, const QList<int>& cuts)
{
    FeedResult r;
    ReceiveBuffer buf;
    int pos = 0;
    for (int cut : cuts) {
        buf.append(stream.mid(pos, cut));
        pos += cut;
        for (;;) {
            proto::Packet p;
            QString e;
            if (!buf.takeNextPacket(&p, &e)) {
                if (!e.isEmpty()) r.error = e;
                break;
            }
            r.got.append(p);
        }
        if (!r.error.isEmpty())
            break;
    }
    if (r.error.isEmpty()) {
        buf.append(stream.mid(pos));
        for (;;) {
            proto::Packet p;
            QString e;
            if (!buf.takeNextPacket(&p, &e)) {
                if (!e.isEmpty()) r.error = e;
                break;
            }
            r.got.append(p);
        }
    }
    r.leftInBuffer = buf.size();
    return r;
}

static QByteArray jsonPayload(const char* text)
{
    QJsonObject o;
    o.insert(QStringLiteral("text"), QString::fromUtf8(text));
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

static QByteArray rawHeader(quint16 magic, quint32 payloadLen)
{
    QByteArray hdr;
    QDataStream ds(&hdr, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << magic << proto::kVersion << quint8(proto::MessageType::ChatGroup)
       << quint16(0) << quint32(1) << payloadLen << quint16(0);
    return hdr;
}

int main()
{
    std::printf("== QtLANChat Day3 protocol self-test ==\n");

    // 0) 常量与头长度
    check(proto::kHeaderSize == 16, "kHeaderSize == 16");
    {
        proto::PacketHeader h;
        check(h.toBytes().size() == 16, "header.toBytes() is exactly 16 bytes");
    }

    const QByteArray payA = jsonPayload("hello A");
    const QByteArray payB = QByteArray();                    // 空载荷
    const QByteArray payC = jsonPayload("中文 C");

    proto::Packet A; A.header.type = proto::MessageType::ChatGroup;   A.header.requestId = 7; A.payload = payA;
    proto::Packet B; B.header.type = proto::MessageType::Heartbeat;   B.header.requestId = 8; B.payload = payB;
    proto::Packet C; C.header.type = proto::MessageType::ChatPrivate; C.header.requestId = 9; C.payload = payC;
    for (auto* p : {&A, &B, &C})
        p->header.payloadLen = quint32(p->payload.size());

    const QByteArray sA = ProtocolCodec::encode(A);
    const QByteArray sB = ProtocolCodec::encode(B);
    const QByteArray sC = ProtocolCodec::encode(C);

    // 头部字段回读（验收 B2/B3）
    {
        proto::PacketHeader h;
        const bool okHdr = proto::PacketHeader::fromBytes(sA, 0, &h);
        check(okHdr, "B2 header fields read back (magic/version/type/flags/requestId/payloadLen)");
        check(h.magic == proto::kMagic && h.version == proto::kVersion
                  && h.type == proto::MessageType::ChatGroup && h.flags == 0
                  && h.requestId == 7 && h.payloadLen == quint32(payA.size()),
              "B2 header values all match");
        check(int(h.payloadLen) == payA.size(), "B3 payloadLen == actual payload size");
        check(quint8(sA.at(0)) == 0x4C && quint8(sA.at(1)) == 0x43, "B1 wire bytes start with 4C 43");
    }

    // 1) 完整包
    {
        const auto r = feedCuts(sA, {int(sA.size())});
        check(r.error.isEmpty() && r.got.size() == 1 && r.got[0].payload == payA,
              "1) complete packet -> 1 packet");
    }

    // 2) 半包：[前 5 字节][剩余]
    {
        const auto r = feedCuts(sA, {5});
        check(r.error.isEmpty() && r.got.size() == 1 && r.got[0].payload == payA,
              "2) split 5 + rest -> 1 packet");
    }

    // 3) 三分：[前 3][再 3][剩余]
    {
        const auto r = feedCuts(sA, {3, 3});
        check(r.error.isEmpty() && r.got.size() == 1 && r.got[0].payload == payA,
              "3) split 3 + 3 + rest -> 1 packet");
    }

    // 4) 粘连：A+B 一次到
    {
        const QByteArray both = sA + sB;
        const auto r = feedCuts(both, {int(both.size())});
        check(r.error.isEmpty() && r.got.size() == 2
                  && r.got[0].payload == payA && r.got[1].payload == payB
                  && r.got[0].header.requestId == 7 && r.got[1].header.requestId == 8,
              "4) sticky A+B -> 2 packets in order");
    }

    // 5) 混合：[A 前 3][A 后 + B 前 1][B 剩余]
    {
        const QByteArray both = sA + sB;
        const auto r = feedCuts(both, {3, int(sA.size()) - 3 + 1});
        check(r.error.isEmpty() && r.got.size() == 2
                  && r.got[0].payload == payA && r.got[1].payload == payB,
              "5) mixed cuts across boundary -> 2 packets");
    }

    // 6) 空载荷：[A][B(len=0)][C] 一次到 -> 3 个包，B.payload.isEmpty()
    {
        const QByteArray all = sA + sB + sC;
        const auto r = feedCuts(all, {int(all.size())});
        check(r.error.isEmpty() && r.got.size() == 3
                  && r.got[1].payload.isEmpty()
                  && r.got[0].payload == payA && r.got[2].payload == payC
                  && r.got[2].header.payloadLen == quint32(payC.size()),
              "6) empty payload packet -> 3 packets, middle empty");
    }

    // 7) 坏 magic -> 上报 error，缓冲被清空，不崩
    {
        const QByteArray bad = rawHeader(0x1234, 4) + QByteArray(4, '\0');
        const auto r = feedCuts(bad, {int(bad.size())});
        check(!r.error.isEmpty() && r.got.isEmpty() && r.leftInBuffer == 0,
              "7) bad magic -> error reported, buffer cleared");
    }

    // 8) 超大长度 -> 上报 error，不尝试分配
    {
        const QByteArray huge = rawHeader(proto::kMagic, 0xFFFFFFFFu);
        const auto r = feedCuts(huge, {int(huge.size())});
        check(!r.error.isEmpty() && r.got.isEmpty() && r.leftInBuffer == 0,
              "8) payloadLen=0xFFFFFFFF -> error, no allocation");
    }

    // 附加：一字节一片喂整包（验收 C3 的离线版）
    {
        QList<int> ones;
        for (int i = 0; i < int(sC.size()) - 1; ++i)
            ones.append(1);
        const auto r = feedCuts(sC, ones);
        check(r.error.isEmpty() && r.got.size() == 1 && r.got[0].payload == payC,
              "extra) 1-byte-at-a-time -> 1 complete packet");
    }

    // 附加：文件数据裸二进制往返
    {
        const QByteArray chunk = QByteArray::fromHex("00FF10AB");
        const QByteArray bytes = ProtocolCodec::encodeFileData(42, 1024, chunk);

        ReceiveBuffer buf;
        buf.append(bytes);
        proto::Packet p;
        QString e;
        const bool gotOne = buf.takeNextPacket(&p, &e);

        quint64 tid = 0;
        quint64 off = 0;
        QByteArray got;
        const bool dec = ProtocolCodec::decodeFileData(p, &tid, &off, &got);

        check(gotOne && e.isEmpty() && dec && tid == 42 && off == 1024 && got == chunk,
              "extra) file data binary round-trip");
    }

    std::printf("== result: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
