#include "data_reader.h"
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QHash>
#include <QMap>
#include <QtMath>
#include <algorithm>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

// 内部工具函数
namespace
{
// QTextStream 使用 UTF-8

void setUtf8(QTextStream& stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    stream.setEncoding(
        QStringConverter::Utf8);

#else

    stream.setCodec(
        "UTF-8");

#endif
}


// ------------------------------------------------------------
// 去除 UTF-8 BOM
// ------------------------------------------------------------

QString removeBom(QString text)
{
    if (!text.isEmpty() &&
        text.at(0).unicode() == 0xFEFF)
    {
        text.remove(0, 1);
    }

    return text;
}


// ------------------------------------------------------------
// CSV 一行按逗号拆开
//
// 当前数据契约中字段本身不包含英文逗号，
// 因此基础版使用 split(',') 即可。
// ------------------------------------------------------------

QStringList splitCsvLine(
    const QString& line)
{
    QStringList fields =
        line.split(
            ',',
            Qt::KeepEmptyParts);

    for (int i = 0;
         i < fields.size();
         ++i)
    {
        fields[i] =
            fields[i].trimmed();
    }

    return fields;
}


// ------------------------------------------------------------
// 生成带行号的错误信息
// ------------------------------------------------------------

QString lineError(
    int line,
    const QString& message)
{
    return QString("第 %1 行：%2")
        .arg(line)
        .arg(message);
}


// ------------------------------------------------------------
// 检查表头
// ------------------------------------------------------------

bool checkHeader(
    const QString& line,
    const QStringList& expected,
    QStringList& errors)
{
    QString cleanLine =
        removeBom(line);

    QStringList actual =
        splitCsvLine(cleanLine);

    if (actual.size() != expected.size())
    {
        errors
            << QString(
                   "表头列数错误：应为 %1 列，实际为 %2 列")
                   .arg(expected.size())
                   .arg(actual.size());

        return false;
    }

    for (int i = 0;
         i < expected.size();
         ++i)
    {
        if (actual[i] != expected[i])
        {
            errors
                << QString(
                       "表头第 %1 列错误：应为“%2”，实际为“%3”")
                       .arg(i + 1)
                       .arg(expected[i])
                       .arg(actual[i]);

            return false;
        }
    }

    return true;
}


// ------------------------------------------------------------
// 计算某个 period 应该对应的时间
//
// 1  -> 00:15
// 4  -> 01:00
// 96 -> 24:00
// ------------------------------------------------------------

QString expectedTime(
    int period)
{
    const int minutes =
        period * 15;

    if (minutes == 1440)
    {
        return "24:00";
    }

    const int hour =
        minutes / 60;

    const int minute =
        minutes % 60;

    return QString("%1:%2")
        .arg(hour, 2, 10, QChar('0'))
        .arg(minute, 2, 10, QChar('0'));
}

}


// ============================================================
// 1. generator_bids.csv
// ============================================================

