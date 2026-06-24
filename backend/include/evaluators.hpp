#pragma once

#include "models.hpp"

#include <string>
#include <unordered_map>

namespace topoopt {

class NetworkCostEvaluator {
public:
    double evaluate(const BusTopology& topology) const;
};

class ReliabilityEvaluator {
public:
    double evaluate_flow(const Flow& flow, const BusTopology& topology) const;
    void evaluate_all(BusTopology& topology) const;
};

class AlgebraicConnectivityEvaluator {
public:
    double evaluate(const BusTopology& topology) const;
};

struct DelayKey {
    int flow_id = 0;
    int node_id = 0;
    char phase = 'T';

    bool operator==(const DelayKey& other) const noexcept;
};

struct DelayKeyHash {
    std::size_t operator()(const DelayKey& key) const noexcept;
};

struct LinkDelayKey {
    int node_a = 0;
    int node_b = 0;
    int flow_id = 0;

    bool operator==(const LinkDelayKey& other) const noexcept;
};

struct LinkDelayKeyHash {
    std::size_t operator()(const LinkDelayKey& key) const noexcept;
};

class CPAEngine {
public:
    explicit CPAEngine(std::unordered_map<LinkDelayKey, double, LinkDelayKeyHash> link_delays);

    double get_base_processing_time(const Flow& flow, const Node& node) const;
    double solve_queueing_delay(
        const Flow& target_flow,
        const Node& node,
        char phase,
        const std::unordered_map<DelayKey, double, DelayKeyHash>& global_delays) const;
    double get_response_time(
        const Flow& flow,
        const Node& node,
        char phase,
        const std::unordered_map<DelayKey, double, DelayKeyHash>& global_delays) const;
    double link_delay(int node_a, int node_b, int flow_id) const;

private:
    struct ArrivalCurve {
        double period = 0.0;
        double jitter = 0.0;
        double max_events(double delta_t, double pending_time = 0.0) const;
    };

    struct Competitor {
        const Flow* flow = nullptr;
        ArrivalCurve curve;
        double cost_per_event = 0.0;
        double local_r_bar = 0.0;
    };

    ArrivalCurve get_arrival_curve(
        const Flow& flow,
        const Node& target_node,
        char phase,
        const std::unordered_map<DelayKey, double, DelayKeyHash>& global_delays) const;

    double calculate_interference(
        const Flow& target_flow,
        char phase,
        double delta_t,
        const std::vector<Competitor>& competitors) const;

    std::unordered_map<LinkDelayKey, double, LinkDelayKeyHash> link_delays_;
};

class GlobalCPAAnalyzer {
public:
    GlobalCPAAnalyzer(std::vector<Flow*> flows, const CPAEngine& engine);
    std::unordered_map<int, double> analyze();

private:
    std::unordered_map<int, double> build_e2e_results() const;

    std::vector<Flow*> flows_;
    const CPAEngine& engine_;
    std::unordered_map<DelayKey, double, DelayKeyHash> global_delays_;
};

std::pair<double, std::unordered_map<int, double>> calculate_topology_latency(BusTopology& topology);

} // namespace topoopt
