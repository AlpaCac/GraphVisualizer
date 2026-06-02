# 安装说明

## 1. 核心依赖库编译与安装 (OGDF)

由于 C++ 多线程与 ABI 的严格限制，**必须使用 Qt 附带的 MinGW 工具链**从源码编译 OGDF 静态库。

### 1.1 准备 Qt 编译器环境

1. 找到 Qt 自带编译器的 `bin` 目录，例如：`C:\Qt\Tools\mingw1120_64\bin`。

2. 打开全新的 PowerShell 终端，临时将该路径置于系统环境变量首位：

   PowerShell

   ```
   $env:PATH = "C:\Qt\Tools\mingw1120_64\bin;" + $env:PATH
   ```

3. 验证编译器版本：

   PowerShell

   ```
   gcc -v
   ```

### 1.2 编译并安装 OGDF

1. 进入 OGDF 源码根目录

2. 执行 CMake 配置，指定生成的构建系统与安装路径（假设安装到 `F:/env/ogdf_install`）：

   PowerShell

   ```
   cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="F:/env/ogdf_install" .
   ```

3. 执行多核编译与安装：

   PowerShell

   ```
   mingw32-make -j8
   mingw32-make install
   ```

   安装完成后，`F:/env/ogdf_install` 目录下应包含 `include` 和 `lib` 文件夹。

## 2. 项目导入与编译 (Qt Creator)

### 2.1 导入项目

1. 打开 Qt Creator，选择 **File (文件) -> Open File or Project (打开文件或项目)**。
2. 选中本项目根目录下的 `CMakeLists.txt` 文件。

### 2.2 配置构建套件 (Kit)

1. 在配置项目 (Configure Project) 界面，**取消勾选所有 MSVC 套件**。
2. **仅勾选** `Desktop Qt 6.x.x MinGW 64-bit`。
3. 点击右下角的 **Configure (配置)** 按钮。

### 2.3 确认 CMake 配置

确保 `CMakeLists.txt` 中指定的 OGDF 路径与实际安装路径一致：

CMake

```
# 若上一阶段 OGDF 安装路径不同，请在此处修改
list(APPEND CMAKE_PREFIX_PATH "F:/env/ogdf_install")

target_include_directories(GraphVisualizer PRIVATE
    "F:/env/ogdf_install/include"
    "F:/env/ogdf_install/include/ogdf-release"
)
```

### 2.4 构建项目

1. 在 Qt Creator 底部栏，将构建模式从 `Debug` 切换为 **`Release`**。
2. 点击顶部菜单栏：**Build (构建) -> Run CMake (运行 CMake)**。
3. 点击左下角的 🔨 (构建按钮) 或按 `Ctrl+B` 完成项目编译。

## 3. 运行与交互说明

编译成功后，点击左下角的 ▶️ (运行按钮) 即可启动程序。

### 交互操作指南

- **平移画布:** 在画面任意空白处按住 **鼠标左键** 并拖动。
- **无极缩放:** 将鼠标悬停在需要查看的区域，滚动 **鼠标滚轮** 进行中心点缩放。
- **图元拖拽:** 按住单个节点可自由拖动，关联的连线会自动跟随重绘。
- **动态自适应:** 程序内部运行独立的拓扑更新线程，数据发生变化时，画面会自动维持当前的视域不跳转，仅在图元内部完成坐标刷新。

# 接口说明（test.cpp）

## 1. 拓扑图构建接口 (全量刷新流)

当后端需要改变图的结构（增加/删除节点或连线）时，推荐使用“清空 -> 重绘 -> 重新布局”的全量刷新工作流。由于 OGDF 底层采用了极速的 C++ 算法，全量刷新在视觉上也是瞬间完成的。

### 1.1 清空画布

- **接口定义:** `void requestClear()`

- **功能说明:** 瞬间销毁当前画布上的所有节点和连线，清空内存字典，为接收新的拓扑数据做准备。

- **调用示例:**

  C++

  ```
  emit requestClear();
  ```

### 1.2 注入节点

- **接口定义:** `void requestAddNode(const QString& id, int type, const QString& label)`

- **功能说明:** 向内存中注入一个新的节点。此时节点尚未显示，等待布局引擎分配坐标。

- **参数说明:**

  - `id` (QString): **核心主键**。节点的唯一标识符（如 IP 地址、MAC、设备号）。绝不能重复。
  - `type` (int): 初始节点颜色预设类型（0为浅蓝色，1为浅橙色）。
  - `label` (QString): 节点上显示的文本文字，支持用 `\n` 换行。

- **调用示例:**

  C++

  ```
  emit requestAddNode("Router_A", 0, "核心路由器\n192.168.1.1");
  ```

### 1.3 注入连线

- **接口定义:** `void requestAddEdge(const QString& src, const QString& dst, int type)`

