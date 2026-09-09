#include "core/clearing_facade.h"

#include "engine/clearing_engine.h"

#include <QHash>

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
