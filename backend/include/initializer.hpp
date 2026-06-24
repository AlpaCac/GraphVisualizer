#pragma once

#include "models.hpp"

#include <vector>

namespace topoopt {

double calculate_distance(const Node& n1, const Node& n2);
BusTopology build_sandbox_topology();

} // namespace topoopt
