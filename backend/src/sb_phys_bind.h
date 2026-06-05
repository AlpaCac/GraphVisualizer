#ifndef SB_PHYS_BIND_H
#define SB_PHYS_BIND_H

#include "sb_models.h"

/* ============================================================
 * 物理对象表绑定（共享模块）
 *
 * BusNode/BusLink 内只持有 physical_node_idx / physical_link_idx，
 * 路由器、可靠性、成本等评估器都需要通过这两个下标反查物理对象。
 * 为了不污染 sb_models.h 的核心结构，统一通过本模块绑定。
 *
 * 用法：上层先调用 sb_phys_bind(p_nodes, c_links)，
 *       随后所有依赖物理参数的模块都能用 sb_phys_node / sb_phys_link
 *       拿到指针。线程不安全，但任务里不需要并发。
 * ============================================================ */

void sb_phys_bind(const PhysicalNode *pnodes, const PhysicalLink *plinks);

const PhysicalNode *sb_phys_node(int physical_node_idx);
const PhysicalLink *sb_phys_link(int physical_link_idx);

/* 便捷宏：直接传 BusNode 或 BusLink 指针 */
#define SB_PN_OF(bn)  sb_phys_node((bn)->physical_node_idx)
#define SB_PL_OF(bl)  sb_phys_link((bl)->physical_link_idx)

#endif
