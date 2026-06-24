#include "evaluators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace topoopt {

namespace {
constexpr double EPSILON = 1e-9;
constexpr double INF = std::numeric_limits<double>::infinity();

double get_delay(const std::unordered_map<DelayKey, double, DelayKeyHash>& delays, int flow_id, int node_id, char phase) {
    const auto it = delays.find(DelayKey{flow_id, node_id, phase});
    return it == delays.end() ? 0.0 : it->second;
}

std::vector<double> jacobi_eigenvalues(std::vector<std::vector<double>> matrix) {
    const int n = static_cast<int>(matrix.size());
    for (int iter = 0; iter < 100 * n * n; ++iter) {
        int p = 0;
        int q = 1;
        double max_offdiag = 0.0;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                const double value = std::abs(matrix[i][j]);
                if (value > max_offdiag) {
                    max_offdiag = value;
                    p = i;
                    q = j;
                }
            }
        }
        if (max_offdiag < 1e-10) {
            break;
        }

        const double app = matrix[p][p];
        const double aqq = matrix[q][q];
        const double apq = matrix[p][q];
        const double phi = 0.5 * std::atan2(2.0 * apq, aqq - app);
        const double c = std::cos(phi);
        const double s = std::sin(phi);

        for (int k = 0; k < n; ++k) {
            if (k == p || k == q) {
                continue;
            }
            const double mkp = matrix[k][p];
            const double mkq = matrix[k][q];
            matrix[k][p] = matrix[p][k] = c * mkp - s * mkq;
            matrix[k][q] = matrix[q][k] = s * mkp + c * mkq;
        }

        matrix[p][p] = c * c * app - 2.0 * s * c * apq + s * s * aqq;
        matrix[q][q] = s * s * app + 2.0 * s * c * apq + c * c * aqq;
        matrix[p][q] = matrix[q][p] = 0.0;
    }

    std::vector<double> values;
    values.reserve(n);
    for (int i = 0; i < n; ++i) {
        values.push_back(matrix[i][i]);
    }
    std::sort(values.begin(), values.end());
    return values;
}
} // namespace

double NetworkCostEvaluator::evaluate(const BusTopology& topology) const {
    double total_cost = 0.0;
    for (const BusLink* link : topology.bus_link_list()) {
        total_cost += link->cost;
    }
    return total_cost;
}

double ReliabilityEvaluator::evaluate_flow(const Flow& flow, const BusTopology& topology) const {
    if (flow.physical_routing_path.empty()) {
        return 0.0;
    }

    double actual = 1.0;
    for (const Node* node : flow.physical_routing_path) {
        actual *= node->reliability;
    }
    for (std::size_t i = 0; i + 1 < flow.physical_routing_path.size(); ++i) {
        const PhysicalLink* link = topology.get_physical_link(flow.physical_routing_path[i], flow.physical_routing_path[i + 1]);
        if (link == nullptr) {
            return 0.0;
        }
        actual *= link->reliability;
    }
    return actual;
}

void ReliabilityEvaluator::evaluate_all(BusTopology& topology) const {
    for (Flow* flow : topology.flow_list()) {
        const double reliability = evaluate_flow(*flow, topology);
        flow->actual_reliability = reliability;
        flow->is_schedulable = flow->is_schedulable && reliability >= flow->reliability_requirement;
    }
}

double AlgebraicConnectivityEvaluator::evaluate(const BusTopology& topology) const {
    const auto nodes = topology.node_list();
    const int n = static_cast<int>(nodes.size());
    if (n < 2) {
        return 0.0;
    }

    std::unordered_map<int, int> id_to_index;
    for (int i = 0; i < n; ++i) {
        id_to_index[nodes[i]->id] = i;
    }

    std::vector<std::vector<double>> laplacian(n, std::vector<double>(n, 0.0));
    for (const BusLink* link : topology.bus_link_list()) {
        const int u = id_to_index[link->node_a->id];
        const int v = id_to_index[link->node_b->id];
        laplacian[u][u] += 1.0;
        laplacian[v][v] += 1.0;
        laplacian[u][v] -= 1.0;
        laplacian[v][u] -= 1.0;
    }

    auto eigenvalues = jacobi_eigenvalues(std::move(laplacian));
    double fiedler = eigenvalues[1];
    if (fiedler < 1e-10) {
        fiedler = 0.0;
    }
    return fiedler;
}

bool DelayKey::operator==(const DelayKey& other) const noexcept {
    return flow_id == other.flow_id && node_id == other.node_id && phase == other.phase;
}

std::size_t DelayKeyHash::operator()(const DelayKey& key) const noexcept {
    return (static_cast<std::size_t>(key.flow_id) << 33U) ^
           (static_cast<std::size_t>(key.node_id) << 1U) ^
           static_cast<std::size_t>(key.phase);
}

bool LinkDelayKey::operator==(const LinkDelayKey& other) const noexcept {
    return node_a == other.node_a && node_b == other.node_b && flow_id == other.flow_id;
}

