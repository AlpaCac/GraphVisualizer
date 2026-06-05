#include "sb_models.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void str_copy(char *dst, const char *src, size_t cap) {
    if (!src) { dst[0]='\0'; return; }
    size_t n = strlen(src); if (n >= cap) n = cap - 1;
    memcpy(dst, src, n); dst[n] = '\0';
}
static inline void sort_pair(int *a, int *b) { if (*a > *b){int t=*a;*a=*b;*b=t;} }

void sb_phys_node_init(PhysicalNode *n){memset(n,0,sizeof(*n));n->id=-1;n->reliability=1.0;
    n->meta.runtime.online=1;n->meta.runtime.healthScore=1.0;
    n->meta.typeInfo.staticType=NODE_TYPE_UNKNOWN;n->meta.typeInfo.dynamicRole=NODE_ROLE_EDGE;
    n->meta.topo.mobilityModel=MOBILITY_STATIC;}
void sb_phys_link_init(PhysicalLink *l){memset(l,0,sizeof(*l));l->id=-1;l->node_a_id=-1;l->node_b_id=-1;
    l->reliability=1.0;l->medium=IF_ETHERNET;}
void sb_bus_node_init(BusNode *n){memset(n,0,sizeof(*n));n->id=-1;n->physical_node_idx=-1;}
void sb_bus_link_init(BusLink *l){memset(l,0,sizeof(*l));l->id=-1;l->node_a_id=-1;l->node_b_id=-1;l->physical_link_idx=-1;}
void sb_flow_init(Flow *f){memset(f,0,sizeof(*f));f->id=-1;f->source_node_id=-1;f->target_node_id=-1;
    f->worst_case_delay=-1.0;f->actual_reliability=-1.0;f->is_schedulable=1;f->has_meta=0;}
void sb_topology_init(BusTopology *t){memset(t,0,sizeof(*t));
    for(int i=0;i<(int)(sizeof(t->node_id2idx)/sizeof(int));i++)t->node_id2idx[i]=-1;
    for(int i=0;i<SB_LINKMAP_CAP;i++){t->link_map[i].key_a=-1;t->link_map[i].key_b=-1;t->link_map[i].link_idx=-1;}}

static uint32_t linkmap_hash(int a,int b){uint32_t h=(uint32_t)a*2654435761u;h^=(uint32_t)b*40503u;h^=h>>16;return h&(SB_LINKMAP_CAP-1);}
static int linkmap_put(BusTopology *t,int a,int b,int idx){sort_pair(&a,&b);uint32_t i=linkmap_hash(a,b);
    for(int p=0;p<SB_LINKMAP_CAP;p++){LinkMapEntry *e=&t->link_map[i];
        if(e->key_a==-1){e->key_a=a;e->key_b=b;e->link_idx=idx;return 0;}
        if(e->key_a==a&&e->key_b==b){e->link_idx=idx;return 0;}
        i=(i+1)&(SB_LINKMAP_CAP-1);}return -1;}
static int linkmap_get(const BusTopology *t,int a,int b){sort_pair(&a,&b);uint32_t i=linkmap_hash(a,b);
    for(int p=0;p<SB_LINKMAP_CAP;p++){const LinkMapEntry *e=&t->link_map[i];
        if(e->key_a==-1)return -1;
        if(e->key_a==a&&e->key_b==b)return e->link_idx;
        i=(i+1)&(SB_LINKMAP_CAP-1);}return -1;}

int sb_topology_add_node(BusTopology *t, const BusNode *n){
    if(t->node_count>=SB_MAX_NODES)return -1;
    if(n->id<0||n->id>=(int)(sizeof(t->node_id2idx)/sizeof(int)))return -1;
    if(t->node_id2idx[n->id]!=-1)return -1;
    int idx=t->node_count++;t->nodes[idx]=*n;
    t->nodes[idx].link_count=0;t->nodes[idx].source_count=0;
    t->nodes[idx].sink_count=0;t->nodes[idx].relay_count=0;
    t->node_id2idx[n->id]=idx;return idx;}
BusNode *sb_topology_get_node(BusTopology *t,int node_id){
    if(node_id<0||node_id>=(int)(sizeof(t->node_id2idx)/sizeof(int)))return NULL;
    int idx=t->node_id2idx[node_id]; if(idx<0)return NULL; return &t->nodes[idx];}
int sb_topology_add_link(BusTopology *t, const BusLink *l){
    if(t->link_count>=SB_MAX_LINKS)return -1;
    BusNode *a=sb_topology_get_node(t,l->node_a_id);
    BusNode *b=sb_topology_get_node(t,l->node_b_id);
    if(!a||!b)return -1;
    if(a->link_count>=SB_MAX_LINKS_PER_NODE||b->link_count>=SB_MAX_LINKS_PER_NODE)return -1;
    int idx=t->link_count++; t->links[idx]=*l;
    t->links[idx].passing_count=0; t->links[idx].current_load=0.0;
    a->link_idx[a->link_count++]=idx; b->link_idx[b->link_count++]=idx;
    linkmap_put(t,l->node_a_id,l->node_b_id,idx); return idx;}
