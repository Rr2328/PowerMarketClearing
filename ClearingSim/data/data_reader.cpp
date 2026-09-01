#include "data_reader.h"

#include <QFile>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextStream>

#include <cmath>

namespace
{

struct CsvRow
{
    int lineNumber = 0;
    QStringList columns;
};

void addError(
    QStringList &errors,
    const QString &prefix,
    const QString &message)
{
    if (prefix.isEmpty())
    {
        errors.append(message);
    }
    else
    {
        errors.append(
            prefix + " · " + message);
    }
}

// CSV 行分割
QStringList splitCsvLine(const QString &line)
{
    QStringList columns =
        line.split(',', Qt::KeepEmptyParts);

    for (QString &column : columns)
    {
        column = column.trimmed();
    }

    if (!columns.isEmpty() &&
        columns[0].startsWith(QChar(0xFEFF)))
    {
        columns[0].remove(0, 1);
    }

    return columns;
}

// CSV 公共读取函数
bool readCsvRows(
    const QString &filePath,
    const QStringList &expectedHeader,
    const QString &prefix,
    QVector<CsvRow> &rows,
    QStringList &errors)
{
    rows.clear();

    QFile file(filePath);

    if (!file.exists())
    {
        addError(
            errors,
            prefix,
            "文件不存在：" + filePath);

        return false;
    }

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        addError(
            errors,
            prefix,
            "文件无法打开：" + filePath);

        return false;
    }

    QTextStream in(&file);

    if (in.atEnd())
    {
        addError(
            errors,
            prefix,
            "CSV 文件为空：" + filePath);

        return false;
    }

    const QStringList header =
        splitCsvLine(
            in.readLine().trimmed());

    if (header != expectedHeader)
    {
        addError(
            errors,
            prefix,
            "CSV 表头不匹配，应为：" +
                expectedHeader.join(','));

        return false;
    }

    int lineNumber = 1;

    while (!in.atEnd())
    {
        ++lineNumber;

        const QString line =
            in.readLine().trimmed();

        if (line.isEmpty())
        {
            continue;
        }

        const QStringList columns =
            splitCsvLine(line);

        if (columns.size() != expectedHeader.size())
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行列数错误：应为 %2 列，实际为 %3 列")
                    .arg(lineNumber)
                    .arg(expectedHeader.size())
                    .arg(columns.size()));

            continue;
        }

        CsvRow row;
        row.lineNumber = lineNumber;
        row.columns = columns;

        rows.push_back(row);
    }

    if (rows.isEmpty())
    {
        if (errors.isEmpty())
        {
            addError(
                errors,
                prefix,
                "CSV 文件中没有有效数据：" +
                    filePath);
        }

        return false;
    }

    return errors.isEmpty();
}

// 空字段检查
bool checkNotEmpty(
    const QString &value,
    int lineNumber,
    const QString &fieldName,
    const QString &prefix,
    QStringList &errors)
{
    if (value.trimmed().isEmpty())
    {
        addError(
            errors,
            prefix,
            QString("第 %1 行 %2 为空")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    return true;
}

// 正整数检查
bool parsePositiveInt(
    const QString &text,
    int &value,
    int lineNumber,
    const QString &fieldName,
    const QString &prefix,
    QStringList &errors)
{
    bool ok = false;

    value = text.toInt(&ok);

    if (!ok)
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行 %2 不是有效整数：%3")
                .arg(lineNumber)
                .arg(fieldName)
                .arg(text));

        return false;
    }

    if (value <= 0)
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行 %2 必须大于 0")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    return true;
}

// 小数位数检查
bool checkPrecision(
    const QString &text,
    int decimals,
    int lineNumber,
    const QString &fieldName,
    const QString &prefix,
    QStringList &errors)
{
    const QRegularExpression expression(
        QString("^\\d+\\.\\d{%1}$")
            .arg(decimals));

    if (!expression.match(text).hasMatch())
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行 %2 应保留 %3 位小数：%4")
                .arg(lineNumber)
                .arg(fieldName)
                .arg(decimals)
                .arg(text));

        return false;
    }

    return true;
}

