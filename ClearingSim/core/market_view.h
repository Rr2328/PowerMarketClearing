#ifndef MARKET_VIEW_H
#define MARKET_VIEW_H

#include "data/data_reader.h"

#include <QPair>
#include <QVector>

// MarketView：纯数据视图工具
//   把"按 period 过滤 + 量价聚合"这类 UI 层共用的业务逻辑下沉到 core，
//   避免 mainwindow.cpp 多处各自遍历 generatorBids/consumerBids。
//
// 注意：与 ClearingFacade 不同——这里只读、不动撮合逻辑；
//   facade 负责"渗透率→撮合→结算"，MarketView 负责"挑选+聚合"。
namespace MarketView {

// 单条阶梯段（出力/价格）
struct CurvePoint {
    double quantity;   // 段量（MW）
    double price;       // 报价（元/MWh），新能源出力恒为 0
};

// 单时段关键数字（购电总量 / 发电总量 / 负荷曲线该时段值）
struct PeriodSums {
    double conMW = 0.0;        // 购电申报总量（含 period<=0 的"不限时段"条目）
    double genMW = 0.0;        // 发电申报总量（同上）
    double loadMW = 0.0;       // 负荷曲线该时段值；未找到 = 0
    bool loadFound = false;      // 负荷曲线是否含该时段
};

// 当前时段相对原始基准的累计缩放因子（用于 #92 periodHint 显示）
//   ratio = curSum / baseSum；baseSum<=0 时返回 -1 表示无意义
//   注：baseSum 来自 marketBaseline（AppSession 拍快照）
double scaleRatio(const MarketData &market,
                  const MarketData &marketBaseline,
                  int period);

// 发电侧供给阶梯（含渗透率换算的 RENEW 0 价段 + 该时段常规机组）
//   0 量段（停机申报）不返回——与引擎入参口径一致
QVector<CurvePoint> genSupplyCurve(const MarketData &market,
                                   int period,
                                   double penetration = 0.0);

// 购电侧需求阶梯（按 price 降序排列）
//   0 量段不返回
QVector<CurvePoint> conDemandCurve(const MarketData &market,
                                   int period);

// 单时段关键数字聚合
PeriodSums periodSums(const MarketData &market, int period);

} // namespace MarketView

#endif // MARKET_VIEW_H