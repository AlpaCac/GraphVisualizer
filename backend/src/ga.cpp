#include "ga.hpp"

#include "evaluators.hpp"
#include "initializer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace topoopt {

namespace {
constexpr double INF = std::numeric_limits<double>::infinity();

struct UnionFind {
    std::unordered_map<int, int> parent;

    explicit UnionFind(const std::vector<NodeSpec>& nodes) {
        for (const NodeSpec& node : nodes) {
            parent[node.id] = node.id;
        }
    }

    int find(int id) {
        if (parent[id] == id) {
            return id;
        }
        parent[id] = find(parent[id]);
        return parent[id];
    }

    bool unite(int a, int b) {
        const int root_a = find(a);
        const int root_b = find(b);
        if (root_a == root_b) {
            return false;
        }
        parent[root_a] = root_b;
        return true;
    }
};

LinkSpec make_link_spec(int id, const NodeSpec& a, const NodeSpec& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dist = std::sqrt(dx * dx + dy * dy);

    LinkSpec link;
    link.id = id;
    link.node_a = a.id;
    link.node_b = b.id;
    link.bandwidth = 100000.0 / (1.0 + std::pow(dist / 100.0, 2.0));
    link.propagation_delay = 0.01 + (dist * 0.005);
    link.cost = 10.0 + 0.1 * dist;
    link.reliability = 0.999 - 0.01 * std::pow(dist / 400.0, 2.0);
    return link;
}

double logical_link_cost(const Node& a, const Node& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dist = std::sqrt(dx * dx + dy * dy);
    return 10.0 + 0.1 * dist;
}

[[maybe_unused]] double mac_success_efficiency(const Flow& flow, double bottleneck_bandwidth, const MacParams& mac_params) {
    if (bottleneck_bandwidth <= 0.0) {
        return 0.0;
    }
    const double payload_time_ms = flow.message_size / bottleneck_bandwidth;
    const double overhead_ms = (mac_params.header_us + mac_params.ack_us +
                                mac_params.sifs_us + mac_params.difs_us + mac_params.sigma_us) / 1000.0;
    const double time_efficiency = payload_time_ms > 0.0
                                       ? payload_time_ms / (payload_time_ms + overhead_ms)
                                       : 0.0;
    const double success_probability = std::max(0.0, (1.0 - mac_params.p_cap) * (1.0 - mac_params.p_e_base));
    return time_efficiency * success_probability;
}

double flow_other_link_load(const PhysicalLink& link, const Flow& flow) {
    double load = 0.0;
    for (const Flow* passing_flow : link.passing_flows) {
        if (passing_flow != nullptr && passing_flow->id != flow.id) {
            load += flow_bandwidth_demand(*passing_flow);
        }
    }
    return load;
}

double calculate_flow_bottleneck_available_bandwidth(const Flow& flow, const BusTopology& topology) {
    if (flow.physical_routing_path.size() < 2) {
        return 0.0;
    }

    double bottleneck_bandwidth = INF;
    for (std::size_t i = 0; i + 1 < flow.physical_routing_path.size(); ++i) {
        const PhysicalLink* link = topology.get_physical_link(flow.physical_routing_path[i], flow.physical_routing_path[i + 1]);
        if (link == nullptr || link->bandwidth <= 0.0) {
            return 0.0;
        }
        const double other_load = flow_other_link_load(*link, flow);
        const double available_bandwidth = std::max(0.0, link->bandwidth - other_load);
        bottleneck_bandwidth = std::min(bottleneck_bandwidth, available_bandwidth);
    }
    return std::isinf(bottleneck_bandwidth) ? 0.0 : bottleneck_bandwidth;
}

double calculate_flow_throughput_score(const Flow& flow, const BusTopology& topology) {
    const double demand = flow_bandwidth_demand(flow);
    if (demand <= 0.0) {
        return 1.0;
    }
    const double effective_throughput_bytes_per_ms =
        calculate_flow_bottleneck_available_bandwidth(flow, topology) *
        flow.actual_reliability.value_or(0.0);
    return std::clamp(effective_throughput_bytes_per_ms / demand, 0.0, 1.0);
}

double baseline_fiedler_for_environment(const GAEnvironment& environment) {
    Individual seed;
    seed.role_gene = environment.seed_role_gene;
    seed.link_gene = environment.seed_link_gene;
    BusTopology topology = decode_to_topology(seed, environment);
    return AlgebraicConnectivityEvaluator().evaluate(topology);
}

std::vector<int> path_node_ids(const std::vector<Node*>& path) {
    std::vector<int> ids;
    ids.reserve(path.size());
    for (const Node* node : path) {
        if (node != nullptr) {
            ids.push_back(node->id);
        }
    }
    return ids;
}

std::set<std::pair<int, int>> path_edge_set(const std::vector<int>& path) {
    std::set<std::pair<int, int>> edges;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        edges.insert(ordered_pair(path[i], path[i + 1]));
    }
    return edges;
}

std::set<std::pair<int, int>> seed_logical_link_set(const GAEnvironment& environment) {
    std::set<std::pair<int, int>> links;
    for (std::size_t i = 0; i < environment.seed_link_gene.size() && i < environment.candidate_bus_links.size(); ++i) {
        if (environment.seed_link_gene[i] == 1) {
            links.insert(environment.candidate_bus_links[i]);
        }
    }
    return links;
}

std::set<std::pair<int, int>> current_logical_link_set(const BusTopology& topology) {
    std::set<std::pair<int, int>> links;
    for (const BusLink* link : topology.bus_link_list()) {
        links.insert(ordered_pair(link->node_a->id, link->node_b->id));
    }
    return links;
}

double normalized_weighted_sum(const std::vector<std::pair<double, double>>& weighted_terms) {
    double value = 0.0;
    double weight_sum = 0.0;
    for (const auto& [weight, term] : weighted_terms) {
        if (weight <= 0.0) {
            continue;
        }
        value += weight * std::clamp(term, 0.0, 1.0);
        weight_sum += weight;
    }
    return weight_sum > 0.0 ? value / weight_sum : 0.0;
}

