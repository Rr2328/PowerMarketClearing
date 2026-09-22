# 电力现货市场出清仿真平台（ClearingSim）

> 东南大学《程序设计语言工程》课程设计项目 · 集思广ee小组 · C++17 / Qt 6 · Git + GitHub · [数据契约 V1.3](docs/data-contract.md)

## 一、项目简介

本项目是一个**电力现货市场日前出清教学仿真平台**。它以发电侧、购电侧的双侧申报为输入，完成分时段出清、结算与结果展示；支持切换出清方式、调节新能源渗透率，并从发电侧、购电侧和平台方三个视角观察结果。本项目用于规则学习与实验演示，不用于真实市场交易或结算。

**核心功能：**

- **数据管理**：解析 CSV 申报数据（96 时段原生粒度长表，兼容旧窄表自动展开），内置逐项数据校验与错误提示
- **三条出清路径**：分段报价撮合；基于机组二次成本曲线和净负荷求解出力及统一价格；基于 HiGHS 混合整数规划的 UC 机组组合，求解启停与出力计划
- **结算对比**：分段撮合支持统一边际价格（MCP）与按报价结算（PAB）；二次曲线和 UC 路径不提供 PAB 切换
- **渗透率模拟**：新能源渗透率 0–100% 可调（默认 20%），P_re(t)=渗透率×负荷(t)，观察稀缺定价触发（出清价封顶 1500 元/MWh）
- **结果可视化**：供需曲线、出清电价曲线（24/96 时段双粒度）、结算明细表、UC 启停甘特图、稀缺时段标注
- **数据导出**：按当前视角导出结算日报、分时电价曲线；UC 路径还可导出机组启停与出力计划（CSV）

## 二、代码结构与完成人

下表按**模块主责与集成职责**整理，便于从代码位置直接找到对应完成人；它不是逐行 Git 作者统计。跨模块接口和最终联调由成员协作完成。

| 代码位置 | 主要内容 | 主责 / 协作 |
| --- | --- | --- |
| `ClearingSim/data/`：`csv_utils`、`data_reader`、`data_validator`、`scenario_manager` 及数据测试 | CSV 解析、长/窄表兼容、数据校验、96→24 时段场景处理 | **陈美伊**主责；刘偲燃负责主界面接线与集成验证 |
| `ClearingSim/engine/`：`clearing_engine`、`quadratic_clearing` | 分段报价撮合、MCP/PAB 结算、二次成本曲线求解 | **方郑锞**完成基础撮合与二次曲线初版；**刘偲燃**完成模式接入、后续修正与测试对拍 |
| `ClearingSim/uc/`：`uc_solver` | UC 机组组合建模、HiGHS 求解器调用及相关测试 | **方郑锞**参与求解方案与工具调研；**刘偲燃**完成当前仓库中的求解器接入、测试及界面结果联动 |
| `ClearingSim/core/clearing_facade.{h,cpp}` | 统一出清入口、模式分派及结果结构 | **刘偲燃**负责接口聚合与模式联调；**方郑锞**参与算法接口修改与检查 |
| `ClearingSim/core/`：`app_session`、`perspective`、`market_view`；`ClearingSim/main.cpp`、`mainwindow.{h,cpp}` | 实验状态、三视角、Qt 页面与图表、结果导出 | **刘偲燃**主责；陈美伊、方郑锞分别对接数据和算法接口 |
| `ClearingSim/CMakeLists.txt`、各模块 `*_test.cpp` | 构建与 CTest 接入、模块测试及端到端对拍 | **陈美伊**完成数据模块部分测试；**刘偲燃**负责 CTest 接入及跨模块、UC 等测试集成；算法用例由成员协作维护 |

`data/samples/` 的场景与样例由小组共同维护；[数据契约](docs/data-contract.md)用于统一跨模块输入输出。`third_party/HiGHS/` 是第三方代码，**不属于成员原创实现**。其他分工见下文“协作记录”。

## 三、构建、运行与验证

**环境**：Qt 6.5+（Qt Widgets、Qt Charts，建议安装 Qt Creator）、CMake ≥ 3.19，以及与所选 Qt 套件匹配的 C++ 编译器。小组开发环境为 **Qt 6.8.3 + MinGW**。Windows 下如需让 CTest 自动补齐 Qt/编译器运行库路径，建议使用 CMake ≥ 3.22。