bool DataReader::readGeneratorBids(
    const QString& filePath,
    QVector<GeneratorBid>& data,
    QStringList& errors)
{
    data.clear();
    errors.clear();


    QFile file(filePath);


    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        errors
            << QString(
                   "无法打开文件：%1")
                   .arg(filePath);

        return false;
    }


    QTextStream stream(&file);

    setUtf8(stream);


    if (stream.atEnd())
    {
        errors
            << "机组申报文件为空";

        return false;
    }


    const QStringList expectedHeader =
        {
            "机组ID",
            "机组名称",
            "机组类型",
            "申报段",
            "申报电价(元/MWh)",
            "申报电量(MWh)"
        };


    if (!checkHeader(
            stream.readLine(),
            expectedHeader,
            errors))
    {
        return false;
    }


    // ID + 申报段不能重复
    QSet<QString> usedKeys;


    // 同一 ID 的名称、类型应保持一致
    QHash<QString, QString> names;

    QHash<QString, QString> types;


    // 用于检查每台机组的分段价格
    QMap<QString, QMap<int, double>>
        segmentPrices;


    int lineNumber =
        1;


    while (!stream.atEnd())
    {
        QString line =
            stream.readLine();

        ++lineNumber;


        if (line.trimmed().isEmpty())
        {
            continue;
        }


        QStringList fields =
            splitCsvLine(line);


        // ----------------------------------------------------
        // 列数
        // ----------------------------------------------------

        if (fields.size() != 6)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "机组申报应为6列，实际为%1列")
                           .arg(fields.size()));

            continue;
        }


        QString id =
            fields[0];

        QString name =
            fields[1];

        QString type =
            fields[2];


        bool segmentOk =
            false;

        bool priceOk =
            false;

        bool quantityOk =
            false;


        int segment =
            fields[3].toInt(
                &segmentOk);


        double price =
            fields[4].toDouble(
                &priceOk);


        double quantity =
            fields[5].toDouble(
                &quantityOk);


        bool rowValid =
            true;


        // ----------------------------------------------------
        // 基础字段
        // ----------------------------------------------------

        if (id.isEmpty())
        {
            errors
                << lineError(
                       lineNumber,
                       "机组ID不能为空");

            rowValid = false;
        }


        if (name.isEmpty())
        {
            errors
                << lineError(
                       lineNumber,
                       "机组名称不能为空");

            rowValid = false;
        }


        if (type != "火电" &&
            type != "风电" &&
            type != "光伏")
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "机组类型“%1”无效")
                           .arg(type));

            rowValid = false;
        }


        // ----------------------------------------------------
        // 申报段 1~5
        // ----------------------------------------------------

        if (!segmentOk)
        {
            errors
                << lineError(
                       lineNumber,
                       "申报段不是整数");

            rowValid = false;
        }

        else if (segment < 1 ||
                 segment > 5)
        {
            errors
                << lineError(
                       lineNumber,
                       "申报段必须在1~5之间");

            rowValid = false;
        }


        // ----------------------------------------------------
        // 申报电价
        //
        // 当前契约：
        // 0~540 元/MWh。
        // 新能源允许报0价。
        // ----------------------------------------------------

        if (!priceOk)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "申报电价“%1”不是数字")
                           .arg(fields[4]));

            rowValid = false;
        }

        else if (price < 0.0 ||
                 price > 540.0)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "申报电价必须在0~540元/MWh之间，当前为%1")
                           .arg(price));

            rowValid = false;
        }


        // ----------------------------------------------------
        // 申报电量
        // ----------------------------------------------------

        if (!quantityOk)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "申报电量“%1”不是数字")
                           .arg(fields[5]));

            rowValid = false;
        }

        else if (quantity <= 0.0)
        {
            errors
                << lineError(
                       lineNumber,
                       "申报电量必须大于0");

            rowValid = false;
        }


        // ----------------------------------------------------
        // ID + segment 唯一
        // ----------------------------------------------------

        QString key =
            id
            +
            "#"
            +
            QString::number(segment);


        if (segmentOk &&
            usedKeys.contains(key))
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "机组%1的申报段%2重复")
                           .arg(id)
                           .arg(segment));

            rowValid = false;
        }


        // ----------------------------------------------------
        // 同一机组名称一致
        // ----------------------------------------------------

        if (!id.isEmpty())
        {
            if (names.contains(id) &&
                names[id] != name)
            {
                errors
                    << lineError(
                           lineNumber,
                           QString(
                               "机组%1的机组名称前后不一致")
                               .arg(id));

                rowValid = false;
            }


            if (types.contains(id) &&
                types[id] != type)
            {
                errors
                    << lineError(
                           lineNumber,
                           QString(
                               "机组%1的机组类型前后不一致")
                               .arg(id));

                rowValid = false;
            }
        }


        if (!rowValid)
        {
            continue;
        }


        usedKeys.insert(key);

        names[id] =
            name;

        types[id] =
            type;


        segmentPrices[id][segment] =
            price;


        GeneratorBid bid;

        bid.id =
            id;

        bid.name =
            name;

        bid.type =
            type;

        bid.segment =
            segment;

        bid.price =
            price;

        bid.quantity =
            quantity;


        data.push_back(
            bid);
    }


    file.close();


    // ========================================================
    // 检查机组分段价格单调不减
    // ========================================================

    for (auto it =
         segmentPrices.constBegin();

         it !=
         segmentPrices.constEnd();

         ++it)
    {
        const QString id =
            it.key();

        const QMap<int, double>& prices =
            it.value();


        if (prices.size() > 5)
        {
            errors
                << QString(
                       "机组%1申报段超过5段")
                       .arg(id);
        }


        bool first =
            true;

        double previousPrice =
            0.0;


        for (auto priceIt =
             prices.constBegin();

             priceIt !=
             prices.constEnd();

             ++priceIt)
        {
            if (!first &&
                priceIt.value() <
                    previousPrice)
            {
                errors
                    << QString(
                           "机组%1的申报电价未按申报段单调不减")
                           .arg(id);

                break;
            }


            previousPrice =
                priceIt.value();

            first =
                false;
        }
    }


    if (!errors.isEmpty())
    {
        data.clear();

        return false;
    }


    if (data.isEmpty())
    {
        errors
            << "没有读取到有效机组申报";

        return false;
    }


    // 按机组 ID、申报段排序
    std::sort(
        data.begin(),
        data.end(),

        [](const GeneratorBid& a,
           const GeneratorBid& b)
        {
            if (a.id == b.id)
            {
                return
                    a.segment
                    <
                    b.segment;
            }

            return
                a.id
                <
                b.id;
        });


    return true;
}