std::size_t LinkDelayKeyHash::operator()(const LinkDelayKey& key) const noexcept {
    return (static_cast<std::size_t>(key.node_a) << 34U) ^
           (static_cast<std::size_t>(key.node_b) << 17U) ^
           static_cast<std::size_t>(key.flow_id);
}

CPAEngine::CPAEngine(std::unordered_map<LinkDelayKey, double, LinkDelayKeyHash> link_delays)
    : link_delays_(std::move(link_delays)) {}

double CPAEngine::get_base_processing_time(const Flow& flow, const Node& node) const {
    if (node.cpu_capacity <= 0.0) {
        return INF;
    }
    return flow.message_size / node.cpu_capacity;
}

double CPAEngine::ArrivalCurve::max_events(double delta_t, double pending_time) const {
    if (delta_t <= 0.0) {
        return 0.0;
    }
    if (std::isinf(pending_time) || std::isinf(jitter)) {
        return INF;
    }
    return std::ceil((delta_t + pending_time + jitter - EPSILON) / period);
}

CPAEngine::ArrivalCurve CPAEngine::get_arrival_curve(
    const Flow& flow,
    const Node& target_node,
    char phase,
    const std::unordered_map<DelayKey, double, DelayKeyHash>& global_delays) const {
    double cumulative_jitter = 0.0;

    for (const Node* current_node : flow.physical_routing_path) {
        if (current_node->id == target_node.id) {
            if (phase == 'T' && contains_flow(current_node->relay_flows, &flow)) {
                cumulative_jitter += get_delay(global_delays, flow.id, current_node->id, 'R');
            }
            break;
        }

        if (contains_flow(current_node->source_flows, &flow)) {
            cumulative_jitter += get_delay(global_delays, flow.id, current_node->id, 'T');
        } else if (contains_flow(current_node->relay_flows, &flow)) {
            cumulative_jitter += get_delay(global_delays, flow.id, current_node->id, 'R');
            cumulative_jitter += get_delay(global_delays, flow.id, current_node->id, 'T');
        }
    }

    return ArrivalCurve{flow.period, cumulative_jitter};
}

double CPAEngine::calculate_interference(
    const Flow& target_flow,
    char phase,
    double delta_t,
    const std::vector<Competitor>& competitors) const {
    double total_interference = 0.0;
    double max_lower_priority_blocking = 0.0;

    for (const Competitor& competitor : competitors) {
        if (competitor.flow->id == target_flow.id) {
            const double events = competitor.curve.max_events(delta_t, competitor.local_r_bar);
            total_interference += std::max(0.0, events - 1.0) * competitor.cost_per_event;
            continue;
        }

        if (phase == 'T') {
            if (competitor.flow->priority > target_flow.priority) {
                const double events = competitor.curve.max_events(delta_t, competitor.local_r_bar);
                total_interference += events * competitor.cost_per_event;
            } else {
                max_lower_priority_blocking = std::max(max_lower_priority_blocking, competitor.cost_per_event);
            }
        } else if (phase == 'R') {
            const double burst_events = competitor.curve.max_events(EPSILON, competitor.local_r_bar);
            total_interference += burst_events * competitor.cost_per_event;
        }
    }

    if (phase == 'T') {
        total_interference += max_lower_priority_blocking;
    }
    return total_interference;
}

double CPAEngine::solve_queueing_delay(
    const Flow& target_flow,
    const Node& node,
    char phase,
    const std::unordered_map<DelayKey, double, DelayKeyHash>& global_delays) const {
    const double base_time = get_base_processing_time(target_flow, node);
    const std::vector<Flow*>& raw_competitors = phase == 'R' ? node.sink_flows : node.source_flows;
    std::vector<Flow*> competitors = raw_competitors;
    competitors.insert(competitors.end(), node.relay_flows.begin(), node.relay_flows.end());

    std::vector<Competitor> precomputed;
    precomputed.reserve(competitors.size());
    for (const Flow* comp_flow : competitors) {
        const auto curve = get_arrival_curve(*comp_flow, node, phase, global_delays);
        const double cost_per_event = get_base_processing_time(*comp_flow, node);
        const double local_r_bar = comp_flow->id == target_flow.id
                                       ? 0.0
                                       : get_delay(global_delays, comp_flow->id, node.id, phase) + cost_per_event;
        precomputed.push_back(Competitor{comp_flow, curve, cost_per_event, local_r_bar});
    }

    double t = base_time;
    constexpr double max_timeout = 200000.0;
    while (t < max_timeout) {
        const double interference = calculate_interference(target_flow, phase, t, precomputed);
        const double demand = base_time + interference;
        if (t >= demand) {
            return t - base_time;
        }
        t = demand;
    }
    return INF;
}

double CPAEngine::get_response_time(
    const Flow& flow,
    const Node& node,
    char phase,
    const std::unordered_map<DelayKey, double, DelayKeyHash>& global_delays) const {
    return get_delay(global_delays, flow.id, node.id, phase) + get_base_processing_time(flow, node);
}

