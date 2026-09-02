#ifndef CLEARING_FACADE_H
#define CLEARING_FACADE_H

#include <QString>
#include <QVector>

#include "data/data_reader.h"
#include "data/scenario_manager.h"

// ------------------------------------------------------------------
// 出清结果数据结构（界面消费的唯一接口，也是与 B 位引擎的对齐边界）
//   2026-09-02 起内部已接入 B 位真实引擎 ClearMarket
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
    double loadMW = 0.0;      // 该时段总需求（购电侧申报总量，负荷概念已退场）
    double renewMW = 0.0;     // 该时段新能源实际消纳量（渗透率×负荷换算，0 价优先中标）
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
// 出清外观类 ClearingFacade（原 FakeEngine，2026-09-02 更名）：
//   只做"构造入参 → 调真引擎 → 聚合结果"三件事，
//   撮合与结算全部在 B 位真实引擎（ClearMarket + settle）内完成。
//   新能源以 0 价供给段参与撮合（价格接受者，优先中标）。
//   V1.3（#67/#88）：申报数据带 period 维度，引擎恒跑 96 期、逐时段取
//   该时段申报撮合；新能源出力按渗透率换算（契约 §5.3，B/C 同式）；
//   24 时段仅为聚合视图（契约 §7.2），窄表导入数据已展开为 96 期同量同价。
// ------------------------------------------------------------------

class ClearingFacade
{
public:
    // 渗透率换算（契约 §5.3，B 与 C 必须用同一式）：
    //   P_re(t) = 渗透率 × 负荷(t)；无负荷曲线时回退 购电申报总量(t)。
    //   撮合入参（B）与供需图预览（C）统一走本函数，保证两边数字一致。
    static double renewCapacityAt(const MarketData &market, int period,
                                  double penetration);

    // 一键演示：单时段基准出清（对拍锚点 250/50/12500，不含新能源）
    static ClearingResult clearBenchmark(const MarketData &market, const QString &mode);

    // 开始仿真：连续出清。penetration ∈ [0,1]（界面滑块 0–100% ÷ 100）。
    //   引擎内部恒跑 96 期；periodCount=24 时按契约 §7.2 聚合为小时视图。
    static ClearingResult clearPeriods(const MarketData &market, int periodCount,
                                       double penetration, const QString &mode);

private:
    // 单个时段的出清核心：构造真引擎入参 → ClearMarket → 聚合为 PeriodResult
    static PeriodResult clearOne(const MarketData &market, int period,
                                 const QString &time, double demandMW, double renewMW,
                                 double scale, const QString &mode);

    // 时段标签：24 时段 "01:00"~"24:00" / 96 时段 "00:15"~"24:00"
    static QString periodTime(int period, int periodCount);
};

#endif // CLEARING_FACADE_H
