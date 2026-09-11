#include "core/clearing_facade.h"

#include "engine/clearing_engine.h"
#include "../uc/uc_solver.h"

#include <QHash>
#include <QDebug>

#include <algorithm>

// ============================================================
// 引擎外壳（2026-09-02 起接入 B 位真实引擎）
//   本文件是界面的"引擎外壳"：对外接口结构（ClearingResult /
//   PeriodResult / EntityCleared）保持不变，内部已替换为 B 位
//   真实出清引擎 ClearMarket（逐对撮合）+ MCP/PAB 双模式结算。
//   新能源以 0 价供给段参与撮合（价格接受者，优先中标）。
// ============================================================

namespace {

// 指定时段的购电申报总量（该时段需求）
//   period <= 0 的申报（手工构造的旧行为数据）视为全天恒量，计入每个时段
double totalDemandAt(const MarketData &market, int period)
{
    double sum = 0.0;
    for (const auto &c : market.consumerBids) {
        if (c.period == period || c.period <= 0)
            sum += c.quantity;
    }
    return sum;
}

// 负荷曲线在指定时段的取值；无该时段数据返回 -1
double loadAt(const MarketData &market, int period)
{
    for (const auto &l : market.loadCurve) {
        if (l.period == period)
            return l.load;
    }
    return -1.0;
}

} // namespace

// ------------------------------------------------------------------
// 渗透率换算（契约 §5.3，B 与 C 用同一式）：
//   P_re(t) = 渗透率 × 负荷(t)；无负荷曲线时回退 购电申报总量(t)。
//   注：形状曲线只决定各新能源机组之间的分配，进入撮合的 0 价 RENEW
//   供给段总量恒为 P_re(t)，故此处无需逐机组拆分。
// ------------------------------------------------------------------
double ClearingFacade::renewCapacityAt(const MarketData &market, int period,
                                       double penetration)
{
    if (penetration <= 0.0)
        return 0.0;

    const double base = loadAt(market, period);
    const double demand = (base > 0.0) ? base : totalDemandAt(market, period);
    return penetration * demand;
}

// ------------------------------------------------------------------
// 一键演示：单时段基准出清（真引擎）
// ------------------------------------------------------------------
ClearingResult ClearingFacade::clearBenchmark(const MarketData &market, const QString &mode)
{
    // 基准例（窄表）已展开为 96 期同量同价：取时段 1 的申报，等价于旧单时段行为
    const double demand = totalDemandAt(market, 1);

    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("内置基准例 · 真引擎出清");

    result.periods.append(
        clearOne(market, 1, QStringLiteral("全日"), demand, 0.0, 1.0, mode));
    return result;
}

// ------------------------------------------------------------------
// 开始仿真：连续出清（真引擎）
//   V1.3（#67/#88）：引擎内部恒跑 96 期——逐时段取该时段申报撮合，
//   新能源 P_re(t) = 渗透率 × 负荷(t)（0 价优先中标，契约 §5.3）；
//   periodCount=24 时按契约 §7.2 聚合为小时视图（价 = 4 期均值，
//   电量/出力 = 均值（= 小时 MWh/h 口径），费用 = 求和，明细不聚合
//   ——取该小时出清价最高的 15 分钟截面）。
// ------------------------------------------------------------------
ClearingResult ClearingFacade::clearPeriods(const MarketData &market, int periodCount,
                                        double penetration, const QString &mode)
{
    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("连续仿真 · 真引擎出清");

    double anyDemand = 0.0;
    for (const auto &c : market.consumerBids)
        anyDemand += c.quantity;
    if (anyDemand <= 0.0)
        return result;

    // 第一步：恒跑 96 期原始出清
    QVector<PeriodResult> raw;
    raw.reserve(96);
    for (int period = 1; period <= 96; ++period) {
        const QString time = periodTime(period, 96);
        const double demand = totalDemandAt(market, period);
        const double renewCap = renewCapacityAt(market, period, penetration);
        raw.append(
            clearOne(market, period, time, demand, renewCap, 1.0, mode));
    }

    // 第二步：96 期直出 / 24 期聚合视图
    if (periodCount == 24) {
        for (int hour = 1; hour <= 24; ++hour) {
            PeriodResult agg;
            agg.period = hour;
            agg.time = periodTime(hour, 24);

            int peakIdx = -1;              // 该小时出清价最高的 15 分钟截面
            double priceSum = 0.0, loadSum = 0.0, renewSum = 0.0, volSum = 0.0;
            for (int q = 0; q < 4; ++q) {
                const PeriodResult &pr = raw[(hour - 1) * 4 + q];
                priceSum += pr.clearingPrice;
                loadSum += pr.loadMW;
                renewSum += pr.renewMW;
                volSum += pr.clearedMW;
                agg.genFee += pr.genFee;
                agg.conFee += pr.conFee;
                if (peakIdx < 0 || pr.clearingPrice > raw[(hour - 1) * 4 + peakIdx].clearingPrice)
                    peakIdx = q;
            }
            agg.clearingPrice = priceSum / 4.0;
            agg.loadMW = loadSum / 4.0;      // 小时平均功率（= MWh/h）
            agg.renewMW = renewSum / 4.0;
            agg.clearedMW = volSum / 4.0;
            agg.genDetails = raw[(hour - 1) * 4 + peakIdx].genDetails;
            agg.conDetails = raw[(hour - 1) * 4 + peakIdx].conDetails;
            result.periods.append(agg);
        }
    } else {
        result.periods = raw;
    }
    return result;
}

