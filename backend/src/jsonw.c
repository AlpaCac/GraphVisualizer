#include "jsonw.h"
#include <math.h>
#include <string.h>

/* ============================================================
 * 单一逗号策略：
 *   need_comma[depth-1] = 1 时，下一个"元素或键"前先写逗号。
 *   - key 是"元素的开头" -> 前置逗号；写完置 0（值还没写）
 *   - 值写完 -> need_comma=1
 *   - 在数组里直接写值 -> 同样视为元素，前置逗号 + 写完置 1
 * ============================================================ */

void jw_init(JsonWriter *jw, FILE *out) {
    jw->out = out;
    jw->depth = 0;
    for (int i = 0; i < JW_MAX_DEPTH; i++) jw->need_comma[i] = 0;
}
void jw_done(JsonWriter *jw) { fflush(jw->out); }

static void jw_pre_element(JsonWriter *jw) {
    if (jw->depth > 0 && jw->need_comma[jw->depth - 1]) fputc(',', jw->out);
}
static void jw_post_element(JsonWriter *jw) {
    if (jw->depth > 0) jw->need_comma[jw->depth - 1] = 1;
}

static void jw_write_str_escaped(JsonWriter *jw, const char *s) {
    fputc('"', jw->out);
    if (s) {
        for (const unsigned char *p = (const unsigned char*)s; *p; p++) {
            switch (*p) {
                case '"':  fputs("\\\"", jw->out); break;
                case '\\': fputs("\\\\", jw->out); break;
                case '\n': fputs("\\n",  jw->out); break;
                case '\r': fputs("\\r",  jw->out); break;
                case '\t': fputs("\\t",  jw->out); break;
                default:
                    if (*p < 0x20) fprintf(jw->out, "\\u%04x", *p);
                    else fputc((char)*p, jw->out);
            }
        }
    }
    fputc('"', jw->out);
}

void jw_obj_open(JsonWriter *jw) {
    jw_pre_element(jw);
    fputc('{', jw->out);
    if (jw->depth < JW_MAX_DEPTH) {
        jw->need_comma[jw->depth] = 0;
        jw->depth++;
    }
}
void jw_obj_close(JsonWriter *jw) {
    fputc('}', jw->out);
    if (jw->depth > 0) jw->depth--;
    jw_post_element(jw);
}
void jw_arr_open(JsonWriter *jw) {
    jw_pre_element(jw);
    fputc('[', jw->out);
    if (jw->depth < JW_MAX_DEPTH) {
        jw->need_comma[jw->depth] = 0;
        jw->depth++;
    }
}
void jw_arr_close(JsonWriter *jw) {
    fputc(']', jw->out);
    if (jw->depth > 0) jw->depth--;
    jw_post_element(jw);
}

void jw_key(JsonWriter *jw, const char *name) {
    jw_pre_element(jw);
    jw_write_str_escaped(jw, name);
    fputc(':', jw->out);
    if (jw->depth > 0) jw->need_comma[jw->depth - 1] = 0;
}

void jw_str(JsonWriter *jw, const char *s) {
    jw_pre_element(jw);
    jw_write_str_escaped(jw, s ? s : "");
    jw_post_element(jw);
}

void jw_int(JsonWriter *jw, long long v) {
    jw_pre_element(jw);
    fprintf(jw->out, "%lld", v);
    jw_post_element(jw);
}

void jw_dbl(JsonWriter *jw, double v) {
    jw_pre_element(jw);
    if (isnan(v) || isinf(v)) fputs("null", jw->out);
    else fprintf(jw->out, "%.17g", v);
    jw_post_element(jw);
}

void jw_bool(JsonWriter *jw, int v) {
    jw_pre_element(jw);
    fputs(v ? "true" : "false", jw->out);
    jw_post_element(jw);
}

void jw_null(JsonWriter *jw) {
    jw_pre_element(jw);
    fputs("null", jw->out);
    jw_post_element(jw);
}

void jw_kv_str (JsonWriter *jw, const char *k, const char *v)  { jw_key(jw, k); jw_str (jw, v); }
void jw_kv_int (JsonWriter *jw, const char *k, long long v)    { jw_key(jw, k); jw_int (jw, v); }
void jw_kv_dbl (JsonWriter *jw, const char *k, double v)       { jw_key(jw, k); jw_dbl (jw, v); }
void jw_kv_bool(JsonWriter *jw, const char *k, int v)          { jw_key(jw, k); jw_bool(jw, v); }

void jw_kv_int_arr(JsonWriter *jw, const char *k, const int *arr, int n) {
    jw_key(jw, k);
    jw_arr_open(jw);
    for (int i = 0; i < n; i++) jw_int(jw, arr[i]);
    jw_arr_close(jw);
}

void jw_kv_dbl_arr(JsonWriter *jw, const char *k, const double *arr, int n) {
    jw_key(jw, k);
    jw_arr_open(jw);
    for (int i = 0; i < n; i++) jw_dbl(jw, arr[i]);
    jw_arr_close(jw);
}
