#include "market_view.h"
#include "clearing_facade.h"

#include <algorithm>

namespace MarketView {

// 内部辅助：判断 bid 的 period 是否属于指定时段
//   - period > 0 时必须 == targetPeriod
//   - period <= 0 表示"不限时段"（兼容旧窄表语义），任何 targetPeriod 都计入
static inline bool periodMatches(int bidPeriod, int targetPeriod)
{
    return bidPeriod <= 0 || bidPeriod == targetPeriod;
}

double scaleRatio(const MarketData &market,
                  const MarketData &marketBaseline,
                  int period)
{
    if (marketBaseline.generatorBids.isEmpty())
        return -1.0;

    double curSum = 0.0, baseSum = 0.0;
    for (const auto &g : market.generatorBids)
        if (periodMatches(g.period, period))
            curSum += g.quantity;
    for (const auto &g : marketBaseline.generatorBids)
        if (periodMatches(g.period, period))
            baseSum += g.quantity;

    if (baseSum <= 0.0)
        return -1.0;
    return curSum / baseSum;
}

QVector<CurvePoint> genSupplyCurve(const MarketData &market,
                                   int period,
                                   double penetration)
{
    QVector<CurvePoint> out;

    // RENEW 0 价段——直接调 ClearingFacade 保持单一真相
    const double renewMW = ClearingFacade::renewCapacityAt(market, period, penetration);
    if (renewMW > 0.0)
        out.append({renewMW, 0.0});

    for (const auto &g : market.generatorBids) {
        if (!periodMatches(g.period, period))
            continue;
        if (g.quantity <= 0.0)
            continue;   // 0 量段不进引擎，也不画阶梯
        out.append({g.quantity, g.price});
    }

    std::sort(out.begin(), out.end(),
              [](const CurvePoint &a, const CurvePoint &b) {
                  return a.price < b.price;
              });
    return out;
}

QVector<CurvePoint> conDemandCurve(const MarketData &market, int period)
{
    QVector<CurvePoint> out;
    for (const auto &c : market.consumerBids) {
        if (!periodMatches(c.period, period))
            continue;
        if (c.quantity <= 0.0)
            continue;
        out.append({c.quantity, c.price});
    }

    std::sort(out.begin(), out.end(),
              [](const CurvePoint &a, const CurvePoint &b) {
                  return a.price > b.price;   // 购电按报价降序
              });
    return out;
}

PeriodSums periodSums(const MarketData &market, int period)
{
    PeriodSums sums;
    for (const auto &c : market.consumerBids)
        if (periodMatches(c.period, period))
            sums.conMW += c.quantity;
    for (const auto &g : market.generatorBids)
        if (periodMatches(g.period, period))
            sums.genMW += g.quantity;
    for (const auto &lp : market.loadCurve) {
        if (lp.period == period) {
            sums.loadMW = lp.load;
            sums.loadFound = true;
            break;
        }
    }
    return sums;
}

} // namespace MarketView