BusLink *sb_topology_get_link(BusTopology *t,int a,int b){
    int idx=linkmap_get(t,a,b); if(idx<0)return NULL; return &t->links[idx];}
int sb_topology_add_flow(BusTopology *t, const Flow *f){
    if(t->flow_count>=SB_MAX_FLOWS)return -1;
    BusNode *src=sb_topology_get_node(t,f->source_node_id);
    BusNode *dst=sb_topology_get_node(t,f->target_node_id);
    if(!src||!dst)return -1;
    if(src->source_count>=SB_MAX_FLOWS_PER_NODE||dst->sink_count>=SB_MAX_FLOWS_PER_NODE)return -1;
    int idx=t->flow_count++; t->flows[idx]=*f;
    t->flows[idx].path_len=0;t->flows[idx].worst_case_delay=-1.0;
    t->flows[idx].actual_reliability=-1.0;t->flows[idx].is_schedulable=1;
    src->source_flows[src->source_count++]=idx;
    dst->sink_flows[dst->sink_count++]=idx; return idx;}

int sb_flow_set_path(Flow *f, const int *seq, int hops){
    if(hops<0||hops>SB_MAX_PATH_HOPS)return -1;
    for(int i=0;i<hops;i++)f->routing_path[i]=seq[i];
    f->path_len=hops; return 0;}
void sb_flow_clear_path(Flow *f){f->path_len=0;}

int sb_flowgraph_add_dependency(FlowGraph *fg,int up,int down){
    if(up<0||up>=fg->count||down<0||down>=fg->count)return -1;
    Flow *u=&fg->flows[up],*d=&fg->flows[down];
    if(u->downstream_count>=SB_MAX_DEPS_PER_FLOW||d->upstream_count>=SB_MAX_DEPS_PER_FLOW)return -1;
    u->downstream_idx[u->downstream_count++]=down;
    d->upstream_idx[d->upstream_count++]=up; return 0;}

int sb_flow_apply_taskdata(Flow *f,const TaskData *td,sb_node_id_resolver resolver,void *ctx){
    if(!f||!td)return -1;
    sb_flow_init(f);
    if(td->basic.taskId[0]){char *end=NULL;long v=strtol(td->basic.taskId,&end,10);
        if(end&&*end=='\0')f->id=(int)v;}
    str_copy(f->flow_name,td->basic.taskName,SB_STR_LEN);
    str_copy(f->topic,task_type_to_str(td->basic.taskType),SB_STR_LEN);
    if(resolver){
        f->source_node_id=resolver(td->comm.srcNodeId,ctx);
        if(td->comm.dstCount>0)f->target_node_id=resolver(td->comm.dstNodeIds[0],ctx);}
    f->deadline=td->qos.deadlineMs>0?td->qos.deadlineMs:td->qos.maxDelayMs;
    f->reliability_requirement=td->qos.minReliability;
    f->message_size=(double)td->comm.packetSizeBytes;
    if(td->comm.sendFrequencyHz>0.0)f->period=1000.0/td->comm.sendFrequencyHz;
    else if(td->comm.serviceDuration>0.0)f->period=td->comm.serviceDuration;
    else f->period=0.0;
    f->priority=(int)td->constraint.priority+1;
    if(td->constraint.importanceLevel>f->priority)f->priority=td->constraint.importanceLevel;
    f->pre_processing_time=0.0;f->post_processing_time=0.0;
    f->meta=*td;f->has_meta=1;return 0;}

void sb_topology_print(const BusTopology *t){
    printf("== BusTopology: nodes=%d links=%d flows=%d ==\n",t->node_count,t->link_count,t->flow_count);
    for(int i=0;i<t->node_count;i++){const BusNode *n=&t->nodes[i];
        printf(" Node#%-2d id=%-2d role=%-4s links=%d src=%d sink=%d relay=%d\n",
               i,n->id,n->is_core?"CORE":"EDGE",n->link_count,n->source_count,n->sink_count,n->relay_count);}
    for(int i=0;i<t->link_count;i++){const BusLink *l=&t->links[i];
        printf(" Link#%-2d id=%-2d %d <-> %d load=%.2f passing=%d\n",
               i,l->id,l->node_a_id,l->node_b_id,l->current_load,l->passing_count);}
    for(int i=0;i<t->flow_count;i++){const Flow *f=&t->flows[i];
        printf(" Flow#%-2d id=%-2d %s  %d->%d  prio=%d period=%.1f deadline=%.1f hops=%d\n",
               i,f->id,f->flow_name,f->source_node_id,f->target_node_id,
               f->priority,f->period,f->deadline,f->path_len);}}
