#ifndef SCENARIO_MANAGER_H
#define SCENARIO_MANAGER_H

#include "data_reader.h"

#include <QString>
#include <QStringList>
#include <QVector>

// 单时段场景数据
struct PeriodScenario
{
    int period = 0;
    QString time;

    double loadMW = 0.0;

    QVector<RenewableOutput> renewableBase;
};

// 场景数据管理
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
        int periodCount,
        QVector<PeriodScenario> &scenarios,
        QStringList &errors);
};

#endif // SCENARIO_MANAGER_H