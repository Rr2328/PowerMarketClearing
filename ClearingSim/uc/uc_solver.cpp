// ============================================================
// uc_solver.cpp —— SCUC 求解器实现（S2 线性规划 + S3 完整 MILP）
//
// 建模 = 把数学式一行行"抄"成 HiGHS API 调用：
//   addVar(下界, 上界)                        → 一个决策变量
//   changeColCost(变量号, 系数)               → 目标函数里的一项
//   changeColIntegrality(变量号, kInteger)    → 声明 0-1 变量
//   addRow(行下界, 行上界, 非零数, 列号, 系数) → 一条约束
//   run() → getSolution()                     → 最优解（+ LP 的对偶变量）
// ============================================================

#include "uc_solver.h"

#include <highs/Highs.h>

#include <cmath>
#include <limits>

// ============================================================
// S2：线性规划版（全机组运行）
// ============================================================
UcSolution solveUcLp(const QVector<GeneratorMeta> &metas,
                     const QVector<double> &demandMW)
{
    UcSolution sol;
    const int G = metas.size();       // 机组数
    const int T = demandMW.size();    // 时段数
    if (G == 0 || T == 0) {
        sol.message = QStringLiteral("无机组或无需求数据");
        return sol;
    }

    const double kInf = std::numeric_limits<double>::infinity();

    Highs h;
    h.setOptionValue("output_flag", false);   // 关闭求解器日志刷屏

    // ---------- 决策变量 ----------
    // p[g][t]：机组 g 在 t 时段的出力，列号 = g*T + t（二维摊平成一维）。
    // 全部机组运行，出力直接夹在 [pMin, pMax]（约束②，u≡1）。
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t)
            h.addVar(metas[g].pMin, metas[g].pMax);

    // ---------- 目标函数 ----------
    // min Σ_t Σ_g  c_g · p[g,t]
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t)
            h.changeColCost(g * T + t, metas[g].marginalCost);

    // ---------- 约束① 功率平衡 ----------
    // 每时段一条等式：Σ_g p[g,t] = D(t)。
    // 等式约束的对偶变量 λ(t) = "t 时段多 1 MW 需求，总成本增加多少"，
    // 即边际电价/出清价（方案 §4.4 ★）。
    for (int t = 0; t < T; ++t) {
        QVector<HighsInt> cols(G);
        QVector<double> coefs(G, 1.0);
        for (int g = 0; g < G; ++g)
            cols[g] = g * T + t;
        h.addRow(demandMW[t], demandMW[t], G, cols.constData(), coefs.constData());
    }

    // ---------- 求解 ----------
    h.run();
    if (h.getModelStatus() != HighsModelStatus::kOptimal) {
        sol.message = QStringLiteral("LP 无最优解（需求超出全部机组能力范围，或参数矛盾）");
        return sol;
    }

    // ---------- 取结果 ----------
    const HighsSolution &s = h.getSolution();
    sol.p.resize(G);
    sol.u.resize(G);
    for (int g = 0; g < G; ++g) {
        sol.p[g].resize(T);
        sol.u[g].fill(1, T);          // S2 阶段全机组恒开机
        for (int t = 0; t < T; ++t)
            sol.p[g][t] = s.col_value[g * T + t];
    }
    sol.lambda.resize(T);
    for (int t = 0; t < T; ++t)
        sol.lambda[t] = s.row_dual[t];   // 功率平衡行的对偶 = 出清价
    sol.totalCost = h.getObjectiveValue();
    sol.ok = true;
    return sol;
}