// 浮点数检查
bool parseDouble(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    const QString &prefix,
    QStringList &errors)
{
    bool ok = false;

    value = text.toDouble(&ok);

    if (!ok ||
        !std::isfinite(value))
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行 %2 不是有效数字：%3")
                .arg(lineNumber)
                .arg(fieldName)
                .arg(text));

        return false;
    }

    return true;
}

// 申报价格检查
bool parsePrice(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &prefix,
    QStringList &errors)
{
    if (!checkPrecision(
            text,
            3,
            lineNumber,
            "申报电价",
            prefix,
            errors))
    {
        return false;
    }

    if (!parseDouble(
            text,
            value,
            lineNumber,
            "申报电价",
            prefix,
            errors))
    {
        return false;
    }

    if (value < 0.0 ||
        value > 540.0)
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行申报电价必须在 0~540 元/MWh")
                .arg(lineNumber));

        return false;
    }

    return true;
}

// 正电量检查
bool parsePositiveQuantity(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &prefix,
    QStringList &errors)
{
    if (!checkPrecision(
            text,
            1,
            lineNumber,
            "申报电量",
            prefix,
            errors))
    {
        return false;
    }

    if (!parseDouble(
            text,
            value,
            lineNumber,
            "申报电量",
            prefix,
            errors))
    {
        return false;
    }

    if (value <= 0.0)
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行申报电量必须大于 0")
                .arg(lineNumber));

        return false;
    }

    return true;
}

