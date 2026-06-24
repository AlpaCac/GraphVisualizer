#include "routing.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace topoopt {

namespace {
constexpr double INF = std::numeric_limits<double>::infinity();

double node_static_load(const std::vector<Flow*>& flows) {
    double load = 0.0;
    for (const Flow* flow : flows) {
        load += flow_bandwidth_demand(*flow);
    }
    return load;
}
} // namespace

DeterministicRouter::DeterministicRouter(double congestion_penalty_alpha)
    : alpha_(congestion_penalty_alpha) {}

void DeterministicRouter::reset_routing_states(BusTopology& topology, const std::vector<Flow*>& flows) const {
    for (BusLink* link : topology.bus_link_list()) {
        link->dynamic_weight = 0.0;
        link->current_load = 0.0;
        link->physical_path.clear();
    }

    for (PhysicalLink* link : topology.physical_link_list()) {
        link->current_load = 0.0;
        link->passing_flows.clear();
    }

    for (Node* node : topology.node_list()) {
        node->relay_flows.clear();
        if (node->cpu_capacity > 0.0) {
            node->dynamic_weight = alpha_ * ((node_static_load(node->source_flows) + node_static_load(node->sink_flows)) /
                                             node->cpu_capacity);
        } else {
            node->dynamic_weight = 0.0;
        }
    }

    for (Flow* flow : flows) {
        flow->logical_routing_path.clear();
        flow->physical_routing_path.clear();
        flow->worst_case_delay.reset();
        flow->actual_reliability.reset();
        flow->actual_throughput_gbps.reset();
        flow->throughput_score.reset();
        flow->is_schedulable = true;
    }
}

void DeterministicRouter::route_all_flows(BusTopology& topology) const {
    auto flows = topology.flow_list();
    reset_routing_states(topology, flows);

    std::sort(flows.begin(), flows.end(), [](const Flow* a, const Flow* b) {
        if (a->priority != b->priority) {
            return a->priority > b->priority;
        }
        return a->id < b->id;
    });

    for (Flow* flow : flows) {
        const double demand = flow_bandwidth_demand(*flow);
        auto [logic_path, physical_path, physical_segments] = find_deterministic_path(
            topology, flow->source_node, flow->target_node, demand, flow);

        flow->logical_routing_path = std::move(logic_path);
        flow->physical_routing_path = std::move(physical_path);

        if (flow->logical_routing_path.size() > 1 && flow->physical_routing_path.size() > 1) {
            update_link_states(topology, flow, demand, physical_segments);
        }
    }
}

std::tuple<std::vector<Node*>, std::vector<Node*>, DeterministicRouter::PathSegments>
DeterministicRouter::find_deterministic_path(
    BusTopology& topology,
    Node* source_node,
    Node* target_node,
    double demand,
    Flow* flow) const {
    std::unordered_map<int, double> distances;
    std::unordered_map<int, Node*> predecessors;
    std::unordered_map<int, std::vector<Node*>> predecessor_physical_paths;

    for (Node* node : topology.node_list()) {
        distances[node->id] = INF;
        predecessors[node->id] = nullptr;
    }
    distances[source_node->id] = 0.0;

    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    std::unordered_set<int> visited;
    pq.push({0.0, source_node->id});

    while (!pq.empty()) {
        const auto [current_dist, current_id] = pq.top();
        pq.pop();
        if (visited.count(current_id) != 0U) {
            continue;
        }
        visited.insert(current_id);

        Node* current_node = topology.get_node(current_id);
        if (current_id == target_node->id) {
            break;
        }

        for (BusLink* bus_link : current_node->bus_links) {
            Node* neighbor = bus_link->node_b->id == current_id ? bus_link->node_a : bus_link->node_b;
            if (visited.count(neighbor->id) != 0U) {
                continue;
            }

            auto [physical_segment, total_link_cost] = find_physical_shortest_path(current_node, neighbor, demand, flow);
            if (physical_segment.empty()) {
                continue;
            }

            const double alt_dist = current_dist + total_link_cost;
            if (alt_dist < distances[neighbor->id]) {
                distances[neighbor->id] = alt_dist;
                predecessors[neighbor->id] = current_node;
                predecessor_physical_paths[neighbor->id] = std::move(physical_segment);
                pq.push({alt_dist, neighbor->id});
            } else if (std::abs(alt_dist - distances[neighbor->id]) < 1e-12) {
                Node* previous = predecessors[neighbor->id];
                if (previous != nullptr && current_id < previous->id) {
                    predecessors[neighbor->id] = current_node;
                    predecessor_physical_paths[neighbor->id] = std::move(physical_segment);
                }
            }
        }
    }

    if (predecessors[target_node->id] == nullptr && target_node->id != source_node->id) {
        return {{}, {}, {}};
    }

    std::vector<Node*> logic_path;
    PathSegments physical_segments;
    int current_id = target_node->id;
    while (true) {
        Node* current = topology.get_node(current_id);
        logic_path.push_back(current);
        Node* previous = predecessors[current_id];
        if (previous == nullptr) {
            break;
        }
        physical_segments.push_back(predecessor_physical_paths[current_id]);
        current_id = previous->id;
    }

    std::reverse(logic_path.begin(), logic_path.end());
    std::reverse(physical_segments.begin(), physical_segments.end());

    std::vector<Node*> physical_path;
    if (!physical_segments.empty()) {
        physical_path = physical_segments.front();
        for (std::size_t i = 1; i < physical_segments.size(); ++i) {
            physical_path.insert(physical_path.end(), physical_segments[i].begin() + 1, physical_segments[i].end());
        }
    } else if (logic_path.size() == 1) {
        physical_path = logic_path;
    }

    return {logic_path, physical_path, physical_segments};
}

