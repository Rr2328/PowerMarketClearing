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
        << "[PASS]" << testName;
    }
    else
    {
        qCritical().noquote()
        << "[FAIL]" << testName;

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
            dir.exists("data/samples/scenario"))
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


void printErrors(
    const QStringList &errors)
{
    for (const QString &error : errors)
    {
        qInfo().noquote()
        << "   " << error;
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
        if (item.generatorId == generatorId &&
            item.period == period)
        {
            result = item;
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
        << "========== ScenarioManager V1.2 Test ==========";

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
        repoRoot +
        "/data/samples/scenario";

    check(
        QDir(scenarioDir).exists(),
        "定位 scenario 场景数据目录");

    qInfo().noquote()
        << "Scenario directory:"
        << QDir::toNativeSeparators(
               scenarioDir);


    DataFileSet files;

    files.generatorBidsFile =
        scenarioDir +
        "/generator_bids.csv";

    files.consumerBidsFile =
        scenarioDir +
        "/consumer_bids.csv";

    files.loadCurveFile =
        scenarioDir +
        "/load_curve.csv";

    files.renewableOutputFile =
        scenarioDir +
        "/renewable_output.csv";


    MarketData data;
    QStringList errors;

    bool ok =
        DataReader::readAll(
            files,
            data,
            errors);

    check(
        ok,
        "读取场景基础数据");

    if (!ok)
    {
        printErrors(errors);
        return 1;
    }


    errors.clear();

    ok =
        DataReader::validateRelations(
            data,
            errors);

    check(
        ok,
        "场景基础数据跨文件一致");

    if (!ok)
    {
        printErrors(errors);
        return 1;
    }


    // 负荷 96→24
    QVector<LoadPoint> load24;

    errors.clear();

    ok =
        ScenarioManager::aggregateLoadTo24(
            data.loadCurve,
            load24,
            errors);

    check(
        ok,
        "负荷 96→24 聚合");

    check(
        load24.size() == 24,
        "24 时段负荷数量正确");

    if (!ok)
    {
        printErrors(errors);
    }

    if (data.loadCurve.size() >= 4 &&
        !load24.isEmpty())
    {
        const double expected =
            (
                data.loadCurve[0].load +
                data.loadCurve[1].load +
                data.loadCurve[2].load +
                data.loadCurve[3].load
                ) / 4.0;

        check(
            std::abs(
                load24[0].load -
                expected) < 0.000001,
            "24 时段负荷采用相邻 4 点平均");
    }


    // 新能源 96→24
    QVector<RenewableOutput> renewable24;

    errors.clear();

    ok =
        ScenarioManager::
        aggregateRenewableTo24(
            data.renewableOutputs,
            renewable24,
            errors);

    check(
        ok,
        "新能源 96→24 聚合");

    QSet<QString> renewableIds;

    for (const RenewableOutput &item :
         data.renewableOutputs)
    {
        renewableIds.insert(
            item.generatorId);
    }

    check(
        renewable24.size() ==
            renewableIds.size() * 24,
        "24 时段新能源数量正确");

    if (!ok)
    {
        printErrors(errors);
    }


    if (!data.renewableOutputs.isEmpty())
    {
        const QString generatorId =
            data.renewableOutputs[0]
                .generatorId;

        double total = 0.0;
        bool sourceFound = true;

        for (int period = 1;
             period <= 4;
             ++period)
        {
            RenewableOutput item;

            if (!findRenewable(
                    data.renewableOutputs,
                    generatorId,
                    period,
                    item))
            {
                sourceFound = false;
                break;
            }

            total += item.output;
        }

        RenewableOutput aggregated;

        const bool targetFound =
            findRenewable(
                renewable24,
                generatorId,
                1,
                aggregated);

        check(
            sourceFound &&
                targetFound &&
                std::abs(
                    aggregated.output -
                    total / 4.0) <
                    0.000001,
            "24 时段新能源采用相邻 4 点平均");
    }


    // 构建 96 时段场景
    QVector<PeriodScenario> scenarios96;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data,
            TimeGranularity::
            QuarterHourly96,
            scenarios96,
            errors);

    check(
        ok,
        "构建 96 时段场景");

    check(
        scenarios96.size() == 96,
        "96 时段场景数量正确");

    if (ok &&
        !scenarios96.isEmpty())
    {
        check(
            scenarios96[0]
                    .renewableBase
                    .size() ==
                renewableIds.size(),
            "96 时段场景包含全部新能源机组");

        check(
            std::abs(
                scenarios96[0]
                    .intervalHours -
                0.25) <
                0.000001,
            "96 时段长度为 0.25 小时");

        check(
            std::abs(
                scenarios96[0]
                    .loadMW -
                data.loadCurve[0]
                    .load) <
                0.000001,
            "96 时段场景负荷值正确");
    }

    if (!ok)
    {
        printErrors(errors);
    }


    // 构建 24 时段场景
    QVector<PeriodScenario> scenarios24;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data,
            TimeGranularity::
            Hourly24,
            scenarios24,
            errors);

    check(
        ok,
        "构建 24 时段场景");

    check(
        scenarios24.size() == 24,
        "24 时段场景数量正确");

    if (ok &&
        !scenarios24.isEmpty())
    {
        check(
            scenarios24[0]
                    .renewableBase
                    .size() ==
                renewableIds.size(),
            "24 时段场景包含全部新能源机组");

        check(
            std::abs(
                scenarios24[0]
                    .intervalHours -
                1.0) <
                0.000001,
            "24 时段长度为 1 小时");

        check(
            !load24.isEmpty() &&
                std::abs(
                    scenarios24[0]
                        .loadMW -
                    load24[0]
                        .load) <
                    0.000001,
            "24 时段场景负荷值正确");
    }

    if (!ok)
    {
        printErrors(errors);
    }


    // 非法颗粒度
    QVector<PeriodScenario> invalidScenarios;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data,
            static_cast<
                TimeGranularity>(48),
            invalidScenarios,
            errors);

    check(
        !ok,
        "识别非法时段颗粒度");


    // 缺失负荷时段
    MarketData brokenLoad =
        data;

    if (!brokenLoad.loadCurve
             .isEmpty())
    {
        brokenLoad.loadCurve
            .removeLast();
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
        "识别负荷时段缺失");


    // 缺失新能源时段
    MarketData brokenRenewable =
        data;

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
        "识别新能源时段缺失");


    qInfo().noquote()
        << "==========================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All ScenarioManager tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}