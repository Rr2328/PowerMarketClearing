#include "data_reader.h"
#include "scenario_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <cmath>

namespace
{

int failedTests = 0;


void check(
    bool condition,
    const QString &testName)
{
    if (condition)
    {
        qInfo().noquote()
        << "[PASS]"
        << testName;
    }
    else
    {
        qCritical().noquote()
        << "[FAIL]"
        << testName;

        ++failedTests;
    }
}


QString searchRepoRoot(
    const QString &startPath)
{
    QDir dir(startPath);

    while (true)
    {
        if (dir.exists("ClearingSim") &&
            dir.exists(
                "data/samples/scenario"))
        {
            return dir.absolutePath();
        }

        if (!dir.cdUp())
        {
            break;
        }
    }

    return QString();
}


QString findRepoRoot()
{
    QString root =
        searchRepoRoot(
            QCoreApplication::
            applicationDirPath());

    if (!root.isEmpty())
    {
        return root;
    }

    root =
        searchRepoRoot(
            QDir::currentPath());

    if (!root.isEmpty())
    {
        return root;
    }

    const QFileInfo sourceFile(
        QString::fromUtf8(__FILE__));

    return searchRepoRoot(
        sourceFile.absolutePath());
}


DataFileSet makeFileSet(
    const QString &scenarioDir,
    int periodCount)
{
    const QString suffix =
        QString::number(periodCount) +
        "period";

    DataFileSet files;

    files.generatorBidsFile =
        QDir(scenarioDir)
            .filePath(
                "generator_bids_" +
                suffix +
                ".csv");

    files.consumerBidsFile =
        QDir(scenarioDir)
            .filePath(
                "consumer_bids_" +
                suffix +
                ".csv");

    files.loadCurveFile =
        QDir(scenarioDir)
            .filePath(
                "load_curve_" +
                suffix +
                ".csv");

    files.renewableOutputFile =
        QDir(scenarioDir)
            .filePath(
                "renewable_output_" +
                suffix +
                ".csv");

    return files;
}


void printErrors(
    const QStringList &errors)
{
    for (const QString &error : errors)
    {
        qInfo().noquote()
        << "   "
        << error;
    }
}


bool findRenewable(
    const QVector<RenewableOutput> &data,
    const QString &generatorId,
    int period,
    RenewableOutput &result)
{
    for (const RenewableOutput &item : data)
    {
        if (item.generatorId ==
                generatorId &&
            item.period ==
                period)
        {
            result = item;
            return true;
        }
    }

    return false;
}


QSet<QString> renewableIds(
    const QVector<RenewableOutput> &data)
{
    QSet<QString> ids;

    for (const RenewableOutput &item : data)
    {
        ids.insert(
            item.generatorId);
    }

    return ids;
}


int countGeneratorBids(
    const QVector<GeneratorBid> &data,
    int period)
{
    int count = 0;

    for (const GeneratorBid &bid : data)
    {
        if (bid.period == period)
        {
            ++count;
        }
    }

    return count;
}


int countConsumerBids(
    const QVector<ConsumerBid> &data,
    int period)
{
    int count = 0;

    for (const ConsumerBid &bid : data)
    {
        if (bid.period == period)
        {
            ++count;
        }
    }

    return count;
}


int countRenewables(
    const QVector<RenewableOutput> &data,
    int period)
{
    int count = 0;

    for (const RenewableOutput &item : data)
    {
        if (item.period == period)
        {
            ++count;
        }
    }

    return count;
}


bool allGeneratorBidsBelongToPeriod(
    const PeriodScenario &scenario)
{
    if (scenario.generatorBids.isEmpty())
    {
        return false;
    }

    for (const GeneratorBid &bid :
         scenario.generatorBids)
    {
        if (bid.period !=
            scenario.period)
        {
            return false;
        }
    }

    return true;
}


bool allConsumerBidsBelongToPeriod(
    const PeriodScenario &scenario)
{
    if (scenario.consumerBids.isEmpty())
    {
        return false;
    }

    for (const ConsumerBid &bid :
         scenario.consumerBids)
    {
        if (bid.period !=
            scenario.period)
        {
            return false;
        }
    }

    return true;
}


bool allRenewablesBelongToPeriod(
    const PeriodScenario &scenario)
{
    for (const RenewableOutput &item :
         scenario.renewableBase)
    {
        if (item.period !=
            scenario.period)
        {
            return false;
        }
    }

    return true;
}


bool sameGeneratorBid(
    const GeneratorBid &a,
    const GeneratorBid &b)
{
    return
        a.id == b.id &&
        a.name == b.name &&
        a.type == b.type &&
        a.segment == b.segment &&
        std::abs(
            a.price -
            b.price) <
            0.000001 &&
        std::abs(
            a.quantity -
            b.quantity) <
            0.000001 &&
        a.period == b.period;
}


bool sameConsumerBid(
    const ConsumerBid &a,
    const ConsumerBid &b)
{
    return
        a.id == b.id &&
        a.name == b.name &&
        a.segment == b.segment &&
        std::abs(
            a.price -
            b.price) <
            0.000001 &&
        std::abs(
            a.quantity -
            b.quantity) <
            0.000001 &&
        a.period == b.period;
}


bool containsGeneratorBid(
    const QVector<GeneratorBid> &data,
    const GeneratorBid &target)
{
    for (const GeneratorBid &bid : data)
    {
        if (sameGeneratorBid(
                bid,
                target))
        {
            return true;
        }
    }

    return false;
}


bool containsConsumerBid(
    const QVector<ConsumerBid> &data,
    const ConsumerBid &target)
{
    for (const ConsumerBid &bid : data)
    {
        if (sameConsumerBid(
                bid,
                target))
        {
            return true;
        }
    }

    return false;
}

} // namespace


