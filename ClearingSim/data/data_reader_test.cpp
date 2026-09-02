#include "data_reader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{

QString findProjectRoot()
{
    QDir dir(
        QCoreApplication::applicationDirPath());

    while (true)
    {
        const bool hasCMake =
            QFileInfo::exists(
                dir.filePath("CMakeLists.txt"));

        const bool hasDataReader =
            QFileInfo::exists(
                dir.filePath(
                    "data/data_reader.cpp"));

        if (hasCMake &&
            hasDataReader)
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


QString findRepositoryRoot(
    const QString &projectRoot)
{
    if (projectRoot.isEmpty())
    {
        return QString();
    }

    QDir dir(projectRoot);

    if (!dir.cdUp())
    {
        return QString();
    }

    return dir.absolutePath();
}


QString scenarioDataDirectory(
    const QString &repositoryRoot)
{
    return QDir(repositoryRoot)
    .filePath(
        "data/samples/scenario");
}


int countScenarioFiles(
    const QString &directory)
{
    const QStringList fileNames =
        {
            "generator_bids.csv",
            "consumer_bids.csv",
            "load_curve.csv",
            "renewable_output.csv"
        };

    int count = 0;

    for (const QString &fileName :
         fileNames)
    {
        const QString filePath =
            QDir(directory)
                .filePath(fileName);

        if (QFileInfo::exists(filePath))
        {
            ++count;
        }
    }

    return count;
}


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

    out.setEncoding(
        QStringConverter::Utf8);

    out << content;

    return true;
}


QStringList generatorHeader()
{
    QStringList header;

    header
        << "电厂名称"
        << "机组编号"
        << "机组类型";

    for (int segment = 1;
         segment <= 10;
         ++segment)
    {
        header
            << QString(
                   "出力P%1(MW)")
                   .arg(segment)
            << QString(
                   "报价C%1(元/MWh)")
                   .arg(segment);
    }

    return header;
}


QStringList consumerHeader()
{
    QStringList header;

    header
        << "用户名称"
        << "用户编号";

    for (int segment = 1;
         segment <= 10;
         ++segment)
    {
        header
            << QString(
                   "购电量P%1(MWh)")
                   .arg(segment)
            << QString(
                   "报价C%1(元/MWh)")
                   .arg(segment);
    }

    return header;
}


QString makeGeneratorRow(
    const QString &plantName,
    const QString &unitId,
    const QString &unitType,
    const QVector<QPair<QString, QString>>
        &segments)
{
    QStringList columns;

    columns
        << plantName
        << unitId
        << unitType;

    for (int i = 0;
         i < 10;
         ++i)
    {
        if (i < segments.size())
        {
            columns
                << segments[i].first
                << segments[i].second;
        }
        else
        {
            columns
                << ""
                << "";
        }
    }

    return columns.join(',');
}


QString makeConsumerRow(
    const QString &consumerName,
    const QString &consumerId,
    const QVector<QPair<QString, QString>>
        &segments)
{
    QStringList columns;

    columns
        << consumerName
        << consumerId;

    for (int i = 0;
         i < 10;
         ++i)
    {
        if (i < segments.size())
        {
            columns
                << segments[i].first
                << segments[i].second;
        }
        else
        {
            columns
                << ""
                << "";
        }
    }

    return columns.join(',');
}


QString makeCsv(
    const QStringList &header,
    const QStringList &rows)
{
    QString content;

    content +=
        header.join(',');

    content += '\n';

    for (const QString &row :
         rows)
    {
        content += row;
        content += '\n';
    }

    return content;
}


QString makeLoadCsv(
    int periodCount)
{
    QString content =
        "时段,时刻,负荷(MW)\n";

    for (int period = 1;
         period <= periodCount;
         ++period)
    {
        const int totalMinutes =
            (period - 1) * 15;

        const int hour =
            totalMinutes / 60;

        const int minute =
            totalMinutes % 60;

        const QString time =
            QString("%1:%2")
                .arg(
                    hour,
                    2,
                    10,
                    QChar('0'))
                .arg(
                    minute,
                    2,
                    10,
                    QChar('0'));

        content +=
            QString("%1,%2,%3\n")
                .arg(period)
                .arg(time)
                .arg(
                    500.0 +
                        period,
                    0,
                    'f',
                    1);
    }

    return content;
}


QString makeRenewableCsv(
    const QString &generatorId,
    const QString &generatorType,
    int periodCount)
{
    QString content =
        "机组ID,机组类型,时段,出力(MW)\n";

    for (int period = 1;
         period <= periodCount;
         ++period)
    {
        content +=
            QString("%1,%2,%3,%4\n")
                .arg(generatorId)
                .arg(generatorType)
                .arg(period)
                .arg(
                    30.0 +
                        period * 0.1,
                    0,
                    'f',
                    1);
    }

    return content;
}


bool containsError(
    const QStringList &errors,
    const QString &keyword)
{
    for (const QString &error :
         errors)
    {
        if (error.contains(keyword))
        {
            return true;
        }
    }

    return false;
}


void printErrors(
    const QStringList &errors)
{
    for (const QString &error :
         errors)
    {
        qInfo()
        .noquote()
            << "   "
            << error;
    }
}


void check(
    bool condition,
    const QString &testName,
    int &failedTests,
    const QStringList &errors =
    QStringList())
{
    if (condition)
    {
        qInfo()
        .noquote()
            << "[PASS]"
            << testName;
    }
    else
    {
        ++failedTests;

        qInfo()
                .noquote()
            << "[FAIL]"
            << testName;

        printErrors(errors);
    }
}


GeneratorBid makeGeneratorBid(
    const QString &id,
    const QString &name,
    const QString &type)
{
    GeneratorBid bid;

    bid.id = id;
    bid.name = name;
    bid.type = type;

    bid.segment = 1;
    bid.price = 0.0;
    bid.quantity = 50.0;

    return bid;
}


RenewableOutput makeRenewableOutput(
    const QString &id,
    const QString &type)
{
    RenewableOutput output;

    output.generatorId = id;
    output.generatorType = type;

    output.period = 1;
    output.output = 30.0;

    return output;
}

}


