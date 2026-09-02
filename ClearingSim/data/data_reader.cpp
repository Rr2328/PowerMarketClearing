#include "data_reader.h"

#include <QFile>
#include <QHash>
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


bool checkHeader(
    const QStringList &actualHeader,
    const QStringList &expectedHeader,
    QStringList &errors)
{
    if (actualHeader.size() != expectedHeader.size())
    {
        errors.append(
            QString("CSV 表头列数错误：应为 %1 列，实际为 %2 列")
                .arg(expectedHeader.size())
                .arg(actualHeader.size()));

        return false;
    }

    for (int i = 0; i < expectedHeader.size(); ++i)
    {
        if (actualHeader[i] != expectedHeader[i])
        {
            errors.append(
                QString("CSV 表头第 %1 列错误：应为“%2”，实际为“%3”")
                    .arg(i + 1)
                    .arg(expectedHeader[i])
                    .arg(actualHeader[i]));

            return false;
        }
    }

    return true;
}


bool readCsvRows(
    const QString &filePath,
    const QStringList &expectedHeader,
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

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
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

    if (!checkHeader(
            header,
            expectedHeader,
            errors))
    {
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
            errors.append(
                QString("第 %1 行列数错误：应为 %2 列，实际为 %3 列")
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
            errors.append(
                "CSV 文件中没有有效数据：" +
                filePath);
        }

        return false;
    }

    return errors.isEmpty();
}


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


