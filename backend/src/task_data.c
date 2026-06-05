/* ============================================================
 * task_data.c —— 解析实现（纯C，无外部依赖）
 * ============================================================ */
#include "task_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void str_copy(char *dst, const char *src, size_t cap) {
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int str_ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

void task_data_init(TaskData *t) {
    memset(t, 0, sizeof(*t));
    t->basic.taskType            = TASK_TYPE_UNKNOWN;
    t->comm.commMode             = COMM_P2P;
    t->comm.concurrentConnections= 1;
    t->resource.migratable       = 1;
    t->constraint.priority       = PRIO_NORMAL;
    t->constraint.allowDegrade   = 1;
    t->constraint.allowDelay     = 1;
}

TaskType task_type_from_str(const char *s) {
    if (str_ieq(s, "StatusReport"))  return TASK_STATUS_REPORT;
    if (str_ieq(s, "ControlCommand"))return TASK_CONTROL_CMD;
    if (str_ieq(s, "SensorCollect")) return TASK_SENSOR_COLLECT;
    if (str_ieq(s, "FileTransfer"))  return TASK_FILE_TRANSFER;
    if (str_ieq(s, "Cooperative"))   return TASK_COOPERATIVE;
    if (str_ieq(s, "FaultRecovery")) return TASK_FAULT_RECOVERY;
    return TASK_TYPE_UNKNOWN;
}
const char *task_type_to_str(TaskType v) {
    switch (v) {
        case TASK_STATUS_REPORT:  return "StatusReport";
        case TASK_CONTROL_CMD:    return "ControlCommand";
        case TASK_SENSOR_COLLECT: return "SensorCollect";
        case TASK_FILE_TRANSFER:  return "FileTransfer";
        case TASK_COOPERATIVE:    return "Cooperative";
        case TASK_FAULT_RECOVERY: return "FaultRecovery";
        default:                  return "Unknown";
    }
}
CommMode comm_mode_from_str(const char *s) {
    if (str_ieq(s, "P2P") || str_ieq(s, "PointToPoint"))         return COMM_P2P;
    if (str_ieq(s, "P2MP")|| str_ieq(s, "PointToMultipoint"))    return COMM_P2MP;
    if (str_ieq(s, "Converge")||str_ieq(s,"MultipointConverge")) return COMM_CONVERGE;
    if (str_ieq(s, "CrossDomain")||str_ieq(s,"CrossDomainRelay"))return COMM_CROSS_DOMAIN;
    return COMM_P2P;
}
const char *comm_mode_to_str(CommMode v) {
    switch (v) {
        case COMM_P2P:          return "P2P";
        case COMM_P2MP:         return "P2MP";
        case COMM_CONVERGE:     return "Converge";
        case COMM_CROSS_DOMAIN: return "CrossDomain";
        default:                return "P2P";
    }
}
TaskPriority priority_from_str(const char *s) {
    if (str_ieq(s, "Low"))      return PRIO_LOW;
    if (str_ieq(s, "Normal"))   return PRIO_NORMAL;
    if (str_ieq(s, "High"))     return PRIO_HIGH;
    if (str_ieq(s, "Critical")) return PRIO_CRITICAL;
    if (isdigit((unsigned char)s[0])) {
        int v = atoi(s);
        if (v >= PRIO_LOW && v <= PRIO_CRITICAL) return (TaskPriority)v;
    }
    return PRIO_NORMAL;
}
const char *priority_to_str(TaskPriority v) {
    switch (v) {
        case PRIO_LOW:      return "Low";
        case PRIO_NORMAL:   return "Normal";
        case PRIO_HIGH:     return "High";
        case PRIO_CRITICAL: return "Critical";
        default:            return "Normal";
    }
}

