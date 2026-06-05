#ifndef SB_MODELS_H
#define SB_MODELS_H

#include <stdint.h>
#include <stddef.h>

#include "task_data.h"

#define SB_STR_LEN              64
#define SB_DESC_LEN             256
#define SB_MAX_NODES            64
#define SB_MAX_LINKS            512
#define SB_MAX_FLOWS            128
#define SB_MAX_LINKS_PER_NODE   16
#define SB_MAX_FLOWS_PER_NODE   32
#define SB_MAX_PASSING_PER_LINK 32
#define SB_MAX_PATH_HOPS        32
#define SB_MAX_NEIGHBORS        16
#define SB_MAX_INTERFACES       6
#define SB_MAX_DEPS_PER_FLOW    TD_MAX_DEPS

typedef enum { NODE_TYPE_UNKNOWN=0, NODE_TYPE_MASTER, NODE_TYPE_COMPUTE, NODE_TYPE_SENSOR,
               NODE_TYPE_ACTUATOR, NODE_TYPE_RELAY, NODE_TYPE_GATEWAY, NODE_TYPE_BACKUP } NodeStaticType;
typedef enum { NODE_ROLE_EDGE=0, NODE_ROLE_CORE=1, NODE_ROLE_DESTROYED=-1 } NodeDynamicRole;
typedef enum { IF_ETHERNET=0, IF_WIFI, IF_BLUETOOTH, IF_SERIAL_RS422, IF_CAN, IF_5G, IF_USB, IF_OTHER } InterfaceType;
typedef enum { LINK_STATE_INACTIVE=0, LINK_STATE_ACTIVE=1, LINK_STATE_DESTROYED=-1 } LinkState;
typedef enum { MOBILITY_STATIC=0, MOBILITY_LINEAR, MOBILITY_RANDOM_WALK,
               MOBILITY_PATROL, MOBILITY_PRESET_TRACK, MOBILITY_TASK_DRIVEN } MobilityModel;

typedef struct { char networkID[SB_STR_LEN]; char deviceName[SB_STR_LEN]; char ipAddress[SB_STR_LEN];
    char region[SB_STR_LEN]; char subnet[SB_STR_LEN]; double onlineTime; } NodeIdentity;
typedef struct { NodeStaticType staticType; NodeDynamicRole dynamicRole; int isKeyNode; int isBackupNode; } NodeTypeInfo;
typedef struct { char cpuArch[SB_STR_LEN]; int cpuCores; double cpuFreqGHz; double computeWeight;
    double memTotalMB, memAvailableMB, storageTotalGB, storageAvailableGB; int taskQueueLen; } NodeComputeRes;
typedef struct { InterfaceType ifType; char protocol[SB_STR_LEN]; double maxBandwidthMbps,effectiveBandwidthMbps,
    commRadiusM,txPowerDbm,rxSensitivityDbm; int occupied; } NodeInterface;
typedef struct { NodeInterface ifs[SB_MAX_INTERFACES]; int ifCount; int supportCrossDomain; } NodeCommRes;
typedef struct { int online,faulty; double currentLoad,remainingEnergy; int connectionCount,sessionCount;
    double forwardBytes; int carriedTasks; double healthScore,lastUpdateTime; } NodeRuntimeStat;
typedef struct { char commDomain[SB_STR_LEN]; int neighborIds[SB_MAX_NEIGHBORS]; int neighborCount;
    int directReachIds[SB_MAX_NEIGHBORS]; int directReachCount; int hopsToKey,atDomainBoundary;
    MobilityModel mobilityModel; double speedMps,dirDeg,moveStartTime,moveEndTime,posUpdatePeriod; } NodeTopoPos;
typedef struct { NodeIdentity identity; NodeTypeInfo typeInfo; NodeComputeRes compute;
    NodeCommRes comm; NodeRuntimeStat runtime; NodeTopoPos topo; } PhysicalNodeMeta;

