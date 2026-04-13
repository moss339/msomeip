#pragma once

#include <cstdint>
#include <cstddef>
#include <arpa/inet.h>

namespace moss {
namespace msomeip {
namespace tp {

// SOME/IP-TP header (4 bytes after standard SOME/IP header)
// +----------------+----------------+----------------+----------------+
// |   Offset (24 bits)            |M|    Reserved (7 bits)          |
// +----------------+----------------+----------------+----------------+
// M = More flag (1 = more segments follow, 0 = last segment)

struct TpHeader {
    uint32_t offset : 24;
    uint32_t reserved : 7;
    uint32_t more_flag : 1;

    TpHeader() : offset(0), reserved(0), more_flag(0) {}

    TpHeader(uint32_t off, bool more)
        : offset(off), reserved(0), more_flag(more ? 1 : 0) {}

    uint32_t to_network() const {
        uint32_t value = (offset << 8) | (reserved << 1) | more_flag;
        return htonl(value);
    }

    static TpHeader from_network(uint32_t net_value) {
        uint32_t value = ntohl(net_value);
        TpHeader header;
        header.offset = (value >> 8) & 0x00FFFFFF;
        header.reserved = (value >> 1) & 0x7F;
        header.more_flag = value & 0x01;
        return header;
    }
};

static_assert(sizeof(TpHeader) == 4, "TpHeader must be 4 bytes");

// Segment size limits
constexpr size_t UDP_SEGMENT_SIZE = 1392;  // UDP MTU - IP header - UDP header - SOME/IP header
constexpr size_t TCP_SEGMENT_SIZE = 4095;  // SOME/IP-TP max segment size

// Reassembly timeout
constexpr uint32_t DEFAULT_REASSEMBLY_TIMEOUT_MS = 5000;

} // namespace tp
} // namespace msomeip
} // namespace moss