/* ============================================================
 * 极简 JSON 解析器
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
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"';  break;
                case '\\':c = '\\'; break;
                case '/': c = '/';  break;
                default:  c = e;    break;
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
    {
        char *end = NULL;
        double v = strtod(ps->p, &end);
        if (end == ps->p) { ps->err = 1; return NULL; }
        ps->p = end;
        JNode *n = jnode_new(JV_NUM);
        n->num = v;
        return n;
    }
}

static JNode *j_get(const JNode *o, const char *key) {
    if (!o || o->type != JV_OBJ) return NULL;
    for (int i = 0; i < o->count; i++)
        if (o->keys[i] && !strcmp(o->keys[i], key)) return o->items[i];
    return NULL;
}
static const char *j_get_str(const JNode *o, const char *key, const char *def) {
    JNode *n = j_get(o, key);
    return (n && n->type == JV_STR) ? n->str : def;
}
static double j_get_num(const JNode *o, const char *key, double def) {
    JNode *n = j_get(o, key);
    if (!n) return def;
    if (n->type == JV_NUM)  return n->num;
    if (n->type == JV_BOOL) return n->boolean;
    return def;
}
static int j_get_bool(const JNode *o, const char *key, int def) {
    JNode *n = j_get(o, key);
    if (!n) return def;
    if (n->type == JV_BOOL) return n->boolean;
    if (n->type == JV_NUM)  return n->num != 0.0;
    return def;
}

static void fill_task_from_jobj(const JNode *o, TaskData *t) {
    task_data_init(t);
    str_copy(t->basic.taskId,      j_get_str(o, "taskId", ""),      TD_STR_LEN);
    str_copy(t->basic.taskName,    j_get_str(o, "taskName", ""),    TD_STR_LEN);
    t->basic.taskType = task_type_from_str(j_get_str(o, "taskType", "Unknown"));
    str_copy(t->basic.description, j_get_str(o, "description", ""), TD_DESC_LEN);
    str_copy(t->basic.sceneId,     j_get_str(o, "sceneId", ""),     TD_STR_LEN);
    t->basic.startTime  = j_get_num(o, "startTime", 0);
    t->basic.endTime    = j_get_num(o, "endTime", 0);
    t->basic.duration   = j_get_num(o, "duration", 0);
    t->basic.isCritical = j_get_bool(o, "isCritical", 0);

    str_copy(t->comm.srcNodeId, j_get_str(o, "srcNodeId", ""), TD_STR_LEN);
    JNode *dst = j_get(o, "dstNodeIds");
    if (dst && dst->type == JV_ARR) {
        for (int i = 0; i < dst->count && t->comm.dstCount < TD_MAX_DST; i++) {
            if (dst->items[i]->type == JV_STR)
                str_copy(t->comm.dstNodeIds[t->comm.dstCount++],
                         dst->items[i]->str, TD_STR_LEN);
        }
    }
    t->comm.commMode             = comm_mode_from_str(j_get_str(o, "commMode", "P2P"));
    t->comm.packetSizeBytes      = (uint32_t)j_get_num(o, "packetSizeBytes", 0);
    t->comm.sendFrequencyHz      = j_get_num(o, "sendFrequencyHz", 0);
    t->comm.totalDataBytes       = (uint64_t)j_get_num(o, "totalDataBytes", 0);
    t->comm.serviceDuration      = j_get_num(o, "serviceDuration", 0);
    t->comm.concurrentConnections= (uint32_t)j_get_num(o, "concurrentConnections", 1);
    t->comm.isMulticastOrConverge= j_get_bool(o, "isMulticastOrConverge", 0);
    t->comm.isPeriodic           = j_get_bool(o, "isPeriodic", 0);
    t->comm.isBurst              = j_get_bool(o, "isBurst", 0);

    t->qos.maxDelayMs         = j_get_num(o, "maxDelayMs", 0);
    t->qos.maxJitterMs        = j_get_num(o, "maxJitterMs", 0);
    t->qos.minThroughputMbps  = j_get_num(o, "minThroughputMbps", 0);
    t->qos.maxPacketLossRate  = j_get_num(o, "maxPacketLossRate", 0);
    t->qos.minReliability     = j_get_num(o, "minReliability", 0);
    t->qos.deadlineMs         = j_get_num(o, "deadlineMs", 0);
    t->qos.minCompletionRate  = j_get_num(o, "minCompletionRate", 0);
    t->qos.maxRecoveryTimeMs  = j_get_num(o, "maxRecoveryTimeMs", 0);
    t->qos.continuityRequired = j_get_bool(o, "continuityRequired", 0);

    t->resource.requiredCompute       = j_get_num(o, "requiredCompute", 0);
    t->resource.requiredStorage       = j_get_num(o, "requiredStorage", 0);
    t->resource.requiredBandwidthMbps = j_get_num(o, "requiredBandwidthMbps", 0);
    t->resource.requiredCacheMB       = j_get_num(o, "requiredCacheMB", 0);
    t->resource.reserveResource       = j_get_bool(o, "reserveResource", 0);
    t->resource.dedicatedChannel      = j_get_bool(o, "dedicatedChannel", 0);
    t->resource.migratable            = j_get_bool(o, "migratable", 1);
    t->resource.needBackupResource    = j_get_bool(o, "needBackupResource", 0);

    t->constraint.priority        = priority_from_str(j_get_str(o, "priority", "Normal"));
    t->constraint.importanceLevel = (int)j_get_num(o, "importanceLevel", 0);
    t->constraint.preemptive      = j_get_bool(o, "preemptive", 0);
    t->constraint.allowDegrade    = j_get_bool(o, "allowDegrade", 1);
    t->constraint.allowDelay      = j_get_bool(o, "allowDelay", 1);
    t->constraint.allowInterrupt  = j_get_bool(o, "allowInterrupt", 0);

    JNode *deps = j_get(o, "dependencies");
    if (deps && deps->type == JV_ARR) {
        for (int i = 0; i < deps->count && t->constraint.depCount < TD_MAX_DEPS; i++) {
            JNode *d = deps->items[i];
            TaskDependency *td = &t->constraint.dependencies[t->constraint.depCount++];
            if (d->type == JV_STR) {
                str_copy(td->dependsOnTaskId, d->str, TD_STR_LEN);
                td->isHardDependency = 1;
            } else if (d->type == JV_OBJ) {
                str_copy(td->dependsOnTaskId, j_get_str(d, "dependsOnTaskId", ""), TD_STR_LEN);
                td->isHardDependency = j_get_bool(d, "isHardDependency", 1);
            }
        }
    }
    JNode *conf = j_get(o, "conflictTaskIds");
    if (conf && conf->type == JV_ARR) {
        for (int i = 0; i < conf->count && t->constraint.conflictCount < TD_MAX_CONFLICTS; i++) {
            if (conf->items[i]->type == JV_STR)
                str_copy(t->constraint.conflictTaskIds[t->constraint.conflictCount++],
                         conf->items[i]->str, TD_STR_LEN);
        }
    }
}

int task_load_json_string(const char *json, TaskSet *out) {
    out->count = 0;
    JParser ps; ps.p = json; ps.err = 0;
    JNode *root = j_parse_value(&ps);
    if (ps.err || !root) { jnode_free(root); return -1; }

    if (root->type == JV_OBJ) {
        fill_task_from_jobj(root, &out->tasks[out->count++]);
    } else if (root->type == JV_ARR) {
        for (int i = 0; i < root->count && out->count < TD_MAX_TASKS; i++) {
            if (root->items[i]->type == JV_OBJ)
                fill_task_from_jobj(root->items[i], &out->tasks[out->count++]);
        }
    } else {
        jnode_free(root);
        return -2;
    }
    jnode_free(root);
    return 0;
}

static char *read_whole_file(const char *path, long *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    long n = (long)fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = '\0';
    if (size_out) *size_out = n;
    return buf;
}

int task_load_json_file(const char *path, TaskSet *out) {
    char *buf = read_whole_file(path, NULL);
    if (!buf) return -1;
    int rc = task_load_json_string(buf, out);
    free(buf);
    return rc;
}

/* ============================================================
 * CSV 加载
 * ============================================================ */
