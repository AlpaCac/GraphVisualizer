/* ============================================================
 *  config.c —— JSON 配置文件加载与应用到 Sandbox
 *
 *  设计要点：
 *    - 复用 task_data.c 的极简 JSON 解析器（已被工程内置）
 *    - 加载流程：parse JSON -> 提取顶层 keys -> 应用到 Sandbox
 *    - 任一 section 缺失，自动落回 initializer.c 的默认值
 * ============================================================ */
#include "config.h"
#include "sb_phys_bind.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ============================================================
 *  缺省值（与原 initializer.c 一致）
 * ============================================================ */
void sb_link_gen_default(LinkGenParams *p) {
    p->distance_limit_m  = 400.0;
    p->bandwidth_a       = 100000.0;
    p->bandwidth_b       = 100.0;
    p->delay_const       = 0.01;
    p->delay_slope       = 0.005;
    p->cost_const        = 10.0;
    p->cost_slope        = 0.1;
    p->reliability_base  = 0.999;
    p->reliability_decay = 0.01;
}

void sb_ga_params_default(GaParams *p) {
    p->pop_size      = 100;
    p->max_gen       = 50;
    p->mutation_rate = 0.01;
}

/* ============================================================
 *  内置极简 JSON 解析器（与 task_data.c 同结构）
 * ============================================================ */
typedef enum { JV_NULL, JV_BOOL, JV_NUM, JV_STR, JV_ARR, JV_OBJ } JType;

typedef struct JNode {
    JType type;
    double num;
    int    boolean;
    char  *str;
    struct JNode **items;
    char        **keys;
    int           count;
    int           cap;
} JNode;

typedef struct { const char *p; int err; } JParser;

static JNode *jnode_new(JType t) {
    JNode *n = (JNode*)calloc(1, sizeof(JNode));
    n->type = t;
    return n;
}
static void jnode_free(JNode *n) {
    if (!n) return;
    if (n->str) free(n->str);
    for (int i = 0; i < n->count; i++) {
        if (n->keys && n->keys[i]) free(n->keys[i]);
        jnode_free(n->items[i]);
    }
    free(n->items);
    free(n->keys);
    free(n);
}
static void jnode_push(JNode *parent, char *key, JNode *child) {
    if (parent->count >= parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->items = (JNode**)realloc(parent->items, parent->cap * sizeof(JNode*));
        parent->keys  = (char**) realloc(parent->keys,  parent->cap * sizeof(char*));
    }
    parent->keys[parent->count]  = key;
    parent->items[parent->count] = child;
    parent->count++;
}
static void j_skip_ws(JParser *ps) {
    while (*ps->p && isspace((unsigned char)*ps->p)) ps->p++;
    /* 也跳过 // 单行注释，便于配置文件人工编辑 */
    if (ps->p[0] == '/' && ps->p[1] == '/') {
        while (*ps->p && *ps->p != '\n') ps->p++;
        j_skip_ws(ps);
    }
}
static JNode *j_parse_value(JParser *ps);