// 正功率检查
bool parsePositivePower(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    QStringList &errors)
{
    if (!checkPrecision(
            text,
            1,
            lineNumber,
            fieldName,
            QString(),
            errors))
    {
        return false;
    }

    if (!parseDouble(
            text,
            value,
            lineNumber,
            fieldName,
            QString(),
            errors))
    {
        return false;
    }

    if (value <= 0.0)
    {
        errors.append(
            QString(
                "第 %1 行 %2 必须大于 0")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    return true;
}

// 非负功率检查
bool parseNonNegativePower(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    QStringList &errors)
{
    if (!checkPrecision(
            text,
            1,
            lineNumber,
            fieldName,
            QString(),
            errors))
    {
        return false;
    }

    if (!parseDouble(
            text,
            value,
            lineNumber,
            fieldName,
            QString(),
            errors))
    {
        return false;
    }

    if (value < 0.0)
    {
        errors.append(
            QString(
                "第 %1 行 %2 不能为负数")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    return true;
}

// 时段对应时间
QString expectedTime(int period)
{
    const int totalMinutes =
        period * 15;

    if (totalMinutes == 24 * 60)
    {
        return "24:00";
    }

    const int hour =
        totalMinutes / 60;

    const int minute =
        totalMinutes % 60;

    return QString("%1:%2")
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

// 合并错误信息
void appendErrors(
    const QString &fileName,
    const QStringList &sourceErrors,
    QStringList &targetErrors)
{
    for (const QString &error : sourceErrors)
    {
        targetErrors.append(
            "[" + fileName + "] " +
            error);
    }
}

} // namespace


// 发电侧申报读取
bool DataReader::readGeneratorBids(
    const QString &filePath,
    QVector<GeneratorBid> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    const QString prefix =
        "发电";

    const QStringList expectedHeader =
        {
            "机组ID",
            "机组名称",
            "机组类型",
            "申报段",
            "申报电价(元/MWh)",
            "申报电量(MWh)"
        };

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            expectedHeader,
            prefix,
            rows,
            errors))
    {
        return false;
    }

    const QSet<QString> allowedTypes =
        {
            "火电",
            "水电",
            "风电",
            "光伏"
        };

    QVector<GeneratorBid> tempData;

    QSet<QString> segmentKeys;

    QHash<QString, QString> idToName;
    QHash<QString, QString> idToType;
    QHash<QString, QString> nameToId;

    QMap<QString, QMap<int, double>>
        pricesByGenerator;

    for (const CsvRow &row : rows)
    {
        const QStringList &c =
            row.columns;

        GeneratorBid item;

        item.id = c[0];
        item.name = c[1];
        item.type = c[2];

        bool rowValid = true;

        if (!checkNotEmpty(
                item.id,
                row.lineNumber,
                "机组 ID",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.name,
                row.lineNumber,
                "机组名称",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.type,
                row.lineNumber,
                "机组类型",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!allowedTypes.contains(
                item.type))
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行机组类型只能为火电、水电、风电或光伏")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!parsePositiveInt(
                c[3],
                item.segment,
                row.lineNumber,
                "申报段",
                prefix,
                errors))
        {
            rowValid = false;
        }
        else if (item.segment > 5)
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行申报段不能超过 5")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!parsePrice(
                c[4],
                item.price,
                row.lineNumber,
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveQuantity(
                c[5],
                item.quantity,
                row.lineNumber,
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!rowValid)
        {
            continue;
        }

        if (idToName.contains(item.id) &&
            idToName.value(item.id) != item.name)
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行机组 %2 的名称与前面不一致")
                    .arg(row.lineNumber)
                    .arg(item.id));

            continue;
        }

        if (idToType.contains(item.id) &&
            idToType.value(item.id) != item.type)
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行机组 %2 的类型与前面不一致")
                    .arg(row.lineNumber)
                    .arg(item.id));

            continue;
        }

        if (nameToId.contains(item.name) &&
            nameToId.value(item.name) != item.id)
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行机组名称重复：%2")
                    .arg(row.lineNumber)
                    .arg(item.name));

            continue;
        }

        const QString segmentKey =
            item.id +
            "|" +
            QString::number(item.segment);

        if (segmentKeys.contains(
                segmentKey))
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行机组 %2 的申报段 %3 重复")
                    .arg(row.lineNumber)
                    .arg(item.id)
                    .arg(item.segment));

            continue;
        }

        segmentKeys.insert(segmentKey);

        idToName[item.id] =
            item.name;

        idToType[item.id] =
            item.type;

        nameToId[item.name] =
            item.id;

        pricesByGenerator[item.id]
            .insert(
                item.segment,
                item.price);

        tempData.push_back(item);
    }

    if (!errors.isEmpty())
    {
        return false;
    }

    for (auto it =
         pricesByGenerator.cbegin();
         it != pricesByGenerator.cend();
         ++it)
    {
        const QString generatorId =
            it.key();

        const QMap<int, double> &segments =
            it.value();

        const int segmentCount =
            segments.size();

        if (segmentCount > 5)
        {
            addError(
                errors,
                prefix,
                QString(
                    "机组 %1 的申报段数超过 5 段")
                    .arg(generatorId));

            continue;
        }

        for (int segment = 1;
             segment <= segmentCount;
             ++segment)
        {
            if (!segments.contains(segment))
            {
                addError(
                    errors,
                    prefix,
                    QString(
                        "机组 %1 的申报段必须从 1 连续编号")
                        .arg(generatorId));

                break;
            }
        }

        for (int segment = 2;
             segment <= segmentCount;
             ++segment)
        {
            if (!segments.contains(segment - 1) ||
                !segments.contains(segment))
            {
                continue;
            }

            if (segments.value(segment) <
                segments.value(segment - 1))
            {
                addError(
                    errors,
                    prefix,
                    QString(
                        "机组 %1 的申报电价必须随申报段单调不减")
                        .arg(generatorId));

                break;
            }
        }
    }

    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }

    data = tempData;

    return true;
}


