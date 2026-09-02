#include "scenario_manager.h"

#include <QMap>
#include <QSet>

namespace
{

// 24 时段时间标签
QString hourText(int period)
{
    if (period == 24)
    {
        return "24:00";
    }

    return QString("%1:00")
        .arg(
            period,
            2,
            10,
            QChar('0'));
}

// 负荷时段整理
bool buildLoadMap(
    const QVector<LoadPoint> &data,
    int expectedCount,
    QMap<int, LoadPoint> &loadMap,
    QStringList &errors)
{
    loadMap.clear();

    if (data.size() != expectedCount)
    {
        errors.append(
            QString("负荷数据应包含 %1 个时段，实际为 %2 个")
                .arg(expectedCount)
                .arg(data.size()));

        return false;
    }

    for (const LoadPoint &item : data)
    {
        if (item.period < 1 ||
            item.period > expectedCount)
        {
            errors.append(
                QString("负荷时段 %1 超出 1~%2 范围")
                    .arg(item.period)
                    .arg(expectedCount));

            continue;
        }

        if (loadMap.contains(item.period))
        {
            errors.append(
                QString("负荷时段 %1 重复")
                    .arg(item.period));

            continue;
        }

        loadMap.insert(
            item.period,
            item);
    }

    for (int period = 1;
         period <= expectedCount;
         ++period)
    {
        if (!loadMap.contains(period))
        {
            errors.append(
                QString("负荷数据缺少时段 %1")
                    .arg(period));
        }
    }

    return errors.isEmpty();
}

// 新能源时段整理
bool buildRenewableMap(
    const QVector<RenewableOutput> &data,
    int expectedCount,
    QMap<QString, QMap<int, RenewableOutput>> &renewableMap,
    QStringList &errors)
{
    renewableMap.clear();

    QMap<QString, QString> types;

    for (const RenewableOutput &item : data)
    {
        if (item.generatorId.isEmpty())
        {
            errors.append(
                "新能源机组 ID 为空");

            continue;
        }

        if (item.period < 1 ||
            item.period > expectedCount)
        {
            errors.append(
                QString("新能源机组 %1 的时段 %2 超出 1~%3 范围")
                    .arg(item.generatorId)
                    .arg(item.period)
                    .arg(expectedCount));

            continue;
        }

        if (types.contains(item.generatorId) &&
            types.value(item.generatorId) !=
                item.generatorType)
        {
            errors.append(
                QString("新能源机组 %1 的类型不一致")
                    .arg(item.generatorId));

            continue;
        }

        if (renewableMap[item.generatorId]
                .contains(item.period))
        {
            errors.append(
                QString("新能源机组 %1 的时段 %2 重复")
                    .arg(item.generatorId)
                    .arg(item.period));

            continue;
        }

        types[item.generatorId] =
            item.generatorType;

        renewableMap[item.generatorId]
            .insert(
                item.period,
                item);
    }

    for (auto it =
         renewableMap.cbegin();
         it != renewableMap.cend();
         ++it)
    {
        const QString generatorId =
            it.key();

        const QMap<int, RenewableOutput> &periods =
            it.value();

        if (periods.size() != expectedCount)
        {
            errors.append(
                QString("新能源机组 %1 应包含 %2 个时段，实际为 %3 个")
                    .arg(generatorId)
                    .arg(expectedCount)
                    .arg(periods.size()));

            continue;
        }

        for (int period = 1;
             period <= expectedCount;
             ++period)
        {
            if (!periods.contains(period))
            {
                errors.append(
                    QString("新能源机组 %1 缺少时段 %2")
                        .arg(generatorId)
                        .arg(period));

                break;
            }
        }
    }

    return errors.isEmpty();
}

} // namespace


