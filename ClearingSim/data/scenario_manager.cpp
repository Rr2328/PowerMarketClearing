#include "scenario_manager.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace
{

int periodCountFromGranularity(
    TimeGranularity granularity)
{
    switch (granularity)
    {
    case TimeGranularity::Hourly24:
        return 24;

    case TimeGranularity::QuarterHourly96:
        return 96;
    }

    return 0;
}


double intervalHoursFromGranularity(
    TimeGranularity granularity)
{
    switch (granularity)
    {
    case TimeGranularity::Hourly24:
        return 1.0;

    case TimeGranularity::QuarterHourly96:
        return 0.25;
    }

    return 0.0;
}


bool checkLoadPeriods(
    const QVector<LoadPoint> &loadCurve,
    int expectedCount,
    QStringList &errors)
{
    QSet<int> periods;

    for (const LoadPoint &point :
         loadCurve)
    {
        if (point.period < 1 ||
            point.period > expectedCount)
        {
            errors.append(
                QString(
                    "负荷曲线出现非法时段 %1，当前模式应为 1~%2")
                    .arg(point.period)
                    .arg(expectedCount));

            return false;
        }

        if (periods.contains(
                point.period))
        {
            errors.append(
                QString(
                    "负荷曲线时段 %1 重复")
                    .arg(point.period));

            return false;
        }

        periods.insert(
            point.period);
    }

    if (periods.size() !=
        expectedCount)
    {
        errors.append(
            QString(
                "负荷数据应包含 %1 个时段，实际为 %2 个")
                .arg(expectedCount)
                .arg(periods.size()));

        return false;
    }

    for (int period = 1;
         period <= expectedCount;
         ++period)
    {
        if (!periods.contains(
                period))
        {
            errors.append(
                QString(
                    "负荷曲线缺少时段 %1")
                    .arg(period));

            return false;
        }
    }

    return true;
}


bool checkBidPeriods(
    const MarketData &data,
    int expectedCount,
    QStringList &errors)
{
    for (const GeneratorBid &bid :
         data.generatorBids)
    {
        if (bid.period < 1 ||
            bid.period > expectedCount)
        {
            errors.append(
                QString(
                    "发电申报机组 %1 出现非法时段 %2")
                    .arg(bid.id)
                    .arg(bid.period));

            return false;
        }
    }

    for (const ConsumerBid &bid :
         data.consumerBids)
    {
        if (bid.period < 1 ||
            bid.period > expectedCount)
        {
            errors.append(
                QString(
                    "购电申报用户 %1 出现非法时段 %2")
                    .arg(bid.id)
                    .arg(bid.period));

            return false;
        }
    }

    for (const RenewableOutput &item :
         data.renewableOutputs)
    {
        if (item.period < 1 ||
            item.period > expectedCount)
        {
            errors.append(
                QString(
                    "新能源机组 %1 出现非法时段 %2")
                    .arg(item.generatorId)
                    .arg(item.period));

            return false;
        }
    }

    return true;
}

} // namespace


bool ScenarioManager::aggregateLoadTo24(
    const QVector<LoadPoint> &load96,
    QVector<LoadPoint> &load24,
    QStringList &errors)
{
    load24.clear();
    errors.clear();

    if (load96.size() != 96)
    {
        errors.append(
            QString(
                "96→24 聚合要求负荷数据为 96 个时段，实际为 %1 个")
                .arg(load96.size()));

        return false;
    }

    QHash<int, LoadPoint> loadMap;

    for (const LoadPoint &point :
         load96)
    {
        if (point.period < 1 ||
            point.period > 96)
        {
            errors.append(
                QString(
                    "负荷曲线出现非法时段 %1")
                    .arg(point.period));

            return false;
        }

        if (loadMap.contains(
                point.period))
        {
            errors.append(
                QString(
                    "负荷曲线时段 %1 重复")
                    .arg(point.period));

            return false;
        }

        loadMap.insert(
            point.period,
            point);
    }

    for (int period = 1;
         period <= 96;
         ++period)
    {
        if (!loadMap.contains(
                period))
        {
            errors.append(
                QString(
                    "负荷曲线缺少时段 %1")
                    .arg(period));

            return false;
        }
    }

    for (int hour = 1;
         hour <= 24;
         ++hour)
    {
        const int firstPeriod =
            (hour - 1) * 4 + 1;

        double totalLoad = 0.0;

        for (int offset = 0;
             offset < 4;
             ++offset)
        {
            const int sourcePeriod =
                firstPeriod + offset;

            totalLoad +=
                loadMap.value(
                           sourcePeriod)
                    .load;
        }

        const int lastPeriod =
            firstPeriod + 3;

        LoadPoint result;

        result.period =
            hour;

        result.time =
            loadMap.value(
                       lastPeriod)
                .time;

        result.load =
            totalLoad / 4.0;

        load24.push_back(
            result);
    }

    return true;
}


