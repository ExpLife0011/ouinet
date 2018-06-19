#include "routing_table.h"

#include <set>

using namespace ouinet::bittorrent::dht;

RoutingTable::RoutingTable(NodeID node_id) :
    _node_id(node_id)
{
    _root_node = std::make_unique<RoutingTreeNode>();
    _root_node->bucket = std::make_unique<RoutingBucket>();
}

/*
 * Find the routing table bucket containing a particular ID in its namespace.
 *
 * If split_buckets is set, and the containing bucket does not have a routing
 * node in it for the ID, then try to split the bucket until there is room in
 * the resulting bucket for the ID. This may or may not succeed.
 *
 * No matter whether split_buckets is set, the returned bucket may or may not
 * contain a routing node for the target ID, and may or may not have room to
 * add such a node.
 */
RoutingBucket* RoutingTable::find_bucket(NodeID id, bool split_buckets)
{
    RoutingTreeNode* tree_node = _root_node.get();
    std::set<RoutingTreeNode*> ancestors;
    ancestors.insert(tree_node);
    bool node_contains_self = true;
    int depth = 0;
    while (!tree_node->bucket) {
        if (id.bit(depth)) {
            tree_node = tree_node->right_child.get();
        } else {
            tree_node = tree_node->left_child.get();
        }
        if (id.bit(depth) != _node_id.bit(depth)) {
            node_contains_self = false;
        }
        depth++;
        ancestors.insert(tree_node);
    }

    if (split_buckets) {
        /*
         * If the contact is already in this bucket, return it.
         */
        for (size_t i = 0; i < tree_node->bucket->nodes.size(); i++) {
            if (tree_node->bucket->nodes[i].contact.id == id) {
                return tree_node->bucket.get();
            }
        }

        /*
         * If the bucket is full and allowed to be split, split it.
         *
         * A full bucket may be split in three conditions:
         * - if the bucket ID space contains _node_id;
         * - if the bucket depth is not a multiple of TREE_BASE (this turns the
         *   routing table into a 2^TREE_BASE-ary tree rather than a binary
         *   one, and saves in round trips);
         * - if the bucket is a descendent of the deepest ancestor of _node_id
         *   that contains at least BUCKET_SIZE nodes.
         */
        const int TREE_BASE = 5;
        RoutingTreeNode* exhaustive_root = exhaustive_routing_subtable_fragment_root();

        while (tree_node->bucket->nodes.size() == RoutingBucket::BUCKET_SIZE && depth < 160) {
            if (
                !node_contains_self
                && (depth % TREE_BASE) == 0
                && !ancestors.count(exhaustive_root)
            ) {
                break;
            }

            assert(tree_node->bucket);
            assert(!tree_node->left_child);
            assert(!tree_node->right_child);

            /*
             * Buckets that may be split are never supposed to have candidates
             * in them.
             */
            assert(tree_node->bucket->verified_candidates.empty());
            assert(tree_node->bucket->unverified_candidates.empty());

            /*
             * Split the bucket.
             */
            tree_node->left_child = std::make_unique<RoutingTreeNode>();
            tree_node->left_child->bucket = std::make_unique<RoutingBucket>();
            tree_node->right_child = std::make_unique<RoutingTreeNode>();
            tree_node->right_child->bucket = std::make_unique<RoutingBucket>();

            for (const auto& node : tree_node->bucket->nodes) {
                if (node.contact.id.bit(depth)) {
                    tree_node->right_child->bucket->nodes.push_back(node);
                } else {
                    tree_node->left_child->bucket->nodes.push_back(node);
                }
            }
            tree_node->bucket = nullptr;

            if (id.bit(depth)) {
                tree_node = tree_node->right_child.get();
            } else {
                tree_node = tree_node->left_child.get();
            }
            if (_node_id.bit(depth) != id.bit(depth)) {
                node_contains_self = false;
            }
            depth++;
            ancestors.insert(tree_node);

            // TODO: each bucket needs a refresh background coroutine.
        }
    }

    return tree_node->bucket.get();
}

static int count_nodes_in_subtree(RoutingTreeNode* tree_node)
{
    if (tree_node->bucket) {
        return tree_node->bucket->nodes.size();
    } else {
        return count_nodes_in_subtree(tree_node->left_child.get())
             + count_nodes_in_subtree(tree_node->right_child.get());
    }
}

/*
 * The routing table contains every known good node in the smallest subtree
 * that contains _node_id and has at least BUCKET_SIZE contacts in it.
 * This function computes the root of that subtree. Routing tree nodes
 * below this node may always be split when full.
 */
RoutingTreeNode* RoutingTable::exhaustive_routing_subtable_fragment_root() const
{
    std::vector<RoutingTreeNode*> path;
    RoutingTreeNode* tree_node = _root_node.get();

    while (!tree_node->bucket) {
        path.push_back(tree_node);
        if (_node_id.bit(path.size())) {
            tree_node = tree_node->right_child.get();
        } else {
            tree_node = tree_node->left_child.get();
        }
    }

    int size = tree_node->bucket->nodes.size();

    while (size < RoutingBucket::BUCKET_SIZE && !path.empty()) {
        tree_node = path.back();
        path.pop_back();
        if (_node_id.bit(path.size())) {
            size += count_nodes_in_subtree(tree_node->left_child.get());
        } else {
            size += count_nodes_in_subtree(tree_node->right_child.get());
        }
    }

    return tree_node;
}