// ============================================================
// 2. consumer_bids.csv
// ============================================================

bool DataReader::readConsumerBids(
    const QString& filePath,
    QVector<ConsumerBid>& data,
    QStringList& errors)
{
    data.clear();
    errors.clear();


    QFile file(filePath);


    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        errors
            << QString(
                   "无法打开文件：%1")
                   .arg(filePath);

        return false;
    }


    QTextStream stream(&file);

    setUtf8(stream);


    if (stream.atEnd())
    {
        errors
            << "用户申报文件为空";

        return false;
    }


    const QStringList expectedHeader =
        {
            "用户ID",
            "用户名称",
            "申报段",
            "申报电价(元/MWh)",
            "申报电量(MWh)"
        };


    if (!checkHeader(
            stream.readLine(),
            expectedHeader,
            errors))
    {
        return false;
    }


    QSet<QString> usedKeys;

    QHash<QString, QString> names;


    int lineNumber =
        1;


    while (!stream.atEnd())
    {
        QString line =
            stream.readLine();

        ++lineNumber;


        if (line.trimmed().isEmpty())
        {
            continue;
        }


        QStringList fields =
            splitCsvLine(line);


        if (fields.size() != 5)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "用户申报应为5列，实际为%1列")
                           .arg(fields.size()));

            continue;
        }


        QString id =
            fields[0];

        QString name =
            fields[1];


        bool segmentOk =
            false;

        bool priceOk =
            false;

        bool quantityOk =
            false;


        int segment =
            fields[2].toInt(
                &segmentOk);


        double price =
            fields[3].toDouble(
                &priceOk);


        double quantity =
            fields[4].toDouble(
                &quantityOk);


        bool rowValid =
            true;


        if (id.isEmpty())
        {
            errors
                << lineError(
                       lineNumber,
                       "用户ID不能为空");

            rowValid = false;
        }


        if (name.isEmpty())
        {
            errors
                << lineError(
                       lineNumber,
                       "用户名称不能为空");

            rowValid = false;
        }


        if (!segmentOk ||
            segment < 1 ||
            segment > 5)
        {
            errors
                << lineError(
                       lineNumber,
                       "申报段必须是1~5之间的整数");

            rowValid = false;
        }


        if (!priceOk)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "申报电价“%1”不是数字")
                           .arg(fields[3]));

            rowValid = false;
        }

        else if (price < 0.0 ||
                 price > 540.0)
        {
            errors
                << lineError(
                       lineNumber,
                       "申报电价必须在0~540元/MWh之间");

            rowValid = false;
        }


        if (!quantityOk)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "申报电量“%1”不是数字")
                           .arg(fields[4]));

            rowValid = false;
        }

        else if (quantity <= 0.0)
        {
            errors
                << lineError(
                       lineNumber,
                       "申报电量必须大于0");

            rowValid = false;
        }


        QString key =
            id
            +
            "#"
            +
            QString::number(segment);


        if (segmentOk &&
            usedKeys.contains(key))
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "用户%1的申报段%2重复")
                           .arg(id)
                           .arg(segment));

            rowValid = false;
        }


        if (names.contains(id) &&
            names[id] != name)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "用户%1名称前后不一致")
                           .arg(id));

            rowValid = false;
        }


        if (!rowValid)
        {
            continue;
        }


        usedKeys.insert(key);

        names[id] =
            name;


        ConsumerBid bid;

        bid.id =
            id;

        bid.name =
            name;

        bid.segment =
            segment;

        bid.price =
            price;

        bid.quantity =
            quantity;


        data.push_back(
            bid);
    }


    file.close();


    if (!errors.isEmpty())
    {
        data.clear();

        return false;
    }


    if (data.isEmpty())
    {
        errors
            << "没有读取到有效用户申报";

        return false;
    }


    return true;
}