double calculate_bandwidth_occupancy_cost(const BusTopology& topology) {
    const auto links = topology.physical_link_list();
    if (links.empty()) {
        return 0.0;
    }

    double total = 0.0;
    for (const PhysicalLink* link : links) {
        if (link->bandwidth <= 0.0) {
            total += link->current_load > 0.0 ? 1.0 : 0.0;
            continue;
        }
        const double retransmission_factor = 1.0 / std::max(link->reliability, 0.01);
        total += std::clamp((link->current_load / link->bandwidth) * retransmission_factor, 0.0, 1.0);
    }
    return total / static_cast<double>(links.size());
}

double node_flow_load(const std::vector<Flow*>& flows) {
    double load = 0.0;
    for (const Flow* flow : flows) {
        load += flow_bandwidth_demand(*flow);
    }
    return load;
}

double calculate_node_resource_cost(const BusTopology& topology) {
    const auto nodes = topology.node_list();
    if (nodes.empty()) {
        return 0.0;
    }

    double total = 0.0;
    for (const Node* node : nodes) {
        const double traffic_load = node_flow_load(node->source_flows) +
                                    node_flow_load(node->sink_flows) +
                                    node_flow_load(node->relay_flows);
        if (node->cpu_capacity <= 0.0) {
            total += traffic_load > 0.0 ? 1.0 : 0.0;
            continue;
        }
        const double role_factor = node->is_core ? 1.10 : 1.0;
        total += std::clamp((traffic_load / node->cpu_capacity) * role_factor, 0.0, 1.0);
    }
    return total / static_cast<double>(nodes.size());
}

double calculate_interface_occupancy_cost(const BusTopology& topology) {
    const auto nodes = topology.node_list();
    if (nodes.empty()) {
        return 0.0;
    }

    double total = 0.0;
    for (const Node* node : nodes) {
        if (node->max_physical_ports <= 0) {
            total += node->bus_links.empty() ? 0.0 : 1.0;
            continue;
        }
        total += std::clamp(static_cast<double>(node->bus_links.size()) /
                                static_cast<double>(node->max_physical_ports),
                            0.0,
                            1.0);
    }
    return total / static_cast<double>(nodes.size());
}

double calculate_path_switch_cost(const BusTopology& topology, const GAEnvironment& environment) {
    if (environment.baseline_physical_paths.empty()) {
        return 0.0;
    }

    double weighted_distance = 0.0;
    double total_weight = 0.0;
    for (Flow* flow : topology.flow_list()) {
        const auto baseline_it = environment.baseline_physical_paths.find(flow->id);
        if (baseline_it == environment.baseline_physical_paths.end()) {
            continue;
        }

        const auto old_edges = path_edge_set(baseline_it->second);
        const auto new_edges = path_edge_set(path_node_ids(flow->physical_routing_path));
        std::set<std::pair<int, int>> edge_union;
        std::set_union(old_edges.begin(), old_edges.end(),
                       new_edges.begin(), new_edges.end(),
                       std::inserter(edge_union, edge_union.begin()));
        if (edge_union.empty()) {
            continue;
        }

        std::set<std::pair<int, int>> edge_intersection;
        std::set_intersection(old_edges.begin(), old_edges.end(),
                              new_edges.begin(), new_edges.end(),
                              std::inserter(edge_intersection, edge_intersection.begin()));
        const double changed_edges = static_cast<double>(edge_union.size() - edge_intersection.size());
        const double priority_weight = std::max(1.0, static_cast<double>(flow->priority));
        weighted_distance += priority_weight * (changed_edges / static_cast<double>(edge_union.size()));
        total_weight += priority_weight;
    }

    return total_weight > 0.0 ? std::clamp(weighted_distance / total_weight, 0.0, 1.0) : 0.0;
}

double calculate_normalized_network_cost(const BusTopology& topology, const GAEnvironment& environment) {
    const CostModelParams& params = environment.cost_params;

    const double runtime_cost = normalized_weighted_sum({
        {params.bandwidth_weight, calculate_bandwidth_occupancy_cost(topology)},
        {params.node_resource_weight, calculate_node_resource_cost(topology)},
        {params.interface_weight, calculate_interface_occupancy_cost(topology)}
    });

    const auto seed_links = seed_logical_link_set(environment);
    const auto current_links = current_logical_link_set(topology);
    std::set<std::pair<int, int>> added_links;
    std::set<std::pair<int, int>> deleted_links;
    std::set_difference(current_links.begin(), current_links.end(),
                        seed_links.begin(), seed_links.end(),
                        std::inserter(added_links, added_links.begin()));
    std::set_difference(seed_links.begin(), seed_links.end(),
                        current_links.begin(), current_links.end(),
                        std::inserter(deleted_links, deleted_links.begin()));

    const double node_count = std::max<std::size_t>(1, environment.nodes.size());
    const double seed_link_count = std::max<std::size_t>(1, seed_links.size());
    const double add_cost = std::clamp(static_cast<double>(added_links.size()) / static_cast<double>(node_count), 0.0, 1.0);
    const double delete_cost = std::clamp(static_cast<double>(deleted_links.size()) / static_cast<double>(seed_link_count), 0.0, 1.0);
    const double path_switch_cost = calculate_path_switch_cost(topology, environment);

    const double reconstruction_cost = normalized_weighted_sum({
        {params.add_link_weight, add_cost},
        {params.delete_link_weight, delete_cost},
        {params.path_switch_weight, path_switch_cost}
    });

    const double combined = normalized_weighted_sum({
        {params.runtime_weight, runtime_cost},
        {params.reconstruction_weight, reconstruction_cost}
    });
    return std::clamp(combined, 0.0, 1.0) * 100.0;
}

