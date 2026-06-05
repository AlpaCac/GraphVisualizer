#ifndef SB_INITIALIZER_H
#define SB_INITIALIZER_H

#include "sb_models.h"

/* ============================================================
 * 沙箱配置（与 Python initializer.py 完全对齐）
 *   - 20 个物理节点，固定坐标
 *   - 4 个高配中心节点 ID = {4, 7, 14, 15}
 *   - 14 条业务流（3 video + 4 C2 + 3 sync + 4 telemetry）
 * ============================================================ */
#define SB_INIT_NUM_NODES   20
#define SB_INIT_NUM_FLOWS   14
#define SB_LINK_DIST_LIMIT  400.0   /* 候选链路距离上限（米） */

/* 沙箱整体环境，对应 Python build_sandbox 的返回 tuple */
typedef struct {
    PhysicalNode  p_nodes[SB_MAX_NODES];
    int           p_node_count;

    PhysicalLink  c_links[SB_MAX_LINKS];
    int           c_link_count;

    /* BusNode 与 PhysicalNode 一对一映射，作为 Flow 端点的初始引用 */
    BusNode       b_nodes[SB_MAX_NODES];
    int           b_node_count;

    FlowGraph     flow_graph;   /* 14 条全局业务流，flow.id 等于 0..13 */
} Sandbox;

/* ---------- 子步骤（与 Python 同名函数一一对应）---------- */

/* 1) 初始化 20 个物理节点（坐标、CPU、可靠性、端口、networkID/deviceName 元数据） */
void sb_init_physical_nodes(PhysicalNode *out, int *out_count);

/* 2) 基于物理节点生成候选物理链路池（距离 ≤ 400 米） */
void sb_init_candidate_links(const PhysicalNode *nodes, int node_count,
                             PhysicalLink *out, int *out_count);

/* 3) 与物理节点一一映射创建 BusNode；不设 is_core，由后续 GA 解码决定 */
void sb_init_bus_nodes(const PhysicalNode *p_nodes, int p_node_count,
                       BusNode *out, int *out_count);

/* 4) 初始化 14 条业务流；同时把流挂到对应 BusNode 的 source/sink */
void sb_init_flows(BusNode *b_nodes, int b_node_count, FlowGraph *out);

/* ---------- 一键入口 ---------- */
void sb_build_sandbox(Sandbox *sb);

/* ---------- 节点损毁重建 ----------
 *   把 failed_node_id 从 p_nodes 中剔除，并重建候选链路池。
 *   注意：调用方在调用本函数前，应自行处理依赖该节点的 flow / b_node 引用。
 */
void sb_rebuild_with_damage(Sandbox *sb, int failed_node_id);

/* ---------- 工具 ---------- */
double sb_node_distance(const PhysicalNode *a, const PhysicalNode *b);

#endif