- **功能说明:** 在两个已存在的节点之间建立拓扑连接。

- **参数说明:**

  - `src` (QString): 起点节点的 `id`。
  - `dst` (QString): 终点节点的 `id`。
  - `type` (int): 连线的初始类型预设。

- **注意事项:** 必须确保 `src` 和 `dst` 对应的节点已经被 `requestAddNode` 注入，否则该连线会被静默丢弃。

- **调用示例:**

  C++

  ```
  emit requestAddEdge("Router_A", "Switch_B", 0);
  ```

### 1.4 触发引擎布局

- **接口定义:** `void requestLayout()`

- **功能说明:** 通知前端：“数据传输完毕，请调用 OGDF 引擎计算坐标并渲染”。

- **行为特点:** 这是整个框架中最耗时的计算操作。在引擎计算完毕前，用户界面不会发生突变。

- **调用示例:**

  C++

  ```
  emit requestLayout();
  ```

## 2. 状态高频刷新接口 (无损热更新)

针对无需改变网络拓扑结构，只需表达**业务状态变化**（如：设备宕机、链路拥堵、数据流向动画）的场景，绝对**不要**调用 `requestLayout`，请直接使用以下极速热更新接口。

### 2.1 修改节点样式 (设备状态/异构展示)

- **接口定义:** `void requestUpdateNodeStyle(const QString& id, const QColor& color, int shape)`

- **功能说明:** 以极低的性能消耗，瞬间改变指定节点的背景颜色和几何形状，完美适配设备状态指示与设备类型区分。

- **参数说明:**

  - `id` (QString): 目标节点的唯一标识符 (`id`)。
  - `color` (QColor): 目标颜色。必须使用 `QColor()` 包装（如 `QColor(Qt::red)` 或 `QColor(255, 100, 100)`）。
  - `shape` (int): 节点几何形状的枚举值。
    - `0`: **圆形** (常用于普通节点、状态灯、终端设备)
    - `1`: **正方形** (常用于服务器、数据库节点)
    - `2`: **菱形** (常用于告警节点、网关、决策节点)
    - `3`: **六边形** (常用于核心节点、安全设备、集群)

- **调用示例:**

  C++

  ```
  // 模拟核心路由器宕机告警：变成红色菱形
  emit requestUpdateNodeStyle("Router_A", QColor(255, 100, 100), 2);
  
  // 模拟节点恢复健康：变成浅绿色六边形
  emit requestUpdateNodeStyle("Router_A", QColor(150, 255, 150), 3);
  ```

### 2.2 修改连线样式 (实时报警/链路追踪)

- **接口定义:** `void requestUpdateEdgeStyle(const QString& src, const QString& dst, const QColor& color, int thickness, int style)`

- **功能说明:** 瞬间改变画面上某一条指定连线的颜色、粗细和线型。

- **参数说明:**

  - `src` (QString): 起点节点的 `id`。
  - `dst` (QString): 终点节点的 `id`。
  - `color` (QColor): 目标颜色。
  - `thickness` (int): 连线的粗细（像素）。常规为 2，报警建议设为 4 或 5。
  - `style` (int): 线条的样式枚举值。
    - `1`: **实线** (`Qt::SolidLine`)
    - `2`: **虚线** (`Qt::DashLine`)
    - `3`: **点状线** (`Qt::DotLine`)

- **调用示例:**

  C++

  ```
  // 模拟链路过载：将连线设为红色、粗细为4、虚线
  emit requestUpdateEdgeStyle("Router_A", "Switch_B", QColor(Qt::red), 4, 2);
  ```

## 3. 典型业务场景调用时序

### 场景 A：网络拓扑结构发生物理变化（设备增删）

必须严格按照以下顺序发送信号，确保数据与视图一致性：

C++

```
// 1. 抹除旧世界
emit requestClear();

// 2. 发送全量最新节点
emit requestAddNode("N1", 0, "Server");
emit requestAddNode("N2", 1, "Client");

// 3. 发送全量最新连线
emit requestAddEdge("N1", "N2", 0);

// 4. 通知渲染引擎排版
emit requestLayout();
```

### 场景 B：拓扑不变，仅触发网络风暴告警（高频动画）

此场景下，**跳过布局计算**，直接高频发送视图修改指令组合：

C++

```
// 收到告警信号，设备节点变异为红色菱形
emit requestUpdateNodeStyle("N1", QColor(255, 100, 100), 2);

// 同时，该设备发出的链路变为红色粗虚线
emit requestUpdateEdgeStyle("N1", "N2", QColor(Qt::red), 5, 2);

// 1秒后报警解除，恢复原状（浅蓝圆形 + 灰色细实线）
emit requestUpdateNodeStyle("N1", QColor(200, 230, 255), 0);
emit requestUpdateEdgeStyle("N1", "N2", QColor(Qt::gray), 2, 1);
```

