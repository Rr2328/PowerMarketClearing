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

// 单个时段交给算法的数据
struct PeriodScenario
{
    int period = 0;
    QString time;

    double intervalHours = 0.0;
    double loadMW = 0.0;

    QVector<GeneratorBid> generatorBids;
    QVector<ConsumerBid> consumerBids;
    QVector<RenewableOutput> renewableBase;
};

class ScenarioManager
{
public:
    // 保留 96→24 数据转换工具
    static bool aggregateLoadTo24(
        const QVector<LoadPoint> &load96,
        QVector<LoadPoint> &load24,
        QStringList &errors);

    static bool aggregateRenewableTo24(
        const QVector<RenewableOutput> &renewable96,
        QVector<RenewableOutput> &renewable24,
        QStringList &errors);

    // 根据已经读取好的 24/96 时段 MarketData
    // 构建对应的单时段场景
    static bool buildPeriodScenarios(
        const MarketData &data,
        TimeGranularity granularity,
        QVector<PeriodScenario> &scenarios,
        QStringList &errors);
};

#endif // SCENARIO_MANAGER_H