// 用户侧申报读取
bool DataReader::readConsumerBids(
    const QString &filePath,
    QVector<ConsumerBid> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    const QString prefix =
        "购电";

    const QStringList expectedHeader =
        {
            "用户ID",
            "用户名称",
            "申报段",
            "申报电价(元/MWh)",
            "申报电量(MWh)"
        };

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            expectedHeader,
            prefix,
            rows,
            errors))
    {
        return false;
    }

    QVector<ConsumerBid> tempData;

    QSet<QString> segmentKeys;

    QHash<QString, QString> idToName;

    QMap<QString, QMap<int, double>>
        pricesByConsumer;

    for (const CsvRow &row : rows)
    {
        const QStringList &c =
            row.columns;

        ConsumerBid item;

        item.id = c[0];
        item.name = c[1];

        bool rowValid = true;

        if (!checkNotEmpty(
                item.id,
                row.lineNumber,
                "用户 ID",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.name,
                row.lineNumber,
                "用户名称",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveInt(
                c[2],
                item.segment,
                row.lineNumber,
                "申报段",
                prefix,
                errors))
        {
            rowValid = false;
        }
        else if (item.segment > 5)
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行申报段不能超过 5")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!parsePrice(
                c[3],
                item.price,
                row.lineNumber,
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveQuantity(
                c[4],
                item.quantity,
                row.lineNumber,
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!rowValid)
        {
            continue;
        }

        if (idToName.contains(item.id) &&
            idToName.value(item.id) != item.name)
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行用户 %2 的名称与前面不一致")
                    .arg(row.lineNumber)
                    .arg(item.id));

            continue;
        }

        const QString segmentKey =
            item.id +
            "|" +
            QString::number(item.segment);

        if (segmentKeys.contains(
                segmentKey))
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行用户 %2 的申报段 %3 重复")
                    .arg(row.lineNumber)
                    .arg(item.id)
                    .arg(item.segment));

            continue;
        }

        segmentKeys.insert(segmentKey);

        idToName[item.id] =
            item.name;

        pricesByConsumer[item.id]
            .insert(
                item.segment,
                item.price);

        tempData.push_back(item);
    }

    if (!errors.isEmpty())
    {
        return false;
    }

    for (auto it =
         pricesByConsumer.cbegin();
         it != pricesByConsumer.cend();
         ++it)
    {
        const QString consumerId =
            it.key();

        const QMap<int, double> &segments =
            it.value();

        const int segmentCount =
            segments.size();

        if (segmentCount > 5)
        {
            addError(
                errors,
                prefix,
                QString(
                    "用户 %1 的申报段数超过 5 段")
                    .arg(consumerId));

            continue;
        }

        for (int segment = 1;
             segment <= segmentCount;
             ++segment)
        {
            if (!segments.contains(segment))
            {
                addError(
                    errors,
                    prefix,
                    QString(
                        "用户 %1 的申报段必须从 1 连续编号")
                        .arg(consumerId));

                break;
            }
        }

        for (int segment = 2;
             segment <= segmentCount;
             ++segment)
        {
            if (!segments.contains(segment - 1) ||
                !segments.contains(segment))
            {
                continue;
            }

            if (segments.value(segment) >
                segments.value(segment - 1))
            {
                addError(
                    errors,
                    prefix,
                    QString(
                        "用户 %1 的申报电价必须随申报段单调不增")
                        .arg(consumerId));

                break;
            }
        }
    }

    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }

    data = tempData;

    return true;
}


// 负荷曲线读取
bool DataReader::readLoadCurve(
    const QString &filePath,
    QVector<LoadPoint> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    const QStringList expectedHeader =
        {
            "时段",
            "时刻",
            "负荷(MW)"
        };

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            expectedHeader,
            QString(),
            rows,
            errors))
    {
        return false;
    }

    QVector<LoadPoint> tempData;
    QSet<int> periods;

    for (const CsvRow &row : rows)
    {
        const QStringList &c =
            row.columns;

        LoadPoint item;

        item.time = c[1];

        bool rowValid = true;

        if (!parsePositiveInt(
                c[0],
                item.period,
                row.lineNumber,
                "时段",
                QString(),
                errors))
        {
            rowValid = false;
        }
        else if (item.period > 96)
        {
            errors.append(
                QString(
                    "第 %1 行时段必须在 1~96 范围内")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!checkNotEmpty(
                item.time,
                row.lineNumber,
                "时刻",
                QString(),
                errors))
        {
            rowValid = false;
        }

        if (!parsePositivePower(
                c[2],
                item.load,
                row.lineNumber,
                "负荷",
                errors))
        {
            rowValid = false;
        }

        if (!rowValid)
        {
            continue;
        }

        if (item.time !=
            expectedTime(item.period))
        {
            errors.append(
                QString(
                    "第 %1 行时刻应为 %2，实际为 %3")
                    .arg(row.lineNumber)
                    .arg(expectedTime(item.period))
                    .arg(item.time));

            continue;
        }

        if (periods.contains(
                item.period))
        {
            errors.append(
                QString(
                    "第 %1 行时段 %2 重复")
                    .arg(row.lineNumber)
                    .arg(item.period));

            continue;
        }

        periods.insert(
            item.period);

        tempData.push_back(item);
    }

    if (!errors.isEmpty())
    {
        return false;
    }

    if (tempData.size() != 96)
    {
        errors.append(
            QString(
                "负荷曲线必须包含 96 个时段，实际为 %1 个")
                .arg(tempData.size()));
    }

    for (int period = 1;
         period <= 96;
         ++period)
    {
        if (!periods.contains(period))
        {
            errors.append(
                QString(
                    "负荷曲线缺少时段 %1")
                    .arg(period));
        }
    }

    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }

    data = tempData;

    return true;
}