1. 获取完整仓库（包括 `third_party/HiGHS/`）：`git clone https://github.com/Rr2328/PowerMarketClearing.git`；如果使用老师提供的代码压缩包，直接解压即可。
2. 在 Qt Creator 中打开仓库内的 `ClearingSim/CMakeLists.txt`，选择已安装的 Qt 6 套件并构建 `ClearingSim`。HiGHS 随仓库源码构建，无需另外下载；首次构建可能较慢。
3. 运行程序，点击“**一键演示**”体验内置基准例，或在数据导入页选择 [`data/samples/scenario/`](data/samples/scenario/) 中的教学场景。样例的文件用途和预期结果见 [样例说明](data/samples/README.md)。
4. 在 **Qt Creator 显示的实际构建目录**运行测试，不要假定构建目录固定在源码树内：

   ```text
   ctest --output-on-failure
   ```

   CMake 注册了 8 项 CTest 测试，覆盖数据读取、场景、出清、二次曲线、UC 求解及集成链路。基准例的对拍预期为：出清价 **250 元/MWh**、成交量 **50 MWh**、按 MCP 结算 **12,500 元**。测试是否通过应以本机运行输出为准。

## 四、技术栈与第三方依赖

- **语言与界面**：C++17、Qt 6 Widgets、Qt Charts
- **构建与测试**：CMake、CTest；项目构建入口为 `ClearingSim/CMakeLists.txt`
- **数据**：CSV（样例文件为 UTF-8 带 BOM）；字段和校验口径见 [数据契约](docs/data-contract.md)

| 依赖 | 版本 | 许可证 | 引入方式与用途 |
| --- | --- | --- | --- |
| [HiGHS](https://github.com/ERGO-Code/HiGHS) | v1.15.1 | [MIT](third_party/HiGHS/LICENSE.txt) | 源码置于 `third_party/HiGHS/`，通过 CMake 编译；用于 UC 机组组合的线性/混合整数规划求解 |

HiGHS 是第三方开源代码，**不计入成员原创代码**；依据 Git 文件或提交量统计个人贡献时，应排除 `third_party/HiGHS/`。

## 五、目录结构

```text
├── ClearingSim/
│   ├── CMakeLists.txt         # 构建与测试入口
│   ├── main.cpp               # 程序入口
│   ├── mainwindow.{h,cpp}     # Qt 主窗口与交互
│   ├── core/                  # 实验状态、三视角、出清外观接口与结果模型
│   ├── data/                  # CSV 读取、校验与场景管理
│   ├── engine/                # 分段撮合与二次曲线求解
│   └── uc/                    # UC 机组组合求解
├── data/samples/              # 基准例、教学场景与曲线数据
├── docs/                      # 数据契约、现实性对照与 UML 类图
├── third_party/HiGHS/         # 第三方求解器及其许可证
└── README.md
```

测试源码与对应模块放在同一目录；具体文件与主责人见第二节。

## 六、模型边界与补充资料

- **UC 命名**：程序界面和部分源码沿用“SCUC”字样，但当前求解模型主要包含功率平衡、机组出力上下限、爬坡、启动成本及最小连续开停机等约束，**尚未建立输电网络与线路安全约束**。因此在文档中称“UC 机组组合”更准确，不应把它解释为完整的真实市场 SCUC。
- **数据与结果**：内置数据是可复现的教学样例，供需和新能源情景用于观察规则及算法行为；不代表真实市场数据或结算口径。CSV 字段与校验规则见[数据契约](docs/data-contract.md)，样例生成及预期结果见[样例说明](data/samples/README.md)。
- **静态结构**：[UML 类图（PNG）](docs/UML-类图.png)；另提供[矢量图](docs/UML-类图.svg)和[PlantUML 源文件](docs/UML-类图.puml)。

## 七、协作记录

三位成员共同完成需求与功能设计、工作日志和汇报准备。除第二节的代码主责外，陈美伊负责电力市场规则和数据资料调研，方郑锞研究并向组内介绍 GitHub、SourceTree 与求解器的使用，刘偲燃负责数据契约、模块联调、结题 PPT、演示视频和现场汇报。

开发过程中使用模块 `feature` 分支隔离工作，以[数据契约](docs/data-contract.md)统一接口；阶段集成与 Pull Request 审查后收口到 `main`。Git 历史中也有直接的阶段合并，因此不将“所有修改均通过 PR”作为绝对规则。

主要里程碑：2026-08-31 完成开题汇报；2026-09-02 确立 V1.3 数据契约；2026-09-13～14 完成数据、出清、UC 模块的集成；2026-09-15 完成模式互斥与界面联调，并将 8 项测试接入 CTest。最终测试状态请以提交版本的实际测试输出为准。
