/* ============================================================
 * initializer.c —— 与 Python simulation/initializer.py 对齐
 * ============================================================ */
#include "initializer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------- 与 Python 完全一致的硬编码沙箱数据 ---------- */

/* 20 个物理节点的固定坐标 */
static const double kCoords[SB_INIT_NUM_NODES][2] = {
    {120, 150}, {200, 350}, {180, 500}, {300, 200}, {450, 400},
    {600, 300}, {800, 250}, {750, 450}, {900, 600}, {850, 800},
    {650, 750}, {500, 850}, {350, 700}, {150, 800}, {400, 550},
    {550, 500}, {700, 600},  {50, 400}, {950, 300}, {500, 100}
};

/* 高配节点 ID 集合（与 Python high_end_ids = {4, 7, 14, 15} 一致） */
static int is_high_end(int id) {
    return id == 4 || id == 7 || id == 14 || id == 15;
}

/* 14 条业务流配置：(类型, 源, 目的) 与 Python 完全对齐 */
typedef enum { FLOW_VIDEO, FLOW_C2, FLOW_SYNC, FLOW_TELEMETRY } FlowKind;

typedef struct {
    FlowKind kind;
    int      src;
    int      dst;
} FlowSpec;

static const FlowSpec kFlowSpecs[SB_INIT_NUM_FLOWS] = {
    /* Video Backhaul：(0,10) (5,11) (13,12) */
    { FLOW_VIDEO,      0, 10 },
    { FLOW_VIDEO,      5, 11 },
    { FLOW_VIDEO,     13, 12 },
    /* C2 Commands：(10,1) (11,6) (12,14) (10,19) */
    { FLOW_C2,        10,  1 },
    { FLOW_C2,        11,  6 },
    { FLOW_C2,        12, 14 },
    { FLOW_C2,        10, 19 },
    /* Local Swarm Sync：(2,3) (7,8) (15,16) */
    { FLOW_SYNC,       2,  3 },
    { FLOW_SYNC,       7,  8 },
    { FLOW_SYNC,      15, 16 },
    /* Background Telemetry：(4,10) (9,11) (17,12) (18,10) */
    { FLOW_TELEMETRY,  4, 10 },
    { FLOW_TELEMETRY,  9, 11 },
    { FLOW_TELEMETRY, 17, 12 },
    { FLOW_TELEMETRY, 18, 10 },
};

/* ---------- 工具 ---------- */
double sb_node_distance(const PhysicalNode *a, const PhysicalNode *b) {
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return sqrt(dx*dx + dy*dy);
}

/* ============================================================
 * 1) 物理节点
 * ============================================================ */
void sb_init_physical_nodes(PhysicalNode *out, int *out_count) {
    for (int i = 0; i < SB_INIT_NUM_NODES; i++) {
        PhysicalNode *n = &out[i];
        sb_phys_node_init(n);

        n->id = i;
        n->x  = kCoords[i][0];
        n->y  = kCoords[i][1];
        n->memory_capacity = 16384.0;       /* 16 GB，非强约束 */

        if (is_high_end(i)) {
            n->cpu_capacity       = 9000.0;
            n->max_physical_ports = 6;
            n->reliability        = 0.999;
            n->meta.typeInfo.staticType = NODE_TYPE_MASTER;
        } else {
            n->cpu_capacity       = 6000.0;
            n->max_physical_ports = 4;
            n->reliability        = 0.99;
            n->meta.typeInfo.staticType = NODE_TYPE_COMPUTE;
        }

        /* 一并填充表1的轻量元数据，便于序列化 */
        snprintf(n->meta.identity.networkID,  SB_STR_LEN, "NID-%03d", i);
        snprintf(n->meta.identity.deviceName, SB_STR_LEN, "DEV-%02d", i);
        n->meta.compute.computeWeight = n->cpu_capacity;
        n->meta.compute.memTotalMB    = n->memory_capacity;
    }
    *out_count = SB_INIT_NUM_NODES;
}

/* ============================================================
 * 2) 候选物理链路
 *   规则与公式与 Python init_candidate_links 完全一致：
 *     BW   = 100000 / (1 + (d/100)^2)
 *     prop = 0.01 + d * 0.005
 *     cost = 10.0 + 0.1 * d
 *     rel  = 0.999 - 0.01 * (d/400)^2
 * ============================================================ */
void sb_init_candidate_links(const PhysicalNode *nodes, int node_count,
                             PhysicalLink *out, int *out_count) {
    int link_id = 0;
    for (int i = 0; i < node_count; i++) {
        for (int j = i + 1; j < node_count; j++) {
            double d = sb_node_distance(&nodes[i], &nodes[j]);
            if (d > SB_LINK_DIST_LIMIT) continue;

            PhysicalLink *l = &out[link_id];
            sb_phys_link_init(l);

            l->id        = link_id;
            l->node_a_id = nodes[i].id;
            l->node_b_id = nodes[j].id;
            l->bandwidth         = 100000.0 / (1.0 + (d/100.0)*(d/100.0));
            l->propagation_delay = 0.01 + d * 0.005;
            l->cost              = 10.0 + 0.1 * d;
            l->reliability       = 0.999 - 0.01 * (d/400.0)*(d/400.0);
            l->medium            = IF_ETHERNET;

            link_id++;
        }
    }
    *out_count = link_id;
}

/* ============================================================
 * 3) BusNode 一对一映射
 * ============================================================ */
void sb_init_bus_nodes(const PhysicalNode *p_nodes, int p_node_count,
                       BusNode *out, int *out_count) {
    for (int i = 0; i < p_node_count; i++) {
        BusNode *n = &out[i];
        sb_bus_node_init(n);
        n->id                = p_nodes[i].id;
        n->physical_node_idx = i;
        n->is_core           = 0;
    }
    *out_count = p_node_count;
}

