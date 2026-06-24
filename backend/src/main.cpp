#include "config_io.hpp"
#include "ga.hpp"
#include "initializer.hpp"
#include "routing.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace topoopt;

namespace {
std::string node_path_to_string(const std::vector<Node*>& path) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i != 0) {
            out << " -> ";
        }
        out << path[i]->id;
    }
    out << "]";
    return out.str();
}

std::string initial_result_path(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return path.substr(0, dot) + "_initial" + path.substr(dot);
    }
    return path + "_initial";
}

std::string suffixed_result_path(const std::string& path, const std::string& suffix) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return path.substr(0, dot) + "_" + suffix + path.substr(dot);
    }
    return path + "_" + suffix;
}

void print_strategy_summary(const std::string& label, const EvaluationResult& metrics) {
    std::cout << " - " << label
              << ": comp_lat=" << metrics.composite_latency
              << ", throughput=" << metrics.throughput_gbps << " Gbps"
              << ", fiedler=" << metrics.fiedler
              << ", comp_rel=" << metrics.composite_reliability
              << ", cost=" << metrics.cost
              << ", fully_schedulable=" << (metrics.fully_schedulable ? "true" : "false")
              << "\n";
}

void print_usage(const char* program) {
    std::cout
        << "TopoOptV2 C++17 软总线拓扑优化后端\n\n"
        << "用法:\n"
        << "  " << program << " --config <input.json> --output <result.json> [选项]\n"
        << "  " << program << " --config <wuli.json> --output-dir <data-dir> [选项]\n"
        << "  " << program << " --demo\n\n"
        << "选项:\n"
        << "  --config <path>    物理拓扑输入配置文件\n"
        << "  --output <path>    输出基名；实际生成 *_initial.json、\n"
        << "                     *_low_latency.json、*_high_reliability.json\n"
        << "  --output-dir <dir> 前端固定命名模式；普通场景生成 luoji.json、\n"
        << "                     youhua1.json、youhua2.json；损毁输入自动生成\n"
        << "                     对应的 *_sunhui.json 文件\n"
        << "  --pop <n>          NSGA-II 种群规模，覆盖配置文件 ga_params.pop_size\n"
        << "  --gen <n>          NSGA-II 迭代代数，覆盖配置文件 ga_params.max_gen\n"
        << "  --mutation <rate>  基础变异率，覆盖配置文件 ga_params.mutation_rate\n"
        << "  --seed <n>         随机种子，覆盖配置文件 rng_seed\n"
        << "  --demo             运行内置快速演示，不读取配置文件\n"
        << "  -h, --help         显示本帮助\n";
}
} // namespace

