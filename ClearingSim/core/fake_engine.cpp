#include "core/fake_engine.h"

#include <algorithm>
#include <limits>

// ============================================================
// 假引擎实现说明（教学口径，可向老师解释）
//   1) 新能源优先中标：该时段新能源出力先等额抵扣负荷，剩余为净负荷
//   2) 发电侧按报价升序聚合，逐段吸收净负荷，最后一个吸收到的段报价=出清价
//   3) 购电侧按报价降序聚合，与发电侧同一成交总量撮合（A 已校验双侧电量平衡）
//   4) MCP：全场按出清价结算；PAB：各段按自己报价结算
//   B 位真实引擎完成后，替换本文件内部实现即可（接口结构不变）
// ============================================================

namespace {

// 撮合用的段记录（排序后逐段吸收）
struct Segment
{
    double qty = 0.0;      // 该段申报电量 × 缩放系数
    double price = 0.0;    // 该段申报价
    QString id;
    QString name;
    int seg = 0;
};

// 该时段新能源出力汇总（新能源优先中标，报 0 价全部吸收）
double sumRenewable(const QVector<RenewableOutput> &renewables)
{
    double sum = 0.0;
    for (const auto &r : renewables)
        sum += r.output;
    return sum;
}

} // namespace

// ------------------------------------------------------------------
// 一键演示：单时段基准对拍
//   预期：出清价 200 元/MWh，成交 140 MWh，发电侧 28000 元 + 购电侧 28000 元
// ------------------------------------------------------------------
ClearingResult FakeEngine::clearBenchmark(const MarketData &market, const QString &mode)
{
    // 基准例无负荷曲线：需求 = 购电侧申报总量（A 已校验其 = 负荷总电量）
    double demand = 0.0;
    for (const auto &c : market.consumerBids)
        demand += c.quantity;

    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("内置基准例（对拍锚点 200/140/56000）");

    result.periods.append(
        clearOne(market, 1, QStringLiteral("全日"), demand, 0.0, 1.0, mode));
    return result;
}

// ------------------------------------------------------------------
// 开始仿真：逐时段连续出清（24/96 时段）
//   申报按「该时段负荷 ÷ 当日总量」缩放（对齐数据契约 3.4 口径）
// ------------------------------------------------------------------
ClearingResult FakeEngine::clearPeriods(const QVector<PeriodScenario> &scenarios,
                                        const MarketData &market, const QString &mode)
{
    ClearingResult result;
    result.mode = mode;
    result.sourceName = QStringLiteral("内置场景 · 逐时段连续仿真");

    double loadTotal = 0.0;
    for (const auto &s : scenarios)
        loadTotal += s.loadMW;
    if (loadTotal <= 0.0)
        return result;

    for (const auto &s : scenarios) {
        const double scale = s.loadMW / loadTotal;          // 契约 3.4 缩放
        const double renew = sumRenewable(s.renewableBase); // 新能源优先
        const double net = s.loadMW - renew;                // 净负荷
        result.periods.append(
            clearOne(market, s.period, s.time, s.loadMW, renew, scale, mode));
    }
    return result;
}

// ------------------------------------------------------------------
// 单时段撮合核心
// ------------------------------------------------------------------
PeriodResult FakeEngine::clearOne(const MarketData &market, int period,
                                  const QString &time, double loadMW, double renewMW,
                                  double scale, const QString &mode)
{
    PeriodResult out;
    out.period = period;
    out.time = time;
    out.loadMW = loadMW;
    out.renewMW = renewMW;

    // 净需求（benchmark 传入 loadMW=demand、renewMW=0）
    const double demand = loadMW - renewMW;

    fillGenDetails(market, demand, scale, mode, out);
    fillConDetails(market, out.clearedMW, scale, mode, out);
    return out;
}

// ------------------------------------------------------------------
// 发电侧：报价升序逐段吸收，边际段报价 = 出清价
// ------------------------------------------------------------------
void FakeEngine::fillGenDetails(const MarketData &market, double demand,
                                double scale, const QString &mode, PeriodResult &out)
{
    QVector<Segment> segs;
    for (const auto &g : market.generatorBids) {
        Segment s;
        s.qty = g.quantity * scale;
        s.price = g.price;
        s.id = g.id;
        s.name = g.name;
        s.seg = g.segment;
        segs.append(s);
    }
    std::sort(segs.begin(), segs.end(),
              [](const Segment &a, const Segment &b) { return a.price < b.price; });

    double remaining = demand;
    double clearingPrice = 0.0;

    // 第一遍：逐段吸收，确定边际价与各段成交
    for (auto &s : segs) {
        if (remaining <= 0.0)
            break;
        const double take = std::min(s.qty, remaining);
        if (take <= 0.0)
            continue;
        s.qty = take;                       // 覆写为实际成交
        clearingPrice = s.price;            // 最后一个吸收到的段报价
        remaining -= take;
    }
    if (demand <= 0.0)
        clearingPrice = 0.0;

    // 第二遍：按 MCP / PAB 口径记账
    double fee = 0.0, cleared = 0.0;
    for (const auto &s : segs) {
        if (s.qty <= 0.0)
            continue;
        const double money = (mode == QStringLiteral("PAB"))
                                 ? s.price * s.qty
                                 : clearingPrice * s.qty;
        EntityCleared e;
        e.id = s.id;
        e.name = s.name;
        e.segment = s.seg;
        e.bidPrice = s.price;
        e.clearedMW = s.qty;
        e.money = money;
        out.genDetails.append(e);
        fee += money;
        cleared += s.qty;
    }

    out.clearingPrice = clearingPrice;
    out.clearedMW = cleared;
    out.genFee = fee;
}

// ------------------------------------------------------------------
// 购电侧：报价降序撮合（出价高的用户优先中标），与发电侧同成交量
// ------------------------------------------------------------------
void FakeEngine::fillConDetails(const MarketData &market, double demand,
                                double scale, const QString &mode, PeriodResult &out)
{
    QVector<Segment> segs;
    for (const auto &c : market.consumerBids) {
        Segment s;
        s.qty = c.quantity * scale;
        s.price = c.price;
        s.id = c.id;
        s.name = c.name;
        s.seg = c.segment;
        segs.append(s);
    }
    std::sort(segs.begin(), segs.end(),
              [](const Segment &a, const Segment &b) { return a.price > b.price; });

    double remaining = demand;
    double fee = 0.0;
    for (auto &s : segs) {
        if (remaining <= 0.0)
            break;
        const double take = std::min(s.qty, remaining);
        if (take <= 0.0)
            continue;
        const double money = (mode == QStringLiteral("PAB"))
                                 ? s.price * take
                                 : out.clearingPrice * take;
        EntityCleared e;
        e.id = s.id;
        e.name = s.name;
        e.segment = s.seg;
        e.bidPrice = s.price;
        e.clearedMW = take;
        e.money = money;
        out.conDetails.append(e);
        fee += money;
        remaining -= take;
    }
    out.conFee = fee;
}
