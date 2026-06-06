#ifndef SB_JSONW_H
#define SB_JSONW_H

#include <stdio.h>
#include <stdint.h>

/* ============================================================
 * 极简 JSON Writer
 *
 * 增量式：把 JSON 写到 FILE* 流。调用方负责维护对象/数组嵌套配对：
 *   jw_obj_open, jw_obj_close
 *   jw_arr_open, jw_arr_close
 *   jw_key(name) 写键，紧接着一个值
 *   jw_str(s) / jw_int(n) / jw_dbl(x) / jw_bool(b) / jw_null()
 *
 * Writer 自动处理同一容器内多个值之间的逗号。
 * 不做任何嵌套校验——错配是调用方的责任。
 *
 * 默认输出 pretty JSON（2 空格缩进），便于和输入配置文件保持同类格式。
 *
 * 双精度浮点序列化：
 *   - INF / NaN -> "null"（JSON 标准不允许 inf）
 *   - 默认精度 17 位（往返安全）
 * ============================================================ */

#define JW_MAX_DEPTH 32

typedef struct {
    FILE *out;
    int   need_comma[JW_MAX_DEPTH];    /* 每层"下一个值前是否需要逗号" */
    int   item_count[JW_MAX_DEPTH];     /* 每层已写入元素数，用于收尾缩进 */
    int   after_key;                    /* 刚写完对象 key，下一次写的是 value */
    int   depth;
} JsonWriter;

void jw_init (JsonWriter *jw, FILE *out);
void jw_done (JsonWriter *jw);          /* 末尾 flush，不关闭 FILE */

/* 容器 */
void jw_obj_open (JsonWriter *jw);
void jw_obj_close(JsonWriter *jw);
void jw_arr_open (JsonWriter *jw);
void jw_arr_close(JsonWriter *jw);

/* 键 */
void jw_key(JsonWriter *jw, const char *name);

/* 值 */
void jw_str  (JsonWriter *jw, const char *s);
void jw_int  (JsonWriter *jw, long long v);
void jw_dbl  (JsonWriter *jw, double v);
void jw_bool (JsonWriter *jw, int v);
void jw_null (JsonWriter *jw);

/* 便捷：键 + 值 */
void jw_kv_str (JsonWriter *jw, const char *k, const char *v);
void jw_kv_int (JsonWriter *jw, const char *k, long long v);
void jw_kv_dbl (JsonWriter *jw, const char *k, double v);
void jw_kv_bool(JsonWriter *jw, const char *k, int v);

/* 数组：写定长整型/双精度数组 */
void jw_kv_int_arr(JsonWriter *jw, const char *k, const int *arr, int n);
void jw_kv_dbl_arr(JsonWriter *jw, const char *k, const double *arr, int n);

#endif