/* ============================================================
 * 4) 业务流
 *   参数与 Python init_flows 完全一致
 * ============================================================ */
static void fill_flow(Flow *f, int id, FlowKind kind,
                      BusNode *src, BusNode *dst,
                      int src_idx_in_array, int dst_idx_in_array,
                      int self_idx_in_fg) {
    sb_flow_init(f);
    f->id = id;
    f->source_node_id = src->id;
    f->target_node_id = dst->id;

    switch (kind) {
        case FLOW_VIDEO:
            snprintf(f->flow_name, SB_STR_LEN, "Video_%d_to_%d",    src->id, dst->id);
            snprintf(f->topic,     SB_STR_LEN, "T_Video_%d", id);
            f->pre_processing_time = 2.0; f->post_processing_time = 2.0;
            f->priority = 1;   f->period = 33.0;  f->deadline = 300.0;
            f->reliability_requirement = 0.8;
            f->message_size = 64000.0;
            break;
        case FLOW_C2:
            snprintf(f->flow_name, SB_STR_LEN, "C2_%d_to_%d",       src->id, dst->id);
            snprintf(f->topic,     SB_STR_LEN, "T_C2_%d", id);
            f->pre_processing_time = 0.5; f->post_processing_time = 0.5;
            f->priority = 10;  f->period = 20.0;  f->deadline = 50.0;
            f->reliability_requirement = 0.9;
            f->message_size = 128.0;
            break;
        case FLOW_SYNC:
            snprintf(f->flow_name, SB_STR_LEN, "Sync_%d_to_%d",     src->id, dst->id);
            snprintf(f->topic,     SB_STR_LEN, "T_Sync_%d", id);
            f->pre_processing_time = 0.5; f->post_processing_time = 0.5;
            f->priority = 7;   f->period = 10.0;  f->deadline = 80.0;
            f->reliability_requirement = 0.85;
            f->message_size = 4096.0;
            break;
        case FLOW_TELEMETRY:
            snprintf(f->flow_name, SB_STR_LEN, "Telemetry_%d_to_%d", src->id, dst->id);
            snprintf(f->topic,     SB_STR_LEN, "T_Telemetry_%d", id);
            f->pre_processing_time = 1.0; f->post_processing_time = 1.0;
            f->priority = 5;   f->period = 100.0; f->deadline = 500.0;
            f->reliability_requirement = 0.85;
            f->message_size = 1024.0;
            break;
    }

    /* 挂到源/目的节点（这两个集合在 Python 中是"全程不变"的） */
    if (src->source_count < SB_MAX_FLOWS_PER_NODE)
        src->source_flows[src->source_count++] = self_idx_in_fg;
    if (dst->sink_count < SB_MAX_FLOWS_PER_NODE)
        dst->sink_flows[dst->sink_count++] = self_idx_in_fg;

    (void)src_idx_in_array; (void)dst_idx_in_array;
}

void sb_init_flows(BusNode *b_nodes, int b_node_count, FlowGraph *fg) {
    memset(fg, 0, sizeof(*fg));
    fg->count = 0;

    /* 清空 BusNode 的 source/sink，避免重复初始化 */
    for (int i = 0; i < b_node_count; i++) {
        b_nodes[i].source_count = 0;
        b_nodes[i].sink_count   = 0;
        b_nodes[i].relay_count  = 0;
    }

    for (int i = 0; i < SB_INIT_NUM_FLOWS; i++) {
        const FlowSpec *spec = &kFlowSpecs[i];
        BusNode *src = &b_nodes[spec->src];   /* BusNode 按 id 顺序排列 */
        BusNode *dst = &b_nodes[spec->dst];
        Flow *f = &fg->flows[fg->count];

        fill_flow(f, i, spec->kind, src, dst, spec->src, spec->dst, fg->count);
        fg->count++;
    }
}

/* ============================================================
 * 5) 一键入口
 * ============================================================ */
void sb_build_sandbox(Sandbox *sb) {
    memset(sb, 0, sizeof(*sb));
    sb_init_physical_nodes (sb->p_nodes, &sb->p_node_count);
    sb_init_candidate_links(sb->p_nodes, sb->p_node_count,
                            sb->c_links, &sb->c_link_count);
    sb_init_bus_nodes      (sb->p_nodes, sb->p_node_count,
                            sb->b_nodes, &sb->b_node_count);
    sb_init_flows          (sb->b_nodes, sb->b_node_count, &sb->flow_graph);
}

/* ============================================================
 * 6) 节点损毁重建
 *    与 Python rebuild_physical_world_with_damage 等价：
 *      - 物理节点剔除被损毁节点
 *      - 候选链路池基于剩余节点重新生成
 * ============================================================ */
void sb_rebuild_with_damage(Sandbox *sb, int failed_node_id) {
    PhysicalNode survivors[SB_MAX_NODES];
    int surv_count = 0;

    for (int i = 0; i < sb->p_node_count; i++) {
        if (sb->p_nodes[i].id == failed_node_id) continue;
        survivors[surv_count++] = sb->p_nodes[i];
    }

    /* 回写物理节点 */
    sb->p_node_count = surv_count;
    for (int i = 0; i < surv_count; i++) sb->p_nodes[i] = survivors[i];

    /* 重建候选链路池（链路 id 从 0 重新编号，与 Python 行为一致） */
    sb->c_link_count = 0;
    sb_init_candidate_links(sb->p_nodes, sb->p_node_count,
                            sb->c_links, &sb->c_link_count);
}