// ============================================================
// 3. load_curve.csv
// ============================================================

bool DataReader::readLoadCurve(
    const QString& filePath,
    QVector<LoadPoint>& data,
    QStringList& errors)
{
    data.clear();
    errors.clear();


    QFile file(filePath);


    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        errors
            << QString(
                   "无法打开文件：%1")
                   .arg(filePath);

        return false;
    }


    QTextStream stream(&file);

    setUtf8(stream);


    if (stream.atEnd())
    {
        errors
            << "负荷曲线文件为空";

        return false;
    }


    const QStringList expectedHeader =
        {
            "时段",
            "时刻",
            "负荷(MW)"
        };


    if (!checkHeader(
            stream.readLine(),
            expectedHeader,
            errors))
    {
        return false;
    }


    QSet<int> usedPeriods;


    int lineNumber =
        1;


    while (!stream.atEnd())
    {
        QString line =
            stream.readLine();

        ++lineNumber;


        if (line.trimmed().isEmpty())
        {
            continue;
        }


        QStringList fields =
            splitCsvLine(line);


        if (fields.size() != 3)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "负荷曲线应为3列，实际为%1列")
                           .arg(fields.size()));

            continue;
        }


        bool periodOk =
            false;

        bool loadOk =
            false;


        int period =
            fields[0].toInt(
                &periodOk);


        QString time =
            fields[1];


        double load =
            fields[2].toDouble(
                &loadOk);


        bool rowValid =
            true;


        if (!periodOk ||
            period < 1 ||
            period > 96)
        {
            errors
                << lineError(
                       lineNumber,
                       "时段必须是1~96之间的整数");

            rowValid = false;
        }


        if (periodOk &&
            usedPeriods.contains(period))
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "时段%1重复")
                           .arg(period));

            rowValid = false;
        }


        if (periodOk &&
            period >= 1 &&
            period <= 96)
        {
            QString correctTime =
                expectedTime(period);


            if (time != correctTime)
            {
                errors
                    << lineError(
                           lineNumber,
                           QString(
                               "时段%1的时刻应为%2，实际为%3")
                               .arg(period)
                               .arg(correctTime)
                               .arg(time));

                rowValid = false;
            }
        }


        if (!loadOk)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "负荷“%1”不是数字")
                           .arg(fields[2]));

            rowValid = false;
        }

        else if (load <= 0.0)
        {
            errors
                << lineError(
                       lineNumber,
                       "负荷必须大于0");

            rowValid = false;
        }


        if (!rowValid)
        {
            continue;
        }


        usedPeriods.insert(
            period);


        LoadPoint point;

        point.period =
            period;

        point.time =
            time;

        point.load =
            load;


        data.push_back(
            point);
    }


    file.close();


    // --------------------------------------------------------
    // 必须完整包含 1~96
    // --------------------------------------------------------

    for (int period = 1;
         period <= 96;
         ++period)
    {
        if (!usedPeriods.contains(period))
        {
            errors
                << QString(
                       "缺少负荷时段 %1")
                       .arg(period);
        }
    }


    if (!errors.isEmpty())
    {
        data.clear();

        return false;
    }


    if (data.size() != 96)
    {
        errors
            << QString(
                   "负荷曲线应有96行数据，实际为%1行")
                   .arg(data.size());

        data.clear();

        return false;
    }


    std::sort(
        data.begin(),
        data.end(),

        [](const LoadPoint& a,
           const LoadPoint& b)
        {
            return
                a.period
                <
                b.period;
        });


    return true;
}


// ============================================================
// 4. renewable_output.csv
// ============================================================

