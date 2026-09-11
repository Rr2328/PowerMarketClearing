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
struct PeriodScenario
{
    int period = 0;
    QString time;

    double intervalHours = 0.0;
    double loadMW = 0.0;

    QVector<GeneratorBid> generatorBids;
    QVector<ConsumerBid> consumerBids;

    // 新能源基准出力
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
    // 将96时段负荷聚合为24时段
    static bool aggregateLoadTo24(
        const QVector<LoadPoint> &load96,
        QVector<LoadPoint> &load24,
        QStringList &errors);

    // 将96时段新能源数据聚合为24时段
    static bool aggregateRenewableTo24(
        const QVector<RenewableOutput> &renewable96,
        QVector<RenewableOutput> &renewable24,
        QStringList &errors);

    // 构建时段场景
    static bool buildPeriodScenarios(
        const MarketData &data,
        TimeGranularity granularity,
        QVector<PeriodScenario> &scenarios,
        QStringList &errors);

    // 兼容重载（合并适配）：主线既有调用方（data_pipeline_integration_test 等）
    //   以 int periodCount（24/96）传参——内部换算为 TimeGranularity 后委托
    static bool buildPeriodScenarios(
        const MarketData &data,
        int periodCount,
        QVector<PeriodScenario> &scenarios,
        QStringList &errors);
};

#endif // SCENARIO_MANAGER_H