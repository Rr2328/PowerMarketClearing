// ============================================================
// uc_solver.h —— SCUC 机组组合求解器（方案 §四/§五）
//
// 三种出清模式之一：分段撮合 / 二次曲线 / 求解器 SCUC。
// 数学模型（S3 起完整）：
//   决策变量  u[g,t]∈{0,1} 启停；p[g,t]≥0 出力；su/sd[g,t]∈[0,1] 启动/停机指示
//   目标      min Σ c_g·p[g,t] + noLoadCost_g·u[g,t] + startupCost_g·su[g,t]
//   约束      ① Σ_g p[g,t] + r[t] = D(t)      功率平衡（对偶 = 出清价 λ；r = 失负荷
//                                             松弛，成本 540 元/MWh——需求超出可开
//                                             机容量时 λ 封顶 540，与 V1.3.1 稀缺
//                                             定价、分段引擎封顶行为完全一致，
//                                             模型恒有解、界面不会白屏）
//             ② pMin·u ≤ p ≤ pMax·u          最小技术出力（"启动了就有基础量"）
//             ③ 爬坡                          相邻时段出力限速（0 = 不限制 → 取 pMax）
//             ④ su/sd 指示                    su ≥ u[t]−u[t−1]，恰在启停时 =1
//             ⑤⑥ 最小连续开/停机时间          Σ最近 UT 期 su ≤ u[t]（Rajan–Takriti）
//
// 两个求解入口：
//   solveUcLp    S2 线性规划版（全机组运行，无启停）——用于对拍验证 λ=边际报价；
//   solveUcMilp  S3 完整 MILP 版——真正决定开停机。
//   λ 的取法（教学点）：MILP 本身没有对偶变量。先解 MILP 定开停机与出力基点，
//   再逐时段做"箱型边界内"的经济调度：本时段出力限制在
//   [基点−下爬坡, 基点+上爬坡] ∩ [pMin·u, pMax·u]，功率平衡行的对偶 = 出清价。
//   一句话："UC 定开停、ED 定价格"；爬坡卡住基点时 λ 跳到更贵机组的报价。
// ============================================================

#pragma once

#include <QString>
#include <QVector>

// 机组技术经济参数（S4 起由可选文件 generator_meta.csv 读入）
struct GeneratorMeta
{
    QString name;
    QString id;
    double pMin = 0.0;           // 最小技术出力 MW（老师说的"启动了就有个基础的量"）
    double pMax = 0.0;           // 额定容量 MW
    double rampUp = 0.0;         // 上爬坡限速 MW/时段（≤0 视为不限制）
    double rampDown = 0.0;       // 下爬坡限速 MW/时段（≤0 视为不限制）
    int minUpTime = 1;           // 最小连续开机时段数（1 = 不限制）
    int minDownTime = 1;         // 最小连续停机时段数（1 = 不限制）
    double startupCost = 0.0;    // 每次启动费用 元
    double noLoadCost = 0.0;     // 空载费用 元/时段
    double marginalCost = 0.0;   // 电量成本 元/MWh（线性成本）
};

// 求解的初始状态（可选；缺省 = 全部机组开机、初始出力 = pMin）
struct UcInitialState
{
    QVector<bool> on;     // g < on.size() 才生效
    QVector<double> p;    // 同上；开机机组的初始出力会夹到 ≥ pMin
};

// SCUC 求解结果
struct UcSolution
{
    bool ok = false;
    QString message;

    QVector<QVector<double>> p;   // p[g][t] 出力计划 MW（取自冻结后的 ED 解）
    QVector<QVector<int>> u;      // u[g][t] 启停状态 0/1
    QVector<double> lambda;       // λ(t)：出清价 元/MWh = 本时段被调度的最贵机组
                                  //   边际成本（统一边际出清，与分段撮合同构）；
                                  //   稀缺失负荷时段 = 540（V1.3.1 稀缺封顶）
    QVector<double> shed;         // shed(t)：失负荷 MW（需求超出可开机容量时的缺口，
                                  //   此时 λ = 失负荷价值 540 元/MWh，与 V1.3.1 稀缺封顶同口径）
    double totalCost = 0.0;       // 总成本 元（电量 + 空载 + 启动 + 失负荷惩罚）
};

// S2：线性规划版——全部机组运行、无启停/爬坡约束。
//   用途：验证「Σp=D 等式约束的对偶变量 = 边际机组报价」，
//   即统一边际出清的正统数学口径（方案 §4.4 ★）。
UcSolution solveUcLp(const QVector<GeneratorMeta> &metas,
                     const QVector<double> &demandMW);

// S3：完整 SCUC MILP——0-1 启停 + 爬坡 + 启动费用 + 最小开/停机时间。
UcSolution solveUcMilp(const QVector<GeneratorMeta> &metas,
                       const QVector<double> &demandMW,
                       const UcInitialState &init = {});