// 时段标签：24 时段 "01:00"~"24:00"；96 时段 "00:15"~"24:00"
QString ClearingFacade::periodTime(int period, int periodCount)
{
    if (periodCount == 96) {
        const int totalMinutes = period * 15;
        if (totalMinutes == 24 * 60)
            return QStringLiteral("24:00");
        return QStringLiteral("%1:%2")
            .arg(totalMinutes / 60, 2, 10, QChar('0'))
            .arg(totalMinutes % 60, 2, 10, QChar('0'));
    }
    if (period == 24)
        return QStringLiteral("24:00");
    return QStringLiteral("%1:00").arg(period, 2, 10, QChar('0'));
}

// ------------------------------------------------------------------
// 单时段出清核心：调用 B 位真引擎 ClearMarket，再聚合为界面结构
// ------------------------------------------------------------------
PeriodResult ClearingFacade::clearOne(const MarketData &market, int period,
                                  const QString &time, double demandMW, double renewMW,
                                  double scale, const QString &mode)
{
    PeriodResult out;
    out.period = period;
    out.time = time;
    out.loadMW = demandMW;
    out.renewMW = renewMW;

    // ---- 构造真引擎入参：发电侧（V1.3：只取该时段的申报） ----
    //   新能源 = 渗透率换算出的 RENEW 0 价供给段（价格接受者，优先中标）；
    //   申报表内已无风/光申报行（D4），无需类型过滤。
    QVector<Generator> generators;
    if (renewMW > 0.0) {
        Generator r;
        r.id = QStringLiteral("RENEW");
        r.name = QStringLiteral("新能源出力");
        r.type = QStringLiteral("NEW");
        r.price = 0.0;
        r.capacity = renewMW;
        r.segment = 0;
        generators.append(r);
    }
    for (const auto &g : market.generatorBids) {
        if (g.period > 0 && g.period != period)
            continue;   // 逐时段申报：只取当前时段
        if (g.quantity <= 0.0)
            continue;   // 0 量段 = 停机申报（规则⑥），不进撮合——
                        // 否则会以 0 成交量刷新引擎的边际价指针
        Generator e;
        e.id = g.id;
        e.name = g.name;
        e.price = g.price;
        e.capacity = g.quantity * scale;
        e.segment = g.segment;
        generators.append(e);
    }

    // ---- 购电侧：只取该时段的申报（0 量同样不进撮合） ----
    QVector<Consumer> consumers;
    for (const auto &c : market.consumerBids) {
        if (c.period > 0 && c.period != period)
            continue;
        if (c.quantity <= 0.0)
            continue;
        Consumer e;
        e.id = c.id;
        e.name = c.name;
        e.price = c.price;
        e.demand = c.quantity * scale;
        e.segment = c.segment;
        consumers.append(e);
    }

    // ---- 调用 B 位真引擎：逐对撮合出清 ----
    const ClearResult cr = ClearMarket(generators, consumers);
    const bool pab = (mode == QStringLiteral("PAB"));

    out.clearingPrice = cr.clearingprice;
    out.clearedMW = cr.totalvolume;

    // ---- 逐段明细：从 Trade 聚合，结算口径自管（C 接口 EntityCleared） ----
    //   发电侧：MCP 按出清价结算、PAB 按各段申报价结算
    //   购电侧：统一按出清价结算
    //   （B 位 settle()/SettlementItem 已在 #94 清理）
    QHash<QString, EntityCleared> genMap, conMap;
    for (const auto &t : cr.trade) {
        if (t.volume <= 0.0)
            continue;

        const QString gk = t.generatorID + QLatin1Char('#')
                           + QString::number(t.generatorseg);
        EntityCleared &ge = genMap[gk];
        ge.id = t.generatorID;
        ge.segment = t.generatorseg;
        ge.bidPrice = t.generatorprice;
        ge.clearedMW += t.volume;
        ge.money += pab ? t.volume * t.generatorprice
                        : t.volume * cr.clearingprice;

        const QString ck = t.consumerID + QLatin1Char('#')
                           + QString::number(t.consumerseg);
        EntityCleared &ce = conMap[ck];
        ce.id = t.consumerID;
        ce.segment = t.consumerseg;
        ce.bidPrice = t.consumerprice;
        ce.clearedMW += t.volume;
        ce.money += t.volume * cr.clearingprice;
    }

    // 补充主体名称（Trade 不带 name，从申报数据回填）
    for (auto &e : genMap) {
        if (e.id == QStringLiteral("RENEW")) {
            e.name = QStringLiteral("新能源出力");
            continue;
        }
        for (const auto &g : market.generatorBids) {
            if (g.id == e.id) {
                e.name = g.name;
                break;
            }
        }
    }
    for (auto &e : conMap) {
        for (const auto &c : market.consumerBids) {
            if (c.id == e.id) {
                e.name = c.name;
                break;
            }
        }
    }

    out.genDetails = genMap.values();
    out.conDetails = conMap.values();

    // 展示排序：发电侧按报价升序、购电侧按报价降序
    std::sort(out.genDetails.begin(), out.genDetails.end(),
              [](const EntityCleared &a, const EntityCleared &b) {
                  return a.bidPrice < b.bidPrice;
              });
    std::sort(out.conDetails.begin(), out.conDetails.end(),
              [](const EntityCleared &a, const EntityCleared &b) {
                  return a.bidPrice > b.bidPrice;
              });

    // 新能源实际消纳量（0 价供给段若未被全部吸收，则以成交为准）
    double renewActual = 0.0;
    for (const auto &e : out.genDetails) {
        if (e.id == QStringLiteral("RENEW"))
            renewActual += e.clearedMW;
    }
    out.renewMW = renewActual;

    for (const auto &e : out.genDetails)
        out.genFee += e.money;
    for (const auto &e : out.conDetails)
        out.conFee += e.money;

    return out;
}