// 负荷 96→24 聚合
bool ScenarioManager::aggregateLoadTo24(
    const QVector<LoadPoint> &load96,
    QVector<LoadPoint> &load24,
    QStringList &errors)
{
    load24.clear();
    errors.clear();

    QMap<int, LoadPoint> loadMap;

    if (!buildLoadMap(
            load96,
            96,
            loadMap,
            errors))
    {
        return false;
    }

    for (int hour = 1;
         hour <= 24;
         ++hour)
    {
        double total = 0.0;

        const int firstPeriod =
            (hour - 1) * 4 + 1;

        for (int offset = 0;
             offset < 4;
             ++offset)
        {
            total +=
                loadMap.value(
                           firstPeriod + offset)
                    .load;
        }

        LoadPoint item;

        item.period = hour;
        item.time = hourText(hour);
        item.load = total / 4.0;

        load24.push_back(item);
    }

    return true;
}


// 新能源 96→24 聚合
bool ScenarioManager::aggregateRenewableTo24(
    const QVector<RenewableOutput> &renewable96,
    QVector<RenewableOutput> &renewable24,
    QStringList &errors)
{
    renewable24.clear();
    errors.clear();

    QMap<QString, QMap<int, RenewableOutput>>
        renewableMap;

    if (!buildRenewableMap(
            renewable96,
            96,
            renewableMap,
            errors))
    {
        return false;
    }

    for (auto generatorIt =
         renewableMap.cbegin();
         generatorIt != renewableMap.cend();
         ++generatorIt)
    {
        const QString generatorId =
            generatorIt.key();

        const QMap<int, RenewableOutput> &periods =
            generatorIt.value();

        const QString generatorType =
            periods.first()
                .generatorType;

        for (int hour = 1;
             hour <= 24;
             ++hour)
        {
            const int firstPeriod =
                (hour - 1) * 4 + 1;

            double total = 0.0;

            for (int offset = 0;
                 offset < 4;
                 ++offset)
            {
                total +=
                    periods.value(
                               firstPeriod + offset)
                        .output;
            }

            RenewableOutput item;

            item.generatorId =
                generatorId;

            item.generatorType =
                generatorType;

            item.period =
                hour;

            item.output =
                total / 4.0;

            renewable24.push_back(item);
        }
    }

    return true;
}


// 构建逐时段场景
bool ScenarioManager::buildPeriodScenarios(
    const MarketData &data,
    int periodCount,
    QVector<PeriodScenario> &scenarios,
    QStringList &errors)
{
    scenarios.clear();
    errors.clear();

    QVector<LoadPoint> loadData;
    QVector<RenewableOutput> renewableData;

    if (periodCount == 96)
    {
        loadData =
            data.loadCurve;

        renewableData =
            data.renewableOutputs;
    }
    else if (periodCount == 24)
    {
        QStringList tempErrors;

        if (!aggregateLoadTo24(
                data.loadCurve,
                loadData,
                tempErrors))
        {
            errors.append(tempErrors);
        }

        tempErrors.clear();

        if (!aggregateRenewableTo24(
                data.renewableOutputs,
                renewableData,
                tempErrors))
        {
            errors.append(tempErrors);
        }

        if (!errors.isEmpty())
        {
            return false;
        }
    }
    else
    {
        errors.append(
            "时段数量只能选择 24 或 96");

        return false;
    }

    QMap<int, LoadPoint> loadMap;

    if (!buildLoadMap(
            loadData,
            periodCount,
            loadMap,
            errors))
    {
        return false;
    }

    QMap<QString, QMap<int, RenewableOutput>>
        renewableMap;

    if (!buildRenewableMap(
            renewableData,
            periodCount,
            renewableMap,
            errors))
    {
        return false;
    }

    for (int period = 1;
         period <= periodCount;
         ++period)
    {
        PeriodScenario scenario;

        scenario.period =
            period;

        scenario.time =
            loadMap.value(period)
                .time;

        scenario.loadMW =
            loadMap.value(period)
                .load;

        for (auto generatorIt =
             renewableMap.cbegin();
             generatorIt != renewableMap.cend();
             ++generatorIt)
        {
            scenario.renewableBase
                .push_back(
                    generatorIt.value()
                        .value(period));
        }

        scenarios.push_back(
            scenario);
    }

    return true;
}