int main(
    int argc,
    char *argv[])
{
    QCoreApplication app(
        argc,
        argv);

    qInfo().noquote()
        << "========== ScenarioManager V1.4 Test ==========";


    const QString repoRoot =
        findRepoRoot();

    check(
        !repoRoot.isEmpty(),
        "定位仓库根目录");

    if (repoRoot.isEmpty())
    {
        return 1;
    }

    qInfo().noquote()
        << "Repository root:"
        << QDir::toNativeSeparators(
               repoRoot);


    const QString scenarioDir =
        QDir(repoRoot)
            .filePath(
                "data/samples/scenario");

    check(
        QDir(scenarioDir).exists(),
        "定位 scenario 场景数据目录");

    if (!QDir(scenarioDir).exists())
    {
        return 1;
    }

    qInfo().noquote()
        << "Scenario directory:"
        << QDir::toNativeSeparators(
               scenarioDir);


    // 读取 24 时段数据

    MarketData data24;
    QStringList errors;

    bool ok =
        DataReader::readAll(
            makeFileSet(
                scenarioDir,
                24),
            data24,
            errors);

    check(
        ok,
        "读取 24 时段基础数据");

    if (!ok)
    {
        printErrors(errors);
        return 1;
    }


    // 读取 96 时段数据

    MarketData data96;

    errors.clear();

    ok =
        DataReader::readAll(
            makeFileSet(
                scenarioDir,
                96),
            data96,
            errors);

    check(
        ok,
        "读取 96 时段基础数据");

    if (!ok)
    {
        printErrors(errors);
        return 1;
    }


    // 96 → 24 负荷聚合

    QVector<LoadPoint> load24From96;

    errors.clear();

    ok =
        ScenarioManager::aggregateLoadTo24(
            data96.loadCurve,
            load24From96,
            errors);

    check(
        ok,
        "96→24 负荷聚合工具正常");

    if (!ok)
    {
        printErrors(errors);
    }

    check(
        load24From96.size() == 24,
        "聚合后负荷数量为 24");


    if (ok &&
        data96.loadCurve.size() >= 4 &&
        !load24From96.isEmpty())
    {
        const double expected =
            (
                data96.loadCurve[0].load +
                data96.loadCurve[1].load +
                data96.loadCurve[2].load +
                data96.loadCurve[3].load
                ) / 4.0;

        check(
            std::abs(
                load24From96[0].load -
                expected) <
                0.000001,
            "96→24 负荷采用相邻 4 点平均");

        check(
            load24From96[0].period == 1 &&
                load24From96[23].period == 24,
            "96→24 负荷聚合后的 period 正确");
    }


    // 96 → 24 新能源聚合

    QVector<RenewableOutput>
        renewable24From96;

    errors.clear();

    ok =
        ScenarioManager::
        aggregateRenewableTo24(
            data96.renewableOutputs,
            renewable24From96,
            errors);

    check(
        ok,
        "96→24 新能源聚合工具正常");

    if (!ok)
    {
        printErrors(errors);
    }


    const QSet<QString>
        renewableIds96 =
        renewableIds(
            data96.renewableOutputs);

    check(
        renewable24From96.size() ==
            renewableIds96.size() * 24,
        "聚合后新能源数量正确");


    if (ok &&
        !renewableIds96.isEmpty())
    {
        const QString id =
            *renewableIds96.constBegin();

        double totalOutput = 0.0;
        bool sourceComplete = true;
        QString expectedType;

        for (int period = 1;
             period <= 4;
             ++period)
        {
            RenewableOutput item;

            if (!findRenewable(
                    data96.renewableOutputs,
                    id,
                    period,
                    item))
            {
                sourceComplete = false;
                break;
            }

            totalOutput +=
                item.output;

            expectedType =
                item.generatorType;
        }

        RenewableOutput aggregated;

        const bool found =
            findRenewable(
                renewable24From96,
                id,
                1,
                aggregated);

        check(
            sourceComplete &&
                found &&
                std::abs(
                    aggregated.output -
                    totalOutput / 4.0) <
                    0.000001,
            "96→24 新能源按机组采用相邻 4 点平均");

        check(
            found &&
                aggregated.generatorId ==
                    id &&
                aggregated.generatorType ==
                    expectedType &&
                aggregated.period == 1,
            "新能源聚合后机组信息保持正确");
    }


    // 构建 24 时段场景

    QVector<PeriodScenario>
        scenarios24;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data24,
            TimeGranularity::
            Hourly24,
            scenarios24,
            errors);

    check(
        ok,
        "直接构建 24 时段场景");

    if (!ok)
    {
        printErrors(errors);
    }

    check(
        scenarios24.size() == 24,
        "24 时段场景数量正确");


    if (ok &&
        scenarios24.size() == 24)
    {
        const PeriodScenario &first =
            scenarios24.first();

        const PeriodScenario &last =
            scenarios24.last();

        check(
            first.period == 1 &&
                last.period == 24,
            "24 时段场景 period 范围正确");

        check(
            std::abs(
                first.intervalHours -
                1.0) <
                0.000001,
            "24 时段长度为 1 小时");

        check(
            std::abs(
                first.loadMW -
                data24.loadCurve[0].load) <
                0.000001,
            "24 时段场景 loadMW 与负荷数据一致");

        check(
            first.time ==
                data24.loadCurve[0].time,
            "24 时段场景 time 与负荷数据一致");

        check(
            first.generatorBids.size() ==
                countGeneratorBids(
                    data24.generatorBids,
                    1),
            "24 时段场景发电申报数量正确");

        check(
            first.consumerBids.size() ==
                countConsumerBids(
                    data24.consumerBids,
                    1),
            "24 时段场景购电申报数量正确");

        check(
            first.renewableBase.size() ==
                countRenewables(
                    data24.renewableOutputs,
                    1),
            "24 时段场景新能源数量正确");

        check(
            allGeneratorBidsBelongToPeriod(
                first),
            "24 时段场景只包含本时段发电申报");

        check(
            allConsumerBidsBelongToPeriod(
                first),
            "24 时段场景只包含本时段购电申报");

        check(
            allRenewablesBelongToPeriod(
                first),
            "24 时段场景只包含本时段新能源数据");


        if (!first.generatorBids.isEmpty())
        {
            check(
                containsGeneratorBid(
                    data24.generatorBids,
                    first.generatorBids.first()),
                "24 时段发电申报字段未在场景构建中丢失");
        }


        if (!first.consumerBids.isEmpty())
        {
            check(
                containsConsumerBid(
                    data24.consumerBids,
                    first.consumerBids.first()),
                "24 时段购电申报字段未在场景构建中丢失");
        }
    }


    // 构建 96 时段场景

    QVector<PeriodScenario>
        scenarios96;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data96,
            TimeGranularity::
            QuarterHourly96,
            scenarios96,
            errors);

    check(
        ok,
        "直接构建 96 时段场景");

    if (!ok)
    {
        printErrors(errors);
    }

    check(
        scenarios96.size() == 96,
        "96 时段场景数量正确");


    if (ok &&
        scenarios96.size() == 96)
    {
        const PeriodScenario &first =
            scenarios96.first();

        const PeriodScenario &middle =
            scenarios96[36];

        const PeriodScenario &last =
            scenarios96.last();

        check(
            first.period == 1 &&
                middle.period == 37 &&
                last.period == 96,
            "96 时段场景 period 范围正确");

        check(
            std::abs(
                first.intervalHours -
                0.25) <
                0.000001,
            "96 时段长度为 0.25 小时");

        check(
            std::abs(
                middle.loadMW -
                data96.loadCurve[36].load) <
                0.000001,
            "第 37 时段 loadMW 正确");

        check(
            middle.time ==
                data96.loadCurve[36].time,
            "第 37 时段 time 正确");

        check(
            middle.generatorBids.size() ==
                countGeneratorBids(
                    data96.generatorBids,
                    37),
            "第 37 时段发电申报数量正确");

        check(
            middle.consumerBids.size() ==
                countConsumerBids(
                    data96.consumerBids,
                    37),
            "第 37 时段购电申报数量正确");

        check(
            middle.renewableBase.size() ==
                countRenewables(
                    data96.renewableOutputs,
                    37),
            "第 37 时段新能源数量正确");

        check(
            allGeneratorBidsBelongToPeriod(
                middle),
            "第 37 时段只包含 period=37 的发电申报");

        check(
            allConsumerBidsBelongToPeriod(
                middle),
            "第 37 时段只包含 period=37 的购电申报");

        check(
            allRenewablesBelongToPeriod(
                middle),
            "第 37 时段只包含 period=37 的新能源数据");
    }


    QVector<PeriodScenario>
        invalidScenarios;


    // 24 / 96 数据模式不能混用

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data24,
            TimeGranularity::
            QuarterHourly96,
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别 24 时段数据误用 96 时段模式");


    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data96,
            TimeGranularity::
            Hourly24,
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别 96 时段数据误用 24 时段模式");


    // 非法颗粒度

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data96,
            static_cast<
                TimeGranularity>(48),
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别非法时段颗粒度");


    // 构建场景时负荷缺失

    MarketData brokenLoad =
        data96;

    if (!brokenLoad.loadCurve.isEmpty())
    {
        brokenLoad.loadCurve.removeLast();
    }

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            brokenLoad,
            TimeGranularity::
            QuarterHourly96,
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别 96 时段负荷缺失");


    // 构建场景时新能源缺失

    MarketData brokenRenewable =
        data96;

    if (!brokenRenewable
             .renewableOutputs
             .isEmpty())
    {
        brokenRenewable
            .renewableOutputs
            .removeLast();
    }

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            brokenRenewable,
            TimeGranularity::
            QuarterHourly96,
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别 96 时段新能源缺失");


    // 构建场景时发电申报缺失一个时段

    MarketData brokenGenerator =
        data96;

    for (int i =
         brokenGenerator
             .generatorBids
             .size() - 1;
         i >= 0;
         --i)
    {
        if (brokenGenerator
                .generatorBids[i]
                .period == 96)
        {
            brokenGenerator
                .generatorBids
                .removeAt(i);
        }
    }

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            brokenGenerator,
            TimeGranularity::
            QuarterHourly96,
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别发电侧申报时段缺失");


    // 构建场景时购电申报缺失一个时段

    MarketData brokenConsumer =
        data96;

    for (int i =
         brokenConsumer
             .consumerBids
             .size() - 1;
         i >= 0;
         --i)
    {
        if (brokenConsumer
                .consumerBids[i]
                .period == 96)
        {
            brokenConsumer
                .consumerBids
                .removeAt(i);
        }
    }

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            brokenConsumer,
            TimeGranularity::
            QuarterHourly96,
            invalidScenarios,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "识别购电侧申报时段缺失");


    // 96→24 负荷工具拒绝数量不足

    QVector<LoadPoint>
        shortLoad =
        data96.loadCurve;

    if (!shortLoad.isEmpty())
    {
        shortLoad.removeLast();
    }

    QVector<LoadPoint>
        invalidLoad24;

    errors.clear();

    ok =
        ScenarioManager::
        aggregateLoadTo24(
            shortLoad,
            invalidLoad24,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "96→24 负荷聚合识别时段数量不足");


    // 96→24 负荷工具拒绝重复 period

    QVector<LoadPoint>
        duplicateLoad =
        data96.loadCurve;

    if (duplicateLoad.size() >= 2)
    {
        duplicateLoad.last().period =
            duplicateLoad[
                duplicateLoad.size() - 2]
                .period;
    }

    errors.clear();

    ok =
        ScenarioManager::
        aggregateLoadTo24(
            duplicateLoad,
            invalidLoad24,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "96→24 负荷聚合识别重复时段");


    // 96→24 新能源工具拒绝缺失时段

    QVector<RenewableOutput>
        missingRenewable =
        data96.renewableOutputs;

    if (!missingRenewable.isEmpty())
    {
        missingRenewable.removeLast();
    }

    QVector<RenewableOutput>
        invalidRenewable24;

    errors.clear();

    ok =
        ScenarioManager::
        aggregateRenewableTo24(
            missingRenewable,
            invalidRenewable24,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "96→24 新能源聚合识别机组时段缺失");


    // 96→24 新能源工具拒绝重复数据

    QVector<RenewableOutput>
        duplicateRenewable =
        data96.renewableOutputs;

    if (!duplicateRenewable.isEmpty())
    {
        duplicateRenewable.push_back(
            duplicateRenewable.first());
    }

    errors.clear();

    ok =
        ScenarioManager::
        aggregateRenewableTo24(
            duplicateRenewable,
            invalidRenewable24,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "96→24 新能源聚合识别重复机组时段");


    // 96→24 新能源工具拒绝类型不一致

    QVector<RenewableOutput>
        wrongTypeRenewable =
        data96.renewableOutputs;

    if (!wrongTypeRenewable.isEmpty())
    {
        const QString targetId =
            wrongTypeRenewable.first()
                .generatorId;

        const QString originalType =
            wrongTypeRenewable.first()
                .generatorType;

        const QString wrongType =
            originalType == "风电"
                ? "光伏"
                : "风电";

        for (int i = 1;
             i < wrongTypeRenewable.size();
             ++i)
        {
            if (wrongTypeRenewable[i]
                    .generatorId ==
                targetId)
            {
                wrongTypeRenewable[i]
                    .generatorType =
                    wrongType;

                break;
            }
        }
    }

    errors.clear();

    ok =
        ScenarioManager::
        aggregateRenewableTo24(
            wrongTypeRenewable,
            invalidRenewable24,
            errors);

    check(
        !ok &&
            !errors.isEmpty(),
        "96→24 新能源聚合识别机组类型不一致");


    qInfo().noquote()
        << "==========================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All ScenarioManager V1.4 tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}