typedef struct PhysicalNode {
    int id; double cpu_capacity, memory_capacity, x, y; int max_physical_ports;
    double reliability; int destroyed;
    int linkIdx[SB_MAX_LINKS_PER_NODE]; int linkCount;
    PhysicalNodeMeta meta;
} PhysicalNode;

typedef struct PhysicalLink {
    int id, node_a_id, node_b_id; double bandwidth, propagation_delay, reliability, cost;
    int destroyed; InterfaceType medium;
} PhysicalLink;

typedef struct BusNode {
    int id; int physical_node_idx; int is_core;
    int source_flows[SB_MAX_FLOWS_PER_NODE]; int source_count;
    int sink_flows  [SB_MAX_FLOWS_PER_NODE]; int sink_count;
    int relay_flows [SB_MAX_FLOWS_PER_NODE]; int relay_count;
    int link_idx[SB_MAX_LINKS_PER_NODE]; int link_count;
    double dynamic_weight;
} BusNode;

typedef struct BusLink {
    int id, node_a_id, node_b_id, physical_link_idx;
    double dynamic_weight, current_load;
    int passing_flows[SB_MAX_PASSING_PER_LINK]; int passing_count;
} BusLink;

typedef struct Flow {
    int id; char flow_name[SB_STR_LEN]; char topic[SB_STR_LEN];
    int source_node_id, target_node_id;
    double pre_processing_time, post_processing_time;
    int priority; double period, deadline, reliability_requirement, message_size;
    int upstream_idx  [SB_MAX_DEPS_PER_FLOW]; int upstream_count;
    int downstream_idx[SB_MAX_DEPS_PER_FLOW]; int downstream_count;
    int routing_path[SB_MAX_PATH_HOPS]; int path_len;
    double worst_case_delay, actual_reliability;
    double effective_throughput;       /* byte/ms，路径瓶颈 × min(需求)，由 throughput 模块写回 */
    double throughput_requirement;     /* byte/ms = message_size / period */
    int is_schedulable;
    int has_meta; TaskData meta;
} Flow;

typedef struct FlowGraph { Flow flows[SB_MAX_FLOWS]; int count; } FlowGraph;

#define SB_LINKMAP_CAP 1024
typedef struct { int key_a, key_b, link_idx; } LinkMapEntry;

typedef struct BusTopology {
    BusNode nodes[SB_MAX_NODES]; int node_count;
    int     node_id2idx[SB_MAX_NODES*2];
    BusLink links[SB_MAX_LINKS]; int link_count;
    LinkMapEntry link_map[SB_LINKMAP_CAP];
    Flow    flows[SB_MAX_FLOWS]; int flow_count;
} BusTopology;

void sb_phys_node_init(PhysicalNode *n);
void sb_phys_link_init(PhysicalLink *l);
void sb_bus_node_init (BusNode *n);
void sb_bus_link_init (BusLink *l);
void sb_flow_init     (Flow *f);
void sb_topology_init (BusTopology *t);

int  sb_topology_add_node(BusTopology *t, const BusNode *n);
int  sb_topology_add_link(BusTopology *t, const BusLink *l);
BusNode *sb_topology_get_node(BusTopology *t, int node_id);
BusLink *sb_topology_get_link(BusTopology *t, int node_a_id, int node_b_id);
int  sb_topology_add_flow(BusTopology *t, const Flow *f);

int  sb_flow_set_path(Flow *f, const int *node_id_seq, int hops);
void sb_flow_clear_path(Flow *f);
int  sb_flowgraph_add_dependency(FlowGraph *fg, int up, int down);

typedef int (*sb_node_id_resolver)(const char *networkID, void *ctx);
int sb_flow_apply_taskdata(Flow *f, const TaskData *td,
                           sb_node_id_resolver resolver, void *ctx);

void sb_topology_print(const BusTopology *t);

#endif
