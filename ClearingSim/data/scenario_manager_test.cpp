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

} // namespace


int main(
    int argc,
    char *argv[])
{
    QCoreApplication app(
        argc,
        argv);

    qInfo().noquote()
        << "========== ScenarioManager V1.3 Test ==========";


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


    // ================================
    // 读取 24 时段数据
    // ================================

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


    // ================================
    // 读取 96 时段数据
    // ================================

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


    // ================================
    // 96 → 24 负荷聚合工具
    // ================================

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
    }


    // ================================
    // 96 → 24 新能源聚合工具
    // ================================

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


    // ================================
    // 直接构建 24 时段场景
    // ================================

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
            !first.generatorBids.isEmpty(),
            "24 时段场景包含发电申报");

        check(
            !first.consumerBids.isEmpty(),
            "24 时段场景包含购电申报");

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
    }


    // ================================
    // 直接构建 96 时段场景
    // ================================

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
            !middle.generatorBids.isEmpty(),
            "第 37 时段包含发电申报");

        check(
            !middle.consumerBids.isEmpty(),
            "第 37 时段包含购电申报");

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


    // ================================
    // 错误颗粒度匹配
    // ================================

    QVector<PeriodScenario>
        invalidScenarios;

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
        !ok,
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
        !ok,
        "识别 96 时段数据误用 24 时段模式");


    // ================================
    // 非法颗粒度
    // ================================

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
        !ok,
        "识别非法时段颗粒度");


    // ================================
    // 缺失 96 时段负荷
    // ================================

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
        !ok,
        "识别 96 时段负荷缺失");


    // ================================
    // 缺失新能源时段
    // ================================

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
        !ok,
        "识别 96 时段新能源缺失");


    qInfo().noquote()
        << "==========================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All ScenarioManager V1.3 tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}