int run_cli(int argc, char** argv) {
    bool demo_mode = false;
    std::optional<std::string> config_path;
    std::optional<std::string> output_path;
    std::optional<std::string> output_dir;
    NSGA2Options options;
    bool pop_override = false;
    bool gen_override = false;
    bool mutation_override = false;
    bool seed_override = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--demo") {
            demo_mode = true;
        } else if (arg == "--config") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--config 缺少文件路径");
            }
            config_path = argv[++i];
        } else if (arg == "--output") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--output 缺少文件路径");
            }
            output_path = argv[++i];
        } else if (arg == "--output-dir") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--output-dir 缺少目录路径");
            }
            output_dir = argv[++i];
        } else if (arg == "--pop") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--pop 缺少整数值");
            }
            options.pop_size = std::stoi(argv[++i]);
            pop_override = true;
        } else if (arg == "--gen") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--gen 缺少整数值");
            }
            options.max_gen = std::stoi(argv[++i]);
            gen_override = true;
        } else if (arg == "--mutation") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--mutation 缺少数值");
            }
            options.mutation_rate = std::stod(argv[++i]);
            mutation_override = true;
        } else if (arg == "--seed") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--seed 缺少整数值");
            }
            options.rng_seed = static_cast<unsigned int>(std::stoul(argv[++i]));
            seed_override = true;
        } else {
            throw std::invalid_argument("未知参数: " + arg);
        }
    }

    if (output_path.has_value() && output_dir.has_value()) {
        throw std::invalid_argument("--output 与 --output-dir 不能同时使用");
    }

    if (demo_mode) {
        BusTopology topology = build_sandbox_topology();
        DeterministicRouter router(1.0);
        EvaluationResult metrics = evaluate_topology(topology, router);

        std::cout << "=== TopoOptV2 C++17 Demo ===\n";
        std::cout << "Nodes: " << topology.node_list().size()
                  << ", BusLinks: " << topology.bus_link_list().size()
                  << ", PhysicalLinks: " << topology.physical_link_list().size()
                  << ", Flows: " << topology.flow_list().size() << "\n\n";

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Composite latency ratio: " << metrics.composite_latency << "\n";
        std::cout << "Max latency: " << metrics.max_latency << " ms\n";
        std::cout << "Fiedler connectivity: " << metrics.fiedler << "\n";
        std::cout << "Composite reliability: " << metrics.composite_reliability << "\n";
        std::cout << "Network cost: " << metrics.cost << "\n";
        std::cout << "Fully schedulable: " << (metrics.fully_schedulable ? "true" : "false") << "\n\n";

        for (const Flow* flow : topology.flow_list()) {
            std::cout << "[" << flow->id << "] " << flow->flow_name << "\n";
            std::cout << "  logical : " << node_path_to_string(flow->logical_routing_path) << "\n";
            std::cout << "  physical: " << node_path_to_string(flow->physical_routing_path) << "\n";
            std::cout << "  latency : " << flow->worst_case_delay.value_or(-1.0)
                      << " / deadline " << flow->deadline << " ms\n";
            std::cout << "  reliability: " << flow->actual_reliability.value_or(0.0)
                      << " / required " << flow->reliability_requirement << "\n\n";
        }
        return 0;
    }

    std::cout << std::fixed << std::setprecision(4);
    JsonValue input_root = JsonValue::object();
    GAEnvironment environment;
    if (config_path.has_value()) {
        LoadedConfig loaded = load_config_file(*config_path);
        input_root = std::move(loaded.root);
        environment = std::move(loaded.environment);
        if (!pop_override) {
            options.pop_size = loaded.options.pop_size;
        }
        if (!gen_override) {
            options.max_gen = loaded.options.max_gen;
        }
        if (!mutation_override) {
            options.mutation_rate = loaded.options.mutation_rate;
        }
        if (!seed_override) {
            options.rng_seed = loaded.options.rng_seed;
        }
    } else {
        environment = build_default_ga_environment();
    }

    if (options.pop_size < 2) {
        throw std::invalid_argument("种群规模 --pop 必须大于等于 2");
    }
    if (options.max_gen < 1) {
        throw std::invalid_argument("迭代代数 --gen 必须大于等于 1");
    }
    if (options.mutation_rate < 0.0 || options.mutation_rate > 1.0) {
        throw std::invalid_argument("基础变异率 --mutation 必须位于 [0, 1]");
    }

    NSGA2Result result = run_nsga2(environment, options);

    Individual low_latency = select_strategy_solution(result.population, StrategyKind::LowLatency);
    Individual high_reliability = select_strategy_solution(result.population, StrategyKind::HighReliability);

    DeterministicRouter low_latency_router;
    BusTopology low_latency_topology = decode_to_topology(low_latency, environment);
    EvaluationResult low_latency_metrics = evaluate_topology(low_latency_topology, low_latency_router, environment);

    DeterministicRouter high_reliability_router;
    BusTopology high_reliability_topology = decode_to_topology(high_reliability, environment);
    EvaluationResult high_reliability_metrics = evaluate_topology(high_reliability_topology, high_reliability_router, environment);

    if (output_path.has_value() || output_dir.has_value()) {
        if (!config_path.has_value()) {
            input_root = JsonValue::object();
        }
        if (output_dir.has_value()) {
            const std::filesystem::path directory(*output_dir);
            std::filesystem::create_directories(directory);
            const std::string input_name = config_path.has_value()
                                               ? std::filesystem::path(*config_path).filename().string()
                                               : std::string();
            const bool damaged = input_name.find("_sunhui") != std::string::npos;
            const std::filesystem::path initial_path = directory / (damaged ? "luoji_sunhui.json" : "luoji.json");
            const std::filesystem::path low_latency_path = directory / (damaged ? "youhua1_sunhui.json" : "youhua1.json");
            const std::filesystem::path high_reliability_path = directory / (damaged ? "youhua2_sunhui.json" : "youhua2.json");

            write_strategy_result_configs_to_paths(
                initial_path.string(),
                low_latency_path.string(),
                high_reliability_path.string(),
                std::move(input_root),
                environment,
                low_latency,
                low_latency_metrics,
                low_latency_topology,
                high_reliability,
                high_reliability_metrics,
                high_reliability_topology);
            std::cout << "\n已写出初始逻辑拓扑配置文件: " << initial_path.string() << "\n";
            std::cout << "已写出低时延策略优化配置文件: " << low_latency_path.string() << "\n";
            std::cout << "已写出高可靠策略优化配置文件: " << high_reliability_path.string() << "\n";
        } else {
            write_strategy_result_configs(
                *output_path,
                std::move(input_root),
                environment,
                low_latency,
                low_latency_metrics,
                low_latency_topology,
                high_reliability,
                high_reliability_metrics,
                high_reliability_topology);
            std::cout << "\n已写出初始逻辑拓扑配置文件: " << initial_result_path(*output_path) << "\n";
            std::cout << "已写出低时延策略优化配置文件: " << suffixed_result_path(*output_path, "low_latency") << "\n";
            std::cout << "已写出高可靠策略优化配置文件: " << suffixed_result_path(*output_path, "high_reliability") << "\n";
        }
    }

    std::cout << "\n=== 策略化 Pareto 解选择结果 ===\n";
    print_strategy_summary("低时延策略", low_latency_metrics);
    print_strategy_summary("高可靠策略", high_reliability_metrics);

    std::cout << "\n=== 低时延策略业务流详情 ===\n";
    for (const Flow* flow : low_latency_topology.flow_list()) {
        const bool pass = flow->is_schedulable;
        std::cout << "[" << (pass ? "PASS" : "FAIL") << "] Flow " << flow->id << " " << flow->flow_name << "\n";
        std::cout << "  logical : " << node_path_to_string(flow->logical_routing_path) << "\n";
        std::cout << "  physical: " << node_path_to_string(flow->physical_routing_path) << "\n";
        std::cout << "  latency : " << flow->worst_case_delay.value_or(-1.0)
                  << " / " << flow->deadline << " ms\n";
        std::cout << "  reliability: " << flow->actual_reliability.value_or(0.0)
                  << " / " << flow->reliability_requirement << "\n";
    }

    return 0;
}

int main(int argc, char** argv) {
    try {
        return run_cli(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "TopoOptV2 执行失败: " << error.what() << "\n";
        std::cerr << "使用 --help 查看调用方式。\n";
        return 1;
    }
}
