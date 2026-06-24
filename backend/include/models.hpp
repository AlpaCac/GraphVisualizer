#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace topoopt {

struct Flow;
struct PhysicalLink;
struct BusLink;

struct Node {
    int id = 0;
    double cpu_capacity = 0.0;
    double memory_capacity = 0.0;
    double x = 0.0;
    double y = 0.0;
    int max_physical_ports = 0;
    double reliability = 1.0;
    bool is_core = false;
    bool destroyed = false;
    double dynamic_weight = 0.0;

    std::vector<Flow*> source_flows;
    std::vector<Flow*> sink_flows;
    std::vector<Flow*> relay_flows;
    std::vector<PhysicalLink*> physical_links;
    std::vector<BusLink*> bus_links;
};

struct PhysicalLink {
    int id = 0;
    Node* node_a = nullptr;
    Node* node_b = nullptr;
    double bandwidth = 0.0;
    double propagation_delay = 0.0;
    double reliability = 1.0;
    double cost = 0.0;
    bool destroyed = false;
    double current_load = 0.0;
    std::vector<Flow*> passing_flows;
};

struct BusLink {
    int id = 0;
    Node* node_a = nullptr;
    Node* node_b = nullptr;
    double dynamic_weight = 0.0;
    double current_load = 0.0;
    double cost = 0.0;
    std::vector<Node*> physical_path;
};

struct Flow {
    int id = 0;
    std::string flow_name;
    std::string topic;
    Node* source_node = nullptr;
    Node* target_node = nullptr;
    double pre_processing_time = 0.0;
    double post_processing_time = 0.0;
    int priority = 0;
    double period = 0.0;
    double deadline = 0.0;
    double reliability_requirement = 0.0;
    double message_size = 0.0;

    std::vector<Flow*> upstream_flows;
    std::vector<Flow*> downstream_flows;
    std::vector<Node*> logical_routing_path;
    std::vector<Node*> physical_routing_path;
    std::optional<double> worst_case_delay;
    std::optional<double> actual_reliability;
    std::optional<double> actual_throughput_gbps;
    std::optional<double> throughput_score;
    bool is_schedulable = true;
};

struct PairHash {
    std::size_t operator()(const std::pair<int, int>& value) const noexcept;
};

std::pair<int, int> ordered_pair(int a, int b);

class BusTopology {
public:
    Node* add_node(Node node);
    PhysicalLink* add_physical_link(PhysicalLink link);
    BusLink* add_bus_link(BusLink link);
    Flow* add_flow(Flow flow);

    Node* get_node(int id) const;
    PhysicalLink* get_physical_link(const Node* a, const Node* b) const;
    BusLink* get_bus_link(const Node* a, const Node* b) const;

    std::vector<Node*> node_list() const;
    std::vector<PhysicalLink*> physical_link_list() const;
    std::vector<BusLink*> bus_link_list() const;
    std::vector<Flow*> flow_list() const;

    std::unordered_map<int, Node*> nodes;
    std::unordered_map<int, PhysicalLink*> physical_links;
    std::unordered_map<int, BusLink*> bus_links;

private:
    std::vector<std::unique_ptr<Node>> owned_nodes_;
    std::vector<std::unique_ptr<PhysicalLink>> owned_physical_links_;
    std::vector<std::unique_ptr<BusLink>> owned_bus_links_;
    std::vector<std::unique_ptr<Flow>> owned_flows_;
    std::unordered_map<std::pair<int, int>, PhysicalLink*, PairHash> physical_link_map_;
    std::unordered_map<std::pair<int, int>, BusLink*, PairHash> bus_link_map_;
};

bool contains_flow(const std::vector<Flow*>& flows, const Flow* flow);
double flow_bandwidth_demand(const Flow& flow);

} // namespace topoopt
