#include "data_reader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

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
    QString root;

    root =
        searchRepoRoot(
            QCoreApplication::applicationDirPath());

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

// 创建临时 CSV 文件
bool writeTextFile(
    const QString &filePath,
    const QString &content)
{
    QFile file(filePath);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text))
    {
        return false;
    }

    QTextStream out(&file);

    out << content;

    return true;
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
        << "========== DataReader Test ==========";

    // 定位项目
    const QString repoRoot =
        findRepoRoot();

    check(
        !repoRoot.isEmpty(),
        "定位项目根目录");

    if (repoRoot.isEmpty())
    {
        qCritical().noquote()
        << "无法找到项目根目录";

        return 1;
    }

    qInfo().noquote()
        << "Repo root:"
        << repoRoot;


    // 设置测试文件
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


    // 测试统一读取接口
    MarketData marketData;
    QStringList errors;

    bool ok =
        DataReader::readAll(
            files,
            marketData,
            errors);

    check(
        ok,
        "统一读取四类市场数据");

    if (!ok)
    {
        printErrors(errors);
    }
    else
    {
        qInfo().noquote()
        << "Generator bids:"
        << marketData.generatorBids.size();

        qInfo().noquote()
            << "Consumer bids:"
            << marketData.consumerBids.size();

        qInfo().noquote()
            << "Load points:"
            << marketData.loadCurve.size();

        qInfo().noquote()
            << "Renewable outputs:"
            << marketData.renewableOutputs.size();
    }


    // 异常数据测试
    QTemporaryDir tempDir;

    check(
        tempDir.isValid(),
        "创建临时测试目录");

    if (tempDir.isValid())
    {
        QVector<GeneratorBid> generators;
        QVector<ConsumerBid> consumers;
        QVector<LoadPoint> loadPoints;
        QVector<RenewableOutput> renewable;


        // 重复机组申报分段
        const QString duplicateSegmentFile =
            tempDir.path() +
            "/duplicate_segment.csv";

        writeTextFile(
            duplicateSegmentFile,
            "id,name,type,segment,price,quantity\n"
            "G01,Generator1,thermal,1,300,50\n"
            "G01,Generator1,thermal,1,320,60\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                duplicateSegmentFile,
                generators,
                errors);

        check(
            !ok,
            "识别重复机组申报分段");

        if (!ok)
        {
            printErrors(errors);
        }


        // 非法数字
        const QString invalidPriceFile =
            tempDir.path() +
            "/invalid_price.csv";

        writeTextFile(
            invalidPriceFile,
            "id,name,type,segment,price,quantity\n"
            "G01,Generator1,thermal,1,abc,50\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                invalidPriceFile,
                generators,
                errors);

        check(
            !ok,
            "识别非法数字");

        if (!ok)
        {
            printErrors(errors);
        }


        // 负数申报电量
        const QString negativeQuantityFile =
            tempDir.path() +
            "/negative_quantity.csv";

        writeTextFile(
            negativeQuantityFile,
            "id,name,segment,price,quantity\n"
            "C01,Consumer1,1,600,-20\n");

        errors.clear();

        ok =
            DataReader::readConsumerBids(
                negativeQuantityFile,
                consumers,
                errors);

        check(
            !ok,
            "识别负数申报电量");

        if (!ok)
        {
            printErrors(errors);
        }


        // CSV 列数错误
        const QString columnErrorFile =
            tempDir.path() +
            "/column_error.csv";

        writeTextFile(
            columnErrorFile,
            "id,name,type,segment,price,quantity\n"
            "G01,Generator1,thermal,1,300\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                columnErrorFile,
                generators,
                errors);

        check(
            !ok,
            "识别 CSV 列数错误");

        if (!ok)
        {
            printErrors(errors);
        }


        // 重复负荷时段
        const QString duplicatePeriodFile =
            tempDir.path() +
            "/duplicate_period.csv";

        writeTextFile(
            duplicatePeriodFile,
            "period,time,load\n"
            "1,00:00,500\n"
            "1,01:00,520\n");

        errors.clear();

        ok =
            DataReader::readLoadCurve(
                duplicatePeriodFile,
                loadPoints,
                errors);

        check(
            !ok,
            "识别重复负荷时段");

        if (!ok)
        {
            printErrors(errors);
        }


        // 新能源重复时段
        const QString duplicateRenewableFile =
            tempDir.path() +
            "/duplicate_renewable.csv";

        writeTextFile(
            duplicateRenewableFile,
            "generator_id,generator_type,period,output\n"
            "W01,wind,1,80\n"
            "W01,wind,1,85\n");

        errors.clear();

        ok =
            DataReader::readRenewableOutput(
                duplicateRenewableFile,
                renewable,
                errors);

        check(
            !ok,
            "识别新能源重复时段");

        if (!ok)
        {
            printErrors(errors);
        }
    }


    qInfo().noquote()
        << "====================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All DataReader tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}