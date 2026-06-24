#pragma once

#include "ga.hpp"

#include <string>
#include <utility>
#include <vector>

namespace topoopt {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool bool_value = false;
    double number_value = 0.0;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::vector<std::pair<std::string, JsonValue>> object_value;

    static JsonValue null();
    static JsonValue boolean(bool value);
    static JsonValue number(double value);
    static JsonValue string(std::string value);
    static JsonValue array();
    static JsonValue object();

    bool is_object() const;
    bool is_array() const;
    bool contains(const std::string& key) const;
    const JsonValue& at(const std::string& key) const;
    JsonValue& at(const std::string& key);
    const JsonValue& operator[](const std::string& key) const;
    JsonValue& operator[](const std::string& key);
};

struct LoadedConfig {
    JsonValue root;
    GAEnvironment environment;
    NSGA2Options options;
};

LoadedConfig load_config_file(const std::string& path);
void write_result_config(
    const std::string& path,
    JsonValue root,
    const GAEnvironment& environment,
    const Individual& best,
    const EvaluationResult& result,
    const BusTopology& best_topology);
void write_strategy_result_configs(
    const std::string& path,
    JsonValue root,
    const GAEnvironment& environment,
    const Individual& low_latency,
    const EvaluationResult& low_latency_result,
    const BusTopology& low_latency_topology,
    const Individual& high_reliability,
    const EvaluationResult& high_reliability_result,
    const BusTopology& high_reliability_topology);
void write_strategy_result_configs_to_paths(
    const std::string& initial_path,
    const std::string& low_latency_path,
    const std::string& high_reliability_path,
    JsonValue root,
    const GAEnvironment& environment,
    const Individual& low_latency,
    const EvaluationResult& low_latency_result,
    const BusTopology& low_latency_topology,
    const Individual& high_reliability,
    const EvaluationResult& high_reliability_result,
    const BusTopology& high_reliability_topology);

} // namespace topoopt