std::vector<NodeSpec> default_nodes() {
    const std::vector<std::pair<double, double>> coordinates = {
        {120, 150}, {200, 350}, {180, 500}, {300, 200}, {450, 400},
        {600, 300}, {800, 250}, {750, 450}, {900, 600}, {850, 800},
        {650, 750}, {500, 850}, {350, 700}, {150, 800}, {400, 550},
        {550, 500}, {700, 600}, {50, 400}, {950, 300}, {500, 100}
    };

    std::vector<NodeSpec> nodes;
    nodes.reserve(coordinates.size());
    for (int i = 0; i < static_cast<int>(coordinates.size()); ++i) {
        const bool high_end = (i == 4 || i == 7 || i == 14 || i == 15);
        nodes.push_back(NodeSpec{
            i,
            high_end ? 9000.0 : 6000.0,
            16384.0,
            coordinates[i].first,
            coordinates[i].second,
            high_end ? 6 : 4,
            high_end ? 0.999 : 0.99,
            high_end
        });
    }
    return nodes;
}

std::vector<FlowSpec> default_flows() {
    return {
        {0, "视频传输任务1", 0, 10, 1, 33.0, 300.0, 64000.0, 0.8},
        {1, "视频传输任务2", 5, 11, 1, 33.0, 300.0, 64000.0, 0.8},
        {2, "视频传输任务3", 13, 12, 1, 33.0, 300.0, 64000.0, 0.8},
        {3, "指令控制任务1", 10, 1, 10, 20.0, 100.0, 128.0, 0.9},
        {4, "指令控制任务2", 11, 6, 10, 20.0, 100.0, 128.0, 0.9},
        {5, "指令控制任务3", 12, 14, 10, 20.0, 100.0, 128.0, 0.9},
        {6, "指令控制任务4", 10, 19, 10, 20.0, 100.0, 128.0, 0.9},
        {7, "编队同步任务1", 2, 3, 7, 10.0, 150.0, 4096.0, 0.85},
        {8, "编队同步任务2", 7, 8, 7, 10.0, 150.0, 4096.0, 0.85},
        {9, "编队同步任务3", 15, 16, 7, 10.0, 150.0, 4096.0, 0.85},
        {10, "遥测回传任务1", 4, 10, 5, 100.0, 1000.0, 1024.0, 0.85},
        {11, "遥测回传任务2", 9, 11, 5, 100.0, 1000.0, 1024.0, 0.85},
        {12, "遥测回传任务3", 17, 12, 5, 100.0, 1000.0, 1024.0, 0.85},
        {13, "遥测回传任务4", 18, 10, 5, 100.0, 1000.0, 1024.0, 0.85}
    };
}

std::set<std::pair<int, int>> default_seed_link_pairs() {
    return {
        ordered_pair(0, 3), ordered_pair(1, 4), ordered_pair(2, 14), ordered_pair(3, 4),
        ordered_pair(4, 5), ordered_pair(4, 14), ordered_pair(4, 15), ordered_pair(4, 19),
        ordered_pair(6, 7), ordered_pair(7, 8), ordered_pair(7, 15), ordered_pair(7, 16),
        ordered_pair(7, 18), ordered_pair(9, 11), ordered_pair(10, 11), ordered_pair(11, 13),
        ordered_pair(11, 14), ordered_pair(12, 14), ordered_pair(14, 15), ordered_pair(14, 17)
    };
}

std::string gene_fingerprint(const Individual& individual) {
    std::string out;
    out.reserve(individual.role_gene.size() + individual.link_gene.size());
    for (int value : individual.role_gene) {
        out.push_back(static_cast<char>('0' + value + 1));
    }
    for (int value : individual.link_gene) {
        out.push_back(static_cast<char>('0' + value + 1));
    }
    return out;
}

Individual mutate_seed(const Individual& seed, double rate, std::mt19937& rng) {
    Individual out = seed;

    std::vector<std::size_t> active;
    std::vector<std::size_t> inactive;
    active.reserve(out.link_gene.size());
    inactive.reserve(out.link_gene.size());
    for (std::size_t i = 0; i < out.link_gene.size(); ++i) {
        if (out.link_gene[i] == 1) {
            active.push_back(i);
        } else if (out.link_gene[i] == 0) {
            inactive.push_back(i);
        }
    }

    const std::size_t mutable_gene_count = active.size() + inactive.size();
    if (mutable_gene_count == 0) {
        return out;
    }

    const double lambda = static_cast<double>(mutable_gene_count) * std::max(0.0, rate);
    std::poisson_distribution<int> action_count_dist(lambda);
    const int action_count = std::max(1, action_count_dist(rng));
    std::uniform_int_distribution<int> action_dist(0, 2);

    const auto remove_random = [&](std::vector<std::size_t>& pool) {
        std::uniform_int_distribution<std::size_t> dist(0, pool.size() - 1);
        const std::size_t pos = dist(rng);
        const std::size_t idx = pool[pos];
        pool[pos] = pool.back();
        pool.pop_back();
        return idx;
    };

    const auto add_edge = [&]() {
        if (inactive.empty()) {
            return;
        }
        const std::size_t idx = remove_random(inactive);
        out.link_gene[idx] = 1;
        active.push_back(idx);
    };

    const auto remove_edge = [&]() {
        if (active.empty()) {
            return;
        }
        const std::size_t idx = remove_random(active);
        out.link_gene[idx] = 0;
        inactive.push_back(idx);
    };

    for (int step = 0; step < action_count; ++step) {
        if (active.empty() && inactive.empty()) {
            break;
        }
        if (active.empty()) {
            add_edge();
            continue;
        }
        if (inactive.empty()) {
            remove_edge();
            continue;
        }

        const int action = action_dist(rng);
        if (action == 0) {
            add_edge();
        } else if (action == 1) {
            remove_edge();
        } else {
            remove_edge();
            add_edge();
        }
    }

    out.fitness = {0.0, 0.0, 0.0, 0.0, 0.0};
    out.metrics.clear();
    out.rank = 0;
    out.crowding_distance = 0.0;
    out.is_fully_schedulable = false;
    return out;
}