bool parseDouble(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    QStringList &errors)
{
    bool ok = false;
    value = text.toDouble(&ok);

    if (!ok ||
        !std::isfinite(value))
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


bool checkPricePrecision(
    const QString &text,
    int lineNumber,
    const QString &fieldName,
    QStringList &errors)
{
    const QString value =
        text.trimmed();

    if (value.contains('e', Qt::CaseInsensitive))
    {
        errors.append(
            QString("第 %1 行 %2 不允许使用科学计数法")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    const int dotIndex =
        value.indexOf('.');

    if (dotIndex < 0)
    {
        return true;
    }

    const int decimalPlaces =
        value.size() -
        dotIndex -
        1;

    if (decimalPlaces > 3)
    {
        errors.append(
            QString("第 %1 行 %2 最多保留 3 位小数")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    return true;
}


bool checkPriceRange(
    double price,
    int lineNumber,
    const QString &fieldName,
    QStringList &errors)
{
    if (price < 0.0 ||
        price > 540.0)
    {
        errors.append(
            QString("第 %1 行 %2 必须在 0～540 元/MWh 范围内")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

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
            << QString("出力P%1(MW)")
                   .arg(segment)
            << QString("报价C%1(元/MWh)")
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
            << QString("购电量P%1(MWh)")
                   .arg(segment)
            << QString("报价C%1(元/MWh)")
                   .arg(segment);
    }

    return header;
}


void appendErrors(
    const QString &fileName,
    const QStringList &sourceErrors,
    QStringList &targetErrors)
{
    for (const QString &error :
         sourceErrors)
    {
        targetErrors.append(
            "[" +
            fileName +
            "] " +
            error);
    }
}

}


// 发电侧
bool DataReader::readGeneratorBids(
    const QString &filePath,
    QVector<GeneratorBid> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    constexpr int maxSegments = 10;
    constexpr int baseColumns = 3;

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            generatorHeader(),
            rows,
            errors))
    {
        return false;
    }

    QVector<GeneratorBid> tempData;

    QSet<QString> unitIds;

    for (const CsvRow &row :
         rows)
    {
        const QStringList &c =
            row.columns;

        const QString plantName =
            c[0];

        const QString unitId =
            c[1];

        const QString unitType =
            c[2];

        bool rowValid = true;

        if (!checkNotEmpty(
                plantName,
                row.lineNumber,
                "电厂名称",
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                unitId,
                row.lineNumber,
                "机组编号",
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                unitType,
                row.lineNumber,
                "机组类型",
                errors))
        {
            rowValid = false;
        }

        if (!unitId.isEmpty())
        {
            if (unitIds.contains(
                    unitId))
            {
                errors.append(
                    QString(
                        "第 %1 行机组编号 %2 重复")
                        .arg(row.lineNumber)
                        .arg(unitId));

                rowValid = false;
            }
            else
            {
                unitIds.insert(
                    unitId);
            }
        }

        QVector<GeneratorBid>
            rowBids;

        bool hasSegment = false;
        bool emptySegmentFound = false;
        bool hasPreviousPrice = false;

        double previousPrice =
            0.0;

        for (int segment = 1;
             segment <= maxSegments;
             ++segment)
        {
            const int quantityIndex =
                baseColumns +
                (segment - 1) * 2;

            const int priceIndex =
                quantityIndex + 1;

            const QString quantityText =
                c[quantityIndex]
                    .trimmed();

            const QString priceText =
                c[priceIndex]
                    .trimmed();

            if (quantityText.isEmpty() &&
                priceText.isEmpty())
            {
                emptySegmentFound =
                    true;

                continue;
            }

            hasSegment = true;

            if (quantityText.isEmpty() ||
                priceText.isEmpty())
            {
                errors.append(
                    QString(
                        "第 %1 行第 %2 段报价不完整")
                        .arg(row.lineNumber)
                        .arg(segment));

                rowValid = false;
                emptySegmentFound =
                    true;

                continue;
            }

            if (emptySegmentFound)
            {
                errors.append(
                    QString(
                        "第 %1 行机组 %2 的报价段不连续")
                        .arg(row.lineNumber)
                        .arg(unitId));

                rowValid = false;
            }

            double quantity = 0.0;
            double price = 0.0;

            const QString quantityName =
                QString("P%1")
                    .arg(segment);

            const QString priceName =
                QString("C%1")
                    .arg(segment);

            const bool quantityOk =
                parseNonNegativeDouble(
                    quantityText,
                    quantity,
                    row.lineNumber,
                    quantityName,
                    errors);

            const bool priceOk =
                parseDouble(
                    priceText,
                    price,
                    row.lineNumber,
                    priceName,
                    errors);

            if (!quantityOk ||
                !priceOk)
            {
                rowValid = false;
                continue;
            }

            if (quantity <= 0.0)
            {
                errors.append(
                    QString(
                        "第 %1 行 P%2 必须大于 0")
                        .arg(row.lineNumber)
                        .arg(segment));

                rowValid = false;
            }

            if (!checkPriceRange(
                    price,
                    row.lineNumber,
                    priceName,
                    errors))
            {
                rowValid = false;
            }

            if (!checkPricePrecision(
                    priceText,
                    row.lineNumber,
                    priceName,
                    errors))
            {
                rowValid = false;
            }

            if (hasPreviousPrice &&
                price < previousPrice)
            {
                errors.append(
                    QString(
                        "第 %1 行机组 %2 的报价必须单调非递减")
                        .arg(row.lineNumber)
                        .arg(unitId));

                rowValid = false;
            }

            GeneratorBid item;

            item.id =
                unitId;

            item.name =
                plantName;

            item.type =
                unitType;

            item.segment =
                segment;

            item.price =
                price;

            item.quantity =
                quantity;

            rowBids.push_back(
                item);

            previousPrice =
                price;

            hasPreviousPrice =
                true;
        }

        if (!hasSegment)
        {
            errors.append(
                QString(
                    "第 %1 行机组 %2 没有填写报价")
                    .arg(row.lineNumber)
                    .arg(unitId));

            rowValid = false;
        }

        if (rowValid)
        {
            for (const GeneratorBid &bid :
                 rowBids)
            {
                tempData.push_back(
                    bid);
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


// 用户侧
bool DataReader::readConsumerBids(
    const QString &filePath,
    QVector<ConsumerBid> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    constexpr int maxSegments = 10;
    constexpr int baseColumns = 2;

    QVector<CsvRow> rows;

    if (!readCsvRows(
            filePath,
            consumerHeader(),
            rows,
            errors))
    {
        return false;
    }

    QVector<ConsumerBid> tempData;

    QSet<QString> consumerIds;

    for (const CsvRow &row :
         rows)
    {
        const QStringList &c =
            row.columns;

        const QString consumerName =
            c[0];

        const QString consumerId =
            c[1];

        bool rowValid = true;

        if (!checkNotEmpty(
                consumerName,
                row.lineNumber,
                "用户名称",
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                consumerId,
                row.lineNumber,
                "用户编号",
                errors))
        {
            rowValid = false;
        }

        if (!consumerId.isEmpty())
        {
            if (consumerIds.contains(
                    consumerId))
            {
                errors.append(
                    QString(
                        "第 %1 行用户编号 %2 重复")
                        .arg(row.lineNumber)
                        .arg(consumerId));

                rowValid = false;
            }
            else
            {
                consumerIds.insert(
                    consumerId);
            }
        }

        QVector<ConsumerBid>
            rowBids;

        bool hasSegment = false;
        bool emptySegmentFound = false;
        bool hasPreviousPrice = false;

        double previousPrice =
            0.0;

        for (int segment = 1;
             segment <= maxSegments;
             ++segment)
        {
            const int quantityIndex =
                baseColumns +
                (segment - 1) * 2;

            const int priceIndex =
                quantityIndex + 1;

            const QString quantityText =
                c[quantityIndex]
                    .trimmed();

            const QString priceText =
                c[priceIndex]
                    .trimmed();

            if (quantityText.isEmpty() &&
                priceText.isEmpty())
            {
                emptySegmentFound =
                    true;

                continue;
            }

            hasSegment = true;

            if (quantityText.isEmpty() ||
                priceText.isEmpty())
            {
                errors.append(
                    QString(
                        "第 %1 行第 %2 段报价不完整")
                        .arg(row.lineNumber)
                        .arg(segment));

                rowValid = false;
                emptySegmentFound =
                    true;

                continue;
            }

            if (emptySegmentFound)
            {
                errors.append(
                    QString(
                        "第 %1 行用户 %2 的报价段不连续")
                        .arg(row.lineNumber)
                        .arg(consumerId));

                rowValid = false;
            }

            double quantity = 0.0;
            double price = 0.0;

            const QString quantityName =
                QString("P%1")
                    .arg(segment);

            const QString priceName =
                QString("C%1")
                    .arg(segment);

            const bool quantityOk =
                parseNonNegativeDouble(
                    quantityText,
                    quantity,
                    row.lineNumber,
                    quantityName,
                    errors);

            const bool priceOk =
                parseDouble(
                    priceText,
                    price,
                    row.lineNumber,
                    priceName,
                    errors);

            if (!quantityOk ||
                !priceOk)
            {
                rowValid = false;
                continue;
            }

            if (quantity <= 0.0)
            {
                errors.append(
                    QString(
                        "第 %1 行 P%2 必须大于 0")
                        .arg(row.lineNumber)
                        .arg(segment));

                rowValid = false;
            }

            if (!checkPriceRange(
                    price,
                    row.lineNumber,
                    priceName,
                    errors))
            {
                rowValid = false;
            }

            if (!checkPricePrecision(
                    priceText,
                    row.lineNumber,
                    priceName,
                    errors))
            {
                rowValid = false;
            }

            if (hasPreviousPrice &&
                price > previousPrice)
            {
                errors.append(
                    QString(
                        "第 %1 行用户 %2 的报价必须单调非递增")
                        .arg(row.lineNumber)
                        .arg(consumerId));

                rowValid = false;
            }

            ConsumerBid item;

            item.id =
                consumerId;

            item.name =
                consumerName;

            item.segment =
                segment;

            item.price =
                price;

            item.quantity =
                quantity;

            rowBids.push_back(
                item);

            previousPrice =
                price;

            hasPreviousPrice =
                true;
        }

        if (!hasSegment)
        {
            errors.append(
                QString(
                    "第 %1 行用户 %2 没有填写报价")
                    .arg(row.lineNumber)
                    .arg(consumerId));

            rowValid = false;
        }

        if (rowValid)
        {
            for (const ConsumerBid &bid :
                 rowBids)
            {
                tempData.push_back(
                    bid);
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


// 负荷曲线
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
            rows,
            errors))
    {
        return false;
    }

    QVector<LoadPoint> tempData;

    QSet<int> periods;

    for (const CsvRow &row :
         rows)
    {
        const QStringList &c =
            row.columns;

        LoadPoint item;

        item.time =
            c[1];

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
                "时刻",
                errors))
        {
            rowValid = false;
        }

        if (!parseNonNegativeDouble(
                c[2],
                item.load,
                row.lineNumber,
                "负荷",
                errors))
        {
            rowValid = false;
        }

        if (rowValid &&
            (item.period < 1 ||
             item.period > 96))
        {
            errors.append(
                QString(
                    "第 %1 行时段 %2 超出 1～96 范围")
                    .arg(row.lineNumber)
                    .arg(item.period));

            rowValid = false;
        }

        if (rowValid)
        {
            if (periods.contains(
                    item.period))
            {
                errors.append(
                    QString(
                        "第 %1 行时段 %2 重复")
                        .arg(row.lineNumber)
                        .arg(item.period));

                rowValid = false;
            }
            else
            {
                periods.insert(
                    item.period);
            }
        }

        if (rowValid)
        {
            tempData.push_back(
                item);
        }
    }

    if (periods.size() != 96)
    {
        errors.append(
            QString(
                "负荷曲线应包含 96 个时段，实际为 %1 个")
                .arg(periods.size()));
    }

    for (int period = 1;
         period <= 96;
         ++period)
    {
        if (!periods.contains(
                period))
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


// 新能源基准出力
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
            rows,
            errors))
    {
        return false;
    }

    QVector<RenewableOutput>
        tempData;

    QSet<QString> keys;

    QHash<QString, QSet<int>>
        generatorPeriods;

    for (const CsvRow &row :
         rows)
    {
        const QStringList &c =
            row.columns;

        RenewableOutput item;

        item.generatorId =
            c[0];

        item.generatorType =
            c[1];

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

        if (item.generatorType != "风电" &&
            item.generatorType != "光伏")
        {
            errors.append(
                QString(
                    "第 %1 行新能源类型必须为风电或光伏")
                    .arg(row.lineNumber));

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

        if (rowValid &&
            (item.period < 1 ||
             item.period > 96))
        {
            errors.append(
                QString(
                    "第 %1 行时段 %2 超出 1～96 范围")
                    .arg(row.lineNumber)
                    .arg(item.period));

            rowValid = false;
        }

        if (rowValid)
        {
            const QString key =
                item.generatorId +
                "|" +
                QString::number(
                    item.period);

            if (keys.contains(
                    key))
            {
                errors.append(
                    QString(
                        "第 %1 行机组 %2 在时段 %3 的新能源出力重复")
                        .arg(row.lineNumber)
                        .arg(item.generatorId)
                        .arg(item.period));

                rowValid = false;
            }
            else
            {
                keys.insert(
                    key);
            }
        }

        if (rowValid)
        {
            generatorPeriods[
                item.generatorId]
                .insert(
                    item.period);

            tempData.push_back(
                item);
        }
    }

    for (auto it =
         generatorPeriods.constBegin();
         it != generatorPeriods.constEnd();
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
                    "新能源机组 %1 应包含 96 个时段，实际为 %2 个")
                    .arg(generatorId)
                    .arg(periods.size()));
        }

        for (int period = 1;
             period <= 96;
             ++period)
        {
            if (!periods.contains(
                    period))
            {
                errors.append(
                    QString(
                        "新能源机组 %1 缺少时段 %2")
                        .arg(generatorId)
                        .arg(period));
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


// 统一读取
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


// 跨文件一致性
bool DataReader::validateRelations(
    const MarketData &data,
    QStringList &errors)
{
    errors.clear();

    QHash<QString, QString>
        generatorTypes;

    QSet<QString>
        renewableGeneratorIds;

    for (const GeneratorBid &bid :
         data.generatorBids)
    {
        if (!generatorTypes.contains(
                bid.id))
        {
            generatorTypes.insert(
                bid.id,
                bid.type);
        }

        if (bid.type == "风电" ||
            bid.type == "光伏")
        {
            renewableGeneratorIds.insert(
                bid.id);
        }
    }

    QSet<QString>
        renewableOutputIds;

    QSet<QString>
        missingGeneratorIds;

    QSet<QString>
        typeMismatchIds;

    for (const RenewableOutput &item :
         data.renewableOutputs)
    {
        renewableOutputIds.insert(
            item.generatorId);

        if (!generatorTypes.contains(
                item.generatorId))
        {
            if (!missingGeneratorIds.contains(
                    item.generatorId))
            {
                errors.append(
                    QString(
                        "新能源机组 %1 在发电侧申报中不存在")
                        .arg(item.generatorId));

                missingGeneratorIds.insert(
                    item.generatorId);
            }

            continue;
        }

        const QString generatorType =
            generatorTypes.value(
                item.generatorId);

        if (generatorType !=
            item.generatorType)
        {
            if (!typeMismatchIds.contains(
                    item.generatorId))
            {
                errors.append(
                    QString(
                        "新能源机组 %1 的机组类型不一致")
                        .arg(item.generatorId));

                typeMismatchIds.insert(
                    item.generatorId);
            }
        }
    }

    for (const QString &generatorId :
         renewableGeneratorIds)
    {
        if (!renewableOutputIds.contains(
                generatorId))
        {
            errors.append(
                QString(
                    "新能源机组 %1 缺少新能源出力数据")
                    .arg(generatorId));
        }
    }

    return errors.isEmpty();
}