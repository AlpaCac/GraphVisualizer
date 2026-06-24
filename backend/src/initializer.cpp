#include "initializer.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace topoopt {

double calculate_distance(const Node& n1, const Node& n2) {
    const double dx = n1.x - n2.x;
    const double dy = n1.y - n2.y;
    return std::sqrt(dx * dx + dy * dy);
}

static void add_flow(
    BusTopology& topology,
    int id,
    const std::string& name,
    const std::string& topic,
    int src,
    int dst,
    double pre,
    double post,
    int priority,
    double period,
    double deadline,
    double reliability_requirement,
    double message_size) {
    Flow flow;
    flow.id = id;
    flow.flow_name = name;
    flow.topic = topic;
    flow.source_node = topology.get_node(src);
    flow.target_node = topology.get_node(dst);
    flow.pre_processing_time = pre;
    flow.post_processing_time = post;
    flow.priority = priority;
    flow.period = period;
    flow.deadline = deadline;
    flow.reliability_requirement = reliability_requirement;
    flow.message_size = message_size;
    topology.add_flow(std::move(flow));
}

BusTopology build_sandbox_topology() {
    BusTopology topology;
    const std::vector<std::pair<double, double>> coordinates = {
        {120, 150}, {200, 350}, {180, 500}, {300, 200}, {450, 400},
        {600, 300}, {800, 250}, {750, 450}, {900, 600}, {850, 800},
        {650, 750}, {500, 850}, {350, 700}, {150, 800}, {400, 550},
        {550, 500}, {700, 600}, {50, 400}, {950, 300}, {500, 100}
    };

    for (int i = 0; i < static_cast<int>(coordinates.size()); ++i) {
        const bool high_end = (i == 4 || i == 7 || i == 14 || i == 15);
        Node node;
        node.id = i;
        node.cpu_capacity = high_end ? 9000.0 : 6000.0;
        node.memory_capacity = 16384.0;
        node.x = coordinates[i].first;
        node.y = coordinates[i].second;
        node.max_physical_ports = high_end ? 6 : 4;
        node.reliability = high_end ? 0.999 : 0.99;
        node.is_core = high_end;
        topology.add_node(std::move(node));
    }

    int physical_id = 0;
    int bus_id = 0;
    const auto nodes = topology.node_list();
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (std::size_t j = i + 1; j < nodes.size(); ++j) {
            Node* a = nodes[i];
            Node* b = nodes[j];
            const double dist = calculate_distance(*a, *b);
            if (dist > 400.0) {
                continue;
            }

            const double bandwidth = 100000.0 / (1.0 + std::pow(dist / 100.0, 2.0));
            const double propagation_delay = 0.01 + (dist * 0.005);
            const double cost = 10.0 + 0.1 * dist;
            const double reliability = 0.999 - 0.01 * std::pow(dist / 400.0, 2.0);

            PhysicalLink physical;
            physical.id = physical_id++;
            physical.node_a = a;
            physical.node_b = b;
            physical.bandwidth = bandwidth;
            physical.propagation_delay = propagation_delay;
            physical.reliability = reliability;
            physical.cost = cost;
            topology.add_physical_link(std::move(physical));

            BusLink bus;
            bus.id = bus_id++;
            bus.node_a = a;
            bus.node_b = b;
            bus.dynamic_weight = propagation_delay;
            bus.cost = cost;
            topology.add_bus_link(std::move(bus));
        }
    }

    int flow_id = 0;
    for (const auto& pair : std::vector<std::pair<int, int>>{{0, 10}, {5, 11}, {13, 12}}) {
        add_flow(topology, flow_id, "Video_" + std::to_string(pair.first) + "_to_" + std::to_string(pair.second),
                 "T_Video_" + std::to_string(flow_id), pair.first, pair.second,
                 2.0, 2.0, 1, 33.0, 300.0, 0.8, 64000.0);
        ++flow_id;
    }

    for (const auto& pair : std::vector<std::pair<int, int>>{{10, 1}, {11, 6}, {12, 14}, {10, 19}}) {
        add_flow(topology, flow_id, "C2_" + std::to_string(pair.first) + "_to_" + std::to_string(pair.second),
                 "T_C2_" + std::to_string(flow_id), pair.first, pair.second,
                 0.5, 0.5, 10, 20.0, 50.0, 0.9, 128.0);
        ++flow_id;
    }

    for (const auto& pair : std::vector<std::pair<int, int>>{{2, 3}, {7, 8}, {15, 16}}) {
        add_flow(topology, flow_id, "Sync_" + std::to_string(pair.first) + "_to_" + std::to_string(pair.second),
                 "T_Sync_" + std::to_string(flow_id), pair.first, pair.second,
                 0.5, 0.5, 7, 10.0, 80.0, 0.85, 4096.0);
        ++flow_id;
    }

    for (const auto& pair : std::vector<std::pair<int, int>>{{4, 10}, {9, 11}, {17, 12}, {18, 10}}) {
        add_flow(topology, flow_id, "Telemetry_" + std::to_string(pair.first) + "_to_" + std::to_string(pair.second),
                 "T_Telemetry_" + std::to_string(flow_id), pair.first, pair.second,
                 1.0, 1.0, 5, 100.0, 500.0, 0.85, 1024.0);
        ++flow_id;
    }

    return topology;
}

} // namespace topoopt