Individual tournament_select(const std::vector<Individual>& population, std::mt19937& rng) {
    std::uniform_int_distribution<std::size_t> dist(0, population.size() - 1);
    const Individual* best = &population[dist(rng)];
    const Individual* challenger = &population[dist(rng)];
    if (challenger->rank < best->rank ||
        (challenger->rank == best->rank && challenger->crowding_distance > best->crowding_distance)) {
        best = challenger;
    }
    return *best;
}

std::pair<Individual, Individual> crossover(const Individual& p1, const Individual& p2, std::mt19937& rng) {
    std::bernoulli_distribution coin(0.5);
    Individual c1;
    Individual c2;
    c1.role_gene.reserve(p1.role_gene.size());
    c2.role_gene.reserve(p1.role_gene.size());
    c1.link_gene.reserve(p1.link_gene.size());
    c2.link_gene.reserve(p1.link_gene.size());

    for (std::size_t i = 0; i < p1.role_gene.size(); ++i) {
        if (p1.role_gene[i] == p2.role_gene[i] || coin(rng)) {
            c1.role_gene.push_back(p1.role_gene[i]);
            c2.role_gene.push_back(p2.role_gene[i]);
        } else {
            c1.role_gene.push_back(p2.role_gene[i]);
            c2.role_gene.push_back(p1.role_gene[i]);
        }
    }

    for (std::size_t i = 0; i < p1.link_gene.size(); ++i) {
        if (p1.link_gene[i] == p2.link_gene[i] || coin(rng)) {
            c1.link_gene.push_back(p1.link_gene[i]);
            c2.link_gene.push_back(p2.link_gene[i]);
        } else {
            c1.link_gene.push_back(p2.link_gene[i]);
            c2.link_gene.push_back(p1.link_gene[i]);
        }
    }

    return {c1, c2};
}

Individual mutate(const Individual& individual, double mutation_rate, std::mt19937& rng) {
    return mutate_seed(individual, mutation_rate, rng);
}

Individual generate_single_immigrant(const Individual& seed, const GAEnvironment& environment, std::mt19937& rng) {
    while (true) {
        Individual mutant = mutate_seed(seed, 0.1, rng);
        BusTopology topology = decode_to_topology(mutant, environment);
        if (validate_topology(topology)) {
            return mutant;
        }
    }
}

void apply_result_to_individual(Individual& individual, const EvaluationResult& result) {
    individual.is_fully_schedulable = result.fully_schedulable;
    individual.fitness = {
        result.composite_latency,
        -result.composite_throughput_score,
        -result.fiedler,
        -result.composite_reliability,
        result.cost
    };
    individual.metrics = {
        {"comp_lat", result.composite_latency},
        {"max_lat", result.max_latency},
        {"fiedler", result.fiedler},
        {"conn_norm", result.connectivity_norm},
        {"throughput_gbps", result.throughput_gbps},
        {"throughput_score", result.composite_throughput_score},
        {"comp_rel", result.composite_reliability},
        {"cost", result.cost}
    };
}
} // namespace

GAEnvironment build_default_ga_environment() {
    GAEnvironment environment;
    environment.nodes = default_nodes();
    environment.flows = default_flows();

    for (const NodeSpec& node : environment.nodes) {
        environment.seed_role_gene.push_back(node.is_core ? 1 : 0);
    }

    std::vector<LinkSpec> candidate_physical;
    for (std::size_t i = 0; i < environment.nodes.size(); ++i) {
        for (std::size_t j = i + 1; j < environment.nodes.size(); ++j) {
            const double dx = environment.nodes[i].x - environment.nodes[j].x;
            const double dy = environment.nodes[i].y - environment.nodes[j].y;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 400.0) {
                candidate_physical.push_back(make_link_spec(0, environment.nodes[i], environment.nodes[j]));
            }
        }
    }

    std::sort(candidate_physical.begin(), candidate_physical.end(), [](const LinkSpec& a, const LinkSpec& b) {
        return a.cost < b.cost;
    });

    UnionFind uf(environment.nodes);
    for (const LinkSpec& edge : candidate_physical) {
        if (uf.unite(edge.node_a, edge.node_b)) {
            LinkSpec selected = edge;
            selected.id = static_cast<int>(environment.physical_links.size());
            environment.physical_links.push_back(selected);
            if (environment.physical_links.size() == environment.nodes.size() - 1) {
                break;
            }
        }
    }

    auto already_selected = [&environment](const LinkSpec& edge) {
        return std::any_of(environment.physical_links.begin(), environment.physical_links.end(), [&](const LinkSpec& selected) {
            return ordered_pair(edge.node_a, edge.node_b) == ordered_pair(selected.node_a, selected.node_b);
        });
    };

    for (const LinkSpec& edge : candidate_physical) {
        if (environment.physical_links.size() >= 30) {
            break;
        }
        if (!already_selected(edge)) {
            LinkSpec selected = edge;
            selected.id = static_cast<int>(environment.physical_links.size());
            environment.physical_links.push_back(selected);
        }
    }

    const auto seed_pairs = default_seed_link_pairs();
    for (std::size_t i = 0; i < environment.nodes.size(); ++i) {
        for (std::size_t j = i + 1; j < environment.nodes.size(); ++j) {
            const auto pair = ordered_pair(environment.nodes[i].id, environment.nodes[j].id);
            environment.candidate_bus_links.push_back(pair);
            environment.seed_link_gene.push_back(seed_pairs.count(pair) == 0 ? 0 : 1);
        }
    }

    environment.baseline_fiedler = baseline_fiedler_for_environment(environment);
    DeterministicRouter router;
    populate_baseline_paths(environment, router);
    return environment;
}

void populate_baseline_paths(GAEnvironment& environment, const DeterministicRouter& router) {
    Individual seed;
    seed.role_gene = environment.seed_role_gene;
    seed.link_gene = environment.seed_link_gene;
    BusTopology topology = decode_to_topology(seed, environment);
    router.route_all_flows(topology);

    environment.baseline_physical_paths.clear();
    for (Flow* flow : topology.flow_list()) {
        if (!flow->physical_routing_path.empty()) {
            environment.baseline_physical_paths[flow->id] = path_node_ids(flow->physical_routing_path);
        }
    }
}

