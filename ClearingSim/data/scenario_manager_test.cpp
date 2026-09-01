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

// 测试结果输出
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

// 查找项目根目录
QString searchRepoRoot(
    const QString &startPath)
{
    QDir dir(startPath);

    while (true)
    {
        if (dir.exists("ClearingSim") &&
            dir.exists("data/samples"))
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

// 自动定位项目根目录
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

// 输出错误信息
void printErrors(
    const QStringList &errors)
{
    for (const QString &error : errors)
    {
        qInfo().noquote()
        << "   " << error;
    }
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
        << "========== ScenarioManager Test ==========";

    const QString repoRoot =
        findRepoRoot();

    check(
        !repoRoot.isEmpty(),
        "定位项目根目录");

    if (repoRoot.isEmpty())
    {
        return 1;
    }

    DataFileSet files;

    files.generatorBidsFile =
        repoRoot +
        "/data/samples/benchmark/generator_bids.csv";

    files.consumerBidsFile =
        repoRoot +
        "/data/samples/benchmark/consumer_bids.csv";

    files.loadCurveFile =
        repoRoot +
        "/data/samples/curves/load_curve.csv";

    files.renewableOutputFile =
        repoRoot +
        "/data/samples/curves/renewable_output.csv";

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


    // 检查首小时平均值
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


    // 构建 96 时段场景
    QVector<PeriodScenario> scenarios96;

    errors.clear();

    ok =
        ScenarioManager::
        buildPeriodScenarios(
            data,
            96,
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
            24,
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
            48,
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
            96,
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
            96,
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