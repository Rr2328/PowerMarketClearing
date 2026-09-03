#ifndef SCENARIO_MANAGER_H
#define SCENARIO_MANAGER_H

#include "data_reader.h"

#include <QString>
#include <QStringList>
#include <QVector>

enum class TimeGranularity
{
    Hourly24,
    QuarterHourly96
};

// 单个时段的数据场景
// 这里保存的是数据管理层整理完成后的标准数据，
// 后续再转换成算法所需要的 TimeMarketData。
struct PeriodScenario
{
    int period = 0;
    QString time;

    double intervalHours = 0.0;
    double loadMW = 0.0;

    QVector<GeneratorBid> generatorBids;
    QVector<ConsumerBid> consumerBids;

    // 当前保存新能源基准出力。
    // 后续新能源渗透率的实际出力计算方式，
    // 等组内接口确定后再统一处理。
    QVector<RenewableOutput> renewableBase;

    void clear()
    {
        period = 0;
        time.clear();

        intervalHours = 0.0;
        loadMW = 0.0;

        generatorBids.clear();
        consumerBids.clear();
        renewableBase.clear();
    }
};

class ScenarioManager
{
public:
    static bool aggregateLoadTo24(
        const QVector<LoadPoint> &load96,
        QVector<LoadPoint> &load24,
        QStringList &errors);

    static bool aggregateRenewableTo24(
        const QVector<RenewableOutput> &renewable96,
        QVector<RenewableOutput> &renewable24,
        QStringList &errors);

    static bool buildPeriodScenarios(
        const MarketData &data,
        TimeGranularity granularity,
        QVector<PeriodScenario> &scenarios,
        QStringList &errors);
};

#endif // SCENARIO_MANAGER_H