// 新能源出力读取
bool DataReader::readRenewableOutput(
    const QString &filePath,
    QVector<RenewableOutput> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    const QStringList expectedHeader =
        {
            "机组ID",
            "机组类型",
            "时段",
            "出力(MW)"
        };

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            expectedHeader,
            QString(),
            rows,
            errors))
    {
        return false;
    }

    const QSet<QString> allowedTypes =
        {
            "风电",
            "光伏"
        };

    QVector<RenewableOutput> tempData;

    QSet<QString> keys;

    QHash<QString, QString>
        typeByGenerator;

    QHash<QString, QSet<int>>
        periodsByGenerator;

    for (const CsvRow &row : rows)
    {
        const QStringList &c =
            row.columns;

        RenewableOutput item;

        item.generatorId = c[0];
        item.generatorType = c[1];

        bool rowValid = true;

        if (!checkNotEmpty(
                item.generatorId,
                row.lineNumber,
                "新能源机组 ID",
                QString(),
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.generatorType,
                row.lineNumber,
                "新能源类型",
                QString(),
                errors))
        {
            rowValid = false;
        }

        if (!allowedTypes.contains(
                item.generatorType))
        {
            errors.append(
                QString(
                    "第 %1 行新能源类型只能为风电或光伏")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!parsePositiveInt(
                c[2],
                item.period,
                row.lineNumber,
                "时段",
                QString(),
                errors))
        {
            rowValid = false;
        }
        else if (item.period > 96)
        {
            errors.append(
                QString(
                    "第 %1 行时段必须在 1~96 范围内")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!parseNonNegativePower(
                c[3],
                item.output,
                row.lineNumber,
                "新能源出力",
                errors))
        {
            rowValid = false;
        }

        if (!rowValid)
        {
            continue;
        }

        if (typeByGenerator.contains(
                item.generatorId) &&
            typeByGenerator.value(
                item.generatorId) !=
                item.generatorType)
        {
            errors.append(
                QString(
                    "第 %1 行机组 %2 的新能源类型与前面不一致")
                    .arg(row.lineNumber)
                    .arg(item.generatorId));

            continue;
        }

        const QString key =
            item.generatorId +
            "|" +
            QString::number(
                item.period);

        if (keys.contains(key))
        {
            errors.append(
                QString(
                    "第 %1 行机组 %2 在时段 %3 的新能源出力重复")
                    .arg(row.lineNumber)
                    .arg(item.generatorId)
                    .arg(item.period));

            continue;
        }

        keys.insert(key);

        typeByGenerator[
            item.generatorId] =
            item.generatorType;

        periodsByGenerator[
            item.generatorId]
            .insert(item.period);

        tempData.push_back(item);
    }

    if (!errors.isEmpty())
    {
        return false;
    }

    for (auto it =
         periodsByGenerator.cbegin();
         it != periodsByGenerator.cend();
         ++it)
    {
        const QString generatorId =
            it.key();

        const QSet<int> &periods =
            it.value();

        if (periods.size() != 96)
        {
            errors.append(
                QString(
                    "新能源机组 %1 必须包含 96 个时段，实际为 %2 个")
                    .arg(generatorId)
                    .arg(periods.size()));

            continue;
        }

        for (int period = 1;
             period <= 96;
             ++period)
        {
            if (!periods.contains(period))
            {
                errors.append(
                    QString(
                        "新能源机组 %1 缺少时段 %2")
                        .arg(generatorId)
                        .arg(period));

                break;
            }
        }
    }

    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }

    data = tempData;

    return true;
}


