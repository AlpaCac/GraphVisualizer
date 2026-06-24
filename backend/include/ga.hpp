#pragma once

#include "models.hpp"
#include "routing.hpp"

#include <array>
#include <random>
#include <unordered_map>
#include <vector>

namespace topoopt {

struct Individual {
    std::vector<int> role_gene;
    std::vector<int> link_gene;
    std::array<double, 5> fitness = {0.0, 0.0, 0.0, 0.0, 0.0};
    int rank = 0;
    double crowding_distance = 0.0;
    std::unordered_map<std::string, double> metrics;
    bool is_fully_schedulable = false;
};

struct EvaluationResult {
    double composite_latency = 0.0;
    double max_latency = 0.0;
    double fiedler = 0.0;
    double connectivity_norm = 0.0;
    double throughput_gbps = 0.0;
    double composite_throughput_score = 0.0;
    double composite_reliability = 0.0;
    double cost = 0.0;
    bool fully_schedulable = false;
};

struct MacParams {
    double sigma_us = 9.0;
    double sifs_us = 10.0;
    double difs_us = 28.0;
    double ack_us = 24.0;
    double header_us = 20.0;
    double p_cap = 0.0;
    double p_e_base = 0.01;
};

struct CostModelParams {
    double runtime_weight = 0.7;
    double reconstruction_weight = 0.3;
    double bandwidth_weight = 0.4;
    double node_resource_weight = 0.3;
    double interface_weight = 0.3;
    double add_link_weight = 0.35;
    double delete_link_weight = 0.20;
    double path_switch_weight = 0.45;
};

struct NodeSpec {
    int id = 0;
    double cpu_capacity = 0.0;
    double memory_capacity = 0.0;
    double x = 0.0;
    double y = 0.0;
    int max_physical_ports = 0;
    double reliability = 1.0;
    bool is_core = false;
};

struct LinkSpec {
    int id = 0;
    int node_a = 0;
    int node_b = 0;
    double bandwidth = 0.0;
    double propagation_delay = 0.0;
    double reliability = 1.0;
    double cost = 0.0;
};

struct FlowSpec {
    int id = 0;
    std::string name;
    int src = 0;
    int dst = 0;
    int priority = 0;
    double period = 0.0;
    double deadline = 0.0;
    double message_size = 0.0;
    double reliability_requirement = 0.0;
};

struct GAEnvironment {
    std::vector<NodeSpec> nodes;
    std::vector<LinkSpec> physical_links;
    std::vector<std::pair<int, int>> candidate_bus_links;
    std::vector<int> seed_role_gene;
    std::vector<int> seed_link_gene;
    std::vector<FlowSpec> flows;
    MacParams mac_params;
    CostModelParams cost_params;
    std::unordered_map<int, std::vector<int>> baseline_physical_paths;
    double baseline_fiedler = 0.0;
};

struct NSGA2Options {
    int pop_size = 100;
    int max_gen = 50;
    double mutation_rate = 0.01;
    unsigned int rng_seed = 42;
};

struct NSGA2Result {
    std::vector<Individual> population;
    Individual best;
};

enum class StrategyKind {
    LowLatency,
    HighReliability
};

GAEnvironment build_default_ga_environment();
void populate_baseline_paths(GAEnvironment& environment, const DeterministicRouter& router);
BusTopology decode_to_topology(const Individual& individual, const GAEnvironment& environment);
bool validate_topology(const BusTopology& topology);
EvaluationResult evaluate_topology(BusTopology& topology, const DeterministicRouter& router);
EvaluationResult evaluate_topology(BusTopology& topology, const DeterministicRouter& router, const GAEnvironment& environment);
double calculate_flow_throughput_gbps(const Flow& flow, const BusTopology& topology, const MacParams& mac_params);
EvaluationResult evaluate_individual(
    Individual& individual,
    const GAEnvironment& environment,
    const DeterministicRouter& router);
bool dominates(const Individual& lhs, const Individual& rhs);
std::vector<std::vector<std::size_t>> fast_non_dominated_sort(std::vector<Individual>& population);
void calculate_crowding_distance(std::vector<Individual>& population, const std::vector<std::size_t>& front);
std::vector<Individual> generate_initial_population(
    const Individual& seed,
    const GAEnvironment& environment,
    std::size_t pop_size,
    std::mt19937& rng);
std::vector<Individual> generate_offspring_population(
    const std::vector<Individual>& population,
    const Individual& seed,
    const GAEnvironment& environment,
    std::size_t pop_size,
    double mutation_rate,
    std::mt19937& rng);
std::vector<Individual> environmental_selection(
    std::vector<Individual> parents,
    std::vector<Individual> offspring,
    std::size_t pop_size,
    std::mt19937& rng);
Individual select_strategy_solution(
    std::vector<Individual> population,
    StrategyKind strategy);
NSGA2Result run_nsga2(const GAEnvironment& environment, const NSGA2Options& options);

} // namespace topoopt
