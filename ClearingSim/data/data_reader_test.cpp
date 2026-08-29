#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#include "data_reader.h"


// ============================================================
// 打印错误信息
// ============================================================

void printErrors(
    const QStringList& errors)
{
    for (int i = 0;
         i < errors.size();
         ++i)
    {
        qWarning().noquote()
        << "  "
        << errors[i];
    }
}


// ============================================================
// 测试 generator_bids.csv
// ============================================================

bool testGeneratorFile(
    const QString& title,
    const QString& filePath,
    int expectedRows)
{
    qInfo().noquote()
    << "\n========================================";

    qInfo().noquote()
        << title;

    qInfo().noquote()
        << "========================================";


    QVector<GeneratorBid> data;

    QStringList errors;


    bool ok =
        DataReader::readGeneratorBids(
            filePath,
            data,
            errors);


    if (!ok)
    {
        qWarning().noquote()
        << "[FAIL] 文件读取失败";

        printErrors(errors);

        return false;
    }


    qInfo().noquote()
        << "[PASS] 文件读取成功";

    qInfo().noquote()
        << "申报记录数："
        << data.size();


    QSet<QString> generatorIds;


    for (int i = 0;
         i < data.size();
         ++i)
    {
        generatorIds.insert(
            data[i].id);
    }


    qInfo().noquote()
        << "机组数量："
        << generatorIds.size();


    if (data.size() != expectedRows)
    {
        qWarning().noquote()
        << "[FAIL] 数据行数不正确";

        qWarning().noquote()
            << "预期："
            << expectedRows;

        qWarning().noquote()
            << "实际："
            << data.size();

        return false;
    }


    // 只打印前5条，避免场景文件太长
    int printCount =
        data.size();

    if (printCount > 5)
    {
        printCount = 5;
    }


    for (int i = 0;
         i < printCount;
         ++i)
    {
        const GeneratorBid& g =
            data[i];


        qInfo().noquote()
            << QString(
                   "%1 | %2 | %3 | 段%4 | %5 元/MWh | %6 MWh")
                   .arg(g.id)
                   .arg(g.name)
                   .arg(g.type)
                   .arg(g.segment)
                   .arg(
                       g.price,
                       0,
                       'f',
                       3)
                   .arg(
                       g.quantity,
                       0,
                       'f',
                       1);
    }


    return true;
}


// ============================================================
// 测试 consumer_bids.csv
// ============================================================

bool testConsumerFile(
    const QString& filePath)
{
    qInfo().noquote()
    << "\n========================================";

    qInfo().noquote()
        << "TEST - benchmark consumer_bids.csv";

    qInfo().noquote()
        << "========================================";


    QVector<ConsumerBid> data;

    QStringList errors;


    bool ok =
        DataReader::readConsumerBids(
            filePath,
            data,
            errors);


    if (!ok)
    {
        qWarning().noquote()
        << "[FAIL] 文件读取失败";

        printErrors(errors);

        return false;
    }


    if (data.size() != 2)
    {
        qWarning().noquote()
        << "[FAIL] benchmark 应有2条用户申报";

        return false;
    }


    qInfo().noquote()
        << "[PASS] 用户申报读取成功";


    for (int i = 0;
         i < data.size();
         ++i)
    {
        const ConsumerBid& c =
            data[i];


        qInfo().noquote()
            << QString(
                   "%1 | %2 | 段%3 | %4 元/MWh | %5 MWh")
                   .arg(c.id)
                   .arg(c.name)
                   .arg(c.segment)
                   .arg(
                       c.price,
                       0,
                       'f',
                       3)
                   .arg(
                       c.quantity,
                       0,
                       'f',
                       1);
    }


    return true;
}


// ============================================================
// 测试 load_curve.csv
// ============================================================

bool testLoadFile(
    const QString& filePath)
{
    qInfo().noquote()
    << "\n========================================";

    qInfo().noquote()
        << "TEST - load_curve.csv";

    qInfo().noquote()
        << "========================================";


    QVector<LoadPoint> data;

    QStringList errors;


    bool ok =
        DataReader::readLoadCurve(
            filePath,
            data,
            errors);


    if (!ok)
    {
        qWarning().noquote()
        << "[FAIL] 文件读取失败";

        printErrors(errors);

        return false;
    }


    if (data.size() != 96)
    {
        qWarning().noquote()
        << "[FAIL] 负荷曲线应为96行";

        return false;
    }


    qInfo().noquote()
        << "[PASS] 96时段负荷读取成功";

    qInfo().noquote()
        << "数据行数："
        << data.size();


    qInfo().noquote()
        << "第1时段："
        << data.first().time
        << data.first().load
        << "MW";


    qInfo().noquote()
        << "第96时段："
        << data.last().time
        << data.last().load
        << "MW";


    return true;
}


// ============================================================
// 测试 renewable_output.csv
// ============================================================