#define CSV_MAX_COLS 64

static int split_line(char *line, char sep, char **cols, int max_cols) {
    int n = 0;
    char *p = line;
    cols[n++] = p;
    while (*p && n < max_cols) {
        if (*p == sep) { *p = '\0'; cols[n++] = p + 1; }
        p++;
    }
    return n;
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)*(e-1))) *(--e) = '\0';
    return s;
}

static void csv_set_field(TaskData *t, const char *key, const char *val) {
    if      (!strcmp(key, "taskId"))      str_copy(t->basic.taskId, val, TD_STR_LEN);
    else if (!strcmp(key, "taskName"))    str_copy(t->basic.taskName, val, TD_STR_LEN);
    else if (!strcmp(key, "taskType"))    t->basic.taskType = task_type_from_str(val);
    else if (!strcmp(key, "description")) str_copy(t->basic.description, val, TD_DESC_LEN);
    else if (!strcmp(key, "sceneId"))     str_copy(t->basic.sceneId, val, TD_STR_LEN);
    else if (!strcmp(key, "startTime"))   t->basic.startTime = atof(val);
    else if (!strcmp(key, "endTime"))     t->basic.endTime = atof(val);
    else if (!strcmp(key, "duration"))    t->basic.duration = atof(val);
    else if (!strcmp(key, "isCritical"))  t->basic.isCritical = atoi(val);

    else if (!strcmp(key, "srcNodeId"))   str_copy(t->comm.srcNodeId, val, TD_STR_LEN);
    else if (!strcmp(key, "dstNodeIds")) {
        char tmp[512]; str_copy(tmp, val, sizeof(tmp));
        char *segs[TD_MAX_DST]; int n = split_line(tmp, ';', segs, TD_MAX_DST);
        for (int i = 0; i < n && t->comm.dstCount < TD_MAX_DST; i++) {
            char *s = trim(segs[i]);
            if (*s) str_copy(t->comm.dstNodeIds[t->comm.dstCount++], s, TD_STR_LEN);
        }
    }
    else if (!strcmp(key, "commMode"))             t->comm.commMode = comm_mode_from_str(val);
    else if (!strcmp(key, "packetSizeBytes"))      t->comm.packetSizeBytes = (uint32_t)strtoul(val,0,10);
    else if (!strcmp(key, "sendFrequencyHz"))      t->comm.sendFrequencyHz = atof(val);
    else if (!strcmp(key, "totalDataBytes"))       t->comm.totalDataBytes = (uint64_t)strtoull(val,0,10);
    else if (!strcmp(key, "serviceDuration"))      t->comm.serviceDuration = atof(val);
    else if (!strcmp(key, "concurrentConnections"))t->comm.concurrentConnections = (uint32_t)strtoul(val,0,10);
    else if (!strcmp(key, "isMulticastOrConverge"))t->comm.isMulticastOrConverge = atoi(val);
    else if (!strcmp(key, "isPeriodic"))           t->comm.isPeriodic = atoi(val);
    else if (!strcmp(key, "isBurst"))              t->comm.isBurst = atoi(val);

    else if (!strcmp(key, "maxDelayMs"))         t->qos.maxDelayMs = atof(val);
    else if (!strcmp(key, "maxJitterMs"))        t->qos.maxJitterMs = atof(val);
    else if (!strcmp(key, "minThroughputMbps"))  t->qos.minThroughputMbps = atof(val);
    else if (!strcmp(key, "maxPacketLossRate"))  t->qos.maxPacketLossRate = atof(val);
    else if (!strcmp(key, "minReliability"))     t->qos.minReliability = atof(val);
    else if (!strcmp(key, "deadlineMs"))         t->qos.deadlineMs = atof(val);
    else if (!strcmp(key, "minCompletionRate"))  t->qos.minCompletionRate = atof(val);
    else if (!strcmp(key, "maxRecoveryTimeMs"))  t->qos.maxRecoveryTimeMs = atof(val);
    else if (!strcmp(key, "continuityRequired")) t->qos.continuityRequired = atoi(val);

    else if (!strcmp(key, "requiredCompute"))       t->resource.requiredCompute = atof(val);
    else if (!strcmp(key, "requiredStorage"))       t->resource.requiredStorage = atof(val);
    else if (!strcmp(key, "requiredBandwidthMbps")) t->resource.requiredBandwidthMbps = atof(val);
    else if (!strcmp(key, "requiredCacheMB"))       t->resource.requiredCacheMB = atof(val);
    else if (!strcmp(key, "reserveResource"))       t->resource.reserveResource = atoi(val);
    else if (!strcmp(key, "dedicatedChannel"))      t->resource.dedicatedChannel = atoi(val);
    else if (!strcmp(key, "migratable"))            t->resource.migratable = atoi(val);
    else if (!strcmp(key, "needBackupResource"))    t->resource.needBackupResource = atoi(val);

    else if (!strcmp(key, "priority"))        t->constraint.priority = priority_from_str(val);
    else if (!strcmp(key, "importanceLevel")) t->constraint.importanceLevel = atoi(val);
    else if (!strcmp(key, "preemptive"))      t->constraint.preemptive = atoi(val);
    else if (!strcmp(key, "allowDegrade"))    t->constraint.allowDegrade = atoi(val);
    else if (!strcmp(key, "allowDelay"))      t->constraint.allowDelay = atoi(val);
    else if (!strcmp(key, "allowInterrupt"))  t->constraint.allowInterrupt = atoi(val);
    else if (!strcmp(key, "dependencies")) {
        char tmp[512]; str_copy(tmp, val, sizeof(tmp));
        char *segs[TD_MAX_DEPS]; int n = split_line(tmp, ';', segs, TD_MAX_DEPS);
        for (int i = 0; i < n && t->constraint.depCount < TD_MAX_DEPS; i++) {
            char *s = trim(segs[i]);
            if (!*s) continue;
            TaskDependency *td = &t->constraint.dependencies[t->constraint.depCount++];
            char *colon = strchr(s, ':');
            if (colon) { *colon = '\0'; td->isHardDependency = atoi(colon + 1); }
            else       { td->isHardDependency = 1; }
            str_copy(td->dependsOnTaskId, trim(s), TD_STR_LEN);
        }
    }
    else if (!strcmp(key, "conflictTaskIds")) {
        char tmp[512]; str_copy(tmp, val, sizeof(tmp));
        char *segs[TD_MAX_CONFLICTS]; int n = split_line(tmp, ';', segs, TD_MAX_CONFLICTS);
        for (int i = 0; i < n && t->constraint.conflictCount < TD_MAX_CONFLICTS; i++) {
            char *s = trim(segs[i]);
            if (*s) str_copy(t->constraint.conflictTaskIds[t->constraint.conflictCount++], s, TD_STR_LEN);
        }
    }
}

