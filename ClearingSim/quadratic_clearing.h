#ifndef QUADRATIC_CLEARING_H
#define QUADRATIC_CLEARING_H

#include <QString>
#include <QVector>

// ------------------------------------------------------------------
// 二次曲线出清（选题 2026v2 第 4 页第 (10) 问）
//   发电侧以二次成本曲线 C(P) = aP² + bP + c 申报（替代 10 段量价对），
//   用户侧为固定需求 D（选题给定的简化假设）。
// 定价口径（与 V1.3.1 契约 §7.3「统一边际出清」同构，曲线连续化）：
//   边际成本 MC(P) = 2aP + b；机组视为按 MC 报价，
//   给定电价 λ 的供给 P_i(λ) = clamp((λ − b) / 2a, 0, pMax)；
//   出清价 λ* 满足 Σ P_i(λ*) = D，二分搜索求得。
// 本模块自包含（仅依赖 Qt 核心），不依赖任何本工程其它头文件。
// 来源：组员 fangzhengke 的 quadratic_clearing（2026-09-08），
//   移植时修正：a≈0 除零保护、需求超容量改为稀缺封顶（衔接 V1.3.1）、
//   结果补 ok/shortfall 字段、删除对旧架构 market_runner 的依赖。
// ------------------------------------------------------------------

// 二次成本机组参数（机组物理属性，全天一条曲线，非逐时段申报）
struct QuadraticGenerator
{
    QString id;      // 机组编号（可与分段模式 GeneratorBid 的 id 对齐）
    QString name;    // 电厂名称
    double a = 0.0;  // 二次项系数（P²）
    double b = 0.0;  // 一次项系数（P）
    double c = 0.0;  // 常数项（空载成本，定价不用，进总成本 KPI）
    double pMax = 0.0; // 最大出力（MW）
};

// 单机出清明细
struct QuadraticDispatchItem
{
    QString id;
    QString name;
    double output = 0.0;       // 成交出力（MW）
    double marginalCost = 0.0; // 边际成本 MC = 2aP + b（元/MWh）
};

// 单时段二次出清结果
struct QuadraticClearResult
{
    bool ok = false;            // 出清是否可行（需求可被供给满足）
    double clearingPrice = 0.0; // 统一出清价 λ*（元/MWh）
    double totalVolume = 0.0;   // 火电侧成交总量（MW）
    double shortfall = 0.0;     // 供给缺口（>0 表示需求超 ΣpMax，触发稀缺封顶）
    QVector<QuadraticDispatchItem> dispatch;
};

// 单时段二次曲线出清：
//   generators 二次机组集合；demandMW = 净负荷（负荷 − 新能源，调用方换算）。
//   需求超 ΣpMax 时不再返回空结果，而是 λ 封顶到最高边际成本（稀缺口径），
//   shortfall = 缺口，全部机组顶格。
QuadraticClearResult quadraticClearing(QVector<QuadraticGenerator> generators,
                                       double demandMW);

#endif // QUADRATIC_CLEARING_H
