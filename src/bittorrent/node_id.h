#pragma once

#include <boost/asio/ip/address.hpp>
#include <string>
#include <array>
#include "../namespaces.h"

namespace ouinet { namespace bittorrent {

struct NodeID {
    std::array<unsigned char, 20> buffer;

    bool bit(int n) const;
    std::string to_hex() const;
    std::string to_bytestring() const;
    static NodeID from_bytestring(const std::string& bytestring);
    static const NodeID& zero();
    static NodeID generate(asio::ip::address address);

    bool operator==(const NodeID& other) const { return buffer == other.buffer; }
};

}} // namespaces
