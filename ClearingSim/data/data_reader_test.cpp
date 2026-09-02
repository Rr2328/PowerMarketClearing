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
        if (QFileInfo::exists(
                dir.filePath(
                    "CMakeLists.txt")) &&
            QFileInfo::exists(
                dir.filePath(
                    "data/data_reader.cpp")))
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

    QDir dir(
        projectRoot);

    if (!dir.cdUp())
    {
        return QString();
    }

    return dir.absolutePath();
}


QString scenarioDirectory(
    const QString &repositoryRoot)
{
    return QDir(
               repositoryRoot)
        .filePath(
            "data/samples/scenario");
}


QStringList scenarioFileNames()
{
    return
        {
            "generator_bids_24period.csv",
            "consumer_bids_24period.csv",
            "load_curve_24period.csv",
            "renewable_output_24period.csv",

            "generator_bids_96period.csv",
            "consumer_bids_96period.csv",
            "load_curve_96period.csv",
            "renewable_output_96period.csv"
        };
}


int countScenarioFiles(
    const QString &directory)
{
    int count = 0;

    for (const QString &fileName :
         scenarioFileNames())
    {
        if (QFileInfo::exists(
                QDir(directory)
                    .filePath(
                        fileName)))
        {
            ++count;
        }
    }

    return count;
}


DataFileSet makeFileSet(
    const QString &directory,
    int periodCount)
{
    const QString suffix =
        QString::number(
            periodCount) +
        "period";

    DataFileSet files;

    files.generatorBidsFile =
        QDir(directory)
            .filePath(
                "generator_bids_" +
                suffix +
                ".csv");

    files.consumerBidsFile =
        QDir(directory)
            .filePath(
                "consumer_bids_" +
                suffix +
                ".csv");

    files.loadCurveFile =
        QDir(directory)
            .filePath(
                "load_curve_" +
                suffix +
                ".csv");

    files.renewableOutputFile =
        QDir(directory)
            .filePath(
                "renewable_output_" +
                suffix +
                ".csv");

    return files;
}


bool writeTextFile(
    const QString &filePath,
    const QString &content)
{
    QFile file(
        filePath);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text))
    {
        return false;
    }

    QTextStream out(
        &file);

    out.setEncoding(
        QStringConverter::Utf8);

    out << content;

    return true;
}


QStringList generatorHeader()
{
    QStringList header;

    header
        << "period"
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
        << "period"
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
    int period,
    const QString &id,
    const QString &type,
    const QVector<
        QPair<QString, QString>>
        &segments)
{
    QStringList columns;

    columns
        << QString::number(
               period)
        << "测试电厂"
        << id
        << type;

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
    int period,
    const QString &id,
    const QVector<
        QPair<QString, QString>>
        &segments)
{
    QStringList columns;

    columns
        << QString::number(
               period)
        << "测试用户"
        << id;

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
    QString content =
        header.join(',') +
        '\n';

    for (const QString &row :
         rows)
    {
        content +=
            row +
            '\n';
    }

    return content;
}


QString makeGeneratorCsv(
    int periodCount,
    const QString &type =
    "火电")
{
    QStringList rows;

    for (int period = 1;
         period <= periodCount;
         ++period)
    {
        rows.push_back(
            makeGeneratorRow(
                period,
                "G1",
                type,
                {
                    {
                        type == "光伏" &&
                                period <= 6
                            ? "0"
                            : "50.0",
                        type == "光伏"
                            ? "0.000"
                            : "150.000"
                    },
                    {
                        type == "光伏"
                            ? ""
                            : "30.0",
                        type == "光伏"
                            ? ""
                            : "200.000"
                    }
                }));
    }

    return makeCsv(
        generatorHeader(),
        rows);
}


QString makeConsumerCsv(
    int periodCount)
{
    QStringList rows;

    for (int period = 1;
         period <= periodCount;
         ++period)
    {
        rows.push_back(
            makeConsumerRow(
                period,
                "U1",
                {
                    {
                        "50.0",
                        "400.000"
                    },
                    {
                        "30.0",
                        "350.000"
                    }
                }));
    }

    return makeCsv(
        consumerHeader(),
        rows);
}


