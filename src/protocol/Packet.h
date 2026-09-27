#pragma once

#include <QByteArray>

#include "PacketHeader.h"

namespace proto {

struct Packet {
    PacketHeader header;
    QByteArray   payload;
};

} // namespace proto