bool testRenewableFile(
    const QString& filePath)
{
    qInfo().noquote()
    << "\n========================================";

    qInfo().noquote()
        << "TEST - renewable_output.csv";

    qInfo().noquote()
        << "========================================";


    QVector<RenewableOutput> data;

    QStringList errors;


    bool ok =
        DataReader::readRenewableOutput(
            filePath,
            data,
            errors);


    if (!ok)
    {
        qWarning().noquote()
        << "[FAIL] 文件读取失败";

        printErrors(errors);

        return false;
    }


    QSet<QString> generatorIds;


    for (int i = 0;
         i < data.size();
         ++i)
    {
        generatorIds.insert(
            data[i].generatorId);
    }


    qInfo().noquote()
        << "[PASS] 新能源出力读取成功";

    qInfo().noquote()
        << "数据行数："
        << data.size();

    qInfo().noquote()
        << "新能源机组数量："
        << generatorIds.size();


    if (data.size() != 288)
    {
        qWarning().noquote()
        << "[FAIL] 应有 3 x 96 = 288 行新能源数据";

        return false;
    }


    if (generatorIds.size() != 3)
    {
        qWarning().noquote()
        << "[FAIL] 应有3台新能源机组";

        return false;
    }


    return true;
}


// ============================================================
// 自动找到仓库根目录
//
// 当前文件位置：
//
// PowerMarket-Clearing/
// └── ClearingSim/
//     └── data/
//         └── data_reader_test.cpp
//
// 因此从当前源文件目录向上两级即可到仓库根目录。
// ============================================================

QString findRepositoryRoot()
{
    QFileInfo sourceFile(
        QString::fromUtf8(__FILE__));


    QDir directory(
        sourceFile.absolutePath());


    // data -> ClearingSim
    directory.cdUp();


    // ClearingSim -> PowerMarket-Clearing
    directory.cdUp();


    return directory.absolutePath();
}


// ============================================================
// main
// ============================================================

int main(
    int argc,
    char* argv[])
{
    QCoreApplication app(
        argc,
        argv);


    qInfo().noquote()
        << "========================================";

    qInfo().noquote()
        << "A Data Module - Basic Test";

    qInfo().noquote()
        << "========================================";


    // ========================================================
    // 不再使用 REPO_ROOT_DIR 宏
    //
    // 直接根据当前源文件位置找到仓库根目录。
    // ========================================================

    QString repoRoot =
        findRepositoryRoot();


    qInfo().noquote()
        << "Repository root:";

    qInfo().noquote()
        << repoRoot;


    // ========================================================
    // 构造四类CSV路径
    // ========================================================

    QString benchmarkGenerator =
        repoRoot
        +
        "/data/samples/benchmark/generator_bids.csv";


    QString benchmarkConsumer =
        repoRoot
        +
        "/data/samples/benchmark/consumer_bids.csv";


    QString scenarioGenerator =
        repoRoot
        +
        "/data/samples/scenario/generator_bids.csv";


    QString loadCurve =
        repoRoot
        +
        "/data/samples/curves/load_curve.csv";


    QString renewableOutput =
        repoRoot
        +
        "/data/samples/curves/renewable_output.csv";


    qInfo().noquote()
        << "\nCSV path check:";

    qInfo().noquote()
        << benchmarkGenerator;

    qInfo().noquote()
        << benchmarkConsumer;

    qInfo().noquote()
        << scenarioGenerator;

    qInfo().noquote()
        << loadCurve;

    qInfo().noquote()
        << renewableOutput;


    // ========================================================
    // 开始测试
    // ========================================================

    bool allPassed =
        true;


    // --------------------------------------------------------
    // benchmark generator
    // --------------------------------------------------------

    if (!testGeneratorFile(
            "TEST - benchmark generator_bids.csv",
            benchmarkGenerator,
            2))
    {
        allPassed =
            false;
    }


    // --------------------------------------------------------
    // benchmark consumer
    // --------------------------------------------------------

    if (!testConsumerFile(
            benchmarkConsumer))
    {
        allPassed =
            false;
    }


    // --------------------------------------------------------
    // scenario generator
    //
    // 8台机组，共12条分段申报记录
    // --------------------------------------------------------

    if (!testGeneratorFile(
            "TEST - scenario generator_bids.csv",
            scenarioGenerator,
            12))
    {
        allPassed =
            false;
    }


    // --------------------------------------------------------
    // 负荷曲线
    // --------------------------------------------------------

    if (!testLoadFile(
            loadCurve))
    {
        allPassed =
            false;
    }


    // --------------------------------------------------------
    // 新能源曲线
    // --------------------------------------------------------

    if (!testRenewableFile(
            renewableOutput))
    {
        allPassed =
            false;
    }


    // ========================================================
    // 最终结果
    // ========================================================

    qInfo().noquote()
        << "\n========================================";


    if (allPassed)
    {
        qInfo().noquote()
        << "ALL DATA TESTS PASSED";
    }
    else
    {
        qWarning().noquote()
        << "SOME DATA TESTS FAILED";
    }


    qInfo().noquote()
        << "========================================";


    return
        allPassed
            ?
            0
            :
            1;
}