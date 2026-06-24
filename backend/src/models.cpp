#include "models.hpp"

#include <algorithm>

namespace topoopt {

std::size_t PairHash::operator()(const std::pair<int, int>& value) const noexcept {
    const auto lhs = static_cast<std::size_t>(value.first);
    const auto rhs = static_cast<std::size_t>(value.second);
    return (lhs << 32U) ^ rhs;
}

std::pair<int, int> ordered_pair(int a, int b) {
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

Node* BusTopology::add_node(Node node) {
    auto stored = std::make_unique<Node>(std::move(node));
    Node* ptr = stored.get();
    nodes[ptr->id] = ptr;
    owned_nodes_.push_back(std::move(stored));
    return ptr;
}

PhysicalLink* BusTopology::add_physical_link(PhysicalLink link) {
    auto stored = std::make_unique<PhysicalLink>(std::move(link));
    PhysicalLink* ptr = stored.get();
    physical_links[ptr->id] = ptr;
    physical_link_map_[ordered_pair(ptr->node_a->id, ptr->node_b->id)] = ptr;
    ptr->node_a->physical_links.push_back(ptr);
    ptr->node_b->physical_links.push_back(ptr);
    owned_physical_links_.push_back(std::move(stored));
    return ptr;
}

BusLink* BusTopology::add_bus_link(BusLink link) {
    auto stored = std::make_unique<BusLink>(std::move(link));
    BusLink* ptr = stored.get();
    bus_links[ptr->id] = ptr;
    bus_link_map_[ordered_pair(ptr->node_a->id, ptr->node_b->id)] = ptr;
    ptr->node_a->bus_links.push_back(ptr);
    ptr->node_b->bus_links.push_back(ptr);
    owned_bus_links_.push_back(std::move(stored));
    return ptr;
}

Flow* BusTopology::add_flow(Flow flow) {
    auto stored = std::make_unique<Flow>(std::move(flow));
    Flow* ptr = stored.get();
    owned_flows_.push_back(std::move(stored));
    if (ptr->source_node != nullptr) {
        ptr->source_node->source_flows.push_back(ptr);
    }
    if (ptr->target_node != nullptr) {
        ptr->target_node->sink_flows.push_back(ptr);
    }
    return ptr;
}

Node* BusTopology::get_node(int id) const {
    auto it = nodes.find(id);
    return it == nodes.end() ? nullptr : it->second;
}

PhysicalLink* BusTopology::get_physical_link(const Node* a, const Node* b) const {
    if (a == nullptr || b == nullptr) {
        return nullptr;
    }
    auto it = physical_link_map_.find(ordered_pair(a->id, b->id));
    return it == physical_link_map_.end() ? nullptr : it->second;
}

BusLink* BusTopology::get_bus_link(const Node* a, const Node* b) const {
    if (a == nullptr || b == nullptr) {
        return nullptr;
    }
    auto it = bus_link_map_.find(ordered_pair(a->id, b->id));
    return it == bus_link_map_.end() ? nullptr : it->second;
}

std::vector<Node*> BusTopology::node_list() const {
    std::vector<Node*> out;
    out.reserve(nodes.size());
    for (const auto& item : nodes) {
        out.push_back(item.second);
    }
    std::sort(out.begin(), out.end(), [](const Node* a, const Node* b) { return a->id < b->id; });
    return out;
}

std::vector<PhysicalLink*> BusTopology::physical_link_list() const {
    std::vector<PhysicalLink*> out;
    out.reserve(physical_links.size());
    for (const auto& item : physical_links) {
        out.push_back(item.second);
    }
    std::sort(out.begin(), out.end(), [](const PhysicalLink* a, const PhysicalLink* b) { return a->id < b->id; });
    return out;
}

std::vector<BusLink*> BusTopology::bus_link_list() const {
    std::vector<BusLink*> out;
    out.reserve(bus_links.size());
    for (const auto& item : bus_links) {
        out.push_back(item.second);
    }
    std::sort(out.begin(), out.end(), [](const BusLink* a, const BusLink* b) { return a->id < b->id; });
    return out;
}

std::vector<Flow*> BusTopology::flow_list() const {
    std::vector<Flow*> out;
    out.reserve(owned_flows_.size());
    for (const auto& flow : owned_flows_) {
        out.push_back(flow.get());
    }
    std::sort(out.begin(), out.end(), [](const Flow* a, const Flow* b) { return a->id < b->id; });
    return out;
}

bool contains_flow(const std::vector<Flow*>& flows, const Flow* flow) {
    return std::find(flows.begin(), flows.end(), flow) != flows.end();
}

double flow_bandwidth_demand(const Flow& flow) {
    const double period = flow.period > 0.0 ? flow.period : 0.001;
    return flow.message_size / period;
}

} // namespace topoopt
