#ifndef FAKE_ENGINE_H
#define FAKE_ENGINE_H

#include <QString>
#include <QVector>

#include "data/data_reader.h"
#include "data/scenario_manager.h"

// ------------------------------------------------------------------
// 出清结果数据结构（界面消费的唯一接口，也是 B 位返工后的对齐边界）
//   B 的 ClearingEngine 完成后：只需把 FakeEngine::clearPeriods 内部
//   换成真实算法（或提供同名静态函数），界面与视角过滤零改动。
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
// 假引擎垫片（B 位算法完成前的过渡实现）
//   目标：形态正确 + benchmark 对拍锚点（200 元/MWh / 140 MWh / 56000 元）
//         + 逐主体明细完整，让界面从第一天起就按最终结构消费数据。
// ------------------------------------------------------------------
class FakeEngine
{
public:
    // 一键演示：单时段基准对拍（预期出清价 200、成交 140、发电 28000 + 购电 28000）
    static ClearingResult clearBenchmark(const MarketData &market, const QString &mode);

    // 开始仿真：逐时段连续出清（负荷双驼峰 + 新能源优先 + 申报按负荷缩放）
    static ClearingResult clearPeriods(const QVector<PeriodScenario> &scenarios,
                                       const MarketData &market, const QString &mode);

private:
    // 单个时段的撮合核心：输入该时段需求与新能源，输出 PeriodResult
    static PeriodResult clearOne(const MarketData &market, int period,
                                 const QString &time, double loadMW, double renewMW,
                                 double scale, const QString &mode);

    // 发电侧：按报价升序逐段吸收需求，记录每段成交与收入
    static void fillGenDetails(const MarketData &market, double demand,
                               double scale, const QString &mode,
                               PeriodResult &out);

    // 购电侧：按报价降序逐段撮合（申报总量 = 负荷总量，A 已校验），记录每段费用
    static void fillConDetails(const MarketData &market, double demand,
                               double scale, const QString &mode,
                               PeriodResult &out);
};

#endif // FAKE_ENGINE_H
