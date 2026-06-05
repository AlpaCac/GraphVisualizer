#ifndef SB_CONNECTIVITY_H
#define SB_CONNECTIVITY_H

#include "sb_models.h"

/* ============================================================
 * 代数连通度评估器（Fiedler 值）
 *
 * 算法：
 *   1. 构造对称拉普拉斯矩阵 L = D - A（N×N）
 *   2. Jacobi 旋转求全部特征值
 *   3. 返回第二小特征值，并做 <1e-10 -> 0 的精度修正
 *
 * 与 Python evaluation/connectivity.py 等价。
 *   - 节点数 < 2 -> 返回 0.0
 *   - 不连通图 -> 第二小特征值 ≈ 0 -> 返回 0.0
 *
 * 不需要 sb_phys_bind，因为只看拓扑结构、不看物理参数。
 * ============================================================ */

double sb_connectivity_evaluate(const BusTopology *topo);

/* ============================================================
 * 归一化代数连通度（对应方案 5.1.3 节）
 *
 *   C_conn(t) = λ_2(t) / (λ_2_base + ε)
 *
 * 用于在不同规模 / 不同场景拓扑之间进行可比较的连通能力评估。
 *   - lambda_base：参考基线拓扑的代数连通度（≥0）
 *   - 返回值：当前拓扑的相对连通能力，0 表示完全断裂，1 表示等同于基线
 *
 * 调用方需先用初始 / 全激活拓扑跑一次 sb_connectivity_evaluate 得到 baseline。
 * ============================================================ */
double sb_connectivity_evaluate_normalized(const BusTopology *topo,
                                           double lambda_base);

#endif
