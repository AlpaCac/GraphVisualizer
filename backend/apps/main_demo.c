#include "sb_models.h"
#include <stdio.h>
#include <string.h>

/* 用 deviceName 做 networkID 到 id 的解析（简单示例） */
typedef struct { PhysicalNode *p; int n; } Ctx;
static int resolver(const char *netid, void *vctx) {
    Ctx *c = (Ctx*)vctx;
    for (int i = 0; i < c->n; i++)
        if (strcmp(c->p[i].meta.identity.networkID, netid) == 0)
            return c->p[i].id;
    return -1;
}

int main(void) {
    /* 1. 物理节点（3 个，演示） */
    PhysicalNode p[3];
    for (int i = 0; i < 3; i++) {
        sb_phys_node_init(&p[i]);
        p[i].id = i;
        p[i].cpu_capacity = 6000.0; p[i].memory_capacity = 16384.0;
        p[i].x = i*100; p[i].y = 0; p[i].max_physical_ports = 4;
        p[i].reliability = 0.99;
        snprintf(p[i].meta.identity.networkID, SB_STR_LEN, "NID-%03d", i);
        snprintf(p[i].meta.identity.deviceName, SB_STR_LEN, "DEV-%d", i);
    }

    /* 2. 拓扑：3 节点，链路 0-1, 1-2 */
    BusTopology topo; sb_topology_init(&topo);
    for (int i = 0; i < 3; i++) {
        BusNode bn; sb_bus_node_init(&bn);
        bn.id = p[i].id; bn.physical_node_idx = i;
        bn.is_core = (i == 1);
        sb_topology_add_node(&topo, &bn);
    }
    BusLink l1; sb_bus_link_init(&l1);
    l1.id=0; l1.node_a_id=0; l1.node_b_id=1; l1.physical_link_idx=0;
    sb_topology_add_link(&topo, &l1);
    BusLink l2; sb_bus_link_init(&l2);
    l2.id=1; l2.node_a_id=1; l2.node_b_id=2; l2.physical_link_idx=1;
    sb_topology_add_link(&topo, &l2);

    /* 3. 一个表2任务 -> Flow */
    TaskData td; memset(&td, 0, sizeof(td));
    strcpy(td.basic.taskId, "7");
    strcpy(td.basic.taskName, "姿态控制指令");
    td.basic.taskType = TASK_CONTROL_CMD;
    strcpy(td.comm.srcNodeId, "NID-000");
    strcpy(td.comm.dstNodeIds[0], "NID-002"); td.comm.dstCount = 1;
    td.comm.packetSizeBytes = 128; td.comm.sendFrequencyHz = 50.0;
    td.qos.deadlineMs = 30.0; td.qos.minReliability = 0.999;
    td.constraint.priority = PRIO_CRITICAL;

    Ctx ctx = { p, 3 };
    Flow f; sb_flow_apply_taskdata(&f, &td, resolver, &ctx);
    sb_topology_add_flow(&topo, &f);

    /* 4. 给这条流一条路径并查链路 */
    int seq[] = {0, 1, 2};
    sb_flow_set_path(&topo.flows[0], seq, 3);
    BusLink *qa = sb_topology_get_link(&topo, 2, 1);  /* 反向查询应同样命中 */
    printf("query (2,1) -> %p, id=%d\n", (void*)qa, qa?qa->id:-1);

    sb_topology_print(&topo);
    return 0;
}
