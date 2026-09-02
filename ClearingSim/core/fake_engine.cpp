#include "core/fake_engine.h"

#include "engine/clearing_engine.h"

#include <QHash>

#include <algorithm>

// ============================================================
// 引擎外壳（2026-09-02 起接入 B 位真实引擎）
//   本文件是界面的"引擎外壳"：对外接口结构（ClearingResult /
//   PeriodResult / EntityCleared）保持不变，内部已替换为 B 位
//   真实出清引擎 ClearMarket（逐对撮合）+ MCP/PAB 双模式结算。
//   2026-09-02 #70 起：
//   1) 负荷概念退场：每时段需求 = 购电侧申报总量（恒定，不由曲线驱动）
//   2) 新能源滑块直给：平台视角滑块 MW 作为 RENEW 0 价供给段参与
//      撮合（价格接受者，优先中标）；申报表内风/光段不参与撮合
// ============================================================

namespace {

// 时段时间标签（24 时段整点 / 96 时段 15 分钟）
QString timeText(int period, int periodCount)
{
    if (periodCount == 24)
        return QStringLiteral("%1:00").arg(period, 2, 10, QChar('0'));
    const int totalMin = period * 15;
    return QStringLiteral("%1:%2")
        .arg(totalMin / 60, 2, 10, QChar('0'))
        .arg(totalMin % 60, 2, 10, QChar('0'));
}

} // namespace

// ------------------------------------------------------------------
// 一键演示：单时段基准出清（真引擎）
// ------------------------------------------------------------------
ClearingResult FakeEngine::clearBenchmark(const MarketData &market, const QString &mode)
{
    // 基准例无负荷曲线：需求 = 购电侧申报总量
    double demand = 0.0;
    for (const auto &c : market.consumerBids)
        demand += c.quantity;

    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("内置基准例 · 真引擎出清");

    result.periods.append(
        clearOne(market, 1, QStringLiteral("全日"), demand, 0.0, mode));
    return result;
}

// ------------------------------------------------------------------
// 开始仿真：逐时段连续出清（真引擎）
//   负荷退场：每时段需求 = 购电申报总量（恒定）；
//   新能源 = 滑块 MW 直给（每时段相同，作为 0 价段参与撮合）
// ------------------------------------------------------------------
ClearingResult FakeEngine::clearPeriods(const MarketData &market, int periodCount,
                                        double renewSliderMW, const QString &mode)
{
    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("内置场景 · 逐时段连续仿真（真引擎）");

    if (periodCount != 24 && periodCount != 96)
        return result;

    // 每时段总需求 = 购电侧申报总量（负荷概念已退场）
    double demand = 0.0;
    for (const auto &c : market.consumerBids)
        demand += c.quantity;
    if (demand <= 0.0)
        return result;

    for (int period = 1; period <= periodCount; ++period) {
        result.periods.append(
            clearOne(market, period, timeText(period, periodCount),
                     demand, renewSliderMW, mode));
    }
    return result;
}

// ------------------------------------------------------------------
// 单时段出清核心：调用 B 位真引擎 ClearMarket，再聚合为界面结构
// ------------------------------------------------------------------
PeriodResult FakeEngine::clearOne(const MarketData &market, int period,
                                  const QString &time, double demandMW,
                                  double renewSliderMW, const QString &mode)
{
    PeriodResult out;
    out.period = period;
    out.time = time;
    out.loadMW = demandMW;   // 语义：该时段总需求（购电申报总量，负荷概念已退场）

    // ---- 构造真引擎入参：发电侧 ----
    //   供给 = 滑块 RENEW 0 价段（价格接受者，优先中标）
    //        + 申报表常规段（风/光申报段不参与撮合，出力由滑块代表）
    QVector<Generator> generators;
    if (renewSliderMW > 0.0) {
        Generator r;
        r.id = QStringLiteral("RENEW");
        r.name = QStringLiteral("新能源出力");
        r.type = QStringLiteral("NEW");
        r.price = 0.0;
        r.capacity = renewSliderMW;
        r.segment = 0;
        generators.append(r);
    }
    for (const auto &g : market.generatorBids) {
        if (isRenewableType(g.type))
            continue;
        Generator e;
        e.id = g.id;
        e.name = g.name;
        e.type = g.type;
        e.price = g.price;
        e.capacity = g.quantity;
        e.segment = g.segment;
        generators.append(e);
    }

    // ---- 购电侧：申报全量（负荷退场，无缩放） ----
    QVector<Consumer> consumers;
    for (const auto &c : market.consumerBids) {
        Consumer e;
        e.id = c.id;
        e.name = c.name;
        e.price = c.price;
        e.demand = c.quantity;
        e.segment = c.segment;
        consumers.append(e);
    }

    // ---- 调用 B 位真引擎：逐对撮合出清 ----
    const ClearResult cr = ClearMarket(generators, consumers);
    const bool pab = (mode == QStringLiteral("PAB"));

    out.clearingPrice = cr.clearingprice;
    out.clearedMW = cr.totalvolume;

    // ---- 逐段明细：从 Trade 聚合，结算口径与 B 位 settle() 一致 ----
    //   发电侧：MCP 按出清价结算、PAB 按各段申报价结算
    //   购电侧：统一按出清价结算（与 settle() 现行口径一致）
    QHash<QString, EntityCleared> genMap, conMap;
    double renewCleared = 0.0;   // RENEW 段实际消纳量
    for (const auto &t : cr.trade) {
        if (t.volume <= 0.0)
            continue;

        if (t.generatorID == QStringLiteral("RENEW"))
            renewCleared += t.volume;

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
    out.renewMW = renewCleared;   // 新能源实际消纳量（= RENEW 段成交）

    // 展示排序：发电侧按报价升序、购电侧按报价降序
    std::sort(out.genDetails.begin(), out.genDetails.end(),
              [](const EntityCleared &a, const EntityCleared &b) {
                  return a.bidPrice < b.bidPrice;
              });
    std::sort(out.conDetails.begin(), out.conDetails.end(),
              [](const EntityCleared &a, const EntityCleared &b) {
                  return a.bidPrice > b.bidPrice;
              });

    for (const auto &e : out.genDetails)
        out.genFee += e.money;
    for (const auto &e : out.conDetails)
        out.conFee += e.money;

    return out;
}
