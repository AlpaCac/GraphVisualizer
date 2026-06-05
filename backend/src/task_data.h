/* ============================================================
 * task_data.h —— 表2 任务数据结构（纯C）
 * ============================================================ */
#ifndef TASK_DATA_H
#define TASK_DATA_H

#include <stdint.h>
#include <stddef.h>

#define TD_STR_LEN        64
#define TD_DESC_LEN       256
#define TD_MAX_DST        16
#define TD_MAX_DEPS       8
#define TD_MAX_CONFLICTS  8
#define TD_MAX_TASKS      256

/* ---------- 枚举 ---------- */
typedef enum {
    TASK_STATUS_REPORT = 0,
    TASK_CONTROL_CMD,
    TASK_SENSOR_COLLECT,
    TASK_FILE_TRANSFER,
    TASK_COOPERATIVE,
    TASK_FAULT_RECOVERY,
    TASK_TYPE_UNKNOWN
} TaskType;

typedef enum {
    COMM_P2P = 0,
    COMM_P2MP,
    COMM_CONVERGE,
    COMM_CROSS_DOMAIN
} CommMode;

typedef enum {
    PRIO_LOW = 0,
    PRIO_NORMAL = 1,
    PRIO_HIGH = 2,
    PRIO_CRITICAL = 3
} TaskPriority;

typedef struct {
    char dependsOnTaskId[TD_STR_LEN];
    int  isHardDependency;
} TaskDependency;

typedef struct {
    char     taskId[TD_STR_LEN];
    char     taskName[TD_STR_LEN];
    TaskType taskType;
    char     description[TD_DESC_LEN];
    char     sceneId[TD_STR_LEN];
    double   startTime;
    double   endTime;
    double   duration;
    int      isCritical;
} TaskBasicInfo;

typedef struct {
    char     srcNodeId[TD_STR_LEN];
    char     dstNodeIds[TD_MAX_DST][TD_STR_LEN];
    int      dstCount;
    CommMode commMode;
    uint32_t packetSizeBytes;
    double   sendFrequencyHz;
    uint64_t totalDataBytes;
    double   serviceDuration;
    uint32_t concurrentConnections;
    int      isMulticastOrConverge;
    int      isPeriodic;
    int      isBurst;
} TaskCommRequirement;

typedef struct {
    double maxDelayMs;
    double maxJitterMs;
    double minThroughputMbps;
    double maxPacketLossRate;
    double minReliability;
    double deadlineMs;
    double minCompletionRate;
    double maxRecoveryTimeMs;
    int    continuityRequired;
} TaskQosRequirement;

typedef struct {
    double requiredCompute;
    double requiredStorage;
    double requiredBandwidthMbps;
    double requiredCacheMB;
    int    reserveResource;
    int    dedicatedChannel;
    int    migratable;
    int    needBackupResource;
} TaskResourceRequirement;

typedef struct {
    TaskPriority   priority;
    int            importanceLevel;
    int            preemptive;
    int            allowDegrade;
    int            allowDelay;
    int            allowInterrupt;
    TaskDependency dependencies[TD_MAX_DEPS];
    int            depCount;
    char           conflictTaskIds[TD_MAX_CONFLICTS][TD_STR_LEN];
    int            conflictCount;
} TaskPriorityConstraint;

typedef struct {
    TaskBasicInfo           basic;
    TaskCommRequirement     comm;
    TaskQosRequirement      qos;
    TaskResourceRequirement resource;
    TaskPriorityConstraint  constraint;
} TaskData;

typedef struct {
    TaskData tasks[TD_MAX_TASKS];
    int      count;
} TaskSet;

void task_data_init(TaskData *t);

TaskType     task_type_from_str(const char *s);
const char  *task_type_to_str(TaskType v);
CommMode     comm_mode_from_str(const char *s);
const char  *comm_mode_to_str(CommMode v);
TaskPriority priority_from_str(const char *s);
const char  *priority_to_str(TaskPriority v);

/* ---------- 加载接口 ----------
 * 返回 0 成功，<0 失败。
 *   JSON: 顶层是一个任务对象数组 [ {...}, {...} ]
 *   CSV : 第一行表头，每行一个任务（列表字段用 ';' 分隔）
 */
int task_load_json_file  (const char *path, TaskSet *out);
int task_load_json_string(const char *json, TaskSet *out);
int task_load_csv_file   (const char *path, TaskSet *out);

void task_set_print(const TaskSet *set);

#endif