BusTopology decode_to_topology(const Individual& individual, const GAEnvironment& environment) {
    BusTopology topology;

    for (std::size_t i = 0; i < environment.nodes.size(); ++i) {
        if (i < individual.role_gene.size() && individual.role_gene[i] == -1) {
            continue;
        }
        const NodeSpec& spec = environment.nodes[i];
        Node node;
        node.id = spec.id;
        node.cpu_capacity = spec.cpu_capacity;
        node.memory_capacity = spec.memory_capacity;
        node.x = spec.x;
        node.y = spec.y;
        node.max_physical_ports = spec.max_physical_ports;
        node.reliability = spec.reliability;
        node.is_core = i < individual.role_gene.size() && individual.role_gene[i] == 1;
        topology.add_node(std::move(node));
    }

    for (const FlowSpec& spec : environment.flows) {
        Node* src = topology.get_node(spec.src);
        Node* dst = topology.get_node(spec.dst);
        if (src == nullptr || dst == nullptr) {
            continue;
        }
        Flow flow;
        flow.id = spec.id;
        flow.flow_name = spec.name;
        flow.topic = "Topic_" + std::to_string(spec.id);
        flow.source_node = src;
        flow.target_node = dst;
        flow.pre_processing_time = 1.0;
        flow.post_processing_time = 1.0;
        flow.priority = spec.priority;
        flow.period = spec.period;
        flow.deadline = spec.deadline;
        flow.reliability_requirement = spec.reliability_requirement;
        flow.message_size = spec.message_size;
        topology.add_flow(std::move(flow));
    }

    for (std::size_t i = 0; i < individual.link_gene.size() && i < environment.candidate_bus_links.size(); ++i) {
        if (individual.link_gene[i] != 1) {
            continue;
        }
        const auto [a_id, b_id] = environment.candidate_bus_links[i];
        Node* a = topology.get_node(a_id);
        Node* b = topology.get_node(b_id);
        if (a == nullptr || b == nullptr) {
            continue;
        }
        BusLink link;
        link.id = static_cast<int>(i);
        link.node_a = a;
        link.node_b = b;
        link.dynamic_weight = 0.0;
        link.cost = logical_link_cost(*a, *b);
        topology.add_bus_link(std::move(link));
    }

    for (const LinkSpec& spec : environment.physical_links) {
        Node* a = topology.get_node(spec.node_a);
        Node* b = topology.get_node(spec.node_b);
        if (a == nullptr || b == nullptr) {
            continue;
        }
        PhysicalLink link;
        link.id = spec.id;
        link.node_a = a;
        link.node_b = b;
        link.bandwidth = spec.bandwidth;
        link.propagation_delay = spec.propagation_delay;
        link.reliability = spec.reliability;
        link.cost = spec.cost;
        topology.add_physical_link(std::move(link));
    }

    return topology;
}

bool validate_topology(const BusTopology& topology) {
    const auto nodes = topology.node_list();
    if (nodes.empty()) {
        return false;
    }

    std::unordered_set<int> visited;
    std::queue<Node*> queue;
    visited.insert(nodes.front()->id);
    queue.push(nodes.front());

    while (!queue.empty()) {
        Node* current = queue.front();
        queue.pop();
        for (BusLink* link : current->bus_links) {
            Node* neighbor = link->node_a->id == current->id ? link->node_b : link->node_a;
            if (visited.insert(neighbor->id).second) {
                queue.push(neighbor);
            }
        }
    }

    if (visited.size() != nodes.size()) {
        return false;
    }

    for (const Node* node : nodes) {
        if (node->max_physical_ports > 0 &&
            node->bus_links.size() > static_cast<std::size_t>(node->max_physical_ports)) {
            return false;
        }
    }

    return std::any_of(nodes.begin(), nodes.end(), [](const Node* node) { return node->is_core; });
}

EvaluationResult evaluate_topology(BusTopology& topology, const DeterministicRouter& router) {
    GAEnvironment default_environment;
    default_environment.mac_params = MacParams{};
    default_environment.baseline_fiedler = 0.0;
    return evaluate_topology(topology, router, default_environment);
}

double calculate_flow_throughput_gbps(const Flow& flow, const BusTopology& topology, const MacParams& mac_params) {
    (void)mac_params;
    const double effective_throughput_bytes_per_ms =
        calculate_flow_bottleneck_available_bandwidth(flow, topology) *
        flow.actual_reliability.value_or(0.0);
    return effective_throughput_bytes_per_ms * 8.0e-6;
}