std::pair<std::vector<Node*>, double> DeterministicRouter::find_physical_shortest_path(
    Node* start_node,
    Node* end_node,
    double demand,
    Flow* flow) const {
    std::unordered_map<int, double> distances;
    std::unordered_map<int, Node*> predecessors;
    std::unordered_map<int, Node*> node_objects;
    std::unordered_set<int> visited;

    using Entry = std::tuple<double, int, Node*>;
    auto cmp = [](const Entry& a, const Entry& b) { return std::get<0>(a) > std::get<0>(b); };
    std::priority_queue<Entry, std::vector<Entry>, decltype(cmp)> pq(cmp);

    distances[start_node->id] = 0.0;
    node_objects[start_node->id] = start_node;
    pq.push({0.0, start_node->id, start_node});

    while (!pq.empty()) {
        const auto [current_dist, current_id, current_node] = pq.top();
        pq.pop();
        if (visited.count(current_id) != 0U) {
            continue;
        }
        visited.insert(current_id);
        if (current_id == end_node->id) {
            break;
        }

        for (PhysicalLink* link : current_node->physical_links) {
            if (link->current_load + demand > link->bandwidth * 0.99) {
                continue;
            }
            Node* neighbor = link->node_b->id == current_id ? link->node_a : link->node_b;
            if (visited.count(neighbor->id) != 0U) {
                continue;
            }

            const double trans_delay = link->bandwidth > 0.0 ? flow->message_size / link->bandwidth : 0.0;
            const double edge_delay = link->propagation_delay + trans_delay;
            const double alt_dist = current_dist + edge_delay + neighbor->dynamic_weight;
            const double old_dist = distances.count(neighbor->id) == 0U ? INF : distances[neighbor->id];
            if (alt_dist < old_dist) {
                distances[neighbor->id] = alt_dist;
                predecessors[neighbor->id] = current_node;
                node_objects[neighbor->id] = neighbor;
                pq.push({alt_dist, neighbor->id, neighbor});
            }
        }
    }

    if (predecessors.count(end_node->id) == 0U && start_node->id != end_node->id) {
        return {{}, 0.0};
    }

    std::vector<Node*> path;
    int current_id = end_node->id;
    const double total_cost = distances[current_id];
    while (predecessors.count(current_id) != 0U) {
        path.push_back(node_objects.count(current_id) == 0U ? end_node : node_objects[current_id]);
        current_id = predecessors[current_id]->id;
    }
    path.push_back(start_node);
    std::reverse(path.begin(), path.end());
    return {path, total_cost};
}

void DeterministicRouter::update_link_states(
    BusTopology& topology,
    Flow* flow,
    double demand,
    const PathSegments& physical_segments) const {
    for (std::size_t i = 0; i + 1 < flow->logical_routing_path.size(); ++i) {
        BusLink* link = topology.get_bus_link(flow->logical_routing_path[i], flow->logical_routing_path[i + 1]);
        if (link != nullptr) {
            link->current_load += demand;
            if (i < physical_segments.size()) {
                link->physical_path = physical_segments[i];
            }
        }
    }

    for (std::size_t i = 0; i + 1 < flow->physical_routing_path.size(); ++i) {
        PhysicalLink* link = topology.get_physical_link(flow->physical_routing_path[i], flow->physical_routing_path[i + 1]);
        if (link != nullptr) {
            link->current_load += demand;
            link->passing_flows.push_back(flow);
        }
    }

    for (std::size_t i = 1; i + 1 < flow->physical_routing_path.size(); ++i) {
        Node* relay = flow->physical_routing_path[i];
        if (relay->id != flow->source_node->id && relay->id != flow->target_node->id) {
            relay->relay_flows.push_back(flow);
        }
        if (relay->cpu_capacity > 0.0) {
            const double total_load = node_static_load(relay->source_flows) +
                                      node_static_load(relay->sink_flows) +
                                      node_static_load(relay->relay_flows);
            relay->dynamic_weight = alpha_ * (total_load / relay->cpu_capacity);
        }
    }
}

} // namespace topoopt
