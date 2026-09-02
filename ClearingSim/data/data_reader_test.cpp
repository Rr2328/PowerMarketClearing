#include "data_reader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
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
    for (const QString &error :
         errors)
    {
        qInfo().noquote()
        << "   " << error;
    }
}

// 生成时刻
QString timeText(int period)
{
    const int totalMinutes =
        period * 15;

    if (totalMinutes == 1440)
    {
        return "24:00";
    }

    return QString("%1:%2")
        .arg(
            totalMinutes / 60,
            2,
            10,
            QChar('0'))
        .arg(
            totalMinutes % 60,
            2,
            10,
            QChar('0'));
}

// 生成负荷测试文件
QString buildLoadCsv(int count)
{
    QString content =
        "时段,时刻,负荷(MW)\n";

    for (int period = 1;
         period <= count;
         ++period)
    {
        content +=
            QString("%1,%2,700.0\n")
                .arg(period)
                .arg(timeText(period));
    }

    return content;
}

// 生成新能源测试文件
QString buildRenewableCsv(
    int count)
{
    QString content =
        "机组ID,机组类型,时段,出力(MW)\n";

    for (int period = 1;
         period <= count;
         ++period)
    {
        content +=
            QString(
                "W1,风电,%1,40.0\n")
                .arg(period);
    }

    return content;
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
        << "========== DataReader V1.3 Test ==========";

    const QString repoRoot =
        findRepoRoot();

    check(
        !repoRoot.isEmpty(),
        "定位项目根目录");

    if (repoRoot.isEmpty())
    {
        return 1;
    }

    qInfo().noquote()
        << "Repo root:"
        << repoRoot;


    // 真实样例数据
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


    MarketData marketData;
    QStringList errors;

    bool     ok =
        DataReader::readAll(
            files,
            marketData,
            errors);

    check(
        ok,
        "窄表基准例读取（自动展开 96 期）");

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

        // 窄表展开：申报条数 = 原行数 × 96，且各时段同量同价
        const int genRows =
            marketData.generatorBids.size() / 96;

        const int conRows =
            marketData.consumerBids.size() / 96;

        check(
            genRows > 0 &&
                marketData.generatorBids.size() ==
                    genRows * 96,
            "发电窄表展开为 96 期（行数 × 96）");

        check(
            conRows > 0 &&
                marketData.consumerBids.size() ==
                    conRows * 96,
            "购电窄表展开为 96 期（行数 × 96）");

        bool expandConsistent = true;

        // 展开顺序：每个申报行连续展开 96 期（bids[i] 的 period = i%96+1，
        // 基准行 = 同组首行）
        for (int i = 0;
             i < marketData.generatorBids.size();
             ++i)
        {
            const GeneratorBid &base =
                marketData.generatorBids[
                    i - i % 96];

            const GeneratorBid &item =
                marketData.generatorBids[i];

            if (item.period !=
                    i % 96 + 1 ||
                item.id != base.id ||
                item.price !=
                    base.price ||
                item.quantity !=
                    base.quantity)
            {
                expandConsistent = false;

                break;
            }
        }

        check(
            expandConsistent,
            "窄表展开各时段同量同价（对拍锚点等价性）");
    }


    // V1.3：跨文件校验不再阻断（新能源不进申报表；平衡偏差改 P1 提示）
    errors.clear();

    ok =
        DataReader::validateRelations(
            marketData,
            errors);

    check(
        ok,
        "跨文件校验通过（V1.3：无阻断项）");

    if (!ok)
    {
        printErrors(errors);
    }


    // V1.3 长表真实样例（scenario 双侧逐时段申报）
    DataFileSet scenarioFiles;

    scenarioFiles.generatorBidsFile =
        repoRoot +
        "/data/samples/scenario/generator_bids.csv";

    scenarioFiles.consumerBidsFile =
        repoRoot +
        "/data/samples/scenario/consumer_bids.csv";

    scenarioFiles.loadCurveFile =
        repoRoot +
        "/data/samples/curves/load_curve.csv";

    scenarioFiles.renewableOutputFile =
        repoRoot +
        "/data/samples/curves/renewable_output.csv";

    MarketData scenarioData;

    errors.clear();

    ok =
        DataReader::readAll(
            scenarioFiles,
            scenarioData,
            errors);

    check(
        ok,
        "V1.3 长表样例读取（双侧逐时段）");

    if (!ok)
    {
        printErrors(errors);
    }
    else
    {
        // 每个主体（电厂名称+机组编号）必须覆盖 96 个时段
        QHash<QString, QSet<int>>
            genPeriods;

        for (const GeneratorBid &item :
             scenarioData.generatorBids)
        {
            genPeriods[item.name + "|" +
                       item.id]
                .insert(item.period);
        }

        bool allCovered = true;

        for (auto it =
             genPeriods.cbegin();
             it != genPeriods.cend();
             ++it)
        {
            if (it.value().size() != 96)
            {
                allCovered = false;

                break;
            }
        }

        check(
            allCovered &&
                !genPeriods.isEmpty(),
            "长表发电主体全部覆盖 96 时段");

        QHash<QString, QSet<int>>
            conPeriods;

        for (const ConsumerBid &item :
             scenarioData.consumerBids)
        {
            conPeriods[item.name + "|" +
                       item.id]
                .insert(item.period);
        }

        bool conCovered = true;

        for (auto it =
             conPeriods.cbegin();
             it != conPeriods.cend();
             ++it)
        {
            if (it.value().size() != 96)
            {
                conCovered = false;

                break;
            }
        }

        check(
            conCovered &&
                !conPeriods.isEmpty(),
            "长表购电主体全部覆盖 96 时段");

        // 跨厂重号检查：机组编号可重号（金陵 #1 / 龙潭 #1），身份 = 名称+编号
        check(
            genPeriods.contains(
                QStringLiteral(
                    "金陵电厂|#1机组")) &&
                genPeriods.contains(
                    QStringLiteral(
                        "龙潭电厂|#1机组")),
            "机组编号跨厂重号共存（身份 = 电厂名称+机组编号）");
    }


    QTemporaryDir tempDir;

    check(
        tempDir.isValid(),
        "创建临时测试目录");

    if (tempDir.isValid())
    {
        QVector<GeneratorBid>
            generators;

        QVector<ConsumerBid>
            consumers;

        QVector<LoadPoint>
            loadPoints;

        QVector<RenewableOutput>
            renewable;


        // 表头错误
        const QString badHeaderFile =
            tempDir.path() +
            "/bad_header.csv";

        writeTextFile(
            badHeaderFile,
            "id,name,type,segment,price,quantity\n"
            "G1,一号火电,火电,1,150.000,100.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                badHeaderFile,
                generators,
                errors);

        check(
            !ok,
            "识别固定表头错误");


        // 超过 5 个申报段
        const QString tooManySegments =
            tempDir.path() +
            "/too_many_segments.csv";

        writeTextFile(
            tooManySegments,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,100.000,10.0\n"
            "G1,一号火电,火电,2,110.000,10.0\n"
            "G1,一号火电,火电,3,120.000,10.0\n"
            "G1,一号火电,火电,4,130.000,10.0\n"
            "G1,一号火电,火电,5,140.000,10.0\n"
            "G1,一号火电,火电,6,150.000,10.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                tooManySegments,
                generators,
                errors);

        check(
            !ok,
            "识别超过 5 个申报段");


        // 申报段不连续
        const QString gapSegmentFile =
            tempDir.path() +
            "/gap_segment.csv";

        writeTextFile(
            gapSegmentFile,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,100.000,10.0\n"
            "G1,一号火电,火电,3,120.000,10.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                gapSegmentFile,
                generators,
                errors);

        check(
            !ok,
            "识别申报段不连续");


        // 发电报价方向错误
        const QString generatorPriceFile =
            tempDir.path() +
            "/generator_price.csv";

        writeTextFile(
            generatorPriceFile,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,200.000,10.0\n"
            "G1,一号火电,火电,2,150.000,10.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                generatorPriceFile,
                generators,
                errors);

        check(
            !ok,
            "识别发电报价单调错误");


        // 购电报价方向错误
        const QString consumerPriceFile =
            tempDir.path() +
            "/consumer_price.csv";

        writeTextFile(
            consumerPriceFile,
            "用户ID,用户名称,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "U1,一号用户,1,200.000,10.0\n"
            "U1,一号用户,2,300.000,10.0\n");

        errors.clear();

        ok =
            DataReader::readConsumerBids(
                consumerPriceFile,
                consumers,
                errors);

        check(
            !ok,
            "识别购电报价单调错误");


        // 电价越界
        const QString priceLimitFile =
            tempDir.path() +
            "/price_limit.csv";

        writeTextFile(
            priceLimitFile,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,541.000,10.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                priceLimitFile,
                generators,
                errors);

        check(
            !ok,
            "识别 0~540 电价限制");


        // 电量为 0：V1.3 规则⑥允许（该时段不申报/停机）
        const QString zeroQuantityFile =
            tempDir.path() +
            "/zero_quantity.csv";

        writeTextFile(
            zeroQuantityFile,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,150.000,0.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                zeroQuantityFile,
                generators,
                errors);

        check(
            ok,
            "申报电量 0 合法（V1.3 规则⑥：停机申报）");

        if (!ok)
        {
            printErrors(errors);
        }


        // 电量为负
        const QString negativeQuantityFile =
            tempDir.path() +
            "/negative_quantity.csv";

        writeTextFile(
            negativeQuantityFile,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,150.000,-5.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                negativeQuantityFile,
                generators,
                errors);

        check(
            !ok,
            "识别申报电量为负");


        // 小数精度错误
        const QString precisionFile =
            tempDir.path() +
            "/precision.csv";

        writeTextFile(
            precisionFile,
            "机组ID,机组名称,机组类型,申报段,申报电价(元/MWh),申报电量(MWh)\n"
            "G1,一号火电,火电,1,150.00,100.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                precisionFile,
                generators,
                errors);

        check(
            !ok,
            "识别申报价格小数精度错误");


        // 长表表头段列不成对（5 列 = 3 基础列 + 1 段出力，缺段报价）
        const QString badLongHeaderFile =
            tempDir.path() +
            "/bad_long_header.csv";

        writeTextFile(
            badLongHeaderFile,
            "period,电厂名称,机组编号,第1段出力(MW)\n"
            "1,金陵电厂,#1机组,100.0\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                badLongHeaderFile,
                generators,
                errors);

        check(
            !ok,
            "识别长表表头段列不成对");

        if (!ok)
        {
            printErrors(errors);
        }


        // 长表时段覆盖不足（主体只申报 1 个时段）
        const QString shortLongFile =
            tempDir.path() +
            "/short_long.csv";

        writeTextFile(
            shortLongFile,
            "period,电厂名称,机组编号,第1段出力(MW),第1段报价(元/MWh)\n"
            "1,金陵电厂,#1机组,100.0,150.000\n");

        errors.clear();

        ok =
            DataReader::readGeneratorBids(
                shortLongFile,
                generators,
                errors);

        check(
            !ok,
            "识别长表主体未覆盖 96 时段");


        // 负荷不足 96 点
        const QString shortLoadFile =
            tempDir.path() +
            "/short_load.csv";

        writeTextFile(
            shortLoadFile,
            buildLoadCsv(95));

        errors.clear();

        ok =
            DataReader::readLoadCurve(
                shortLoadFile,
                loadPoints,
                errors);

        check(
            !ok,
            "识别负荷曲线不足 96 点");


        // 新能源不足 96 点
        const QString shortRenewableFile =
            tempDir.path() +
            "/short_renewable.csv";

        writeTextFile(
            shortRenewableFile,
            buildRenewableCsv(95));

        errors.clear();

        ok =
            DataReader::readRenewableOutput(
                shortRenewableFile,
                renewable,
                errors);

        check(
            !ok,
            "识别新能源机组不足 96 点");
    }


    qInfo().noquote()
        << "========================================";

    if (failedTests == 0)
    {
        qInfo().noquote()
        << "All DataReader V1.3 tests passed.";

        return 0;
    }

    qCritical().noquote()
        << failedTests
        << "test(s) failed.";

    return 1;
}