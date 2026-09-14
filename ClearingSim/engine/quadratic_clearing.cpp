#include "quadratic_clearing.h"

#include <QtGlobal>
#include <algorithm>
#include <limits>

namespace {

constexpr double kSupplyTol = 1e-3;   // 供给-需求平衡容差（MW）
constexpr double kCoefEps = 1e-9;     // a 系数零判定阈值
constexpr int kMaxIter = 1000;        // 二分最大迭代次数

// 单机供给曲线：给定电价 λ 的出力（MC 反函数夹 [0, pMax]）
double unitSupply(const QuadraticGenerator &g, double price)
{
    if (g.a < kCoefEps) {
        // a≈0：边际成本恒为 b 的阶梯机组（λ 越过 b 即满发）
        return price > g.b ? g.pMax : 0.0;
    }
    const double p = (price - g.b) / (2.0 * g.a);
    return std::max(0.0, std::min(p, g.pMax));
}

// 单机边际成本
double unitMarginalCost(const QuadraticGenerator &g, double output)
{
    if (g.a < kCoefEps)
        return g.b;
    return 2.0 * g.a * output + g.b;
}

} // namespace

QuadraticClearResult quadraticClearing(QVector<QuadraticGenerator> generators,
                                       double demandMW)
{
    QuadraticClearResult result;
    if (demandMW < 0.0)
        demandMW = 0.0;

    // 恒定供给下界：λ 低于所有 b 时机组出力全 0
    double minB = std::numeric_limits<double>::max();
    bool hasGen = false;
    double capacity = 0.0;
    double priceMax = 0.0;
    for (const auto &g : generators) {
        if (g.pMax <= 0.0)
            continue;
        hasGen = true;
        capacity += g.pMax;
        priceMax = std::max(priceMax, unitMarginalCost(g, g.pMax));
        minB = std::min(minB, g.b);
    }

    if (!hasGen || capacity <= 0.0)
        return result; // ok=false：没有可用机组

    if (demandMW > capacity + kSupplyTol) {
        // 稀缺封顶（衔接契约 V1.3.1）：全部顶格，λ = 最高边际成本
        result.ok = true;
        result.shortfall = demandMW - capacity;
        result.clearingPrice = priceMax;
        result.totalVolume = capacity;
        for (const auto &g : generators) {
            QuadraticDispatchItem item;
            item.id = g.id;
            item.name = g.name;
            item.output = g.pMax;
            item.marginalCost = unitMarginalCost(g, g.pMax);
            result.dispatch.append(item);
        }
        return result;
    }

    if (demandMW <= kSupplyTol) {
        // 零需求：机组全部不出力，价格取开机门槛（最低 b）
        result.ok = true;
        result.clearingPrice = minB;
        for (const auto &g : generators) {
            QuadraticDispatchItem item;
            item.id = g.id;
            item.name = g.name;
            result.dispatch.append(item);
        }
        return result;
    }

    // 二分搜索 λ*：Σ P_i(λ) = D
    double priceLow = 0.0;
    double priceHigh = priceMax;
    double price = 0.5 * (priceLow + priceHigh);
    double total = 0.0;
    for (int iter = 0; iter < kMaxIter; ++iter) {
        total = 0.0;
        for (const auto &g : generators)
            total += unitSupply(g, price);
        if (std::abs(total - demandMW) < kSupplyTol)
            break;
        if (total < demandMW)
            priceLow = price;
        else
            priceHigh = price;
        price = 0.5 * (priceLow + priceHigh);
    }

    // 含阶梯机组（a≈0）时供给可能跳变越过 D 而无法精确相等：
    // 只要 ΣP(λ) ≥ D 即视为可行（多供部分 ≤ 阶梯步长，教学口径可接受）
    result.ok = (total >= demandMW - kSupplyTol);
    result.clearingPrice = price;
    result.totalVolume = total;
    for (const auto &g : generators) {
        QuadraticDispatchItem item;
        item.id = g.id;
        item.name = g.name;
        item.output = unitSupply(g, price);
        item.marginalCost = unitMarginalCost(g, item.output);
        result.dispatch.append(item);
    }
    return result;
}