double CPAEngine::link_delay(int node_a, int node_b, int flow_id) const {
    const auto it = link_delays_.find(LinkDelayKey{node_a, node_b, flow_id});
    return it == link_delays_.end() ? 0.0 : it->second;
}

GlobalCPAAnalyzer::GlobalCPAAnalyzer(std::vector<Flow*> flows, const CPAEngine& engine)
    : flows_(std::move(flows)), engine_(engine) {
    for (const Flow* flow : flows_) {
        for (const Node* node : flow->physical_routing_path) {
            if (contains_flow(node->source_flows, flow)) {
                global_delays_[DelayKey{flow->id, node->id, 'T'}] = 0.0;
            } else if (contains_flow(node->sink_flows, flow)) {
                global_delays_[DelayKey{flow->id, node->id, 'R'}] = 0.0;
            } else if (contains_flow(node->relay_flows, flow)) {
                global_delays_[DelayKey{flow->id, node->id, 'R'}] = 0.0;
                global_delays_[DelayKey{flow->id, node->id, 'T'}] = 0.0;
            }
        }
    }
}

std::unordered_map<int, double> GlobalCPAAnalyzer::analyze() {
    constexpr int max_iters = 100;
    constexpr double epsilon = 0.001;
    bool converged = false;
    int iteration = 0;

    while (!converged && iteration < max_iters) {
        converged = true;
        const auto snapshot = global_delays_;
        auto next_delays = global_delays_;

        for (const Flow* flow : flows_) {
            for (const Node* node : flow->physical_routing_path) {
                std::vector<char> phases;
                if (contains_flow(node->source_flows, flow)) {
                    phases.push_back('T');
                } else if (contains_flow(node->sink_flows, flow)) {
                    phases.push_back('R');
                } else if (contains_flow(node->relay_flows, flow)) {
                    phases.push_back('R');
                    phases.push_back('T');
                }

                for (char phase : phases) {
                    const DelayKey key{flow->id, node->id, phase};
                    const double old_delay = snapshot.at(key);
                    const double new_delay = engine_.solve_queueing_delay(*flow, *node, phase, snapshot);
                    next_delays[key] = new_delay;
                    if (!std::isinf(new_delay) && std::abs(new_delay - old_delay) > epsilon) {
                        converged = false;
                    }
                }
            }
        }

        global_delays_ = std::move(next_delays);
        ++iteration;
    }

    return build_e2e_results();
}

std::unordered_map<int, double> GlobalCPAAnalyzer::build_e2e_results() const {
    std::unordered_map<int, double> results;

    for (const Flow* flow : flows_) {
        bool overloaded = false;
        for (const auto& item : global_delays_) {
            if (item.first.flow_id == flow->id && std::isinf(item.second)) {
                overloaded = true;
                break;
            }
        }
        if (overloaded) {
            results[flow->id] = INF;
            continue;
        }

        double total_delay = flow->pre_processing_time;
        for (std::size_t i = 0; i < flow->physical_routing_path.size(); ++i) {
            const Node* node = flow->physical_routing_path[i];
            const double base_proc = engine_.get_base_processing_time(*flow, *node);

            if (contains_flow(node->source_flows, flow)) {
                total_delay += base_proc + get_delay(global_delays_, flow->id, node->id, 'T');
            } else if (contains_flow(node->sink_flows, flow)) {
                total_delay += base_proc + get_delay(global_delays_, flow->id, node->id, 'R');
            } else if (contains_flow(node->relay_flows, flow)) {
                total_delay += base_proc +
                               get_delay(global_delays_, flow->id, node->id, 'R') +
                               get_delay(global_delays_, flow->id, node->id, 'T');
            }

            if (i + 1 < flow->physical_routing_path.size()) {
                total_delay += engine_.link_delay(node->id, flow->physical_routing_path[i + 1]->id, flow->id);
            }
        }
        total_delay += flow->post_processing_time;
        results[flow->id] = total_delay;
    }

    return results;
}

std::pair<double, std::unordered_map<int, double>> calculate_topology_latency(BusTopology& topology) {
    std::unordered_map<LinkDelayKey, double, LinkDelayKeyHash> link_delays;
    for (const PhysicalLink* link : topology.physical_link_list()) {
        for (const Flow* flow : link->passing_flows) {
            const double delay = link->propagation_delay + flow->message_size / link->bandwidth;
            link_delays[LinkDelayKey{link->node_a->id, link->node_b->id, flow->id}] = delay;
            link_delays[LinkDelayKey{link->node_b->id, link->node_a->id, flow->id}] = delay;
        }
    }

    CPAEngine engine(std::move(link_delays));
    GlobalCPAAnalyzer analyzer(topology.flow_list(), engine);
    auto e2e = analyzer.analyze();

    double max_latency = -1.0;
    for (Flow* flow : topology.flow_list()) {
        const double latency = e2e.count(flow->id) == 0U ? INF : e2e[flow->id];
        flow->worst_case_delay = latency;
        flow->is_schedulable = latency <= flow->deadline;
        max_latency = std::max(max_latency, latency);
    }

    return {max_latency, e2e};
}

} // namespace topoopt