// 统一读取接口
bool DataReader::readAll(
    const DataFileSet &files,
    MarketData &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    MarketData tempData;
    QStringList fileErrors;

    bool allOk = true;

    if (!readGeneratorBids(
            files.generatorBidsFile,
            tempData.generatorBids,
            fileErrors))
    {
        appendErrors(
            "generator_bids.csv",
            fileErrors,
            errors);

        allOk = false;
    }

    fileErrors.clear();

    if (!readConsumerBids(
            files.consumerBidsFile,
            tempData.consumerBids,
            fileErrors))
    {
        appendErrors(
            "consumer_bids.csv",
            fileErrors,
            errors);

        allOk = false;
    }

    fileErrors.clear();

    if (!readLoadCurve(
            files.loadCurveFile,
            tempData.loadCurve,
            fileErrors))
    {
        appendErrors(
            "load_curve.csv",
            fileErrors,
            errors);

        allOk = false;
    }

    fileErrors.clear();

    if (!readRenewableOutput(
            files.renewableOutputFile,
            tempData.renewableOutputs,
            fileErrors))
    {
        appendErrors(
            "renewable_output.csv",
            fileErrors,
            errors);

        allOk = false;
    }

    if (!allOk)
    {
        data.clear();
        return false;
    }

    data = tempData;

    return true;
}


// 跨文件一致性校验
bool DataReader::validateRelations(
    const MarketData &data,
    QStringList &errors)
{
    errors.clear();

    QHash<QString, QString>
        generatorTypes;

    for (const GeneratorBid &item :
         data.generatorBids)
    {
        generatorTypes[item.id] =
            item.type;
    }

    QHash<QString, QString>
        renewableTypes;

    for (const RenewableOutput &item :
         data.renewableOutputs)
    {
        renewableTypes[
            item.generatorId] =
            item.generatorType;
    }

    for (auto it =
         renewableTypes.cbegin();
         it != renewableTypes.cend();
         ++it)
    {
        const QString generatorId =
            it.key();

        const QString renewableType =
            it.value();

        if (!generatorTypes.contains(
                generatorId))
        {
            addError(
                errors,
                "跨文件",
                QString(
                    "新能源机组 %1 未在 generator_bids.csv 中定义")
                    .arg(generatorId));

            continue;
        }

        if (generatorTypes.value(
                generatorId) !=
            renewableType)
        {
            addError(
                errors,
                "跨文件",
                QString(
                    "新能源机组 %1 的类型与 generator_bids.csv 不一致")
                    .arg(generatorId));
        }
    }

    if (!data.consumerBids.isEmpty() &&
        !data.loadCurve.isEmpty())
    {
        double consumerEnergy =
            0.0;

        for (const ConsumerBid &item :
             data.consumerBids)
        {
            consumerEnergy +=
                item.quantity;
        }

        double loadEnergy =
            0.0;

        for (const LoadPoint &item :
             data.loadCurve)
        {
            loadEnergy +=
                item.load * 0.25;
        }

        const double difference =
            consumerEnergy -
            loadEnergy;

        if (std::abs(difference) >
            0.05)
        {
            addError(
                errors,
                "跨文件",
                QString(
                    "购电申报总电量为 %1 MWh，当日负荷总电量为 %2 MWh，差额为 %3 MWh")
                    .arg(
                        consumerEnergy,
                        0,
                        'f',
                        1)
                    .arg(
                        loadEnergy,
                        0,
                        'f',
                        1)
                    .arg(
                        difference,
                        0,
                        'f',
                        1));
        }
    }

    return errors.isEmpty();
}