bool DataReader::readRenewableOutput(
    const QString& filePath,
    QVector<RenewableOutput>& data,
    QStringList& errors)
{
    data.clear();
    errors.clear();


    QFile file(filePath);


    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        errors
            << QString(
                   "无法打开文件：%1")
                   .arg(filePath);

        return false;
    }


    QTextStream stream(&file);

    setUtf8(stream);


    if (stream.atEnd())
    {
        errors
            << "新能源出力文件为空";

        return false;
    }


    const QStringList expectedHeader =
        {
            "机组ID",
            "机组类型",
            "时段",
            "出力(MW)"
        };


    if (!checkHeader(
            stream.readLine(),
            expectedHeader,
            errors))
    {
        return false;
    }


    QSet<QString> keys;


    QHash<QString, QString>
        generatorTypes;


    QHash<QString, QSet<int>>
        generatorPeriods;


    int lineNumber =
        1;


    while (!stream.atEnd())
    {
        QString line =
            stream.readLine();

        ++lineNumber;


        if (line.trimmed().isEmpty())
        {
            continue;
        }


        QStringList fields =
            splitCsvLine(line);


        if (fields.size() != 4)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "新能源出力应为4列，实际为%1列")
                           .arg(fields.size()));

            continue;
        }


        QString id =
            fields[0];

        QString type =
            fields[1];


        bool periodOk =
            false;

        bool outputOk =
            false;


        int period =
            fields[2].toInt(
                &periodOk);


        double output =
            fields[3].toDouble(
                &outputOk);


        bool rowValid =
            true;


        if (id.isEmpty())
        {
            errors
                << lineError(
                       lineNumber,
                       "机组ID不能为空");

            rowValid = false;
        }


        if (type != "风电" &&
            type != "光伏")
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "新能源机组类型“%1”无效")
                           .arg(type));

            rowValid = false;
        }


        if (!periodOk ||
            period < 1 ||
            period > 96)
        {
            errors
                << lineError(
                       lineNumber,
                       "时段必须是1~96之间的整数");

            rowValid = false;
        }


        if (!outputOk)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "出力“%1”不是数字")
                           .arg(fields[3]));

            rowValid = false;
        }

        else if (output < 0.0)
        {
            errors
                << lineError(
                       lineNumber,
                       "新能源出力不能为负数");

            rowValid = false;
        }


        QString key =
            id
            +
            "#"
            +
            QString::number(period);


        if (periodOk &&
            keys.contains(key))
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "%1在时段%2的数据重复")
                           .arg(id)
                           .arg(period));

            rowValid = false;
        }


        if (generatorTypes.contains(id) &&
            generatorTypes[id] != type)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "机组%1的类型前后不一致")
                           .arg(id));

            rowValid = false;
        }


        // ----------------------------------------------------
        // 光伏夜间应为0
        //
        // period 1~24  : 00:15~06:00
        // period 72~96 : 18:00~24:00
        // ----------------------------------------------------

        if (type == "光伏" &&
            outputOk &&
            (period <= 24 ||
             period >= 72) &&
            qAbs(output) > 1e-9)
        {
            errors
                << lineError(
                       lineNumber,
                       QString(
                           "光伏机组%1在夜间时段%2出力应为0")
                           .arg(id)
                           .arg(period));

            rowValid = false;
        }


        if (!rowValid)
        {
            continue;
        }


        keys.insert(key);


        generatorTypes[id] =
            type;


        generatorPeriods[id]
            .insert(period);


        RenewableOutput item;

        item.generatorId =
            id;

        item.generatorType =
            type;

        item.period =
            period;

        item.output =
            output;


        data.push_back(
            item);
    }


    file.close();


    // --------------------------------------------------------
    // 每台新能源机组必须有完整96时段
    // --------------------------------------------------------

    for (auto it =
         generatorPeriods.constBegin();

         it !=
         generatorPeriods.constEnd();

         ++it)
    {
        const QString id =
            it.key();

        const QSet<int>& periods =
            it.value();


        if (periods.size() != 96)
        {
            errors
                << QString(
                       "新能源机组%1应有96个时段，实际为%2个")
                       .arg(id)
                       .arg(periods.size());
        }


        for (int period = 1;
             period <= 96;
             ++period)
        {
            if (!periods.contains(period))
            {
                errors
                    << QString(
                           "新能源机组%1缺少时段%2")
                           .arg(id)
                           .arg(period);
            }
        }
    }


    if (!errors.isEmpty())
    {
        data.clear();

        return false;
    }


    if (data.isEmpty())
    {
        errors
            << "没有读取到新能源出力数据";

        return false;
    }


    std::sort(
        data.begin(),
        data.end(),

        [](const RenewableOutput& a,
           const RenewableOutput& b)
        {
            if (a.generatorId ==
                b.generatorId)
            {
                return
                    a.period
                    <
                    b.period;
            }

            return
                a.generatorId
                <
                b.generatorId;
        });


    return true;
}