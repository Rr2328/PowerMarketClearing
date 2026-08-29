#include "data_reader.h"

#include <QFile>
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
    int expectedColumnCount,
    QVector<CsvRow> &rows,
    QStringList &errors)
{
    rows.clear();

    QFile file(filePath);

    if (!file.exists())
    {
        errors.append("文件不存在：" + filePath);
        return false;
    }

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
    {
        errors.append("文件无法打开：" + filePath);
        return false;
    }

    QTextStream in(&file);

    if (in.atEnd())
    {
        errors.append("CSV 文件为空：" + filePath);
        return false;
    }

    const QString headerLine =
        in.readLine().trimmed();

    if (headerLine.isEmpty())
    {
        errors.append("CSV 表头为空：" + filePath);
        return false;
    }

    const QStringList header =
        splitCsvLine(headerLine);

    if (header.size() != expectedColumnCount)
    {
        errors.append(
            QString("CSV 表头列数错误：应为 %1 列，实际为 %2 列")
                .arg(expectedColumnCount)
                .arg(header.size()));

        return false;
    }

    for (int i = 0; i < header.size(); ++i)
    {
        if (header[i].isEmpty())
        {
            errors.append(
                QString("CSV 表头第 %1 列为空")
                    .arg(i + 1));

            return false;
        }
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

        if (columns.size() != expectedColumnCount)
        {
            errors.append(
                QString("第 %1 行列数错误：应为 %2 列，实际为 %3 列")
                    .arg(lineNumber)
                    .arg(expectedColumnCount)
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
            errors.append(
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
    QStringList &errors)
{
    if (value.trimmed().isEmpty())
    {
        errors.append(
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
    QStringList &errors)
{
    bool ok = false;

    value = text.toInt(&ok);

    if (!ok)
    {
        errors.append(
            QString("第 %1 行 %2 不是有效整数：%3")
                .arg(lineNumber)
                .arg(fieldName)
                .arg(text));

        return false;
    }

    if (value <= 0)
    {
        errors.append(
            QString("第 %1 行 %2 必须大于 0")
                .arg(lineNumber)
                .arg(fieldName));

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
    QStringList &errors)
{
    bool ok = false;

    value = text.toDouble(&ok);

    if (!ok || !std::isfinite(value))
    {
        errors.append(
            QString("第 %1 行 %2 不是有效数字：%3")
                .arg(lineNumber)
                .arg(fieldName)
                .arg(text));

        return false;
    }

    return true;
}

// 非负数检查
bool parseNonNegativeDouble(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    QStringList &errors)
{
    if (!parseDouble(
            text,
            value,
            lineNumber,
            fieldName,
            errors))
    {
        return false;
    }

    if (value < 0.0)
    {
        errors.append(
            QString("第 %1 行 %2 不能为负数")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    return true;
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
            "[" + fileName + "] " + error);
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

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            6,
            rows,
            errors))
    {
        return false;
    }

    QVector<GeneratorBid> tempData;
    QSet<QString> segmentKeys;

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
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.name,
                row.lineNumber,
                "机组名称",
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.type,
                row.lineNumber,
                "机组类型",
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveInt(
                c[3],
                item.segment,
                row.lineNumber,
                "申报分段",
                errors))
        {
            rowValid = false;
        }

        if (!parseDouble(
                c[4],
                item.price,
                row.lineNumber,
                "申报价格",
                errors))
        {
            rowValid = false;
        }

        if (!parseNonNegativeDouble(
                c[5],
                item.quantity,
                row.lineNumber,
                "申报电量",
                errors))
        {
            rowValid = false;
        }

        // 检查重复申报分段
        if (rowValid)
        {
            const QString key =
                item.id +
                "|" +
                QString::number(item.segment);

            if (segmentKeys.contains(key))
            {
                errors.append(
                    QString("第 %1 行机组 %2 的申报分段 %3 重复")
                        .arg(row.lineNumber)
                        .arg(item.id)
                        .arg(item.segment));

                rowValid = false;
            }
            else
            {
                segmentKeys.insert(key);
            }
        }

        if (rowValid)
        {
            tempData.push_back(item);
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

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            5,
            rows,
            errors))
    {
        return false;
    }

    QVector<ConsumerBid> tempData;
    QSet<QString> segmentKeys;

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
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.name,
                row.lineNumber,
                "用户名称",
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveInt(
                c[2],
                item.segment,
                row.lineNumber,
                "申报分段",
                errors))
        {
            rowValid = false;
        }

        if (!parseDouble(
                c[3],
                item.price,
                row.lineNumber,
                "申报价格",
                errors))
        {
            rowValid = false;
        }

        if (!parseNonNegativeDouble(
                c[4],
                item.quantity,
                row.lineNumber,
                "申报电量",
                errors))
        {
            rowValid = false;
        }

        // 检查重复申报分段
        if (rowValid)
        {
            const QString key =
                item.id +
                "|" +
                QString::number(item.segment);

            if (segmentKeys.contains(key))
            {
                errors.append(
                    QString("第 %1 行用户 %2 的申报分段 %3 重复")
                        .arg(row.lineNumber)
                        .arg(item.id)
                        .arg(item.segment));

                rowValid = false;
            }
            else
            {
                segmentKeys.insert(key);
            }
        }

        if (rowValid)
        {
            tempData.push_back(item);
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

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            3,
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
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.time,
                row.lineNumber,
                "时间",
                errors))
        {
            rowValid = false;
        }

        if (!parseNonNegativeDouble(
                c[2],
                item.load,
                row.lineNumber,
                "系统负荷",
                errors))
        {
            rowValid = false;
        }

        // 检查重复时段
        if (rowValid)
        {
            if (periods.contains(item.period))
            {
                errors.append(
                    QString("第 %1 行时段 %2 重复")
                        .arg(row.lineNumber)
                        .arg(item.period));

                rowValid = false;
            }
            else
            {
                periods.insert(item.period);
            }
        }

        if (rowValid)
        {
            tempData.push_back(item);
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

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            4,
            rows,
            errors))
    {
        return false;
    }

    QVector<RenewableOutput> tempData;
    QSet<QString> keys;

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
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.generatorType,
                row.lineNumber,
                "新能源类型",
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveInt(
                c[2],
                item.period,
                row.lineNumber,
                "时段",
                errors))
        {
            rowValid = false;
        }

        if (!parseNonNegativeDouble(
                c[3],
                item.output,
                row.lineNumber,
                "新能源出力",
                errors))
        {
            rowValid = false;
        }

        // 检查同一机组重复时段
        if (rowValid)
        {
            const QString key =
                item.generatorId +
                "|" +
                QString::number(item.period);

            if (keys.contains(key))
            {
                errors.append(
                    QString("第 %1 行机组 %2 在时段 %3 的新能源出力重复")
                        .arg(row.lineNumber)
                        .arg(item.generatorId)
                        .arg(item.period));

                rowValid = false;
            }
            else
            {
                keys.insert(key);
            }
        }

        if (rowValid)
        {
            tempData.push_back(item);
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