EvaluationResult evaluate_topology(BusTopology& topology, const DeterministicRouter& router, const GAEnvironment& environment) {
    if (!validate_topology(topology)) {
        EvaluationResult invalid;
        invalid.composite_latency = INF;
        invalid.max_latency = INF;
        invalid.fiedler = 0.0;
        invalid.connectivity_norm = 0.0;
        invalid.throughput_gbps = 0.0;
        invalid.composite_throughput_score = 0.0;
        invalid.composite_reliability = 0.0;
        invalid.cost = INF;
        invalid.fully_schedulable = false;
        return invalid;
    }

    router.route_all_flows(topology);

    EvaluationResult result;
    for (const Flow* flow : topology.flow_list()) {
        if (flow->physical_routing_path.empty()) {
            result.composite_latency = INF;
            result.max_latency = INF;
            result.fiedler = 0.0;
            result.connectivity_norm = 0.0;
            result.throughput_gbps = 0.0;
            result.composite_throughput_score = 0.0;
            result.composite_reliability = 0.0;
            result.cost = INF;
            result.fully_schedulable = false;
            return result;
        }
    }

    auto [max_latency, e2e_results] = calculate_topology_latency(topology);
    result.max_latency = max_latency;
    result.fiedler = AlgebraicConnectivityEvaluator().evaluate(topology);
    result.connectivity_norm = environment.baseline_fiedler > 1e-12
                                   ? result.fiedler / environment.baseline_fiedler
                                   : (result.fiedler > 0.0 ? 1.0 : 0.0);
    ReliabilityEvaluator().evaluate_all(topology);
    result.cost = calculate_normalized_network_cost(topology, environment);

    double weighted_latency = 0.0;
    double total_latency_weight = 0.0;
    double weighted_throughput = 0.0;
    double total_throughput_weight = 0.0;
    double weighted_reliability = 0.0;
    double total_reliability_weight = 0.0;
    result.fully_schedulable = true;

    for (Flow* flow : topology.flow_list()) {
        const double priority = static_cast<double>(flow->priority);
        const double latency = e2e_results.count(flow->id) == 0U ? INF : e2e_results.at(flow->id);
        const double latency_ratio = flow->deadline > 0.0 ? latency / flow->deadline : INF;
        weighted_latency += priority * latency_ratio;
        total_latency_weight += priority;

        weighted_reliability += priority * flow->actual_reliability.value_or(0.0);
        total_reliability_weight += priority;
        const double flow_throughput_gbps = calculate_flow_throughput_gbps(*flow, topology, environment.mac_params);
        const double flow_throughput_score = calculate_flow_throughput_score(*flow, topology);
        flow->actual_throughput_gbps = flow_throughput_gbps;
        flow->throughput_score = flow_throughput_score;
        flow->is_schedulable = flow->is_schedulable && flow_throughput_score >= 1.0 - 1e-9;

        weighted_throughput += priority * flow_throughput_score;
        total_throughput_weight += priority;
        result.throughput_gbps += flow_throughput_gbps;
        result.fully_schedulable = result.fully_schedulable && flow->is_schedulable;
    }

    result.composite_latency = total_latency_weight > 0.0 ? weighted_latency / total_latency_weight : INF;
    result.composite_throughput_score = total_throughput_weight > 0.0 ? weighted_throughput / total_throughput_weight : 0.0;
    result.composite_reliability = total_reliability_weight > 0.0 ? weighted_reliability / total_reliability_weight : 0.0;
    return result;
}

EvaluationResult evaluate_individual(
    Individual& individual,
    const GAEnvironment& environment,
    const DeterministicRouter& router) {
    BusTopology topology = decode_to_topology(individual, environment);
    EvaluationResult result = evaluate_topology(topology, router, environment);
    apply_result_to_individual(individual, result);
    return result;
}

bool dominates(const Individual& lhs, const Individual& rhs) {
    if (lhs.is_fully_schedulable && !rhs.is_fully_schedulable) {
        return true;
    }
    if (!lhs.is_fully_schedulable && rhs.is_fully_schedulable) {
        return false;
    }

    bool strictly_better = false;
    for (std::size_t i = 0; i < lhs.fitness.size(); ++i) {
        if (lhs.fitness[i] > rhs.fitness[i]) {
            return false;
        }
        if (lhs.fitness[i] < rhs.fitness[i]) {
            strictly_better = true;
        }
    }
    return strictly_better;
}

std::vector<std::vector<std::size_t>> fast_non_dominated_sort(std::vector<Individual>& population) {
    std::vector<std::vector<std::size_t>> domination_set(population.size());
    std::vector<int> dominated_count(population.size(), 0);
    std::vector<std::vector<std::size_t>> fronts(1);

    for (std::size_t i = 0; i < population.size(); ++i) {
        for (std::size_t j = 0; j < population.size(); ++j) {
            if (i == j) {
                continue;
            }
            if (dominates(population[i], population[j])) {
                domination_set[i].push_back(j);
            } else if (dominates(population[j], population[i])) {
                dominated_count[i] += 1;
            }
        }
        if (dominated_count[i] == 0) {
            population[i].rank = 1;
            fronts[0].push_back(i);
        }
    }

    std::size_t front_index = 0;
    while (front_index < fronts.size()) {
        std::vector<std::size_t> next_front;
        for (std::size_t p : fronts[front_index]) {
            for (std::size_t q : domination_set[p]) {
                dominated_count[q] -= 1;
                if (dominated_count[q] == 0) {
                    population[q].rank = static_cast<int>(front_index) + 2;
                    next_front.push_back(q);
                }
            }
        }
        ++front_index;
        if (!next_front.empty()) {
            fronts.push_back(std::move(next_front));
        }
    }

    return fronts;
}

void calculate_crowding_distance(std::vector<Individual>& population, const std::vector<std::size_t>& front) {
    if (front.empty()) {
        return;
    }
    if (front.size() <= 2) {
        for (std::size_t idx : front) {
            population[idx].crowding_distance = INF;
        }
        return;
    }

    for (std::size_t idx : front) {
        population[idx].crowding_distance = 0.0;
    }

    for (std::size_t objective = 0; objective < population.front().fitness.size(); ++objective) {
        std::vector<std::size_t> sorted = front;
        std::sort(sorted.begin(), sorted.end(), [&](std::size_t a, std::size_t b) {
            return population[a].fitness[objective] < population[b].fitness[objective];
        });

        population[sorted.front()].crowding_distance = INF;
        population[sorted.back()].crowding_distance = INF;

        const double min_value = population[sorted.front()].fitness[objective];
        const double max_value = population[sorted.back()].fitness[objective];
        if (max_value == min_value) {
            continue;
        }

        for (std::size_t i = 1; i + 1 < sorted.size(); ++i) {
            if (std::isinf(population[sorted[i]].crowding_distance)) {
                continue;
            }
            population[sorted[i]].crowding_distance +=
                (population[sorted[i + 1]].fitness[objective] - population[sorted[i - 1]].fitness[objective]) /
                (max_value - min_value);
        }
    }
}

