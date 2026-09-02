#include "data_reader.h"
#include "scenario_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>

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
    for (const QString &error :
         errors)
    {
        qInfo().noquote()
        << "   "
        << error;
    }
}


QSet<QString> generatorIds(
    const QVector<GeneratorBid> &data)
{
    QSet<QString> ids;

    for (const GeneratorBid &bid :
         data)
    {
        ids.insert(
            bid.id);
    }

    return ids;
}


QSet<QString> consumerIds(
    const QVector<ConsumerBid> &data)
{
    QSet<QString> ids;

    for (const ConsumerBid &bid :
         data)
    {
        ids.insert(
            bid.id);
    }

    return ids;
}


QSet<QString> renewableIds(
    const QVector<RenewableOutput> &data)
{
    QSet<QString> ids;

    for (const RenewableOutput &item :
         data)
    {
        ids.insert(
            item.generatorId);
    }

    return ids;
}


int countGeneratorBids(
    const MarketData &data,
    int period)
{
    int count = 0;

    for (const GeneratorBid &bid :
         data.generatorBids)
    {
        if (bid.period ==
            period)
        {
            ++count;
        }
    }

    return count;
}


int countConsumerBids(
    const MarketData &data,
    int period)
{
    int count = 0;

    for (const ConsumerBid &bid :
         data.consumerBids)
    {
        if (bid.period ==
            period)
        {
            ++count;
        }
    }

    return count;
}


bool scenarioMatchesMarketData(
    const PeriodScenario &scenario,
    const MarketData &data)
{
    return
        scenario.generatorBids.size() ==
            countGeneratorBids(
                data,
                scenario.period) &&
        scenario.consumerBids.size() ==
            countConsumerBids(
                data,
                scenario.period);
}


bool runPipelineTest(
    const QString &scenarioDir,
    int periodCount,
    TimeGranularity granularity)
{
    qInfo().noquote()
    << "-----"
    << periodCount
    << "period pipeline -----";


    MarketData data;

    QStringList errors;

    bool ok =
        DataReader::readAll(
            makeFileSet(
                scenarioDir,
                periodCount),
            data,
            errors);

    check(
        ok,
        QString(
            "%1 时段 CSV → MarketData")
            .arg(periodCount));

    if (!ok)
    {
        printErrors(errors);
        return false;
    }


    check(
        data.loadCurve.size() ==
            periodCount,
        QString(
            "%1 时段负荷数量正确")
            .arg(periodCount));


    const QSet<QString> genIds =
        generatorIds(
            data.generatorBids);

    const QSet<QString> conIds =
        consumerIds(
            data.consumerBids);

    const QSet<QString> renIds =
        renewableIds(
            data.renewableOutputs);


    check(
        !genIds.isEmpty(),
        QString(
            "%1 时段包含发电主体")
            .arg(periodCount));

    check(
        !conIds.isEmpty(),
        QString(
            "%1 时段包含购电主体")
            .arg(periodCount));

    check(
        !renIds.isEmpty(),
        QString(
            "%1 时段包含新能源主体")
            .arg(periodCount));


    QVector<PeriodScenario>
        scenarios;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data,
            granularity,
            scenarios,
            errors);

    check(
        ok,
        QString(
            "%1 时段 MarketData → PeriodScenario")
            .arg(periodCount));

    if (!ok)
    {
        printErrors(errors);
        return false;
    }


    check(
        scenarios.size() ==
            periodCount,
        QString(
            "%1 时段场景数量正确")
            .arg(periodCount));


    if (!scenarios.isEmpty())
    {
        const PeriodScenario &first =
            scenarios.first();

        const PeriodScenario &last =
            scenarios.last();

        check(
            first.period == 1 &&
                last.period == periodCount,
            QString(
                "%1 时段场景编号完整")
                .arg(periodCount));

        check(
            scenarioMatchesMarketData(
                first,
                data),
            QString(
                "%1 时段第1场景申报数量与 MarketData 一致")
                .arg(periodCount));

        check(
            scenarioMatchesMarketData(
                last,
                data),
            QString(
                "%1 时段最后场景申报数量与 MarketData 一致")
                .arg(periodCount));

        check(
            !first.generatorBids.isEmpty() &&
                !first.consumerBids.isEmpty(),
            QString(
                "%1 时段单场景可直接提供给算法")
                .arg(periodCount));
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
        QString(
            "%1 时段 MarketData 可完整复制")
            .arg(periodCount));


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
        << "========== MarketData Integration V1.3 Test ==========";


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
        "定位 scenario 场景目录");

    if (!QDir(scenarioDir).exists())
    {
        return 1;
    }


    runPipelineTest(
        scenarioDir,
        24,
        TimeGranularity::
        Hourly24);


    runPipelineTest(
        scenarioDir,
        96,
        TimeGranularity::
        QuarterHourly96);


    qInfo().noquote()
        << "===============================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All MarketData Integration V1.3 tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}