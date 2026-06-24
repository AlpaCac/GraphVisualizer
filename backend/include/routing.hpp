#pragma once

#include "models.hpp"

#include <tuple>

namespace topoopt {

class DeterministicRouter {
public:
    explicit DeterministicRouter(double congestion_penalty_alpha = 1.0);

    void reset_routing_states(BusTopology& topology, const std::vector<Flow*>& flows) const;
    void route_all_flows(BusTopology& topology) const;

private:
    using PathSegments = std::vector<std::vector<Node*>>;

    std::tuple<std::vector<Node*>, std::vector<Node*>, PathSegments> find_deterministic_path(
        BusTopology& topology,
        Node* source_node,
        Node* target_node,
        double demand,
        Flow* flow) const;

    std::pair<std::vector<Node*>, double> find_physical_shortest_path(
        Node* start_node,
        Node* end_node,
        double demand,
        Flow* flow) const;

    void update_link_states(
        BusTopology& topology,
        Flow* flow,
        double demand,
        const PathSegments& physical_segments) const;

    double alpha_;
};

} // namespace topoopt