std::vector<Individual> generate_initial_population(
    const Individual& seed,
    const GAEnvironment& environment,
    std::size_t pop_size,
    std::mt19937& rng) {
    std::vector<Individual> population;
    population.reserve(pop_size);
    population.push_back(seed);

    std::size_t attempts = 0;
    std::cout << "初始化种群 (目标: " << pop_size << ")...\n";
    while (population.size() < pop_size) {
        ++attempts;
        Individual mutant = mutate_seed(seed, 0.02, rng);
        BusTopology topology = decode_to_topology(mutant, environment);
        if (validate_topology(topology)) {
            population.push_back(std::move(mutant));
        }
        if (attempts % 10000 == 0) {
            std::cout << "已尝试 " << attempts << " 次，当前收集到合法解: " << population.size() << " 个\n";
        }
    }
    std::cout << "初始化完成！总生成/校验次数: " << attempts
              << "，最终种群规模: " << population.size() << "\n";
    return population;
}

std::vector<Individual> generate_offspring_population(
    const std::vector<Individual>& population,
    const Individual& seed,
    const GAEnvironment& environment,
    std::size_t pop_size,
    double mutation_rate,
    std::mt19937& rng) {
    std::vector<Individual> offspring;
    offspring.reserve(pop_size);
    int immigrant_count = 0;

    while (offspring.size() < pop_size) {
        Individual parent1 = tournament_select(population, rng);
        Individual parent2 = tournament_select(population, rng);
        int valid_children_found = 0;
        int attempts = 0;

        while (valid_children_found < 2 && attempts < 50) {
            ++attempts;
            auto [c1, c2] = crossover(parent1, parent2, rng);
            Individual m1 = mutate(c1, mutation_rate, rng);
            Individual m2 = mutate(c2, mutation_rate, rng);

            if (valid_children_found < 2) {
                BusTopology topo = decode_to_topology(m1, environment);
                if (validate_topology(topo)) {
                    offspring.push_back(std::move(m1));
                    ++valid_children_found;
                    if (offspring.size() >= pop_size) {
                        break;
                    }
                }
            }

            if (valid_children_found < 2) {
                BusTopology topo = decode_to_topology(m2, environment);
                if (validate_topology(topo)) {
                    offspring.push_back(std::move(m2));
                    ++valid_children_found;
                    if (offspring.size() >= pop_size) {
                        break;
                    }
                }
            }
        }

        while (valid_children_found < 2 && offspring.size() < pop_size) {
            offspring.push_back(generate_single_immigrant(seed, environment, rng));
            ++valid_children_found;
            ++immigrant_count;
        }
    }

    std::cout << "  成功生成 " << offspring.size() << " 个合法子代 (移民: " << immigrant_count << " 个).\n";
    return offspring;
}

std::vector<Individual> environmental_selection(
    std::vector<Individual> parents,
    std::vector<Individual> offspring,
    std::size_t pop_size,
    std::mt19937& rng) {
    std::vector<Individual> combined;
    combined.reserve(parents.size() + offspring.size());
    combined.insert(combined.end(), std::make_move_iterator(parents.begin()), std::make_move_iterator(parents.end()));
    combined.insert(combined.end(), std::make_move_iterator(offspring.begin()), std::make_move_iterator(offspring.end()));

    std::vector<Individual> unique;
    std::unordered_set<std::string> seen;
    for (Individual& individual : combined) {
        const std::string key = gene_fingerprint(individual);
        if (seen.insert(key).second) {
            unique.push_back(std::move(individual));
        }
    }

    auto fronts = fast_non_dominated_sort(unique);
    std::vector<Individual> next;
    next.reserve(pop_size);

    for (const auto& front : fronts) {
        calculate_crowding_distance(unique, front);
        if (next.size() + front.size() <= pop_size) {
            for (std::size_t idx : front) {
                next.push_back(unique[idx]);
            }
        } else {
            std::vector<std::size_t> sorted = front;
            std::sort(sorted.begin(), sorted.end(), [&](std::size_t a, std::size_t b) {
                return unique[a].crowding_distance > unique[b].crowding_distance;
            });
            const std::size_t remaining = pop_size - next.size();
            for (std::size_t i = 0; i < remaining && i < sorted.size(); ++i) {
                next.push_back(unique[sorted[i]]);
            }
            break;
        }
    }

    if (next.empty() && !unique.empty()) {
        next.push_back(unique.front());
    }
    std::uniform_int_distribution<std::size_t> pick(0, next.size() - 1);
    while (next.size() < pop_size) {
        next.push_back(next[pick(rng)]);
    }

    return next;
}

Individual select_strategy_solution(
    std::vector<Individual> population,
    StrategyKind strategy) {
    if (population.empty()) {
        return Individual{};
    }

    auto fronts = fast_non_dominated_sort(population);
    const std::vector<std::size_t>* candidates = fronts.empty() || fronts.front().empty()
                                                     ? nullptr
                                                     : &fronts.front();
    std::vector<std::size_t> fallback;
    if (candidates == nullptr) {
        fallback.resize(population.size());
        std::iota(fallback.begin(), fallback.end(), 0);
        candidates = &fallback;
    }

    const auto finite_or_worst = [](double value, bool smaller_is_better) {
        if (std::isfinite(value)) {
            return value;
        }
        return smaller_is_better ? INF : -INF;
    };

    auto better = [&](const Individual& lhs, const Individual& rhs) {
        if (lhs.is_fully_schedulable != rhs.is_fully_schedulable) {
            return lhs.is_fully_schedulable;
        }

        const double lhs_latency = finite_or_worst(lhs.metrics.at("comp_lat"), true);
        const double rhs_latency = finite_or_worst(rhs.metrics.at("comp_lat"), true);
        const double lhs_reliability = lhs.metrics.at("comp_rel");
        const double rhs_reliability = rhs.metrics.at("comp_rel");
        const double lhs_throughput = lhs.metrics.at("throughput_score");
        const double rhs_throughput = rhs.metrics.at("throughput_score");
        const double lhs_connectivity = lhs.metrics.at("fiedler");
        const double rhs_connectivity = rhs.metrics.at("fiedler");
        const double lhs_cost = finite_or_worst(lhs.metrics.at("cost"), true);
        const double rhs_cost = finite_or_worst(rhs.metrics.at("cost"), true);

        if (strategy == StrategyKind::LowLatency) {
            if (std::abs(lhs_latency - rhs_latency) > 1e-12) {
                return lhs_latency < rhs_latency;
            }
            if (std::abs(lhs_throughput - rhs_throughput) > 1e-12) {
                return lhs_throughput > rhs_throughput;
            }
            if (std::abs(lhs_reliability - rhs_reliability) > 1e-12) {
                return lhs_reliability > rhs_reliability;
            }
            return lhs_cost < rhs_cost;
        }

        if (std::abs(lhs_reliability - rhs_reliability) > 1e-12) {
            return lhs_reliability > rhs_reliability;
        }
        if (std::abs(lhs_connectivity - rhs_connectivity) > 1e-12) {
            return lhs_connectivity > rhs_connectivity;
        }
        if (std::abs(lhs_throughput - rhs_throughput) > 1e-12) {
            return lhs_throughput > rhs_throughput;
        }
        if (std::abs(lhs_latency - rhs_latency) > 1e-12) {
            return lhs_latency < rhs_latency;
        }
        return lhs_cost < rhs_cost;
    };

    std::size_t best_index = candidates->front();
    for (std::size_t idx : *candidates) {
        if (better(population[idx], population[best_index])) {
            best_index = idx;
        }
    }
    return population[best_index];
}

