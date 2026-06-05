# ============================================================
# 天鸿软总线仿真 —— 纯 C 实现
#
# 使用：
#   make           构建主程序 + 所有测试 + 基准
#   make run-ga    构建并跑一次 NSGA-II 主程序
#   make run-tests 跑所有等价性测试（与 Python 参考实现对照过）
#   make bench     跑性能基准
#   make clean     清理
# ============================================================
CC      := gcc
CSTD    := -std=gnu99
WARN    := -Wall -Wextra -Wno-unused-parameter
OPT     := -O2
POSIX   := -D_POSIX_C_SOURCE=199309L
LDLIBS  := -lm
ifeq ($(OS),Windows_NT)
EXEEXT  := .exe
MKDIR_P = powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$1' | Out-Null"
RM_RF   = powershell -NoProfile -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue '$(BIN_DIR)', 'experiment.json'"
else
EXEEXT  :=
MKDIR_P = mkdir -p "$1"
RM_RF   = rm -rf $(BIN_DIR) experiment.json
endif
SRC_DIR := src
APP_DIR := apps
TEST_DIR:= tests
BENCH_DIR:= bench
BIN_DIR := build
CFLAGS  := $(CSTD) $(WARN) $(OPT) $(POSIX) -I$(SRC_DIR)
# 库源文件
LIB_SRC := \
  $(SRC_DIR)/task_data.c \
  $(SRC_DIR)/sb_models.c \
  $(SRC_DIR)/sb_phys_bind.c \
  $(SRC_DIR)/initializer.c \
  $(SRC_DIR)/routing.c \
  $(SRC_DIR)/cost.c \
  $(SRC_DIR)/reliability.c \
  $(SRC_DIR)/connectivity.c \
  $(SRC_DIR)/latency.c \
  $(SRC_DIR)/throughput.c \
  $(SRC_DIR)/config.c \
  $(SRC_DIR)/ga.c \
  $(SRC_DIR)/jsonw.c
LIB_OBJ := $(LIB_SRC:%.c=$(BIN_DIR)/%.o)
# 入口/测试/基准源 -> 可执行
APPS    := $(patsubst $(APP_DIR)/%.c,$(BIN_DIR)/%,$(wildcard $(APP_DIR)/*.c))
TESTS   := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(wildcard $(TEST_DIR)/*.c))
BENCHES := $(patsubst $(BENCH_DIR)/%.c,$(BIN_DIR)/%,$(wildcard $(BENCH_DIR)/*.c))
# Apps / tests / benches -> executables.
APPS    := $(patsubst $(APP_DIR)/%.c,$(BIN_DIR)/%$(EXEEXT),$(wildcard $(APP_DIR)/*.c))
TESTS   := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%$(EXEEXT),$(wildcard $(TEST_DIR)/*.c))
BENCHES := $(patsubst $(BENCH_DIR)/%.c,$(BIN_DIR)/%$(EXEEXT),$(wildcard $(BENCH_DIR)/*.c))
.PHONY: all clean run-ga run-demo run-full run-tests bench
all: $(APPS) $(TESTS) $(BENCHES)
	@echo "================================================================"
	@echo "Build complete. Binaries in $(BIN_DIR)/"
	@echo "  ./build/main_ga       — NSGA-II 拓扑优化（常规演化）"
	@echo "  ./build/main_full     — 完整实验：常规 + 故障注入 + 重建 + JSON 输出"
	@echo "  ./build/main_demo     — 数据模型 / TaskData 演示"
	@echo "  ./build/test_*        — 等价性测试（与 Python 对照）"
	@echo "  ./build/bench_*       — 性能基准"
	@echo "================================================================"
# 通用规则：每个 .c -> 各自的 .o
$(BIN_DIR)/%.o: %.c | $(BIN_DIR)
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@
# apps / tests / benches 各自链接所有库 obj
$(BIN_DIR)/%$(EXEEXT): $(APP_DIR)/%.c $(LIB_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJ) $(LDLIBS) -o $@
$(BIN_DIR)/%$(EXEEXT): $(TEST_DIR)/%.c $(LIB_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJ) $(LDLIBS) -o $@
$(BIN_DIR)/%$(EXEEXT): $(BENCH_DIR)/%.c $(LIB_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJ) $(LDLIBS) -o $@
$(BIN_DIR):
	@$(call MKDIR_P,$(BIN_DIR))
# 便捷目标
run-ga: $(BIN_DIR)/main_ga$(EXEEXT)
	@$(BIN_DIR)/main_ga$(EXEEXT)
run-full: $(BIN_DIR)/main_full$(EXEEXT)
	@$(BIN_DIR)/main_full$(EXEEXT) 42 experiment.json
	@echo ""
	@echo "JSON output: experiment.json"
run-demo: $(BIN_DIR)/main_demo$(EXEEXT)
	@$(BIN_DIR)/main_demo$(EXEEXT)
run-tests: $(TESTS)
	@echo "---- test_initializer ----"
	@$(BIN_DIR)/test_initializer$(EXEEXT)
	@echo "---- test_routing ----"
	@$(BIN_DIR)/test_routing$(EXEEXT)
	@echo "---- test_eval ----"
	@$(BIN_DIR)/test_eval$(EXEEXT)
	@echo "---- test_connectivity ----"
	@$(BIN_DIR)/test_connectivity$(EXEEXT)
	@echo "---- test_latency ----"
	@$(BIN_DIR)/test_latency$(EXEEXT)
bench: $(BENCHES)
	@echo "---- bench_conn ----"
	@$(BIN_DIR)/bench_conn$(EXEEXT)
	@echo "---- bench_lat ----"
	@$(BIN_DIR)/bench_lat$(EXEEXT)
clean:
	$(RM_RF)