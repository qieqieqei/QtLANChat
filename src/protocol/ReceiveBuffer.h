#pragma once

#include <QByteArray>
#include <QString>

#include "Packet.h"

// 只负责「从字节流里切出完整包」，不解析包内容。
// 全局命名空间（与 ProtocolCodec 一致），避免和 proto 里的协议数据混淆。
class ReceiveBuffer
{
public:
    void append(const QByteArray& data);
    int  size() const;
    void clear();

    // 头不够 / 包不完整 -> 返回 false 且缓冲一个字节都不动
    // 解析出错误（magic 错、长度超限）-> 返回 false 并置 error，缓冲被清空
    bool takeNextPacket(proto::Packet* out, QString* error = nullptr);

private:
    QByteArray m_buf;
};
