#include "core/fake_engine.h"

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

// 购电侧申报总量（该时段需求）
double totalDemand(const MarketData &market)
{
    double sum = 0.0;
    for (const auto &c : market.consumerBids)
        sum += c.quantity;
    return sum;
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
        clearOne(market, 1, QStringLiteral("全日"), demand, 0.0, 1.0, mode));
    return result;
}

// ------------------------------------------------------------------
// 开始仿真：连续出清（真引擎）
//   2026-09-02 设计口径更新：负荷概念退场，平台与负荷无关。
//   每时段需求 = 购电侧申报总量（恒定；待 96 时段宽表申报后按时段变化），
//   新能源出力 = P2 滑块直给 MW（价格接受者，0 价优先中标）。
// ------------------------------------------------------------------
ClearingResult FakeEngine::clearPeriods(const MarketData &market, int periodCount,
                                        double renewMW, const QString &mode)
{
    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("连续仿真 · 真引擎出清");

    const double demand = totalDemand(market);
    if (demand <= 0.0)
        return result;

    for (int period = 1; period <= periodCount; ++period) {
        const QString time = periodTime(period, periodCount);
        result.periods.append(
            clearOne(market, period, time, demand, renewMW, 1.0, mode));
    }
    return result;
}

// 时段标签：24 时段 "01:00"~"24:00"；96 时段 "00:15"~"24:00"
QString FakeEngine::periodTime(int period, int periodCount)
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
PeriodResult FakeEngine::clearOne(const MarketData &market, int period,
                                  const QString &time, double demandMW, double renewMW,
                                  double scale, const QString &mode)
{
    PeriodResult out;
    out.period = period;
    out.time = time;
    out.loadMW = demandMW;
    out.renewMW = renewMW;

    // ---- 构造真引擎入参：发电侧 ----
    //   新能源 = P2 滑块直给 RENEW 0 价供给段（价格接受者，优先中标）；
    //   申报表内风电/光伏机组仅为申报展示，不参与撮合（isRenewableType 过滤）。
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
        if (isRenewableType(g.type))
            continue;   // 新能源由滑块直给，申报段不进撮合
        Generator e;
        e.id = g.id;
        e.name = g.name;
        e.type = g.type;
        e.price = g.price;
        e.capacity = g.quantity * scale;
        e.segment = g.segment;
        generators.append(e);
    }

    // ---- 购电侧：按契约 3.4 缩放 ----
    QVector<Consumer> consumers;
    for (const auto &c : market.consumerBids) {
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

    // ---- 逐段明细：从 Trade 聚合，结算口径与 B 位 settle() 一致 ----
    //   发电侧：MCP 按出清价结算、PAB 按各段申报价结算
    //   购电侧：统一按出清价结算（与 settle() 现行口径一致）
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
