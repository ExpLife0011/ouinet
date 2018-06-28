#pragma once

#include <boost/asio/ip/address.hpp>
#include <string>
#include <array>
#include "../namespaces.h"

namespace ouinet { namespace bittorrent {

struct NodeID {
    using Buffer = std::array<uint8_t, 20>;

    struct Range {
        Buffer stencil;
        size_t mask;

        NodeID random_id() const;
        Range reduce(bool bit) const;

        static const Range& max();
    };

    Buffer buffer;

    // XXX: `bit(0)` is the most signifficant, perhaps the function should be
    // called `rbit` ('r' for reverse)?
    bool bit(int n) const;
    void set_bit(int n, bool value);

    std::string to_hex() const;
    std::string to_bytestring() const;
    static NodeID from_bytestring(const std::string& bytestring);
    static const NodeID& zero();
    static NodeID generate(asio::ip::address address);

    bool operator==(const NodeID& other) const { return buffer == other.buffer; }
};

}} // namespaces