// ------------------------------------------------------------------
// 二次曲线模式（选题 2026v2 (10) 问）
//   发电侧 C(P)=aP²+bP+c 连续申报，用户侧固定需求（负荷口径，选题简化）。
//   与分段模式共用渗透率换算（契约 §5.3 同式）：
//     P_re(t) = 渗透率 × 负荷(t)（无曲线回退购电申报总量），
//     净负荷 D(t) = max(0, 负荷(t) − P_re(t)) → 二分出清 λ*。
//   结算口径（MCP 同构）：统一出清价结算——
//     发电侧收入 = λ* × P_i，新能源 0 价段收入 = λ* × P_re，
//     购电侧按申报量占比分摊 λ* × 负荷(t)。
//   需求超 ΣpMax → 稀缺封顶（衔接 V1.3.1）：λ 顶到最高边际成本 + shortfall。
// ------------------------------------------------------------------
ClearingResult ClearingFacade::clearPeriodsQuadratic(const MarketData &market,
                                                     int periodCount, double penetration)
{
    ClearingResult result;
    result.mode = QStringLiteral("QUAD");
    result.sourceName = QStringLiteral("二次曲线出清 · 二分边际定价");

    if (market.quadraticGens.isEmpty())
        return result; // 模式未启用（未提供 generator_quadratic.csv）

    // 96 期逐时段原始出清
    QVector<PeriodResult> raw;
    raw.reserve(96);
    for (int period = 1; period <= 96; ++period) {
        // V1.3.3：需求优先取购电申报总量（P1 逐时段可编辑——改总量即移动 λ*）；
        //   无申报数据时回退负荷曲线。渗透率换算仍以负荷曲线为基准（契约第五章）
        double load = totalDemandAt(market, period);
        if (load <= 0.0)
            load = loadAt(market, period);

        const double renewCap = renewCapacityAt(market, period, penetration);
        const double renewActual = std::min(renewCap, std::max(0.0, load));
        const double netLoad = std::max(0.0, load - renewActual);

        const QuadraticClearResult qr =
            quadraticClearing(market.quadraticGens, netLoad);

        PeriodResult out;
        out.period = period;
        out.time = periodTime(period, 96);
        out.loadMW = load;
        out.renewMW = renewActual;
        out.clearingPrice = qr.clearingPrice;
        out.clearedMW = renewActual + qr.totalVolume;

        // 发电侧明细：新能源 0 价段 + 各二次机组
        if (renewActual > 0.0) {
            EntityCleared re;
            re.id = QStringLiteral("RENEW");
            re.name = QStringLiteral("新能源出力");
            re.segment = 0;
            re.bidPrice = 0.0;
            re.clearedMW = renewActual;
            re.money = renewActual * qr.clearingPrice;
            out.genDetails.append(re);
        }
        for (const auto &d : qr.dispatch) {
            EntityCleared e;
            e.id = d.id;
            e.name = d.name;
            e.segment = 1;
            e.bidPrice = d.marginalCost;   // 边际成本申报口径
            e.clearedMW = d.output;
            e.money = d.output * qr.clearingPrice;
            out.genDetails.append(e);
        }

        // 购电侧：固定需求按申报量占比分摊
        double conSum = 0.0;
        for (const auto &c : market.consumerBids) {
            if (c.period == period || c.period <= 0)
                conSum += c.quantity;
        }
        if (conSum > 0.0) {
            for (const auto &c : market.consumerBids) {
                if (!(c.period == period || c.period <= 0))
                    continue;
                EntityCleared e;
                e.id = c.id;
                e.name = c.name;
                e.segment = c.segment;
                e.bidPrice = qr.clearingPrice;   // 统一出清价结算
                e.clearedMW = load * (c.quantity / conSum);
                e.money = e.clearedMW * qr.clearingPrice;
                out.conDetails.append(e);
            }
        }

        for (const auto &e : out.genDetails)
            out.genFee += e.money;
        for (const auto &e : out.conDetails)
            out.conFee += e.money;

        raw.append(out);
    }

    // 24 期聚合（口径与 clearPeriods 完全一致）
    if (periodCount == 24) {
        for (int hour = 1; hour <= 24; ++hour) {
            PeriodResult agg;
            agg.period = hour;
            agg.time = periodTime(hour, 24);

            int peakIdx = -1;
            double priceSum = 0.0, loadSum = 0.0, renewSum = 0.0, volSum = 0.0;
            for (int q = 0; q < 4; ++q) {
                const PeriodResult &pr = raw[(hour - 1) * 4 + q];
                priceSum += pr.clearingPrice;
                loadSum += pr.loadMW;
                renewSum += pr.renewMW;
                volSum += pr.clearedMW;
                agg.genFee += pr.genFee;
                agg.conFee += pr.conFee;
                if (peakIdx < 0 || pr.clearingPrice > raw[(hour - 1) * 4 + peakIdx].clearingPrice)
                    peakIdx = q;
            }
            agg.clearingPrice = priceSum / 4.0;
            agg.loadMW = loadSum / 4.0;
            agg.renewMW = renewSum / 4.0;
            agg.clearedMW = volSum / 4.0;
            agg.genDetails = raw[(hour - 1) * 4 + peakIdx].genDetails;
            agg.conDetails = raw[(hour - 1) * 4 + peakIdx].conDetails;
            result.periods.append(agg);
        }
    } else {
        result.periods = raw;
    }
    return result;
}

