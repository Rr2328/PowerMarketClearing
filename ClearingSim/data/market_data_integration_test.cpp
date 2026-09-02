#include "data_reader.h"
#include "scenario_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
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


bool hasGenerator(
    const MarketData &data,
    const QString &id)
{
    for (const GeneratorBid &bid :
         data.generatorBids)
    {
        if (bid.id == id)
        {
            return true;
        }
    }

    return false;
}


bool hasConsumer(
    const MarketData &data,
    const QString &id)
{
    for (const ConsumerBid &bid :
         data.consumerBids)
    {
        if (bid.id == id)
        {
            return true;
        }
    }

    return false;
}


int countRenewablePeriods(
    const MarketData &data,
    const QString &generatorId)
{
    QSet<int> periods;

    for (const RenewableOutput &item :
         data.renewableOutputs)
    {
        if (item.generatorId ==
            generatorId)
        {
            periods.insert(
                item.period);
        }
    }

    return periods.size();
}


bool findGeneratorBid(
    const MarketData &data,
    const QString &id,
    int segment,
    GeneratorBid &result)
{
    for (const GeneratorBid &bid :
         data.generatorBids)
    {
        if (bid.id == id &&
            bid.segment == segment)
        {
            result = bid;
            return true;
        }
    }

    return false;
}


bool findConsumerBid(
    const MarketData &data,
    const QString &id,
    int segment,
    ConsumerBid &result)
{
    for (const ConsumerBid &bid :
         data.consumerBids)
    {
        if (bid.id == id &&
            bid.segment == segment)
        {
            result = bid;
            return true;
        }
    }

    return false;
}


bool hasAllLoadPeriods(
    const MarketData &data)
{
    QSet<int> periods;

    for (const LoadPoint &point :
         data.loadCurve)
    {
        periods.insert(
            point.period);
    }

    if (periods.size() != 96)
    {
        return false;
    }

    for (int period = 1;
         period <= 96;
         ++period)
    {
        if (!periods.contains(period))
        {
            return false;
        }
    }

    return true;
}


bool scenarioContainsRenewable(
    const PeriodScenario &scenario,
    const QString &generatorId)
{
    for (const RenewableOutput &item :
         scenario.renewableBase)
    {
        if (item.generatorId ==
            generatorId)
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
        << "========== MarketData Integration Test ==========";


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
        "定位 scenario 场景目录");

    if (!QDir(scenarioDir).exists())
    {
        return 1;
    }

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
        "四类 CSV 统一读取为 MarketData");

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
        "MarketData 跨文件关系正确");

    if (!ok)
    {
        printErrors(errors);
        return 1;
    }


    check(
        !data.generatorBids.isEmpty(),
        "MarketData 包含发电侧申报");

    check(
        !data.consumerBids.isEmpty(),
        "MarketData 包含购电侧申报");

    check(
        data.loadCurve.size() == 96,
        "MarketData 包含 96 点负荷");

    check(
        data.renewableOutputs.size() == 192,
        "MarketData 包含 192 条新能源数据");


    check(
        hasGenerator(data, "G1"),
        "MarketData 包含 G1");

    check(
        hasGenerator(data, "G2"),
        "MarketData 包含 G2");

    check(
        hasGenerator(data, "G3"),
        "MarketData 包含 G3");

    check(
        hasGenerator(data, "W1"),
        "MarketData 包含 W1");

    check(
        hasGenerator(data, "S1"),
        "MarketData 包含 S1");


    check(
        hasConsumer(data, "U1"),
        "MarketData 包含 U1");

    check(
        hasConsumer(data, "U2"),
        "MarketData 包含 U2");

    check(
        hasConsumer(data, "U3"),
        "MarketData 包含 U3");


    check(
        hasAllLoadPeriods(data),
        "负荷时段完整覆盖 1~96");


    check(
        countRenewablePeriods(
            data,
            "W1") == 96,
        "W1 包含完整 96 时段");

    check(
        countRenewablePeriods(
            data,
            "S1") == 96,
        "S1 包含完整 96 时段");


    GeneratorBid generatorBid;

    const bool generatorBidFound =
        findGeneratorBid(
            data,
            "G1",
            1,
            generatorBid);

    check(
        generatorBidFound,
        "读取 G1 第 1 段申报");

    if (generatorBidFound)
    {
        check(
            std::abs(
                generatorBid.quantity -
                80.0) <
                0.000001,
            "G1 第 1 段出力正确");

        check(
            std::abs(
                generatorBid.price -
                150.0) <
                0.000001,
            "G1 第 1 段报价正确");
    }


    ConsumerBid consumerBid;

    const bool consumerBidFound =
        findConsumerBid(
            data,
            "U1",
            1,
            consumerBid);

    check(
        consumerBidFound,
        "读取 U1 第 1 段申报");

    if (consumerBidFound)
    {
        check(
            std::abs(
                consumerBid.quantity -
                60.0) <
                0.000001,
            "U1 第 1 段购电量正确");

        check(
            std::abs(
                consumerBid.price -
                500.0) <
                0.000001,
            "U1 第 1 段报价正确");
    }


    QVector<PeriodScenario>
        scenarios96;

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
        "MarketData 构建 96 时段场景");

    if (!ok)
    {
        printErrors(errors);
    }

    check(
        scenarios96.size() == 96,
        "96 时段场景数量正确");


    if (ok &&
        !scenarios96.isEmpty())
    {
        check(
            scenarios96[0].period == 1,
            "第一个场景时段编号正确");

        check(
            std::abs(
                scenarios96[0]
                    .intervalHours -
                0.25) <
                0.000001,
            "96 时段时间长度正确");

        check(
            scenarioContainsRenewable(
                scenarios96[0],
                "W1"),
            "96 时段场景包含 W1");

        check(
            scenarioContainsRenewable(
                scenarios96[0],
                "S1"),
            "96 时段场景包含 S1");
    }


    QVector<PeriodScenario>
        scenarios24;

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
        "MarketData 构建 24 时段场景");

    if (!ok)
    {
        printErrors(errors);
    }

    check(
        scenarios24.size() == 24,
        "24 时段场景数量正确");


    if (ok &&
        !scenarios24.isEmpty())
    {
        check(
            std::abs(
                scenarios24[0]
                    .intervalHours -
                1.0) <
                0.000001,
            "24 时段时间长度正确");

        check(
            scenarioContainsRenewable(
                scenarios24[0],
                "W1"),
            "24 时段场景包含 W1");

        check(
            scenarioContainsRenewable(
                scenarios24[0],
                "S1"),
            "24 时段场景包含 S1");
    }


    MarketData copiedData =
        data;

    check(
        copiedData.generatorBids.size() ==
                data.generatorBids.size() &&
            copiedData.consumerBids.size() ==
                data.consumerBids.size() &&
            copiedData.loadCurve.size() ==
                data.loadCurve.size() &&
            copiedData.renewableOutputs.size() ==
                data.renewableOutputs.size(),
        "MarketData 可完整复制传递");


    copiedData.clear();

    check(
        copiedData.generatorBids.isEmpty() &&
            copiedData.consumerBids.isEmpty() &&
            copiedData.loadCurve.isEmpty() &&
            copiedData.renewableOutputs.isEmpty(),
        "MarketData clear 功能正确");


    qInfo().noquote()
        << "===============================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All MarketData integration tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}