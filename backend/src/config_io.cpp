#include "config_io.hpp"

#include "evaluators.hpp"
#include "initializer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <set>
#include <stdexcept>

namespace topoopt {

namespace {
class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    JsonValue parse() {
        JsonValue value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            throw std::runtime_error("Unexpected trailing JSON content");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) {
            throw std::runtime_error("Unexpected end of JSON");
        }
        const char ch = text_[pos_];
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == '"') {
            return JsonValue::string(parse_string());
        }
        if (ch == 't' || ch == 'f') {
            return parse_bool();
        }
        if (ch == 'n') {
            return parse_null();
        }
        return parse_number();
    }

    JsonValue parse_object() {
        expect('{');
        JsonValue object = JsonValue::object();
        skip_ws();
        if (peek() == '}') {
            ++pos_;
            return object;
        }
        while (true) {
            skip_ws();
            std::string key = parse_string();
            skip_ws();
            expect(':');
            object.object_value.emplace_back(std::move(key), parse_value());
            skip_ws();
            if (peek() == '}') {
                ++pos_;
                break;
            }
            expect(',');
        }
        return object;
    }

    JsonValue parse_array() {
        expect('[');
        JsonValue array = JsonValue::array();
        skip_ws();
        if (peek() == ']') {
            ++pos_;
            return array;
        }
        while (true) {
            array.array_value.push_back(parse_value());
            skip_ws();
            if (peek() == ']') {
                ++pos_;
                break;
            }
            expect(',');
        }
        return array;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (pos_ < text_.size()) {
            char ch = text_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (pos_ >= text_.size()) {
                throw std::runtime_error("Invalid JSON escape");
            }
            char esc = text_[pos_++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                    // Keep unicode escapes ASCII-safe; input files in this project are UTF-8 already.
                    out += "\\u";
                    for (int i = 0; i < 4 && pos_ < text_.size(); ++i) {
                        out.push_back(text_[pos_++]);
                    }
                    break;
                default:
                    throw std::runtime_error("Unsupported JSON escape");
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    JsonValue parse_bool() {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            return JsonValue::boolean(true);
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            return JsonValue::boolean(false);
        }
        throw std::runtime_error("Invalid JSON boolean");
    }

    JsonValue parse_null() {
        if (text_.compare(pos_, 4, "null") != 0) {
            throw std::runtime_error("Invalid JSON null");
        }
        pos_ += 4;
        return JsonValue::null();
    }

    JsonValue parse_number() {
        const std::size_t start = pos_;
        if (peek() == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        return JsonValue::number(std::stod(text_.substr(start, pos_ - start)));
    }

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    char peek() const {
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    void expect(char expected) {
        skip_ws();
        if (peek() != expected) {
            throw std::runtime_error("Unexpected JSON token");
        }
        ++pos_;
    }

    std::string text_;
    std::size_t pos_ = 0;
};

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open input config: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

double number_or(const JsonValue& object, const std::string& key, double fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    return object.at(key).number_value;
}

int int_or(const JsonValue& object, const std::string& key, int fallback) {
    return static_cast<int>(std::llround(number_or(object, key, fallback)));
}

std::string string_or(const JsonValue& object, const std::string& key, const std::string& fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    return object.at(key).string_value;
}

NodeSpec node_spec_from_json(const JsonValue& value) {
    NodeSpec node;
    node.id = int_or(value, "id", 0);
    node.x = number_or(value, "x", 0.0);
    node.y = number_or(value, "y", 0.0);
    node.cpu_capacity = number_or(value, "cpu_capacity", 0.0);
    node.memory_capacity = number_or(value, "memory_mb", 0.0);
    node.max_physical_ports = int_or(value, "max_ports", 0);
    node.reliability = number_or(value, "reliability", 1.0);
    node.is_core = string_or(value, "static_type", "") == "Master";
    return node;
}

LinkSpec link_spec_from_json(const JsonValue& value) {
    LinkSpec link;
    link.id = int_or(value, "id", 0);
    link.node_a = int_or(value, "node_a", 0);
    link.node_b = int_or(value, "node_b", 0);
    link.bandwidth = number_or(value, "bandwidth", 0.0);
    link.propagation_delay = number_or(value, "propagation_delay", 0.0);
    link.reliability = number_or(value, "reliability", 1.0);
    link.cost = number_or(value, "cost", 0.0);
    return link;
}

FlowSpec flow_spec_from_json(const JsonValue& value) {
    FlowSpec flow;
    flow.id = int_or(value, "id", 0);
    flow.name = string_or(value, "name", "Flow_" + std::to_string(flow.id));
    flow.src = int_or(value, "src", 0);
    flow.dst = int_or(value, "dst", 0);
    flow.priority = int_or(value, "priority", 0);
    flow.period = number_or(value, "period", 0.0);
    flow.deadline = number_or(value, "deadline", 0.0);
    flow.message_size = number_or(value, "message_size", 0.0);
    flow.reliability_requirement = number_or(value, "reliability_req", 0.0);
    return flow;
}

MacParams mac_params_from_json(const JsonValue& value) {
    MacParams params;
    params.sigma_us = number_or(value, "sigma_us", params.sigma_us);
    params.sifs_us = number_or(value, "sifs_us", params.sifs_us);
    params.difs_us = number_or(value, "difs_us", params.difs_us);
    params.ack_us = number_or(value, "ack_us", params.ack_us);
    params.header_us = number_or(value, "header_us", params.header_us);
    params.p_cap = number_or(value, "p_cap", params.p_cap);
    params.p_e_base = number_or(value, "p_e_base", params.p_e_base);
    return params;
}

LinkSpec computed_link_spec(int id, const NodeSpec& a, const NodeSpec& b) {
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

class SeedUnionFind {
public:
    explicit SeedUnionFind(const std::vector<NodeSpec>& nodes) {
        for (const NodeSpec& node : nodes) {
            parent_[node.id] = node.id;
        }
    }

    int find(int id) {
        return parent_[id] == id ? id : parent_[id] = find(parent_[id]);
    }

    bool unite(int a, int b) {
        const int root_a = find(a);
        const int root_b = find(b);
        if (root_a == root_b) {
            return false;
        }
        parent_[root_a] = root_b;
        return true;
    }

private:
    std::map<int, int> parent_;
};

bool can_add_seed_edge(const LinkSpec& link, const GAEnvironment& environment, const std::map<int, int>& degree) {
    const auto node_limit = [&](int id) {
        auto it = std::find_if(environment.nodes.begin(), environment.nodes.end(), [&](const NodeSpec& node) {
            return node.id == id;
        });
        return it == environment.nodes.end() ? 0 : it->max_physical_ports;
    };
    const int a_limit = node_limit(link.node_a);
    const int b_limit = node_limit(link.node_b);
    const int a_degree = degree.count(link.node_a) == 0 ? 0 : degree.at(link.node_a);
    const int b_degree = degree.count(link.node_b) == 0 ? 0 : degree.at(link.node_b);
    return (a_limit <= 0 || a_degree < a_limit) && (b_limit <= 0 || b_degree < b_limit);
}

std::map<std::pair<int, int>, std::size_t> candidate_edge_index(const GAEnvironment& environment) {
    std::map<std::pair<int, int>, std::size_t> index;
    for (std::size_t i = 0; i < environment.candidate_bus_links.size(); ++i) {
        index[environment.candidate_bus_links[i]] = i;
    }
    return index;
}

bool add_seed_pair(
    std::vector<int>& seed,
    std::map<int, int>& degree,
    std::pair<int, int> pair,
    const GAEnvironment& environment,
    const std::map<std::pair<int, int>, std::size_t>& candidate_index) {
    const auto it = candidate_index.find(pair);
    if (it == candidate_index.end() || it->second >= seed.size()) {
        return false;
    }
    const std::size_t candidate_idx = it->second;
    if (seed[candidate_idx] == 1) {
        return true;
    }
    LinkSpec link;
    link.node_a = pair.first;
    link.node_b = pair.second;
    if (!can_add_seed_edge(link, environment, degree)) {
        return false;
    }
    seed[candidate_idx] = 1;
    degree[link.node_a] += 1;
    degree[link.node_b] += 1;
    return true;
}

std::vector<int> build_initial_logic_seed(const GAEnvironment& environment) {
    std::vector<int> seed(environment.candidate_bus_links.size(), 0);
    const auto candidate_index = candidate_edge_index(environment);
    std::map<int, int> degree;
    for (const NodeSpec& node : environment.nodes) {
        degree[node.id] = 0;
    }

    std::vector<int> core_ids;
    std::vector<int> edge_ids;
    for (const NodeSpec& node : environment.nodes) {
        if (node.is_core) {
            core_ids.push_back(node.id);
        } else {
            edge_ids.push_back(node.id);
        }
    }
    if (core_ids.empty() && !environment.nodes.empty()) {
        core_ids.push_back(environment.nodes.front().id);
        for (std::size_t i = 1; i < environment.nodes.size(); ++i) {
            edge_ids.push_back(environment.nodes[i].id);
        }
    }
    std::sort(core_ids.begin(), core_ids.end());
    std::sort(edge_ids.begin(), edge_ids.end());

    const auto find_node = [&](int id) -> const NodeSpec* {
        auto it = std::find_if(environment.nodes.begin(), environment.nodes.end(), [&](const NodeSpec& node) {
            return node.id == id;
        });
        return it == environment.nodes.end() ? nullptr : &*it;
    };
    const auto squared_distance = [&](int lhs, int rhs) {
        const NodeSpec* a = find_node(lhs);
        const NodeSpec* b = find_node(rhs);
        if (a == nullptr || b == nullptr) {
            return std::numeric_limits<double>::infinity();
        }
        const double dx = a->x - b->x;
        const double dy = a->y - b->y;
        return dx * dx + dy * dy;
    };

    if (core_ids.size() == 2) {
        add_seed_pair(seed, degree, ordered_pair(core_ids[0], core_ids[1]), environment, candidate_index);
    } else if (core_ids.size() > 2) {
        for (std::size_t i = 0; i < core_ids.size(); ++i) {
            add_seed_pair(
                seed,
                degree,
                ordered_pair(core_ids[i], core_ids[(i + 1) % core_ids.size()]),
                environment,
                candidate_index);
        }
    }

    for (int edge_id : edge_ids) {
        std::vector<int> ranked_cores = core_ids;
        std::sort(ranked_cores.begin(), ranked_cores.end(), [&](int lhs, int rhs) {
            const int lhs_degree = degree.count(lhs) == 0 ? 0 : degree.at(lhs);
            const int rhs_degree = degree.count(rhs) == 0 ? 0 : degree.at(rhs);
            if (lhs_degree != rhs_degree) {
                return lhs_degree < rhs_degree;
            }
            const double lhs_distance = squared_distance(edge_id, lhs);
            const double rhs_distance = squared_distance(edge_id, rhs);
            if (std::abs(lhs_distance - rhs_distance) > 1e-9) {
                return lhs_distance < rhs_distance;
            }
            return lhs < rhs;
        });
        for (int core_id : ranked_cores) {
            if (add_seed_pair(seed, degree, ordered_pair(edge_id, core_id), environment, candidate_index)) {
                break;
            }
        }
    }

    SeedUnionFind final_uf(environment.nodes);
    for (std::size_t i = 0; i < seed.size() && i < environment.candidate_bus_links.size(); ++i) {
        if (seed[i] == 1) {
            final_uf.unite(environment.candidate_bus_links[i].first, environment.candidate_bus_links[i].second);
        }
    }
    std::set<std::pair<int, int>> physical_pairs;
    for (const LinkSpec& link : environment.physical_links) {
        physical_pairs.insert(ordered_pair(link.node_a, link.node_b));
    }
    std::vector<std::pair<int, int>> fallback_pairs = environment.candidate_bus_links;
    std::sort(fallback_pairs.begin(), fallback_pairs.end(), [&](const auto& lhs, const auto& rhs) {
        const bool lhs_physical = physical_pairs.count(lhs) != 0U;
        const bool rhs_physical = physical_pairs.count(rhs) != 0U;
        if (lhs_physical != rhs_physical) {
            return !lhs_physical;
        }
        const int lhs_degree = (degree.count(lhs.first) == 0 ? 0 : degree.at(lhs.first)) +
                               (degree.count(lhs.second) == 0 ? 0 : degree.at(lhs.second));
        const int rhs_degree = (degree.count(rhs.first) == 0 ? 0 : degree.at(rhs.first)) +
                               (degree.count(rhs.second) == 0 ? 0 : degree.at(rhs.second));
        if (lhs_degree != rhs_degree) {
            return lhs_degree < rhs_degree;
        }
        const double lhs_distance = squared_distance(lhs.first, lhs.second);
        const double rhs_distance = squared_distance(rhs.first, rhs.second);
        if (std::abs(lhs_distance - rhs_distance) > 1e-9) {
            return lhs_distance < rhs_distance;
        }
        return lhs < rhs;
    });
    for (const auto& pair : fallback_pairs) {
        if (final_uf.find(pair.first) == final_uf.find(pair.second)) {
            continue;
        }
        if (add_seed_pair(seed, degree, pair, environment, candidate_index)) {
            final_uf.unite(pair.first, pair.second);
        }
    }

    return seed;
}

std::string json_escape(const std::string& input) {
    std::string out;
    for (char ch : input) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

void write_json(std::ostream& out, const JsonValue& value, int indent) {
    const std::string pad(static_cast<std::size_t>(indent), ' ');
    const std::string child_pad(static_cast<std::size_t>(indent + 2), ' ');
    switch (value.type) {
        case JsonValue::Type::Null:
            out << "null";
            break;
        case JsonValue::Type::Bool:
            out << (value.bool_value ? "true" : "false");
            break;
        case JsonValue::Type::Number:
            out << std::setprecision(15) << value.number_value;
            break;
        case JsonValue::Type::String:
            out << '"' << json_escape(value.string_value) << '"';
            break;
        case JsonValue::Type::Array:
            if (value.array_value.empty()) {
                out << "[]";
            } else {
                out << "[\n";
                for (std::size_t i = 0; i < value.array_value.size(); ++i) {
                    out << child_pad;
                    write_json(out, value.array_value[i], indent + 2);
                    out << (i + 1 == value.array_value.size() ? "\n" : ",\n");
                }
                out << pad << "]";
            }
            break;
        case JsonValue::Type::Object:
            out << "{\n";
            for (auto it = value.object_value.begin(); it != value.object_value.end(); ++it) {
                out << child_pad << '"' << json_escape(it->first) << "\": ";
                write_json(out, it->second, indent + 2);
                out << (std::next(it) == value.object_value.end() ? "\n" : ",\n");
            }
            out << pad << "}";
            break;
    }
}

std::string fixed_string(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string throughput_string(double value_gbps) {
    return fixed_string(value_gbps, 4) + " Gbps";
}

void remove_object_keys(JsonValue& value, const std::vector<std::string>& keys) {
    if (!value.is_object()) {
        return;
    }
    value.object_value.erase(
        std::remove_if(value.object_value.begin(), value.object_value.end(), [&](const auto& item) {
            return std::find(keys.begin(), keys.end(), item.first) != keys.end();
        }),
        value.object_value.end());
}

void ensure_node_radius_fields(JsonValue& root) {
    if (!root.contains("nodes") || !root.at("nodes").is_array()) {
        return;
    }
    for (JsonValue& node : root.at("nodes").array_value) {
        if (node.is_object() && !node.contains("R")) {
            node["R"] = JsonValue::number(0.0);
        }
    }
}

std::vector<int> path_ids(const std::vector<Node*>& path) {
    std::vector<int> ids;
    ids.reserve(path.size());
    for (const Node* node : path) {
        ids.push_back(node->id);
    }
    return ids;
}

JsonValue int_array_json(const std::vector<int>& values) {
    JsonValue array = JsonValue::array();
    for (int value : values) {
        array.array_value.push_back(JsonValue::number(value));
    }
    return array;
}

const Flow* find_flow(const BusTopology& topology, int id) {
    for (const Flow* flow : topology.flow_list()) {
        if (flow->id == id) {
            return flow;
        }
    }
    return nullptr;
}

const NodeSpec* find_node_spec(const GAEnvironment& environment, int id) {
    auto it = std::find_if(environment.nodes.begin(), environment.nodes.end(), [&](const NodeSpec& node) {
        return node.id == id;
    });
    return it == environment.nodes.end() ? nullptr : &*it;
}

LinkSpec logical_link_spec_from_pair(const GAEnvironment& environment, std::size_t candidate_index) {
    const auto pair = environment.candidate_bus_links[candidate_index];
    for (const LinkSpec& physical : environment.physical_links) {
        if (ordered_pair(physical.node_a, physical.node_b) == pair) {
            return physical;
        }
    }

    const NodeSpec* a = find_node_spec(environment, pair.first);
    const NodeSpec* b = find_node_spec(environment, pair.second);
    if (a == nullptr || b == nullptr) {
        LinkSpec fallback;
        fallback.id = static_cast<int>(candidate_index);
        fallback.node_a = pair.first;
        fallback.node_b = pair.second;
        return fallback;
    }
    return computed_link_spec(static_cast<int>(candidate_index), *a, *b);
}

JsonValue link_spec_json(const LinkSpec& spec, const std::map<std::pair<int, int>, std::string>& original_types) {
    JsonValue link = JsonValue::object();
    link["id"] = JsonValue::number(spec.id);
    link["node_a"] = JsonValue::number(spec.node_a);
    link["node_b"] = JsonValue::number(spec.node_b);
    link["bandwidth"] = JsonValue::number(spec.bandwidth);
    link["propagation_delay"] = JsonValue::number(spec.propagation_delay);
    link["reliability"] = JsonValue::number(spec.reliability);
    link["cost"] = JsonValue::number(spec.cost);
    const auto type_it = original_types.find(ordered_pair(spec.node_a, spec.node_b));
    link["type"] = JsonValue::string(type_it == original_types.end() ? "logical" : type_it->second);
    return link;
}

JsonValue logic_links_json(
    const GAEnvironment& environment,
    const std::vector<int>& link_gene,
    const std::map<std::pair<int, int>, std::string>& original_types) {
    JsonValue links = JsonValue::array();
    for (std::size_t i = 0; i < link_gene.size() && i < environment.candidate_bus_links.size(); ++i) {
        if (link_gene[i] != 1) {
            continue;
        }
        links.array_value.push_back(link_spec_json(logical_link_spec_from_pair(environment, i), original_types));
    }
    return links;
}

std::string initial_output_path(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return path.substr(0, dot) + "_initial" + path.substr(dot);
    }
    return path + "_initial";
}

std::string suffixed_output_path(const std::string& path, const std::string& suffix) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return path.substr(0, dot) + "_" + suffix + path.substr(dot);
    }
    return path + "_" + suffix;
}

void apply_result_to_root(
    JsonValue& root,
    const GAEnvironment& environment,
    const std::vector<int>& link_gene,
    const EvaluationResult& result,
    const BusTopology& topology,
    const std::map<std::pair<int, int>, std::string>& original_types) {
    ensure_node_radius_fields(root);
    root["links"] = logic_links_json(environment, link_gene, original_types);

    if (root.contains("flows")) {
        for (JsonValue& flow_json : root.at("flows").array_value) {
            const int flow_id = int_or(flow_json, "id", 0);
            const Flow* flow = find_flow(topology, flow_id);
            if (flow == nullptr) {
                continue;
            }
            flow_json["routing_path"] = int_array_json(path_ids(flow->physical_routing_path));
            remove_object_keys(flow_json, {"comp_lat", "C_conn_norm", "E_throughput", "comp_rel", "cost", "pass"});
            flow_json["pass"] = JsonValue::number(flow->is_schedulable ? 1.0 : 0.0);
        }
    }

    JsonValue assess = root.contains("assess_data") ? root.at("assess_data") : JsonValue::object();
    assess["comp_lat"] = JsonValue::string(fixed_string(result.composite_latency, 2));
    assess["C_conn_norm"] = JsonValue::string(fixed_string(result.connectivity_norm * 100.0, 2) + " %");
    assess["E_throughput"] = JsonValue::string(throughput_string(result.throughput_gbps));
    assess["comp_rel"] = JsonValue::string(fixed_string(result.composite_reliability * 100.0, 2) + " %");
    assess["cost"] = JsonValue::string(fixed_string(result.cost, 0));
    const double latency_strategy_score = std::isfinite(result.composite_latency)
                                              ? std::clamp((1.0 - result.composite_latency) * 100.0, 0.0, 100.0)
                                              : 0.0;
    const double reliability_strategy_score = std::clamp(result.composite_reliability * 100.0, 0.0, 100.0);
    assess["data1"] = JsonValue::string(fixed_string(latency_strategy_score, 2));
    assess["data2"] = JsonValue::string(fixed_string(reliability_strategy_score, 2));
    root["assess_data"] = std::move(assess);
}
} // namespace

JsonValue JsonValue::null() {
    return JsonValue{};
}

JsonValue JsonValue::boolean(bool value) {
    JsonValue out;
    out.type = Type::Bool;
    out.bool_value = value;
    return out;
}

JsonValue JsonValue::number(double value) {
    JsonValue out;
    out.type = Type::Number;
    out.number_value = value;
    return out;
}

JsonValue JsonValue::string(std::string value) {
    JsonValue out;
    out.type = Type::String;
    out.string_value = std::move(value);
    return out;
}

JsonValue JsonValue::array() {
    JsonValue out;
    out.type = Type::Array;
    return out;
}

JsonValue JsonValue::object() {
    JsonValue out;
    out.type = Type::Object;
    return out;
}

bool JsonValue::is_object() const {
    return type == Type::Object;
}

bool JsonValue::is_array() const {
    return type == Type::Array;
}

bool JsonValue::contains(const std::string& key) const {
    return is_object() && std::any_of(object_value.begin(), object_value.end(), [&](const auto& item) {
        return item.first == key;
    });
}

const JsonValue& JsonValue::at(const std::string& key) const {
    auto it = std::find_if(object_value.begin(), object_value.end(), [&](const auto& item) {
        return item.first == key;
    });
    if (it == object_value.end()) {
        throw std::out_of_range("JSON key not found: " + key);
    }
    return it->second;
}

JsonValue& JsonValue::at(const std::string& key) {
    auto it = std::find_if(object_value.begin(), object_value.end(), [&](auto& item) {
        return item.first == key;
    });
    if (it == object_value.end()) {
        throw std::out_of_range("JSON key not found: " + key);
    }
    return it->second;
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    static const JsonValue empty;
    auto it = std::find_if(object_value.begin(), object_value.end(), [&](const auto& item) {
        return item.first == key;
    });
    return it == object_value.end() ? empty : it->second;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!is_object()) {
        *this = JsonValue::object();
    }
    auto it = std::find_if(object_value.begin(), object_value.end(), [&](auto& item) {
        return item.first == key;
    });
    if (it != object_value.end()) {
        return it->second;
    }
    object_value.emplace_back(key, JsonValue::null());
    return object_value.back().second;
}

LoadedConfig load_config_file(const std::string& path) {
    JsonParser parser(read_file(path));
    JsonValue root = parser.parse();
    if (!root.is_object()) {
        throw std::runtime_error("Config root must be a JSON object");
    }
    ensure_node_radius_fields(root);

    GAEnvironment environment;
    for (const JsonValue& node_value : root.at("nodes").array_value) {
        environment.nodes.push_back(node_spec_from_json(node_value));
    }
    for (const NodeSpec& node : environment.nodes) {
        environment.seed_role_gene.push_back(node.is_core ? 1 : 0);
    }

    const auto node_by_id = [&environment](int id) -> const NodeSpec& {
        auto it = std::find_if(environment.nodes.begin(), environment.nodes.end(), [&](const NodeSpec& node) {
            return node.id == id;
        });
        if (it == environment.nodes.end()) {
            throw std::runtime_error("Link references unknown node id");
        }
        return *it;
    };

    if (root.contains("links")) {
        for (const JsonValue& link_value : root.at("links").array_value) {
            LinkSpec link = link_spec_from_json(link_value);
            (void)node_by_id(link.node_a);
            (void)node_by_id(link.node_b);
            environment.physical_links.push_back(std::move(link));
        }
    }
    if (environment.physical_links.empty()) {
        throw std::runtime_error("Config must provide physical links in links[]");
    }

    for (std::size_t i = 0; i < environment.nodes.size(); ++i) {
        for (std::size_t j = i + 1; j < environment.nodes.size(); ++j) {
            environment.candidate_bus_links.push_back(
                ordered_pair(environment.nodes[i].id, environment.nodes[j].id));
        }
    }
    for (const JsonValue& flow_value : root.at("flows").array_value) {
        environment.flows.push_back(flow_spec_from_json(flow_value));
    }
    environment.seed_link_gene = build_initial_logic_seed(environment);
    if (root.contains("mac_params")) {
        environment.mac_params = mac_params_from_json(root.at("mac_params"));
    }

    NSGA2Options options;
    if (root.contains("rng_seed")) {
        options.rng_seed = static_cast<unsigned int>(int_or(root, "rng_seed", 42));
    }
    if (root.contains("ga_params")) {
        const JsonValue& ga = root.at("ga_params");
        options.pop_size = int_or(ga, "pop_size", options.pop_size);
        options.max_gen = int_or(ga, "max_gen", options.max_gen);
        options.mutation_rate = number_or(ga, "mutation_rate", options.mutation_rate);
    }

    Individual baseline;
    baseline.role_gene = environment.seed_role_gene;
    baseline.link_gene = environment.seed_link_gene;
    BusTopology baseline_topology = decode_to_topology(baseline, environment);
    environment.baseline_fiedler = AlgebraicConnectivityEvaluator().evaluate(baseline_topology);
    DeterministicRouter router;
    populate_baseline_paths(environment, router);

    return LoadedConfig{std::move(root), std::move(environment), options};
}

void write_result_config(
    const std::string& path,
    JsonValue root,
    const GAEnvironment& environment,
    const Individual& best,
    const EvaluationResult& result,
    const BusTopology& best_topology) {
    std::map<std::pair<int, int>, std::string> original_types;
    if (root.contains("links")) {
        for (const JsonValue& link : root.at("links").array_value) {
            original_types[ordered_pair(int_or(link, "node_a", 0), int_or(link, "node_b", 0))] =
                string_or(link, "type", "optimized");
        }
    }

    JsonValue optimized_root = root;
    apply_result_to_root(optimized_root, environment, best.link_gene, result, best_topology, original_types);

    Individual seed;
    seed.role_gene = environment.seed_role_gene;
    seed.link_gene = environment.seed_link_gene;
    DeterministicRouter router;
    BusTopology initial_topology = decode_to_topology(seed, environment);
    EvaluationResult initial_result = evaluate_topology(initial_topology, router, environment);
    JsonValue initial_root = std::move(root);
    apply_result_to_root(initial_root, environment, environment.seed_link_gene, initial_result, initial_topology, original_types);

    const std::string initial_path = initial_output_path(path);
    std::ofstream initial_output(initial_path);
    if (!initial_output) {
        throw std::runtime_error("Cannot open initial output config: " + initial_path);
    }
    write_json(initial_output, initial_root, 0);
    initial_output << "\n";

    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Cannot open output config: " + path);
    }
    write_json(output, optimized_root, 0);
    output << "\n";
}

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
    const BusTopology& high_reliability_topology) {
    std::map<std::pair<int, int>, std::string> original_types;
    if (root.contains("links")) {
        for (const JsonValue& link : root.at("links").array_value) {
            original_types[ordered_pair(int_or(link, "node_a", 0), int_or(link, "node_b", 0))] =
                string_or(link, "type", "optimized");
        }
    }

    Individual seed;
    seed.role_gene = environment.seed_role_gene;
    seed.link_gene = environment.seed_link_gene;
    DeterministicRouter router;
    BusTopology initial_topology = decode_to_topology(seed, environment);
    EvaluationResult initial_result = evaluate_topology(initial_topology, router, environment);

    JsonValue initial_root = root;
    apply_result_to_root(initial_root, environment, environment.seed_link_gene, initial_result, initial_topology, original_types);

    JsonValue low_latency_root = root;
    apply_result_to_root(
        low_latency_root,
        environment,
        low_latency.link_gene,
        low_latency_result,
        low_latency_topology,
        original_types);

    JsonValue high_reliability_root = std::move(root);
    apply_result_to_root(
        high_reliability_root,
        environment,
        high_reliability.link_gene,
        high_reliability_result,
        high_reliability_topology,
        original_types);

    std::ofstream initial_output(initial_path);
    if (!initial_output) {
        throw std::runtime_error("Cannot open initial output config: " + initial_path);
    }
    write_json(initial_output, initial_root, 0);
    initial_output << "\n";

    std::ofstream low_latency_output(low_latency_path);
    if (!low_latency_output) {
        throw std::runtime_error("Cannot open low-latency output config: " + low_latency_path);
    }
    write_json(low_latency_output, low_latency_root, 0);
    low_latency_output << "\n";

    std::ofstream high_reliability_output(high_reliability_path);
    if (!high_reliability_output) {
        throw std::runtime_error("Cannot open high-reliability output config: " + high_reliability_path);
    }
    write_json(high_reliability_output, high_reliability_root, 0);
    high_reliability_output << "\n";
}

void write_strategy_result_configs(
    const std::string& path,
    JsonValue root,
    const GAEnvironment& environment,
    const Individual& low_latency,
    const EvaluationResult& low_latency_result,
    const BusTopology& low_latency_topology,
    const Individual& high_reliability,
    const EvaluationResult& high_reliability_result,
    const BusTopology& high_reliability_topology) {
    write_strategy_result_configs_to_paths(
        initial_output_path(path),
        suffixed_output_path(path, "low_latency"),
        suffixed_output_path(path, "high_reliability"),
        std::move(root),
        environment,
        low_latency,
        low_latency_result,
        low_latency_topology,
        high_reliability,
        high_reliability_result,
        high_reliability_topology);
}

} // namespace topoopt