// ============================================================
// S3：完整 SCUC MILP
//
// 列布局（每组 G*T 列，机组 g 时段 t 的列号）：
//   p  =      g*T+t        出力 MW，[0, pMax]
//   u  = OFF_U + g*T+t     启停 0-1（整数变量）
//   su = OFF_SU + g*T+t    启动指示 [0,1]（startupCost>0 时恰为"本时段刚启动"）
//   sd = OFF_SD + g*T+t    停机指示 [0,1]
// 行布局：功率平衡最先加（行号 0..T-1，第二阶段取对偶用）。
// ============================================================
UcSolution solveUcMilp(const QVector<GeneratorMeta> &metas,
                       const QVector<double> &demandMW,
                       const UcInitialState &init)
{
    UcSolution sol;
    const int G = metas.size();
    const int T = demandMW.size();
    if (G == 0 || T == 0) {
        sol.message = QStringLiteral("无机组或无需求数据");
        return sol;
    }

    // ---------- 初始状态（缺省 = 全开机、初始出力 = pMin） ----------
    QVector<bool> on0(G);
    QVector<double> p0(G);
    for (int g = 0; g < G; ++g) {
        on0[g] = (g < init.on.size()) ? init.on[g] : true;
        p0[g] = (g < init.p.size()) ? init.p[g] : metas[g].pMin;
        if (on0[g] && p0[g] < metas[g].pMin)
            p0[g] = metas[g].pMin;        // 开机组初始出力不低于最小技术出力
        if (!on0[g])
            p0[g] = 0.0;                  // 停机机组初始出力为 0（爬坡右端不应留 pMin 底数）
    }

    const double kInf = std::numeric_limits<double>::infinity();
    // 失负荷价值（元/MWh）= 平台限价 540：需求超出可开机容量时，
    // 松弛变量 r[t] 放行缺口、λ 封顶 540——与 V1.3.1 稀缺定价同口径，模型恒有解
    const double kShedCost = 540.0;
    const int OFF_U = G * T, OFF_SU = 2 * G * T, OFF_SD = 3 * G * T, OFF_R = 4 * G * T;

    Highs h;
    h.setOptionValue("output_flag", false);
    h.setOptionValue("mip_rel_gap", 0.0);      // 教学规模求解快，直接求到精确最优
    h.setOptionValue("mip_abs_gap", 1e-6);

    // ---------- 决策变量 ----------
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t)
            h.addVar(0.0, metas[g].pMax);                              // p
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t) {
            h.addVar(0.0, 1.0);                                        // u（0-1）
            h.changeColIntegrality(OFF_U + g * T + t, HighsVarType::kInteger);
        }
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t)
            h.addVar(0.0, 1.0);                                        // su（连续）
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t)
            h.addVar(0.0, 1.0);                                        // sd（连续）
    for (int t = 0; t < T; ++t)
        h.addVar(0.0, kInf);                                           // r[t] 失负荷松弛
    for (int t = 0; t < T; ++t)
        h.changeColCost(OFF_R + t, kShedCost);                         // 失负荷惩罚 = 限价 540

    // ---------- 目标函数 ----------
    // min Σ  c_g·p + noLoadCost_g·u + startupCost_g·su
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t) {
            h.changeColCost(g * T + t, metas[g].marginalCost);
            h.changeColCost(OFF_U + g * T + t, metas[g].noLoadCost);
            h.changeColCost(OFF_SU + g * T + t, metas[g].startupCost);
        }

    // ---------- 约束① 功率平衡（先加：行号 = t，第二阶段对偶取这里） ----------
    // Σ_g p[g,t] + r[t] = D(t)：缺口由失负荷松弛 r 补足（成本 540 = 限价），
    // 需求超容量时 λ 封顶 540（对偶被松弛成本钉住），模型恒有解
    for (int t = 0; t < T; ++t) {
        QVector<HighsInt> cols(G + 1);
        QVector<double> coefs(G + 1, 1.0);
        for (int g = 0; g < G; ++g)
            cols[g] = g * T + t;
        cols[G] = OFF_R + t;
        h.addRow(demandMW[t], demandMW[t], G + 1, cols.constData(), coefs.constData());
    }

    // ---------- 约束② 最小技术出力：pMin·u ≤ p ≤ pMax·u ----------
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t) {
            const HighsInt c[2] = { HighsInt(g * T + t), HighsInt(OFF_U + g * T + t) };
            const double coMin[2] = { 1.0, -metas[g].pMin };
            h.addRow(0.0, kInf, 2, c, coMin);          // p − pMin·u ≥ 0
            const double coMax[2] = { 1.0, -metas[g].pMax };
            h.addRow(-kInf, 0.0, 2, c, coMax);         // p − pMax·u ≤ 0
        }

    // ---------- 约束③ 爬坡 ----------
    // 上爬坡：p[t] − p[t−1] ≤ rampUp·u[t−1] + pMax·su[t]
    //   （u[t−1]=1 运行时限速；刚启动 su=1 放宽到 pMax；停机侧自动满足）
    // 下爬坡：p[t−1] − p[t] ≤ rampDown·u[t] + pMax·sd[t]
    //   （对称：运行时限速；刚停机 sd=1 允许直接降到 0）
    // t=0 没有前一时刻，用初始状态 on0/p0 代替（常量并到行上下界里）。
    // ramp ≤ 0 视为"不限速"→ 限速值取 pMax（约束自动失效）。
    for (int g = 0; g < G; ++g) {
        const double upLim = metas[g].rampUp > 0.0 ? metas[g].rampUp : metas[g].pMax;
        const double dnLim = metas[g].rampDown > 0.0 ? metas[g].rampDown : metas[g].pMax;
        for (int t = 0; t < T; ++t) {
            {   // 上爬坡
                QVector<HighsInt> cols;
                QVector<double> coefs;
                double rhs = 0.0;
                cols << g * T + t; coefs << 1.0;                       // p[t]
                if (t > 0) {
                    cols << g * T + (t - 1); coefs << -1.0;            // p[t−1]
                    cols << OFF_U + g * T + (t - 1); coefs << -upLim;  // u[t−1]
                } else {
                    rhs += p0[g];                                      // 初始出力 → 右侧
                    rhs += upLim * (on0[g] ? 1.0 : 0.0);               // 初始状态 → 右侧
                }
                cols << OFF_SU + g * T + t; coefs << -metas[g].pMax;   // su[t]
                h.addRow(-kInf, rhs, cols.size(), cols.constData(), coefs.constData());
            }
            {   // 下爬坡
                QVector<HighsInt> cols;
                QVector<double> coefs;
                double rhs = 0.0;
                if (t > 0) {
                    cols << g * T + (t - 1); coefs << 1.0;             // p[t−1]
                } else {
                    rhs += -p0[g];                                     // p[−1] = 初始出力 → 右侧
                }
                cols << g * T + t; coefs << -1.0;                      // p[t]
                cols << OFF_U + g * T + t; coefs << -dnLim;            // u[t]
                cols << OFF_SD + g * T + t; coefs << -metas[g].pMax;   // sd[t]
                h.addRow(-kInf, rhs, cols.size(), cols.constData(), coefs.constData());
            }
        }
    }

    // ---------- 约束④ 启动/停机指示 ----------
    // su[t] ≥ u[t] − u[t−1]（刚启动 → su=1；startupCost>0 → su 不会虚增）
    // su[t] ≤ u[t]（只有开着的机组才谈得上"本时段启动"）
    // sd[t] ≥ u[t−1] − u[t] 与 sd[t] ≤ 1 − u[t]（停机指示对称）
    for (int g = 0; g < G; ++g)
        for (int t = 0; t < T; ++t) {
            const int su = OFF_SU + g * T + t;
            const int sd = OFF_SD + g * T + t;
            const int ut = OFF_U + g * T + t;
            {   // su[t] − u[t] (+ u[t−1]) ≥ 0，t=0 右端 −on0
                QVector<HighsInt> cols { HighsInt(su), HighsInt(ut) };
                QVector<double> coefs { 1.0, -1.0 };
                double lower = 0.0;
                if (t > 0) {
                    cols << OFF_U + g * T + (t - 1); coefs << 1.0;
                } else {
                    lower = -(on0[g] ? 1.0 : 0.0);
                }
                h.addRow(lower, kInf, cols.size(), cols.constData(), coefs.constData());
            }
            {   // su[t] − u[t] ≤ 0
                const HighsInt c[2] = { HighsInt(su), HighsInt(ut) };
                const double v[2] = { 1.0, -1.0 };
                h.addRow(-kInf, 0.0, 2, c, v);
            }
            {   // sd[t] + u[t] (− u[t−1]) ≥ 0，t=0 下界 on0
                QVector<HighsInt> cols { HighsInt(sd), HighsInt(ut) };
                QVector<double> coefs { 1.0, 1.0 };
                double lower = 0.0;
                if (t > 0) {
                    cols << OFF_U + g * T + (t - 1); coefs << -1.0;
                } else {
                    lower = on0[g] ? 1.0 : 0.0;
                }
                h.addRow(lower, kInf, cols.size(), cols.constData(), coefs.constData());
            }
            {   // sd[t] + u[t] ≤ 1
                const HighsInt c[2] = { HighsInt(sd), HighsInt(ut) };
                const double v[2] = { 1.0, 1.0 };
                h.addRow(-kInf, 1.0, 2, c, v);
            }
            {   // 收紧：su[t] ≤ 1 − u[t−1]（上一期开着就不存在"启动"）
                //   → 运行中的机组 su 恒为 0，爬坡约束不被"伪造启动"钻空子
                double upper = 1.0;
                if (t > 0) {
                    const HighsInt c[2] = { HighsInt(su), HighsInt(OFF_U + g * T + (t - 1)) };
                    const double v[2] = { 1.0, 1.0 };
                    h.addRow(-kInf, upper, 2, c, v);
                } else {
                    upper -= on0[g] ? 1.0 : 0.0;   // t=0：初始状态是常量
                    const HighsInt c[1] = { HighsInt(su) };
                    const double v[1] = { 1.0 };
                    h.addRow(-kInf, upper, 1, c, v);
                }
            }
            {   // 收紧：sd[t] ≤ u[t−1]（上一期停着就不存在"停机"）
                if (t > 0) {
                    const HighsInt c[2] = { HighsInt(sd), HighsInt(OFF_U + g * T + (t - 1)) };
                    const double v[2] = { 1.0, -1.0 };
                    h.addRow(-kInf, 0.0, 2, c, v);
                } else {
                    const HighsInt c[1] = { HighsInt(sd) };
                    const double v[1] = { 1.0 };
                    h.addRow(-kInf, on0[g] ? 1.0 : 0.0, 1, c, v);
                }
            }
        }

    // ---------- 约束⑤ 最小开机时间（Rajan–Takriti）：Σ_{k=t−UT+1..t} su[k] ≤ u[t] ----------
    // 含义：最近 UT 个时段内只要启动过一次，本时段必须还开着。
    for (int g = 0; g < G; ++g) {
        const int UT = metas[g].minUpTime;
        if (UT <= 1)
            continue;                     // 1 = 不限制
        for (int t = 0; t < T; ++t) {
            QVector<HighsInt> cols;
            QVector<double> coefs;
            for (int k = qMax(0, t - UT + 1); k <= t; ++k) {
                cols << OFF_SU + g * T + k; coefs << 1.0;
            }
            cols << OFF_U + g * T + t; coefs << -1.0;
            h.addRow(-kInf, 0.0, cols.size(), cols.constData(), coefs.constData());
        }
    }

    // ---------- 约束⑥ 最小停机时间：Σ_{k=t−DT+1..t} sd[k] + u[t] ≤ 1 ----------
    // 含义：最近 DT 个时段内只要停过一次，本时段必须还停着。
    for (int g = 0; g < G; ++g) {
        const int DT = metas[g].minDownTime;
        if (DT <= 1)
            continue;
        for (int t = 0; t < T; ++t) {
            QVector<HighsInt> cols;
            QVector<double> coefs;
            for (int k = qMax(0, t - DT + 1); k <= t; ++k) {
                cols << OFF_SD + g * T + k; coefs << 1.0;
            }
            cols << OFF_U + g * T + t; coefs << 1.0;
            h.addRow(-kInf, 1.0, cols.size(), cols.constData(), coefs.constData());
        }
    }

    // ---------- 第一阶段：解 MILP，定开停机 ----------
    h.run();
    const HighsModelStatus st = h.getModelStatus();
    if (st != HighsModelStatus::kOptimal) {
        sol.message = QStringLiteral("MILP 无最优解（需求超出可开机容量，或约束参数矛盾）[%1]")
                          .arg(QString::fromLatin1(h.modelStatusToString(st)));
        return sol;
    }
    sol.totalCost = h.getObjectiveValue();      // MILP 总成本（电量+空载+启动）

    // 读出整数启停解（浮点 → 四舍五入）与出力基点
    const HighsSolution &mip = h.getSolution();
    QVector<QVector<double>> pStar(G);
    sol.u.resize(G);
    for (int g = 0; g < G; ++g) {
        sol.u[g].resize(T);
        pStar[g].resize(T);
        for (int t = 0; t < T; ++t) {
            sol.u[g][t] = int(std::lround(mip.col_value[OFF_U + g * T + t]));
            pStar[g][t] = mip.col_value[g * T + t];
        }
    }

    // ---------- 第二阶段：逐时段经济调度定价 ----------
    // 开停机 u 与出力基点 p*（MILP 解）全部冻结：本时段出力被限制在
    // [基点−下爬坡, 基点+上爬坡] ∩ [pMin·u, pMax·u] 箱型边界内——
    // 刚启动的机组不受爬坡限制（与 MILP 里 su 放宽爬坡一致），
    // 停机机组出力为 0。每时段一个独立小 LP：谁便宜谁发电，
    // 功率平衡行的对偶 = 出清价 λ(t)。基点被爬坡卡住时，
    // λ 自动跳到更贵机组的报价（"UC 定开停、ED 定价格"，且无跨期对偶退化）。
    Highs h2;
    h2.setOptionValue("output_flag", false);
    // 列布局：每时段 (G+1) 列——机组 col = t*(G+1)+g，失负荷松弛 col = t*(G+1)+G
    const int G1 = G + 1;
    QVector<double> boxLo(T * G), boxHi(T * G);   // 记录箱型边界（λ 定价用）
    for (int t = 0; t < T; ++t) {
        for (int g = 0; g < G; ++g) {
            double lo = 0.0, hi = 0.0;
            if (sol.u[g][t] == 1) {
                const bool wasOff = (t > 0) ? (sol.u[g][t - 1] == 0) : !on0[g];
                if (wasOff) {
                    lo = metas[g].pMin;            // 刚启动：不受爬坡限制
                    hi = metas[g].pMax;
                } else {
                    const double base = (t > 0) ? pStar[g][t - 1] : p0[g];
                    const double upLim =
                        metas[g].rampUp > 0.0 ? metas[g].rampUp : metas[g].pMax;
                    const double dnLim =
                        metas[g].rampDown > 0.0 ? metas[g].rampDown : metas[g].pMax;
                    lo = qMax(metas[g].pMin, base - dnLim);
                    hi = qMin(metas[g].pMax, base + upLim);
                }
            }
            boxLo[t * G + g] = lo;
            boxHi[t * G + g] = hi;
            h2.addVar(lo, hi);
            h2.changeColCost(t * G1 + g, metas[g].marginalCost);
        }
        // 该时段的失负荷松弛
        h2.addVar(0.0, kInf);
        h2.changeColCost(t * G1 + G, kShedCost);
        QVector<HighsInt> cols(G1);
        QVector<double> coefs(G1, 1.0);
        for (int g = 0; g < G; ++g)
            cols[g] = t * G1 + g;
        cols[G] = t * G1 + G;
        h2.addRow(demandMW[t], demandMW[t], G1, cols.constData(), coefs.constData());
    }
    h2.run();
    if (h2.getModelStatus() == HighsModelStatus::kOptimal) {
        const HighsSolution &ed = h2.getSolution();
        sol.p.resize(G);
        for (int g = 0; g < G; ++g) {
            sol.p[g].resize(T);
            for (int t = 0; t < T; ++t)
                sol.p[g][t] = ed.col_value[t * G1 + g];
        }
        // λ 定价（统一边际出清口径）：
        //   失负荷时段 = 失负荷价值 540（V1.3.1 稀缺封顶）；
        //   有机组出力处于箱型边界内点（lo < p < hi）→ 边际机组 = 内点中最贵的；
        //   全部被调机组都钉在边界（如恰好压 pMin）→ 取被调机组中最便宜的。
        // 不直接取行对偶的原因：机组出力被边界钉住时对偶退化（多个 λ 同为最优，
        // 求解器选择不可控）；边际机组报价是唯一稳定且与分段撮合同构的口径。
        sol.lambda.resize(T);
        sol.shed.resize(T);
        const double kTol = 1e-4;
        for (int t = 0; t < T; ++t) {
            sol.shed[t] = ed.col_value[t * G1 + G];   // 失负荷缺口（稀缺时段 > 0）
            if (sol.shed[t] > 0.5) {
                sol.lambda[t] = kShedCost;
                continue;
            }
            double interiorMax = 0.0, pinnedMin = kInf;
            bool hasInterior = false, hasDispatch = false;
            for (int g = 0; g < G; ++g) {
                const double p = sol.p[g][t];
                if (p <= 0.5)
                    continue;
                hasDispatch = true;
                if (p > boxLo[t * G + g] + kTol && p < boxHi[t * G + g] - kTol) {
                    hasInterior = true;
                    interiorMax = std::max(interiorMax, metas[g].marginalCost);
                } else {
                    pinnedMin = std::min(pinnedMin, metas[g].marginalCost);
                }
            }
            sol.lambda[t] = hasInterior ? interiorMax
                            : hasDispatch ? pinnedMin
                                          : 0.0;   // 无机组被调且无缺口 → 该时段无需求
        }
    } else {
        // 理论上不会发生（MILP 解本身满足所有箱型边界）；防御性兜底
        sol.lambda.resize(T);
        sol.lambda.fill(0.0);
        sol.message = QStringLiteral("第二阶段经济调度未收敛（请检查机组参数）");
    }

    sol.ok = true;
    return sol;
}
