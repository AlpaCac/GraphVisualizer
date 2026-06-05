#include "sb_phys_bind.h"

static const PhysicalNode *g_pnodes = NULL;
static const PhysicalLink *g_plinks = NULL;

void sb_phys_bind(const PhysicalNode *pnodes, const PhysicalLink *plinks) {
    g_pnodes = pnodes;
    g_plinks = plinks;
}

const PhysicalNode *sb_phys_node(int idx) { return &g_pnodes[idx]; }
const PhysicalLink *sb_phys_link(int idx) { return &g_plinks[idx]; }
