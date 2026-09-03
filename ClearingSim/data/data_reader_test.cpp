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


QString makeTime(
    int period,
    int periodCount)
{
    int stepMinutes = 60;

    if (periodCount == 96)
    {
        stepMinutes = 15;
    }

    const int totalMinutes =
        (period - 1) *
        stepMinutes;

    const int hour =
        totalMinutes / 60;

    const int minute =
        totalMinutes % 60;

    return QString(
               "%1:%2")
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


QStringList loadHeader()
{
    return
        {
            "period",
            "时刻",
            "系统负荷(MW)"
        };
}


QStringList renewableHeader()
{
    return
        {
            "period",
            "时刻",
            "机组编号",
            "机组名称",
            "机组类型",
            "可用出力(MW)"
        };
}


QString makeGeneratorRow(
    int period,
    const QString &id,
    const QString &type,
    const QVector<
        QPair<QString, QString>>
        &segments,
    const QString &name =
    "测试电厂")
{
    QStringList columns;

    columns
        << QString::number(
               period)
        << name
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
        &segments,
    const QString &name =
    "测试用户")
{
    QStringList columns;

    columns
        << QString::number(
               period)
        << name
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


QString makeLoadRow(
    int period,
    int periodCount,
    const QString &load)
{
    QStringList columns;

    columns
        << QString::number(
               period)
        << makeTime(
               period,
               periodCount)
        << load;

    return columns.join(',');
}


QString makeRenewableRow(
    int period,
    int periodCount,
    const QString &id,
    const QString &type,
    const QString &output,
    const QString &name =
    "测试新能源机组")
{
    QStringList columns;

    columns
        << QString::number(
               period)
        << makeTime(
               period,
               periodCount)
        << id
        << name
        << type
        << output;

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


MarketData makeValidMarketData24()
{
    MarketData data;

    for (int period = 1;
         period <= 24;
         ++period)
    {
        GeneratorBid thermal;

        thermal.id =
            "G1";

        thermal.name =
            "测试火电机组";

        thermal.type =
            "火电";

        thermal.segment =
            1;

        thermal.price =
            150.0;

        thermal.quantity =
            100.0;

        thermal.period =
            period;

        data.generatorBids.push_back(
            thermal);


        GeneratorBid wind;

        wind.id =
            "W1";

        wind.name =
            "测试风电机组";

        wind.type =
            "风电";

        wind.segment =
            1;

        wind.price =
            0.0;

        wind.quantity =
            50.0;

        wind.period =
            period;

        data.generatorBids.push_back(
            wind);


        ConsumerBid consumer;

        consumer.id =
            "U1";

        consumer.name =
            "测试用户";

        consumer.segment =
            1;

        consumer.price =
            400.0;

        consumer.quantity =
            100.0;

        consumer.period =
            period;

        data.consumerBids.push_back(
            consumer);


        LoadPoint load;

        load.period =
            period;

        load.time =
            makeTime(
                period,
                24);

        load.load =
            500.0;

        data.loadCurve.push_back(
            load);


        RenewableOutput renewable;

        renewable.generatorId =
            "W1";

        renewable.generatorType =
            "风电";

        renewable.period =
            period;

        renewable.output =
            50.0;

        data.renewableOutputs.push_back(
            renewable);
    }

    return data;
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
        << "========== DataReader V1.4 Test ==========";


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


    // 宽表拆成 segment 对象
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


    // 同一机组跨 period 合法
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


    // 新能源允许 0 出力申报
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


    // 常规机组申报量不能为 0
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            const QString quantity =
                period == 1
                    ? "0"
                    : "50.0";

            rows.push_back(
                makeGeneratorRow(
                    period,
                    "G1",
                    "火电",
                    {
                        {
                            quantity,
                            "150.000"
                        }
                    }));
        }

        const QString path =
            tempDir.path() +
            "/thermal_zero.csv";

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
                    "必须大于 0"),
            "识别常规机组零申报量",
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


    // 用户申报量不能为 0
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            const QString quantity =
                period == 1
                    ? "0"
                    : "50.0";

            rows.push_back(
                makeConsumerRow(
                    period,
                    "U1",
                    {
                        {
                            quantity,
                            "400.000"
                        }
                    }));
        }

        const QString path =
            tempDir.path() +
            "/consumer_zero.csv";

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
                    "必须大于 0"),
            "识别用户零申报量",
            failedTests,
            errors);
    }


    // 报价超过允许范围
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            const QString price =
                period == 1
                    ? "541.000"
                    : "150.000";

            rows.push_back(
                makeGeneratorRow(
                    period,
                    "G1",
                    "火电",
                    {
                        {
                            "50.0",
                            price
                        }
                    }));
        }

        const QString path =
            tempDir.path() +
            "/price_range.csv";

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
                    "0～540"),
            "识别报价超过允许范围",
            failedTests,
            errors);
    }


    // 报价最多保留 3 位小数
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            const QString price =
                period == 1
                    ? "150.0000"
                    : "150.000";

            rows.push_back(
                makeGeneratorRow(
                    period,
                    "G1",
                    "火电",
                    {
                        {
                            "50.0",
                            price
                        }
                    }));
        }

        const QString path =
            tempDir.path() +
            "/price_precision.csv";

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
                    "最多保留 3 位小数"),
            "识别报价小数位数错误",
            failedTests,
            errors);
    }


    // 报价段填写不完整
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
                                ""
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
            "/incomplete_segment.csv";

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
                    "报价不完整"),
            "识别报价段填写不完整",
            failedTests,
            errors);
    }


    // 报价段不能断裂
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
                                "150.000"
                            },
                            {
                                "",
                                ""
                            },
                            {
                                "20.0",
                                "220.000"
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
            "/segment_gap.csv";

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
                    "报价段不连续"),
            "识别报价段不连续",
            failedTests,
            errors);
    }


    // 发电侧表头错误
    {
        QStringList header =
            generatorHeader();

        header[0] =
            "wrong";

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
        }

        const QString path =
            tempDir.path() +
            "/bad_header.csv";

        writeTextFile(
            path,
            makeCsv(
                header,
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
                    "第 1 列"),
            "识别发电侧错误表头",
            failedTests,
            errors);
    }


    // 发电申报缺失 period
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            if (period == 12)
            {
                continue;
            }

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

        const QString path =
            tempDir.path() +
            "/missing_period.csv";

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
                    "缺少时段 12"),
            "识别发电申报缺失时段",
            failedTests,
            errors);
    }


    // 同一机组跨时段类型必须一致
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            const QString type =
                period == 2
                    ? "风电"
                    : "火电";

            rows.push_back(
                makeGeneratorRow(
                    period,
                    "G1",
                    type,
                    {
                        {
                            "50.0",
                            "150.000"
                        }
                    }));
        }

        const QString path =
            tempDir.path() +
            "/generator_type.csv";

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
                    "类型与其他时段不一致"),
            "识别机组跨时段类型不一致",
            failedTests,
            errors);
    }


    // 负荷 period 重复
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            rows.push_back(
                makeLoadRow(
                    period,
                    24,
                    "500.0"));

            if (period == 1)
            {
                rows.push_back(
                    makeLoadRow(
                        period,
                        24,
                        "510.0"));
            }
        }

        const QString path =
            tempDir.path() +
            "/load_duplicate.csv";

        writeTextFile(
            path,
            makeCsv(
                loadHeader(),
                rows));

        QVector<LoadPoint> data;
        QStringList errors;

        const bool result =
            DataReader::readLoadCurve(
                path,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "重复"),
            "识别负荷时段重复",
            failedTests,
            errors);
    }


    // 负荷不能为负
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            const QString load =
                period == 1
                    ? "-1.0"
                    : "500.0";

            rows.push_back(
                makeLoadRow(
                    period,
                    24,
                    load));
        }

        const QString path =
            tempDir.path() +
            "/negative_load.csv";

        writeTextFile(
            path,
            makeCsv(
                loadHeader(),
                rows));

        QVector<LoadPoint> data;
        QStringList errors;

        const bool result =
            DataReader::readLoadCurve(
                path,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "不能为负数"),
            "识别负荷为负数",
            failedTests,
            errors);
    }


    // 新能源类型只能为风电或光伏
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            rows.push_back(
                makeRenewableRow(
                    period,
                    24,
                    "R1",
                    "水电",
                    "50.0"));
        }

        const QString path =
            tempDir.path() +
            "/renewable_type.csv";

        writeTextFile(
            path,
            makeCsv(
                renewableHeader(),
                rows));

        QVector<RenewableOutput> data;
        QStringList errors;

        const bool result =
            DataReader::readRenewableOutput(
                path,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "必须为风电或光伏"),
            "识别非法新能源类型",
            failedTests,
            errors);
    }


    // 新能源同一机组同 period 不能重复
    {
        QStringList rows;

        for (int period = 1;
             period <= 24;
             ++period)
        {
            rows.push_back(
                makeRenewableRow(
                    period,
                    24,
                    "W1",
                    "风电",
                    "50.0"));

            if (period == 1)
            {
                rows.push_back(
                    makeRenewableRow(
                        period,
                        24,
                        "W1",
                        "风电",
                        "60.0"));
            }
        }

        const QString path =
            tempDir.path() +
            "/renewable_duplicate.csv";

        writeTextFile(
            path,
            makeCsv(
                renewableHeader(),
                rows));

        QVector<RenewableOutput> data;
        QStringList errors;

        const bool result =
            DataReader::readRenewableOutput(
                path,
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "新能源出力重复"),
            "识别新能源重复时段数据",
            failedTests,
            errors);
    }


    // validateRelations 正常关系
    {
        MarketData data =
            makeValidMarketData24();

        QStringList errors;

        const bool result =
            DataReader::validateRelations(
                data,
                errors);

        check(
            result,
            "跨文件关系校验正常数据通过",
            failedTests,
            errors);
    }


    // 新能源 ID 必须存在于发电侧
    {
        MarketData data =
            makeValidMarketData24();

        for (int period = 1;
             period <= 24;
             ++period)
        {
            RenewableOutput item;

            item.generatorId =
                "S9";

            item.generatorType =
                "光伏";

            item.period =
                period;

            item.output =
                20.0;

            data.renewableOutputs.push_back(
                item);
        }

        QStringList errors;

        const bool result =
            DataReader::validateRelations(
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "在发电侧申报中不存在"),
            "识别新能源机组 ID 不存在",
            failedTests,
            errors);
    }


    // 新能源类型必须和发电侧一致
    {
        MarketData data =
            makeValidMarketData24();

        for (RenewableOutput &item :
             data.renewableOutputs)
        {
            if (item.generatorId ==
                "W1")
            {
                item.generatorType =
                    "光伏";
            }
        }

        QStringList errors;

        const bool result =
            DataReader::validateRelations(
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "机组类型不一致"),
            "识别新能源跨文件类型不一致",
            failedTests,
            errors);
    }


    // 新能源机组必须存在出力数据
    {
        MarketData data =
            makeValidMarketData24();

        data.renewableOutputs.clear();

        QStringList errors;

        const bool result =
            DataReader::validateRelations(
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "缺少新能源出力数据"),
            "识别新能源机组缺少出力数据",
            failedTests,
            errors);
    }


    // 用户跨文件时段完整性
    {
        MarketData data =
            makeValidMarketData24();

        for (int i =
             data.consumerBids.size() - 1;
             i >= 0;
             --i)
        {
            if (data.consumerBids[i].period ==
                24)
            {
                data.consumerBids.removeAt(i);
            }
        }

        QStringList errors;

        const bool result =
            DataReader::validateRelations(
                data,
                errors);

        check(
            !result &&
                containsError(
                    errors,
                    "用户 U1 缺少时段 24"),
            "识别用户跨文件时段缺失",
            failedTests,
            errors);
    }


    // CSV 只有表头
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
            << "All DataReader V1.4 tests passed.";
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