NSGA2Result run_nsga2(const GAEnvironment& environment, const NSGA2Options& options) {
    std::mt19937 rng(options.rng_seed);
    DeterministicRouter router;

    Individual seed;
    seed.role_gene = environment.seed_role_gene;
    seed.link_gene = environment.seed_link_gene;

    std::cout << "=== 环境初始化与拓扑装配 ===\n";
    std::cout << "逻辑节点映射数量: " << environment.nodes.size() << "\n";
    std::cout << "逻辑链路基因空间 (组合总数): " << environment.candidate_bus_links.size() << "\n";
    std::cout << "前置物理链路约束数量: " << environment.physical_links.size() << "\n";

    evaluate_individual(seed, environment, router);
    std::cout << "\n[Baseline Seed] 多维适应度 Fitness: ("
              << seed.fitness[0] << ", " << seed.fitness[1] << ", "
              << seed.fitness[2] << ", " << seed.fitness[3] << ", "
              << seed.fitness[4] << ")\n";
    std::cout << "[Baseline Seed] 物理连通状态及可调度性: "
              << (seed.is_fully_schedulable ? "true" : "false") << "\n";

    std::vector<Individual> population = generate_initial_population(seed, environment, options.pop_size, rng);
    for (Individual& individual : population) {
        evaluate_individual(individual, environment, router);
    }

    auto fronts = fast_non_dominated_sort(population);
    for (const auto& front : fronts) {
        calculate_crowding_distance(population, front);
    }

    std::cout << "\n=== 进入 NSGA-II 优化循环 ===\n";
    for (int gen = 1; gen <= options.max_gen; ++gen) {
        std::vector<Individual> offspring = generate_offspring_population(
            population, seed, environment, options.pop_size, options.mutation_rate, rng);
        for (Individual& individual : offspring) {
            evaluate_individual(individual, environment, router);
        }

        population = environmental_selection(std::move(population), std::move(offspring), options.pop_size, rng);
        fronts = fast_non_dominated_sort(population);
        for (const auto& front : fronts) {
            calculate_crowding_distance(population, front);
        }

        const std::size_t rank1_size = fronts.empty() ? 0 : fronts.front().size();
        const int schedulable_count = static_cast<int>(std::count_if(population.begin(), population.end(), [](const Individual& ind) {
            return ind.is_fully_schedulable;
        }));

        const auto best_latency_it = std::min_element(population.begin(), population.end(), [](const Individual& a, const Individual& b) {
            return a.metrics.at("comp_lat") < b.metrics.at("comp_lat");
        });
        const double best_cost = std::min_element(population.begin(), population.end(), [](const Individual& a, const Individual& b) {
            return a.metrics.at("cost") < b.metrics.at("cost");
        })->metrics.at("cost");
        const double best_throughput_score = std::max_element(population.begin(), population.end(), [](const Individual& a, const Individual& b) {
            return a.metrics.at("throughput_score") < b.metrics.at("throughput_score");
        })->metrics.at("throughput_score");

        std::cout << "\nGen " << gen << "/" << options.max_gen
                  << " | 留存种群规模: " << population.size()
                  << " | 帕累托最优个体数(Rank 1): " << rank1_size
                  << " | 硬约束达标数: " << schedulable_count << "/" << options.pop_size
                  << " | 最佳综合时延: " << best_latency_it->metrics.at("comp_lat")
                  << " | 最佳吞吐得分: " << best_throughput_score
                  << " | 最佳成本下限: " << best_cost << "\n";
    }

    fronts = fast_non_dominated_sort(population);
    for (const auto& front : fronts) {
        calculate_crowding_distance(population, front);
    }
    Individual best = fronts.empty() || fronts.front().empty() ? population.front() : population[fronts.front().front()];

    std::cout << "\n=== 寻优结束 (已达最大迭代上限) ===\n";
    std::cout << "环境抉择首位最优个体:\n";
    std::cout << " - 适应度体系 (时延综合占比, 负面吞吐得分, 负面代数连通度, 负面可靠性, 组网成本):\n   ("
              << best.fitness[0] << ", " << best.fitness[1] << ", "
              << best.fitness[2] << ", " << best.fitness[3] << ", "
              << best.fitness[4] << ")\n";
    std::cout << " - 解析物理指标:\n"
              << "   comp_lat=" << best.metrics["comp_lat"]
              << ", max_lat=" << best.metrics["max_lat"]
              << ", throughput_score=" << best.metrics["throughput_score"]
              << ", fiedler=" << best.metrics["fiedler"]
              << ", comp_rel=" << best.metrics["comp_rel"]
              << ", cost=" << best.metrics["cost"] << "\n";
    std::cout << " - 全网路由业务流满足约束率: "
              << (best.is_fully_schedulable ? "true" : "false") << "\n";

    return NSGA2Result{population, best};
}

} // namespace topoopt
