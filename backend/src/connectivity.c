/* ============================================================
 * connectivity.c —— Fiedler 值
 *
 * 与 Python evaluation/connectivity.py 等价。Python 用 numpy.linalg.eigvalsh
 * 求实对称矩阵特征值；这里手写循环 Jacobi 旋转。
 *
 * 复杂度：每轮 O(N^2) 扫描，N=20 时几十轮即可收敛到机器精度。
 * ============================================================ */
#include "connectivity.h"
#include <math.h>
#include <string.h>

#define JACOBI_MAX_SWEEPS  100
#define JACOBI_EPS         1e-12

/* ---------- Jacobi 旋转 ----------
 *  对 N×N 实对称矩阵 A（按行存储于 a[N*N]）求所有特征值，写入 eig[N]。
 *  循环扫描版（按行扫描上三角）。
 */
static void jacobi_sym_eig(double *a, int N, double *eig) {
    /* 每轮扫描所有上三角元素一次 */
    for (int sweep = 0; sweep < JACOBI_MAX_SWEEPS; sweep++) {
        /* 计算上三角非对角元素的"幅度"，作为收敛判据 */
        double off = 0.0;
        for (int p = 0; p < N; p++)
            for (int q = p + 1; q < N; q++)
                off += fabs(a[p*N + q]);

        if (off < JACOBI_EPS) break;

        /* 扫描上三角，每个 (p,q) 都做一次旋转 */
        for (int p = 0; p < N - 1; p++) {
            for (int q = p + 1; q < N; q++) {
                double apq = a[p*N + q];
                if (fabs(apq) < 1e-300) continue;

                double app = a[p*N + p];
                double aqq = a[q*N + q];

                /* 求旋转角：cot(2θ) = (aqq - app) / (2 * apq) */
                double theta = (aqq - app) / (2.0 * apq);
                double t;
                if (fabs(theta) > 1e15) {
                    t = 0.5 / theta;
                } else {
                    double sign = theta >= 0 ? 1.0 : -1.0;
                    t = sign / (fabs(theta) + sqrt(theta*theta + 1.0));
                }
                double c = 1.0 / sqrt(1.0 + t*t);
                double s = t * c;

                /* 更新 A：旋转矩阵 G(p,q,θ)^T A G(p,q,θ) */
                a[p*N + p] = app - t * apq;
                a[q*N + q] = aqq + t * apq;
                a[p*N + q] = 0.0;
                a[q*N + p] = 0.0;

                for (int r = 0; r < N; r++) {
                    if (r == p || r == q) continue;
                    double arp = a[r*N + p];
                    double arq = a[r*N + q];
                    a[r*N + p] = c * arp - s * arq;
                    a[r*N + q] = s * arp + c * arq;
                    a[p*N + r] = a[r*N + p];   /* 保持对称 */
                    a[q*N + r] = a[r*N + q];
                }
            }
        }
    }

    /* 对角线 -> 特征值 */
    for (int i = 0; i < N; i++) eig[i] = a[i*N + i];

    /* 升序排序（简单插入排序，N 很小） */
    for (int i = 1; i < N; i++) {
        double key = eig[i];
        int j = i - 1;
        while (j >= 0 && eig[j] > key) { eig[j+1] = eig[j]; j--; }
        eig[j+1] = key;
    }
}

/* ============================================================
 * 评估接口
 * ============================================================ */
double sb_connectivity_evaluate(const BusTopology *topo) {
    if (!topo) return 0.0;
    int N = topo->node_count;
    if (N < 2) return 0.0;

    /* 节点顺序：与 Python 一致，即 add_node 的插入顺序 */
    /* 建 id -> 矩阵下标 的映射（直接索引，因 id < SB_MAX_NODES*2） */
    int id2row[SB_MAX_NODES * 2];
    for (int i = 0; i < (int)(sizeof(id2row)/sizeof(int)); i++) id2row[i] = -1;
    for (int i = 0; i < N; i++) id2row[topo->nodes[i].id] = i;

    /* 拉普拉斯矩阵 */
    static double L[SB_MAX_NODES * SB_MAX_NODES];
    memset(L, 0, sizeof(double) * N * N);

    for (int i = 0; i < topo->link_count; i++) {
        const BusLink *l = &topo->links[i];
        int u = id2row[l->node_a_id];
        int v = id2row[l->node_b_id];
        if (u < 0 || v < 0) continue;

        L[u*N + u] += 1.0;
        L[v*N + v] += 1.0;
        L[u*N + v] -= 1.0;
        L[v*N + u] -= 1.0;
    }

    /* 求特征值 */
    static double eig[SB_MAX_NODES];
    jacobi_sym_eig(L, N, eig);

    double fiedler = eig[1];
    if (fiedler < 1e-10) fiedler = 0.0;
    return fiedler;
}

/* ============================================================
 *  归一化连通度（方案 5.1.3 节）
 * ============================================================ */
double sb_connectivity_evaluate_normalized(const BusTopology *topo,
                                           double lambda_base)
{
    double lambda2 = sb_connectivity_evaluate(topo);
    const double eps = 1e-10;
    if (lambda_base < 0) lambda_base = 0;
    double c = lambda2 / (lambda_base + eps);
    if (c < 0.0) c = 0.0;
    return c;
}