static char *j_parse_string_raw(JParser *ps) {
    if (*ps->p != '"') { ps->err = 1; return NULL; }
    ps->p++;
    size_t cap = 16, len = 0;
    char *buf = (char*)malloc(cap);
    while (*ps->p && *ps->p != '"') {
        char c = *ps->p++;
        if (c == '\\' && *ps->p) {
            char e = *ps->p++;
            switch (e) {
                case 'n': c='\n'; break; case 't': c='\t'; break;
                case 'r': c='\r'; break; case '"': c='"'; break;
                case '\\':c='\\'; break; case '/': c='/'; break;
                default:  c=e;    break;
            }
        }
        if (len + 1 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
        buf[len++] = c;
    }
    if (*ps->p != '"') { ps->err = 1; free(buf); return NULL; }
    ps->p++;
    buf[len] = '\0';
    return buf;
}

static JNode *j_parse_object(JParser *ps) {
    JNode *o = jnode_new(JV_OBJ);
    ps->p++;
    j_skip_ws(ps);
    if (*ps->p == '}') { ps->p++; return o; }
    while (1) {
        j_skip_ws(ps);
        char *key = j_parse_string_raw(ps);
        if (ps->err) { jnode_free(o); return NULL; }
        j_skip_ws(ps);
        if (*ps->p != ':') { ps->err = 1; free(key); jnode_free(o); return NULL; }
        ps->p++;
        JNode *val = j_parse_value(ps);
        if (ps->err) { free(key); jnode_free(o); return NULL; }
        jnode_push(o, key, val);
        j_skip_ws(ps);
        if (*ps->p == ',') { ps->p++; continue; }
        if (*ps->p == '}') { ps->p++; break; }
        ps->err = 1; jnode_free(o); return NULL;
    }
    return o;
}
static JNode *j_parse_array(JParser *ps) {
    JNode *a = jnode_new(JV_ARR);
    ps->p++;
    j_skip_ws(ps);
    if (*ps->p == ']') { ps->p++; return a; }
    while (1) {
        JNode *val = j_parse_value(ps);
        if (ps->err) { jnode_free(a); return NULL; }
        jnode_push(a, NULL, val);
        j_skip_ws(ps);
        if (*ps->p == ',') { ps->p++; j_skip_ws(ps); continue; }
        if (*ps->p == ']') { ps->p++; break; }
        ps->err = 1; jnode_free(a); return NULL;
    }
    return a;
}
static JNode *j_parse_value(JParser *ps) {
    j_skip_ws(ps);
    char c = *ps->p;
    if (c == '{') return j_parse_object(ps);
    if (c == '[') return j_parse_array(ps);
    if (c == '"') {
        JNode *n = jnode_new(JV_STR);
        n->str = j_parse_string_raw(ps);
        if (ps->err) { jnode_free(n); return NULL; }
        return n;
    }
    if (!strncmp(ps->p, "true", 4))  { ps->p += 4; JNode*n=jnode_new(JV_BOOL); n->boolean=1; return n; }
    if (!strncmp(ps->p, "false", 5)) { ps->p += 5; JNode*n=jnode_new(JV_BOOL); n->boolean=0; return n; }
    if (!strncmp(ps->p, "null", 4))  { ps->p += 4; return jnode_new(JV_NULL); }
    char *end = NULL;
    double v = strtod(ps->p, &end);
    if (end == ps->p) { ps->err = 1; return NULL; }
    ps->p = end;
    JNode *n = jnode_new(JV_NUM);
    n->num = v;
    return n;
}

static JNode *j_get(const JNode *o, const char *key) {
    if (!o || o->type != JV_OBJ) return NULL;
    for (int i = 0; i < o->count; i++)
        if (o->keys[i] && !strcmp(o->keys[i], key)) return o->items[i];
    return NULL;
}
static double j_num(const JNode *o, const char *key, double def) {
    JNode *v = j_get(o, key);
    return (v && v->type == JV_NUM) ? v->num : def;
}
static int j_int(const JNode *o, const char *key, int def) {
    JNode *v = j_get(o, key);
    return (v && v->type == JV_NUM) ? (int)v->num : def;
}
static const char *j_str(const JNode *o, const char *key, const char *def) {
    JNode *v = j_get(o, key);
    return (v && v->type == JV_STR) ? v->str : def;
}

/* ============================================================
 *  顶层加载流程
 * ============================================================ */
static char *read_file_all(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char*)malloc(n + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t got = fread(buf, 1, n, fp);
    (void)got;
    buf[n] = '\0';
    fclose(fp);
    return buf;
}

static void apply_nodes(const JNode *arr, Sandbox *sb) {
    int n = arr->count;
    if (n > SB_MAX_NODES) n = SB_MAX_NODES;
    sb->p_node_count = n;
    for (int i = 0; i < n; i++) {
        const JNode *o = arr->items[i];
        PhysicalNode *p = &sb->p_nodes[i];
        memset(p, 0, sizeof(*p));
        p->id                 = j_int(o, "id", i);
        p->x                  = j_num(o, "x", 0);
        p->y                  = j_num(o, "y", 0);
        p->cpu_capacity       = j_num(o, "cpu_capacity", 6000);
        p->memory_capacity    = j_num(o, "memory_mb", 16384);
        p->max_physical_ports = j_int(o, "max_ports", 4);
        p->reliability        = j_num(o, "reliability", 0.99);
        p->destroyed          = 0;
        p->linkCount          = 0;

        /* meta 字段（可选）*/
        const char *dn = j_str(o, "device_name", NULL);
        if (dn) {
            strncpy(p->meta.identity.deviceName, dn, SB_STR_LEN - 1);
            strncpy(p->meta.identity.networkID, dn, SB_STR_LEN - 1);
        }
        const char *st = j_str(o, "static_type", NULL);
        if (st) {
            if      (!strcmp(st, "Master"))   p->meta.typeInfo.staticType = NODE_TYPE_MASTER;
            else if (!strcmp(st, "Compute"))  p->meta.typeInfo.staticType = NODE_TYPE_COMPUTE;
            else if (!strcmp(st, "Sensor"))   p->meta.typeInfo.staticType = NODE_TYPE_SENSOR;
            else if (!strcmp(st, "Actuator")) p->meta.typeInfo.staticType = NODE_TYPE_ACTUATOR;
            else if (!strcmp(st, "Relay"))    p->meta.typeInfo.staticType = NODE_TYPE_RELAY;
            else if (!strcmp(st, "Gateway"))  p->meta.typeInfo.staticType = NODE_TYPE_GATEWAY;
            else if (!strcmp(st, "Backup"))   p->meta.typeInfo.staticType = NODE_TYPE_BACKUP;
            else                              p->meta.typeInfo.staticType = NODE_TYPE_UNKNOWN;
        }
    }
}

static void apply_link_gen(const JNode *o, LinkGenParams *p) {
    p->distance_limit_m  = j_num(o, "distance_limit_m",  p->distance_limit_m);
    p->bandwidth_a       = j_num(o, "bandwidth_a",       p->bandwidth_a);
    p->bandwidth_b       = j_num(o, "bandwidth_b",       p->bandwidth_b);
    p->delay_const       = j_num(o, "delay_const",       p->delay_const);
    p->delay_slope       = j_num(o, "delay_slope",       p->delay_slope);
    p->cost_const        = j_num(o, "cost_const",        p->cost_const);
    p->cost_slope        = j_num(o, "cost_slope",        p->cost_slope);
    p->reliability_base  = j_num(o, "reliability_base",  p->reliability_base);
    p->reliability_decay = j_num(o, "reliability_decay", p->reliability_decay);
}

/* 按 LinkGenParams 生成候选链路 */
static void gen_candidate_links(Sandbox *sb, const LinkGenParams *L) {
    int lid = 0;
    sb->c_link_count = 0;
    for (int i = 0; i < sb->p_node_count; i++) {
        for (int j = i + 1; j < sb->p_node_count; j++) {
            PhysicalNode *a = &sb->p_nodes[i];
            PhysicalNode *b = &sb->p_nodes[j];
            double dx = a->x - b->x, dy = a->y - b->y;
            double d = sqrt(dx*dx + dy*dy);
            if (d > L->distance_limit_m) continue;
            if (lid >= SB_MAX_LINKS) break;
            PhysicalLink *pl = &sb->c_links[lid];
            memset(pl, 0, sizeof(*pl));
            pl->id        = lid;
            pl->node_a_id = a->id;
            pl->node_b_id = b->id;
            pl->bandwidth = L->bandwidth_a / (1.0 + pow(d / L->bandwidth_b, 2));
            pl->propagation_delay = L->delay_const + d * L->delay_slope;
            pl->cost      = L->cost_const + d * L->cost_slope;
            double rr     = d / L->distance_limit_m;
            pl->reliability = L->reliability_base - L->reliability_decay * rr * rr;
            pl->destroyed = 0;
            pl->medium    = IF_WIFI;
            lid++;
        }
    }
    sb->c_link_count = lid;
}

static void apply_flows(const JNode *arr, Sandbox *sb) {
    int n = arr->count;
    if (n > SB_MAX_FLOWS) n = SB_MAX_FLOWS;
    sb->flow_graph.count = n;
    for (int i = 0; i < n; i++) {
        const JNode *o = arr->items[i];
        Flow *f = &sb->flow_graph.flows[i];
        memset(f, 0, sizeof(*f));
        f->id                      = j_int(o, "id", i);
        const char *nm = j_str(o, "name", NULL);
        if (nm) strncpy(f->flow_name, nm, SB_STR_LEN - 1);
        else    snprintf(f->flow_name, SB_STR_LEN, "flow_%d", i);
        const char *tp = j_str(o, "topic", NULL);
        if (tp) strncpy(f->topic, tp, SB_STR_LEN - 1);
        f->source_node_id          = j_int(o, "src", 0);
        f->target_node_id          = j_int(o, "dst", 1);
        f->priority                = j_int(o, "priority", 1);
        f->period                  = j_num(o, "period", 100);
        f->deadline                = j_num(o, "deadline", 200);
        f->reliability_requirement = j_num(o, "reliability_req", 0.85);
        f->message_size            = j_num(o, "message_size", 1024);
        f->pre_processing_time     = j_num(o, "pre_proc", 0.0);
        f->post_processing_time    = j_num(o, "post_proc", 0.0);
    }
}

static void apply_ga(const JNode *o, GaParams *p) {
    p->pop_size      = j_int(o, "pop_size",      p->pop_size);
    p->max_gen       = j_int(o, "max_gen",       p->max_gen);
    p->mutation_rate = j_num(o, "mutation_rate", p->mutation_rate);
}

static void apply_mac(const JNode *o, MacParams *p) {
    p->sigma_us       = j_num(o, "sigma_us",       p->sigma_us);
    p->sifs_us        = j_num(o, "sifs_us",        p->sifs_us);
    p->difs_us        = j_num(o, "difs_us",        p->difs_us);
    p->ack_us         = j_num(o, "ack_us",         p->ack_us);
    p->header_us      = j_num(o, "header_us",      p->header_us);
    p->tau_prop_us    = j_num(o, "tau_prop_us",    p->tau_prop_us);
    p->ack_timeout_us = j_num(o, "ack_timeout_us", p->ack_timeout_us);
    p->p_cap          = j_num(o, "p_cap",          p->p_cap);
    p->p_e_base       = j_num(o, "p_e_base",       p->p_e_base);
}

/* ============================================================
 *  入口
 * ============================================================ */
int sb_load_config(const char *path, SandboxConfig *cfg, Sandbox *sb) {
    memset(cfg, 0, sizeof(*cfg));
    sb_link_gen_default(&cfg->link_gen);
    sb_ga_params_default(&cfg->ga);
    sb_mac_params_default(&cfg->mac);

    char *raw = read_file_all(path);
    if (!raw) return -1;

    JParser ps = { .p = raw, .err = 0 };
    JNode *root = j_parse_value(&ps);
    if (!root || ps.err || root->type != JV_OBJ) {
        if (root) jnode_free(root);
        free(raw);
        return -2;
    }

    const char *name = j_str(root, "sandbox_name", "custom");
    strncpy(cfg->sandbox_name, name, SB_STR_LEN - 1);
    cfg->rng_seed = (uint64_t)j_num(root, "rng_seed", 42);

    JNode *jnodes = j_get(root, "nodes");
    if (jnodes && jnodes->type == JV_ARR) {
        cfg->has_nodes = 1;
        apply_nodes(jnodes, sb);
    }

    JNode *jlg = j_get(root, "link_generation");
    if (jlg && jlg->type == JV_OBJ) {
        cfg->has_link_gen = 1;
        apply_link_gen(jlg, &cfg->link_gen);
    }

    JNode *jflows = j_get(root, "flows");
    if (jflows && jflows->type == JV_ARR) {
        cfg->has_flows = 1;
        apply_flows(jflows, sb);
    }

    JNode *jga = j_get(root, "ga_params");
    if (jga && jga->type == JV_OBJ) {
        cfg->has_ga = 1;
        apply_ga(jga, &cfg->ga);
    }

    JNode *jmac = j_get(root, "mac_params");
    if (jmac && jmac->type == JV_OBJ) {
        cfg->has_mac = 1;
        apply_mac(jmac, &cfg->mac);
    }

    jnode_free(root);
    free(raw);

    return sb_apply_config(cfg, sb);
}

int sb_apply_config(const SandboxConfig *cfg, Sandbox *sb) {
    /* Step 1: 若未指定 nodes，落回 initializer 默认 */
    if (!cfg->has_nodes) {
        sb_init_physical_nodes(sb->p_nodes, &sb->p_node_count);
    }

    /* Step 2: 候选链路总是按 LinkGenParams 生成 */
    gen_candidate_links(sb, &cfg->link_gen);

    /* Step 3: 若未指定 flows，落回 initializer 默认 */
    /* BusNode 总要从物理节点重新构造 */
    sb_init_bus_nodes(sb->p_nodes, sb->p_node_count,
                      sb->b_nodes, &sb->b_node_count);
    if (!cfg->has_flows) {
        sb_init_flows(sb->b_nodes, sb->b_node_count, &sb->flow_graph);
    } else {
        /* 配置了 flows 后，把流挂到 BusNode 的 source/sink 上 */
        for (int i = 0; i < sb->flow_graph.count; i++) {
            Flow *f = &sb->flow_graph.flows[i];
            for (int n = 0; n < sb->b_node_count; n++) {
                BusNode *bn = &sb->b_nodes[n];
                if (bn->id == f->source_node_id) {
                    if (bn->source_count < SB_MAX_FLOWS_PER_NODE)
                        bn->source_flows[bn->source_count++] = i;
                }
                if (bn->id == f->target_node_id) {
                    if (bn->sink_count < SB_MAX_FLOWS_PER_NODE)
                        bn->sink_flows[bn->sink_count++] = i;
                }
            }
        }
    }

    /* 绑定物理表，供评估器访问 */
    sb_phys_bind(sb->p_nodes, sb->c_links);
    return 0;
}

void sb_config_print(const SandboxConfig *cfg, const Sandbox *sb) {
    printf("== Sandbox config ==\n");
    printf("  name = %s\n", cfg->sandbox_name);
    printf("  rng_seed = %llu\n", (unsigned long long)cfg->rng_seed);
    printf("  has_nodes=%d has_link_gen=%d has_flows=%d has_ga=%d has_mac=%d\n",
           cfg->has_nodes, cfg->has_link_gen, cfg->has_flows,
           cfg->has_ga, cfg->has_mac);
    printf("  p_nodes=%d c_links=%d flows=%d\n",
           sb->p_node_count, sb->c_link_count, sb->flow_graph.count);
    if (cfg->has_link_gen) {
        printf("  link_gen: dist_limit=%.0f bw_a=%.0f bw_b=%.1f cost=(%.1f+%.2f*d) rel=(%.3f-%.3f*r^2)\n",
               cfg->link_gen.distance_limit_m,
               cfg->link_gen.bandwidth_a, cfg->link_gen.bandwidth_b,
               cfg->link_gen.cost_const, cfg->link_gen.cost_slope,
               cfg->link_gen.reliability_base, cfg->link_gen.reliability_decay);
    }
    if (cfg->has_ga) {
        printf("  ga: pop=%d gen=%d mut=%.3f\n",
               cfg->ga.pop_size, cfg->ga.max_gen, cfg->ga.mutation_rate);
    }
}