int main(
    int argc,
    char *argv[])
{
    QCoreApplication app(
        argc,
        argv);

    int failedTests = 0;

    qInfo()
            .noquote()
        << "========== DataReader V1.2 Test ==========";


    // 项目目录
    const QString projectRoot =
        findProjectRoot();

    check(
        !projectRoot.isEmpty(),
        "定位 ClearingSim 项目根目录",
        failedTests);

    if (projectRoot.isEmpty())
    {
        qInfo()
        .noquote()
            << "========================================";

        qInfo()
                .noquote()
            << failedTests
            << "test(s) failed.";

        return 1;
    }

    qInfo()
            .noquote()
        << "Project root:"
        << QDir::toNativeSeparators(
               projectRoot);


    // 仓库目录
    const QString repositoryRoot =
        findRepositoryRoot(
            projectRoot);

    check(
        !repositoryRoot.isEmpty(),
        "定位仓库根目录",
        failedTests);

    qInfo()
            .noquote()
        << "Repository root:"
        << QDir::toNativeSeparators(
               repositoryRoot);


    // 场景数据目录
    const QString dataDirectory =
        scenarioDataDirectory(
            repositoryRoot);

    const bool dataDirExists =
        QDir(dataDirectory).exists();

    check(
        dataDirExists,
        "定位 scenario 场景数据目录",
        failedTests);

    qInfo()
            .noquote()
        << "Scenario directory:"
        << QDir::toNativeSeparators(
               dataDirectory);


    // 四类真实 CSV
    const int scenarioFileCount =
        countScenarioFiles(
            dataDirectory);

    check(
        scenarioFileCount == 4,
        QString(
            "定位四类真实 CSV 数据（%1/4）")
            .arg(scenarioFileCount),
        failedTests);

    if (scenarioFileCount != 4)
    {
        const QStringList fileNames =
            {
                "generator_bids.csv",
                "consumer_bids.csv",
                "load_curve.csv",
                "renewable_output.csv"
            };

        for (const QString &fileName :
             fileNames)
        {
            const QString filePath =
                QDir(dataDirectory)
                    .filePath(fileName);

            if (QFileInfo::exists(filePath))
            {
                qInfo()
                .noquote()
                    << "   [FOUND]"
                    << fileName;
            }
            else
            {
                qInfo()
                .noquote()
                    << "   [MISSING]"
                    << fileName;
            }
        }
    }


    // 四类真实数据读取
    DataFileSet realFiles;

    realFiles.generatorBidsFile =
        QDir(dataDirectory)
            .filePath(
                "generator_bids.csv");

    realFiles.consumerBidsFile =
        QDir(dataDirectory)
            .filePath(
                "consumer_bids.csv");

    realFiles.loadCurveFile =
        QDir(dataDirectory)
            .filePath(
                "load_curve.csv");

    realFiles.renewableOutputFile =
        QDir(dataDirectory)
            .filePath(
                "renewable_output.csv");

    MarketData realData;
    QStringList errors;

    const bool realReadOk =
        DataReader::readAll(
            realFiles,
            realData,
            errors);

    check(
        realReadOk,
        "横向 P/C 格式读取四类真实数据",
        failedTests,
        errors);


    // 跨文件 ID 错误
    {
        MarketData inconsistentData;

        inconsistentData
            .generatorBids
            .push_back(
                makeGeneratorBid(
                    "W1",
                    "测试风电场",
                    "风电"));

        inconsistentData
            .renewableOutputs
            .push_back(
                makeRenewableOutput(
                    "X1",
                    "风电"));

        errors.clear();

        const bool result =
            DataReader::validateRelations(
                inconsistentData,
                errors);

        const bool detected =
            !result &&
            containsError(
                errors,
                "在发电侧申报中不存在");

        check(
            detected,
            "识别新能源机组 ID 跨文件不一致",
            failedTests,
            errors);
    }


    // 跨文件正确数据
    {
        MarketData consistentData;

        consistentData
            .generatorBids
            .push_back(
                makeGeneratorBid(
                    "W1",
                    "测试风电场",
                    "风电"));

        consistentData
            .generatorBids
            .push_back(
                makeGeneratorBid(
                    "S1",
                    "测试光伏电站",
                    "光伏"));

        consistentData
            .renewableOutputs
            .push_back(
                makeRenewableOutput(
                    "W1",
                    "风电"));

        consistentData
            .renewableOutputs
            .push_back(
                makeRenewableOutput(
                    "S1",
                    "光伏"));

        errors.clear();

        const bool result =
            DataReader::validateRelations(
                consistentData,
                errors);

        check(
            result &&
                errors.isEmpty(),
            "跨文件一致数据通过校验",
            failedTests,
            errors);
    }


    // 临时测试目录
    QTemporaryDir tempDir;

    check(
        tempDir.isValid(),
        "创建临时测试目录",
        failedTests);

    if (!tempDir.isValid())
    {
        qInfo()
        .noquote()
            << "========================================";

        qInfo()
                .noquote()
            << failedTests
            << "test(s) failed.";

        return 1;
    }

    const QString tempPath =
        tempDir.path();


    // 固定表头错误
    {
        QStringList header =
            generatorHeader();

        header[0] =
            "错误表头";

        const QString filePath =
            tempPath +
            "/bad_header.csv";

        const QString row =
            makeGeneratorRow(
                "测试电厂",
                "G1",
                "火电",
                {
                    {
                        "100.0",
                        "150.000"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                header,
                {row}));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "表头"),
            "识别固定表头错误",
            failedTests,
            errors);
    }


    // 超过 10 个申报段
    {
        QStringList header =
            generatorHeader();

        header
            << "出力P11(MW)"
            << "报价C11(元/MWh)";

        QStringList columns;

        columns
            << "测试电厂"
            << "G1"
            << "火电";

        for (int segment = 1;
             segment <= 11;
             ++segment)
        {
            columns
                << "10.0"
                << QString::number(
                       100 +
                       segment);
        }

        const QString filePath =
            tempPath +
            "/too_many_segments.csv";

        writeTextFile(
            filePath,
            makeCsv(
                header,
                {
                    columns.join(',')
                }));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                !errors.isEmpty(),
            "识别超过 10 个申报段",
            failedTests,
            errors);
    }


    // 申报段不连续
    {
        const QString filePath =
            tempPath +
            "/discontinuous_segments.csv";

        const QString row =
            makeGeneratorRow(
                "测试电厂",
                "G1",
                "火电",
                {
                    {
                        "50.0",
                        "150.000"
                    },
                    {
                        "",
                        ""
                    },
                    {
                        "30.0",
                        "200.000"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                generatorHeader(),
                {row}));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "不连续"),
            "识别申报段不连续",
            failedTests,
            errors);
    }


    // 发电报价单调
    {
        const QString filePath =
            tempPath +
            "/generator_monotonic.csv";

        const QString row =
            makeGeneratorRow(
                "测试电厂",
                "G1",
                "火电",
                {
                    {
                        "50.0",
                        "200.000"
                    },
                    {
                        "50.0",
                        "180.000"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                generatorHeader(),
                {row}));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "单调非递减"),
            "识别发电报价单调错误",
            failedTests,
            errors);
    }


    // 购电报价单调
    {
        const QString filePath =
            tempPath +
            "/consumer_monotonic.csv";

        const QString row =
            makeConsumerRow(
                "测试用户",
                "U1",
                {
                    {
                        "50.0",
                        "300.000"
                    },
                    {
                        "40.0",
                        "350.000"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                consumerHeader(),
                {row}));

        QVector<ConsumerBid> data;

        errors.clear();

        const bool result =
            DataReader::readConsumerBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "单调非递增"),
            "识别购电报价单调错误",
            failedTests,
            errors);
    }


    // 电价范围
    {
        const QString filePath =
            tempPath +
            "/price_range.csv";

        const QString row =
            makeGeneratorRow(
                "测试电厂",
                "G1",
                "火电",
                {
                    {
                        "50.0",
                        "541.000"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                generatorHeader(),
                {row}));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "0～540"),
            "识别 0~540 电价限制",
            failedTests,
            errors);
    }


    // 申报电量
    {
        const QString filePath =
            tempPath +
            "/zero_quantity.csv";

        const QString row =
            makeGeneratorRow(
                "测试电厂",
                "G1",
                "火电",
                {
                    {
                        "0",
                        "150.000"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                generatorHeader(),
                {row}));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "必须大于 0"),
            "识别申报电量必须大于 0",
            failedTests,
            errors);
    }


    // 电价精度
    {
        const QString filePath =
            tempPath +
            "/price_precision.csv";

        const QString row =
            makeGeneratorRow(
                "测试电厂",
                "G1",
                "火电",
                {
                    {
                        "50.0",
                        "150.1234"
                    }
                });

        writeTextFile(
            filePath,
            makeCsv(
                generatorHeader(),
                {row}));

        QVector<GeneratorBid> data;

        errors.clear();

        const bool result =
            DataReader::readGeneratorBids(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "3 位小数"),
            "识别申报价格小数精度错误",
            failedTests,
            errors);
    }


    // 负荷不足 96 点
    {
        const QString filePath =
            tempPath +
            "/short_load.csv";

        writeTextFile(
            filePath,
            makeLoadCsv(
                95));

        QVector<LoadPoint> data;

        errors.clear();

        const bool result =
            DataReader::readLoadCurve(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "96"),
            "识别负荷曲线不足 96 点",
            failedTests,
            errors);
    }


    // 新能源不足 96 点
    {
        const QString filePath =
            tempPath +
            "/short_renewable.csv";

        writeTextFile(
            filePath,
            makeRenewableCsv(
                "W1",
                "风电",
                95));

        QVector<RenewableOutput> data;

        errors.clear();

        const bool result =
            DataReader::readRenewableOutput(
                filePath,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "96"),
            "识别新能源机组不足 96 点",
            failedTests,
            errors);
    }


    qInfo()
            .noquote()
        << "========================================";

    if (failedTests == 0)
    {
        qInfo()
        .noquote()
            << "All tests passed.";
    }
    else
    {
        qInfo()
        .noquote()
            << failedTests
            << "test(s) failed.";
    }

    return failedTests == 0
               ? 0
               : 1;
}