#ifndef FAKE_ENGINE_H
#define FAKE_ENGINE_H

#include <QString>
#include <QVector>

#include "data/data_reader.h"
#include "data/scenario_manager.h"

// ------------------------------------------------------------------
// 出清结果数据结构（界面消费的唯一接口，也是与 B 位引擎的对齐边界）
//   2026-09-02 起 FakeEngine 内部已接入 B 位真实引擎 ClearMarket
//   （逐对撮合 + MCP/PAB 结算），界面与视角过滤零改动。
// ------------------------------------------------------------------

// 单个主体在某时段的成交明细（三视角过滤的核心数据源）
struct EntityCleared
{
    QString id;
    QString name;
    int segment = 0;          // 申报段号
    double bidPrice = 0.0;    // 该段申报价（元/MWh）
    double clearedMW = 0.0;   // 中标电量（MWh）
    double money = 0.0;       // 发电侧=收入，购电侧=费用（元）
};

// 单时段出清结果
struct PeriodResult
{
    int period = 0;
    QString time;             // 时段标签，如 "01:00"（24 时段）/"00:15"（96 时段）
    double loadMW = 0.0;      // 该时段负荷
    double renewMW = 0.0;     // 该时段新能源出力（优先中标）
    double clearingPrice = 0.0; // 该时段出清价
    double clearedMW = 0.0;     // 该时段总成交电量
    double genFee = 0.0;        // 发电侧结算总额
    double conFee = 0.0;        // 购电侧结算总额
    QVector<EntityCleared> genDetails;   // 发电侧逐主体明细
    QVector<EntityCleared> conDetails;   // 购电侧逐主体明细
};

// 一次完整仿真的结果（benchmark 只有 1 个时段；连续仿真有 24/96 个）
struct ClearingResult
{
    QString mode;             // "MCP" / "PAB"
    QString sourceName;       // 数据源描述（内置基准例 / 内置场景 / 自定义）
    QVector<PeriodResult> periods;
};

// ------------------------------------------------------------------
// 引擎外壳：内部调用 B 位真实引擎（ClearMarket + MCP/PAB 结算）
//   新能源以 0 价供给段参与撮合（价格接受者，优先中标）。
// ------------------------------------------------------------------
class FakeEngine
{
public:
    // 一键演示：单时段基准出清
    static ClearingResult clearBenchmark(const MarketData &market, const QString &mode);

    // 开始仿真：逐时段连续出清（负荷双驼峰 + 新能源优先 + 申报按负荷缩放）
    static ClearingResult clearPeriods(const QVector<PeriodScenario> &scenarios,
                                       const MarketData &market, const QString &mode);

private:
    // 单个时段的出清核心：构造真引擎入参 → ClearMarket → 聚合为 PeriodResult
    static PeriodResult clearOne(const MarketData &market, int period,
                                 const QString &time, double loadMW, double renewMW,
                                 double scale, const QString &mode);
};

#endif // FAKE_ENGINE_H
