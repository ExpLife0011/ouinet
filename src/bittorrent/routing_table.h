#pragma once

#include <boost/asio/ip/udp.hpp>

#include <chrono>
#include <deque>

#include "node_id.h"
#include "node_id_range.h"

namespace ouinet { namespace bittorrent { namespace dht {

struct NodeContact {
    NodeID id;
    asio::ip::udp::endpoint endpoint;

    std::string to_string() const;
    inline bool operator==(const NodeContact& other) const { return id == other.id && endpoint == other.endpoint; }
};

struct RoutingNode {
    static inline constexpr std::chrono::minutes QUESTIONABLE_TIMEOUT() { return std::chrono::minutes(15); }

    NodeContact contact;
    std::chrono::steady_clock::time_point last_activity;
    int queries_failed;
    bool questionable_ping_ongoing;

    inline bool is_bad() const { return queries_failed > 3; }
    inline bool is_questionable() const { return last_activity + QUESTIONABLE_TIMEOUT() < std::chrono::steady_clock::now(); }
};

struct RoutingBucket {
    static const int BUCKET_SIZE = 8;

    std::vector<RoutingNode> nodes;
    std::deque<RoutingNode> verified_candidates;
    std::deque<RoutingNode> unverified_candidates;
    /*
     * Verified candidates have replied to a query.
     * Unverified candidates need to be pinged first.
     *
     * The number of nodes plus the number of candidates always stays below
     * BUCKET_SIZE.
     */
};

struct RoutingTreeNode {
    /*
     * A tree node is either a leaf with a bucket pointer,
     * or a non-leaf with children.
     */

    NodeIdRange range;

    RoutingTreeNode(NodeIdRange r) : range(std::move(r)) {}

    void split();
    size_t depth() const { return range.mask; }

    std::unique_ptr<RoutingTreeNode> left_child;
    std::unique_ptr<RoutingTreeNode> right_child;
    std::unique_ptr<RoutingBucket> bucket;
};

class RoutingTable {
    public:
    RoutingTable(NodeID);
    RoutingTable(const RoutingTable&) = delete;
    RoutingBucket* find_bucket(NodeID id, bool split_buckets);
    RoutingTreeNode* root() { return _root_node.get(); }

    std::vector<NodeContact> find_closest_routing_nodes(NodeID target, unsigned int count);

    template<class F> void for_each_bucket(F&&);

    private:
    RoutingTreeNode* exhaustive_routing_subtable_fragment_root() const;

    template<class F> void for_each_bucket(F&&, RoutingTreeNode*);

    private:
    NodeID _node_id;
    std::unique_ptr<RoutingTreeNode> _root_node;
};

template<class F>
void RoutingTable::for_each_bucket(F&& f) {
    for_each_bucket(std::forward<F>(f), _root_node.get());
}

template<class F>
void RoutingTable::for_each_bucket(F&& f, RoutingTreeNode* node) {
    if (node->bucket) {
        f(node->range, *node->bucket);
        return;
    }

    for_each_bucket(std::forward<F>(f), node->left_child.get());
    for_each_bucket(std::forward<F>(f), node->right_child.get());
}

}}} // namespaces