bool containsError(
    const QStringList &errors,
    const QString &keyword)
{
    for (const QString &error :
         errors)
    {
        if (error.contains(
                keyword))
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

        printErrors(
            errors);
    }
}


bool containsPeriod(
    const QVector<GeneratorBid> &data,
    int period)
{
    for (const GeneratorBid &bid :
         data)
    {
        if (bid.period ==
            period)
        {
            return true;
        }
    }

    return false;
}


int countSegments(
    const QVector<GeneratorBid> &data,
    int period,
    const QString &id)
{
    int count = 0;

    for (const GeneratorBid &bid :
         data)
    {
        if (bid.period ==
                period &&
            bid.id ==
                id)
        {
            ++count;
        }
    }

    return count;
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
        << "========== DataReader V1.3 Test ==========";


    const QString projectRoot =
        findProjectRoot();

    check(
        !projectRoot.isEmpty(),
        "定位 ClearingSim 项目根目录",
        failedTests);

    if (projectRoot.isEmpty())
    {
        return 1;
    }

    qInfo()
            .noquote()
        << "Project root:"
        << QDir::toNativeSeparators(
               projectRoot);


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


    const QString directory =
        scenarioDirectory(
            repositoryRoot);

    check(
        QDir(directory).exists(),
        "定位 scenario 场景数据目录",
        failedTests);

    qInfo()
            .noquote()
        << "Scenario directory:"
        << QDir::toNativeSeparators(
               directory);


    const int fileCount =
        countScenarioFiles(
            directory);

    check(
        fileCount == 8,
        QString(
            "定位 24/96 时段八类 CSV 数据（%1/8）")
            .arg(fileCount),
        failedTests);


    // 真实 24 时段数据
    {
        MarketData data;
        QStringList errors;

        const bool result =
            DataReader::readAll(
                makeFileSet(
                    directory,
                    24),
                data,
                errors);

        check(
            result,
            "读取完整 24 时段真实数据",
            failedTests,
            errors);

        if (result)
        {
            check(
                data.loadCurve.size() ==
                    24,
                "24 时段负荷数量正确",
                failedTests);

            check(
                containsPeriod(
                    data.generatorBids,
                    1) &&
                    containsPeriod(
                        data.generatorBids,
                        24),
                "24 时段发电申报 period 正确",
                failedTests);
        }
    }


    // 真实 96 时段数据
    {
        MarketData data;
        QStringList errors;

        const bool result =
            DataReader::readAll(
                makeFileSet(
                    directory,
                    96),
                data,
                errors);

        check(
            result,
            "读取完整 96 时段真实数据",
            failedTests,
            errors);

        if (result)
        {
            check(
                data.loadCurve.size() ==
                    96,
                "96 时段负荷数量正确",
                failedTests);

            check(
                containsPeriod(
                    data.generatorBids,
                    1) &&
                    containsPeriod(
                        data.generatorBids,
                        96),
                "96 时段发电申报 period 正确",
                failedTests);
        }
    }


    QTemporaryDir tempDir;

    check(
        tempDir.isValid(),
        "创建临时测试目录",
        failedTests);

    if (!tempDir.isValid())
    {
        return 1;
    }


    // 宽表拆成旧算法需要的 segment 对象
    {
        const QString path =
            tempDir.path() +
            "/segments.csv";

        writeTextFile(
            path,
            makeGeneratorCsv(
                24));

        QVector<GeneratorBid> data;
        QStringList errors;

        const bool result =
            DataReader::readGeneratorBids(
                path,
                data,
                errors);

        check(
            result,
            "宽表申报读取成功",
            failedTests,
            errors);

        if (result)
        {
            check(
                countSegments(
                    data,
                    1,
                    "G1") == 2,
                "P1/C1、P2/C2 拆成 2 个 GeneratorBid",
                failedTests);
        }
    }


    // 同一机组跨 period 是合法的
    {
        const QString path =
            tempDir.path() +
            "/multi_period.csv";

        writeTextFile(
            path,
            makeGeneratorCsv(
                24));

        QVector<GeneratorBid> data;
        QStringList errors;

        const bool result =
            DataReader::readGeneratorBids(
                path,
                data,
                errors);

        check(
            result,
            "允许同一机组出现在不同 period",
            failedTests,
            errors);
    }


    // 同一 period 同一机组重复
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            rows.push_back(
                makeGeneratorRow(
                    period,
                    "G1",
                    "火电",
                    {
                        {
                            "50.0",
                            "150.000"
                        }
                    }));

            if (period == 1)
            {
                rows.push_back(
                    makeGeneratorRow(
                        period,
                        "G1",
                        "火电",
                        {
                            {
                                "60.0",
                                "160.000"
                            }
                        }));
            }
        }

        const QString path =
            tempDir.path() +
            "/duplicate.csv";

        writeTextFile(
            path,
            makeCsv(
                generatorHeader(),
                rows));

        QVector<GeneratorBid> data;
        QStringList errors;

        const bool result =
            DataReader::readGeneratorBids(
                path,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "重复"),
            "识别同 period 重复机组",
            failedTests,
            errors);
    }


    // 新能源允许 0 出力
    {
        const QString path =
            tempDir.path() +
            "/solar.csv";

        writeTextFile(
            path,
            makeGeneratorCsv(
                24,
                "光伏"));

        QVector<GeneratorBid> data;
        QStringList errors;

        const bool result =
            DataReader::readGeneratorBids(
                path,
                data,
                errors);

        check(
            result,
            "允许光伏夜间申报量为 0",
            failedTests,
            errors);
    }


    // 发电报价必须递增
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            if (period == 1)
            {
                rows.push_back(
                    makeGeneratorRow(
                        period,
                        "G1",
                        "火电",
                        {
                            {
                                "50.0",
                                "200.000"
                            },
                            {
                                "30.0",
                                "180.000"
                            }
                        }));
            }
            else
            {
                rows.push_back(
                    makeGeneratorRow(
                        period,
                        "G1",
                        "火电",
                        {
                            {
                                "50.0",
                                "150.000"
                            }
                        }));
            }
        }

        const QString path =
            tempDir.path() +
            "/generator_price.csv";

        writeTextFile(
            path,
            makeCsv(
                generatorHeader(),
                rows));

        QVector<GeneratorBid> data;
        QStringList errors;

        const bool result =
            DataReader::readGeneratorBids(
                path,
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


    // 用户报价必须递减
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            if (period == 1)
            {
                rows.push_back(
                    makeConsumerRow(
                        period,
                        "U1",
                        {
                            {
                                "50.0",
                                "300.000"
                            },
                            {
                                "30.0",
                                "350.000"
                            }
                        }));
            }
            else
            {
                rows.push_back(
                    makeConsumerRow(
                        period,
                        "U1",
                        {
                            {
                                "50.0",
                                "400.000"
                            }
                        }));
            }
        }

        const QString path =
            tempDir.path() +
            "/consumer_price.csv";

        writeTextFile(
            path,
            makeCsv(
                consumerHeader(),
                rows));

        QVector<ConsumerBid> data;
        QStringList errors;

        const bool result =
            DataReader::readConsumerBids(
                path,
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


    // 只有表头
    {
        const QString path =
            tempDir.path() +
            "/header_only.csv";

        writeTextFile(
            path,
            makeCsv(
                generatorHeader(),
                {}));

        QVector<GeneratorBid> data;
        QStringList errors;

        const bool result =
            DataReader::readGeneratorBids(
                path,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "没有有效数据"),
            "识别 CSV 只有表头没有数据",
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
            << "All DataReader V1.3 tests passed.";
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