int task_load_csv_file(const char *path, TaskSet *out) {
    out->count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char header[4096];
    if (!fgets(header, sizeof(header), f)) { fclose(f); return -2; }
    header[strcspn(header, "\r\n")] = '\0';

    char *hcols[CSV_MAX_COLS];
    int   hcount = split_line(header, ',', hcols, CSV_MAX_COLS);
    for (int i = 0; i < hcount; i++) hcols[i] = trim(hcols[i]);

    char line[4096];
    while (fgets(line, sizeof(line), f) && out->count < TD_MAX_TASKS) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        char *vcols[CSV_MAX_COLS];
        int   vcount = split_line(line, ',', vcols, CSV_MAX_COLS);

        TaskData *t = &out->tasks[out->count];
        task_data_init(t);
        int cols = vcount < hcount ? vcount : hcount;
        for (int i = 0; i < cols; i++)
            csv_set_field(t, hcols[i], trim(vcols[i]));
        out->count++;
    }
    fclose(f);
    return 0;
}

void task_set_print(const TaskSet *set) {
    printf("== TaskSet: %d task(s) ==\n", set->count);
    for (int i = 0; i < set->count; i++) {
        const TaskData *t = &set->tasks[i];
        printf("[%d] id=%s name=%s type=%s scene=%s critical=%d\n",
               i, t->basic.taskId, t->basic.taskName,
               task_type_to_str(t->basic.taskType),
               t->basic.sceneId, t->basic.isCritical);
        printf("    src=%s dstCount=%d mode=%s pkt=%u freq=%.2f prio=%s\n",
               t->comm.srcNodeId, t->comm.dstCount,
               comm_mode_to_str(t->comm.commMode),
               t->comm.packetSizeBytes, t->comm.sendFrequencyHz,
               priority_to_str(t->constraint.priority));
        printf("    qos: delay<=%.1fms loss<=%.3f rel>=%.3f deadline=%.1fms\n",
               t->qos.maxDelayMs, t->qos.maxPacketLossRate,
               t->qos.minReliability, t->qos.deadlineMs);
        printf("    res: bw=%.1fMbps reserve=%d dedicated=%d migratable=%d deps=%d\n",
               t->resource.requiredBandwidthMbps, t->resource.reserveResource,
               t->resource.dedicatedChannel, t->resource.migratable,
               t->constraint.depCount);
    }
}