bool ScenarioManager::aggregateRenewableTo24(
    const QVector<RenewableOutput> &renewable96,
    QVector<RenewableOutput> &renewable24,
    QStringList &errors)
{
    renewable24.clear();
    errors.clear();

    if (renewable96.isEmpty())
    {
        errors.append(
            "新能源数据为空");

        return false;
    }

    QSet<QString> generatorIds;

    QHash<QString, QString>
        generatorTypes;

    QHash<QString, RenewableOutput>
        outputMap;

    for (const RenewableOutput &item :
         renewable96)
    {
        if (item.generatorId.isEmpty())
        {
            errors.append(
                "新能源机组编号为空");

            return false;
        }

        if (item.period < 1 ||
            item.period > 96)
        {
            errors.append(
                QString(
                    "新能源机组 %1 出现非法时段 %2")
                    .arg(item.generatorId)
                    .arg(item.period));

            return false;
        }

        if (generatorTypes.contains(
                item.generatorId) &&
            generatorTypes.value(
                item.generatorId) !=
                item.generatorType)
        {
            errors.append(
                QString(
                    "新能源机组 %1 的类型不一致")
                    .arg(item.generatorId));

            return false;
        }

        generatorIds.insert(
            item.generatorId);

        generatorTypes.insert(
            item.generatorId,
            item.generatorType);

        const QString key =
            item.generatorId +
            "|" +
            QString::number(
                item.period);

        if (outputMap.contains(key))
        {
            errors.append(
                QString(
                    "新能源机组 %1 的时段 %2 重复")
                    .arg(item.generatorId)
                    .arg(item.period));

            return false;
        }

        outputMap.insert(
            key,
            item);
    }

    QStringList sortedIds =
        generatorIds.values();

    std::sort(
        sortedIds.begin(),
        sortedIds.end());

    for (const QString &generatorId :
         sortedIds)
    {
        for (int period = 1;
             period <= 96;
             ++period)
        {
            const QString key =
                generatorId +
                "|" +
                QString::number(
                    period);

            if (!outputMap.contains(key))
            {
                errors.append(
                    QString(
                        "新能源机组 %1 缺少时段 %2")
                        .arg(generatorId)
                        .arg(period));

                return false;
            }
        }
    }

    for (int hour = 1;
         hour <= 24;
         ++hour)
    {
        const int firstPeriod =
            (hour - 1) * 4 + 1;

        for (const QString &generatorId :
             sortedIds)
        {
            double totalOutput = 0.0;

            for (int offset = 0;
                 offset < 4;
                 ++offset)
            {
                const int sourcePeriod =
                    firstPeriod + offset;

                const QString key =
                    generatorId +
                    "|" +
                    QString::number(
                        sourcePeriod);

                totalOutput +=
                    outputMap.value(
                                 key)
                        .output;
            }

            RenewableOutput result;

            result.generatorId =
                generatorId;

            result.generatorType =
                generatorTypes.value(
                    generatorId);

            result.period =
                hour;

            result.output =
                totalOutput / 4.0;

            renewable24.push_back(
                result);
        }
    }

    return true;
}


bool ScenarioManager::buildPeriodScenarios(
    const MarketData &data,
    TimeGranularity granularity,
    QVector<PeriodScenario> &scenarios,
    QStringList &errors)
{
    scenarios.clear();
    errors.clear();

    const int expectedCount =
        periodCountFromGranularity(
            granularity);

    const double intervalHours =
        intervalHoursFromGranularity(
            granularity);

    if (expectedCount == 0 ||
        intervalHours <= 0.0)
    {
        errors.append(
            "不支持的时段颗粒度");

        return false;
    }

    if (data.generatorBids.isEmpty())
    {
        errors.append(
            "发电侧申报为空");

        return false;
    }

    if (data.consumerBids.isEmpty())
    {
        errors.append(
            "用户侧申报为空");

        return false;
    }

    if (!checkLoadPeriods(
            data.loadCurve,
            expectedCount,
            errors))
    {
        return false;
    }

    if (!checkBidPeriods(
            data,
            expectedCount,
            errors))
    {
        return false;
    }


    QStringList relationErrors;

    if (!DataReader::validateRelations(
            data,
            relationErrors))
    {
        for (const QString &error :
             relationErrors)
        {
            errors.append(
                error);
        }

        return false;
    }


    QHash<int, LoadPoint>
        loadMap;

    QHash<int, QVector<GeneratorBid>>
        generatorBidMap;

    QHash<int, QVector<ConsumerBid>>
        consumerBidMap;

    QHash<int, QVector<RenewableOutput>>
        renewableMap;


    for (const LoadPoint &point :
         data.loadCurve)
    {
        loadMap.insert(
            point.period,
            point);
    }


    for (const GeneratorBid &bid :
         data.generatorBids)
    {
        generatorBidMap[
            bid.period]
            .push_back(
                bid);
    }


    for (const ConsumerBid &bid :
         data.consumerBids)
    {
        consumerBidMap[
            bid.period]
            .push_back(
                bid);
    }


    for (const RenewableOutput &item :
         data.renewableOutputs)
    {
        renewableMap[
            item.period]
            .push_back(
                item);
    }


    scenarios.reserve(
        expectedCount);


    for (int period = 1;
         period <= expectedCount;
         ++period)
    {
        if (!loadMap.contains(
                period))
        {
            errors.append(
                QString(
                    "缺少负荷时段 %1")
                    .arg(period));

            scenarios.clear();

            return false;
        }

        if (!generatorBidMap.contains(
                period) ||
            generatorBidMap.value(
                               period)
                .isEmpty())
        {
            errors.append(
                QString(
                    "时段 %1 没有发电侧申报")
                    .arg(period));

            scenarios.clear();

            return false;
        }

        if (!consumerBidMap.contains(
                period) ||
            consumerBidMap.value(
                              period)
                .isEmpty())
        {
            errors.append(
                QString(
                    "时段 %1 没有购电侧申报")
                    .arg(period));

            scenarios.clear();

            return false;
        }


        PeriodScenario scenario;

        scenario.period =
            period;

        scenario.time =
            loadMap.value(
                       period)
                .time;

        scenario.intervalHours =
            intervalHours;

        scenario.loadMW =
            loadMap.value(
                       period)
                .load;

        scenario.generatorBids =
            generatorBidMap.value(
                period);

        scenario.consumerBids =
            consumerBidMap.value(
                period);

        scenario.renewableBase =
            renewableMap.value(
                period);


        scenarios.push_back(
            scenario);
    }


    return true;
}