// ------------------------------------------------------------------
// SCUC 机组组合模式（求解器方案 S4，HiGHS MILP）
//   发电侧以 generator_meta.csv 的线性成本参与（marginalCost 元/MWh），
//   一次求解全天 96 期：开停机 u、出力 p（pMin 基础量 / 爬坡 /
//   最小开停机时间 / 启动费都在 MILP 内）。
//   出清价 = 第二阶段逐时段经济调度的功率平衡对偶 λ(t)（uc_solver.h 注释）。
//   需求口径与二次模式一致（V1.3.3）：Σ购电申报量(t) 优先、负荷曲线回退；
//   渗透率换算同式（契约 §5.3）。结算口径 MCP 同构：全部中标电量按 λ 结算。
// ------------------------------------------------------------------
ClearingResult ClearingFacade::clearPeriodsUc(const MarketData &market,
                                              int periodCount, double penetration)
{
    ClearingResult result;
    result.mode = QStringLiteral("UC");
    result.sourceName = QStringLiteral("SCUC 机组组合 · 求解器边际定价");

    if (market.generatorMeta.isEmpty())
        return result; // 模式未启用（未提供 generator_meta.csv）

    // 96 期净负荷向量（求解器一次解全天——爬坡/最小开停机是跨期约束）
    QVector<double> demand;
    QVector<double> renewOf;      // 各期新能源消纳（结果聚合用）
    QVector<double> loadOf;
    demand.reserve(96);
    renewOf.reserve(96);
    loadOf.reserve(96);
    for (int period = 1; period <= 96; ++period) {
        double load = totalDemandAt(market, period);
        if (load <= 0.0)
            load = loadAt(market, period);
        const double renewCap = renewCapacityAt(market, period, penetration);
        const double renewActual = std::min(renewCap, std::max(0.0, load));
        demand.append(std::max(0.0, load - renewActual));
        renewOf.append(renewActual);
        loadOf.append(load);
    }

    const UcSolution s = solveUcMilp(market.generatorMeta, demand);
    if (!s.ok) {
        // 诊断日志：定位应用内求解失败原因（测试同数据可行，需对比入参）
        double dmin = std::numeric_limits<double>::max(), dmax = -dmin, dsum = 0.0;
        for (double v : demand) { dmin = std::min(dmin, v); dmax = std::max(dmax, v); dsum += v; }
        double pMaxSum = 0.0, pMinSum = 0.0;
        for (const auto &g : market.generatorMeta) { pMaxSum += g.pMax; pMinSum += g.pMin; }
        qWarning() << "[UC-DIAG] solve failed:" << s.message
                   << "units =" << market.generatorMeta.size()
                   << "demand min/max/sum =" << dmin << dmax << dsum
                   << "pMinSum/pMaxSum =" << pMinSum << pMaxSum
                   << "genBids =" << market.generatorBids.size()
                   << "conBids =" << market.consumerBids.size()
                   << "loadCurve =" << market.loadCurve.size()
                   << "penetration =" << penetration;
        result.sourceName = QStringLiteral("SCUC 求解失败：%1").arg(s.message);
        return result; // periods 为空，界面显示空态
    }

    // 逐期聚合为 PeriodResult（口径与二次模式一致）
    QVector<PeriodResult> raw;
    raw.reserve(96);
    for (int period = 1; period <= 96; ++period) {
        const int t = period - 1;
        const double lam = s.lambda[t];
        double genSum = 0.0;
        for (int g = 0; g < market.generatorMeta.size(); ++g)
            genSum += s.p[g][t];

        PeriodResult out;
        out.period = period;
        out.time = periodTime(period, 96);
        out.loadMW = loadOf[t];
        out.renewMW = renewOf[t];
        out.clearingPrice = lam;
        out.clearedMW = renewOf[t] + genSum;

        // 发电侧明细：新能源 0 价段 + 各机组（按 λ 统一结算，MCP 同构）
        if (renewOf[t] > 0.0) {
            EntityCleared re;
            re.id = QStringLiteral("RENEW");
            re.name = QStringLiteral("新能源出力");
            re.segment = 0;
            re.bidPrice = 0.0;
            re.clearedMW = renewOf[t];
            re.money = renewOf[t] * lam;
            out.genDetails.append(re);
        }
        for (int g = 0; g < market.generatorMeta.size(); ++g) {
            const auto &m = market.generatorMeta[g];
            EntityCleared e;
            e.id = m.id;
            e.name = m.name;
            e.segment = 0;
            e.bidPrice = m.marginalCost;   // 电量成本申报口径
            e.clearedMW = s.p[g][t];
            e.money = s.p[g][t] * lam;
            out.genDetails.append(e);
        }

        // 购电侧：按申报量占比分摊（与二次模式同式）
        double conSum = 0.0;
        for (const auto &c : market.consumerBids) {
            if (c.period == period || c.period <= 0)
                conSum += c.quantity;
        }
        if (conSum > 0.0) {
            for (const auto &c : market.consumerBids) {
                if (!(c.period == period || c.period <= 0))
                    continue;
                EntityCleared e;
                e.id = c.id;
                e.name = c.name;
                e.segment = c.segment;
                e.bidPrice = lam;              // 统一出清价结算
                e.clearedMW = loadOf[t] * (c.quantity / conSum);
                e.money = e.clearedMW * lam;
                out.conDetails.append(e);
            }
        }

        for (const auto &e : out.genDetails)
            out.genFee += e.money;
        for (const auto &e : out.conDetails)
            out.conFee += e.money;

        raw.append(out);
    }

    // UC 汇总 KPI：启动次数 / 启动成本（初始状态 = 全部开机，故 t=0 不计启动）
    for (int g = 0; g < market.generatorMeta.size(); ++g) {
        for (int t = 1; t < 96; ++t) {
            if (s.u[g][t] == 1 && s.u[g][t - 1] == 0) {
                result.startupCount += 1;
                result.startupCostTotal += market.generatorMeta[g].startupCost;
            }
        }
        for (int t = 0; t < 96; ++t)
            result.noLoadCostTotal += market.generatorMeta[g].noLoadCost * s.u[g][t];
    }
    result.totalCost = s.totalCost;

    // 稀缺提示：需求超出可开机容量的时段由失负荷松弛放行、λ 封顶 540
    // （V1.3.1 同口径）；最大缺口写进数据源描述，P2/P3 可见
    double maxShed = 0.0;
    for (double v : s.shed)
        maxShed = std::max(maxShed, v);
    if (maxShed > 0.5)
        result.sourceName += QStringLiteral(" · 峰时段缺供 %1 MW（稀缺出清价 540）")
                                 .arg(maxShed, 0, 'f', 1);

    // 24 期聚合（口径与 clearPeriods / clearPeriodsQuadratic 完全一致）
    if (periodCount == 24) {
        for (int hour = 1; hour <= 24; ++hour) {
            PeriodResult agg;
            agg.period = hour;
            agg.time = periodTime(hour, 24);

            int peakIdx = -1;
            double priceSum = 0.0, loadSum = 0.0, renewSum = 0.0, volSum = 0.0;
            for (int q = 0; q < 4; ++q) {
                const PeriodResult &pr = raw[(hour - 1) * 4 + q];
                priceSum += pr.clearingPrice;
                loadSum += pr.loadMW;
                renewSum += pr.renewMW;
                volSum += pr.clearedMW;
                agg.genFee += pr.genFee;
                agg.conFee += pr.conFee;
                if (peakIdx < 0 || pr.clearingPrice > raw[(hour - 1) * 4 + peakIdx].clearingPrice)
                    peakIdx = q;
            }
            agg.clearingPrice = priceSum / 4.0;
            agg.loadMW = loadSum / 4.0;
            agg.renewMW = renewSum / 4.0;
            agg.clearedMW = volSum / 4.0;
            agg.genDetails = raw[(hour - 1) * 4 + peakIdx].genDetails;
            agg.conDetails = raw[(hour - 1) * 4 + peakIdx].conDetails;
            result.periods.append(agg);
        }
    } else {
        result.periods = raw;
    }
    return result;
}
