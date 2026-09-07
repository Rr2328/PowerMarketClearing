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

// 申报价格检查（3 位小数、0~540 元/MWh；字段名随格式可变）
bool parseBidPrice(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    const QString &prefix,
    QStringList &errors)
{
    if (!parseDouble(
            text,
            value,
            lineNumber,
            fieldName,
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
                "第 %1 行 %2 必须在 0~540 元/MWh")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    if (!checkPrecision(
            text,
            3,
            lineNumber,
            fieldName,
            prefix,
            errors))
    {
        return false;
    }

    return true;
}

// 申报量检查（1 位小数、非负；V1.3 规则⑥：允许 0 = 该时段不申报/停机）
bool parseBidQuantity(
    const QString &text,
    double &value,
    int lineNumber,
    const QString &fieldName,
    const QString &prefix,
    QStringList &errors)
{
    if (!parseDouble(
            text,
            value,
            lineNumber,
            fieldName,
            prefix,
            errors))
    {
        return false;
    }

    if (value < 0.0)
    {
        addError(
            errors,
            prefix,
            QString(
                "第 %1 行 %2 不能为负数")
                .arg(lineNumber)
                .arg(fieldName));

        return false;
    }

    if (!checkPrecision(
            text,
            1,
            lineNumber,
            fieldName,
            prefix,
            errors))
    {
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

// ------------------------------------------------------------------
// V1.3 申报表读取：长表（period 行 × 段成对列）+ 窄表兼容展开
// ------------------------------------------------------------------

// 无表头校验的 CSV 读取（表头由调用方按格式识别后自行校验）
bool readCsvRowsRaw(
    const QString &filePath,
    const QString &prefix,
    QStringList &header,
    QVector<CsvRow> &rows,
    QStringList &errors)
{
    rows.clear();
    header.clear();

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

    header = splitCsvLine(
        in.readLine().trimmed());

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

        if (columns.size() != header.size())
        {
            addError(
                errors,
                prefix,
                QString(
                    "第 %1 行列数错误：应为 %2 列，实际为 %3 列")
                    .arg(lineNumber)
                    .arg(header.size())
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

    return true;
}

// 长表解析的中间结构（发电/购电共用，字段同构）
struct ParsedBid
{
    QString id;
    QString name;

    int period = 0;
    int segment = 0;

    double price = 0.0;
    double quantity = 0.0;
};

// 长表格式描述（发电/购电各一份）
struct LongTableConfig
{
    QString prefix;         // "发电" / "购电"
    QString nameHeader;     // "电厂名称" / "用户名称"
    QString idHeader;       // "机组编号" / "负荷编号"
    QString qtyHeaderFmt;   // "第%1段出力(MW)" / "第%1段申报量(MW)"
    QString qtyFieldFmt;    // "第%1段出力" / "第%1段申报量"
    bool priceMonotonicUp;  // 发电：段价单调不减；购电：单调不增
};

// #96：长/窄表展开共享 helper（消除 readGeneratorBids / readConsumerBids 重复）
//   窄表解析无 period 字段，需要展开为 96 期同量同价（作对拍锚点）；
//   长表已带 period 字段，调用本函数是 no-op（period != 0 不展开）。
void expandParsedBidsTo96Periods(QVector<ParsedBid> &bids)
{
    QVector<ParsedBid> expanded;
    for (const ParsedBid &bid : bids) {
        if (bid.period > 0) {
            expanded.push_back(bid);   // 长表自带 period，不复制
            continue;
        }
        for (int period = 1; period <= 96; ++period) {
            ParsedBid item = bid;
            item.period = period;
            expanded.push_back(item);
        }
    }
    bids = std::move(expanded);
}

// #96：ParsedBid → GeneratorBid 转换（共享）
void materializeToGeneratorBids(
    const QVector<ParsedBid> &parsed, QVector<GeneratorBid> &out)
{
    out.reserve(out.size() + parsed.size());
    for (const ParsedBid &bid : parsed) {
        GeneratorBid item;
        item.id = bid.id;
        item.name = bid.name;
        item.period = bid.period;
        item.segment = bid.segment;
        item.price = bid.price;
        item.quantity = bid.quantity;
        out.push_back(item);
    }
}

// #96：ParsedBid → ConsumerBid 转换（共享）
void materializeToConsumerBids(
    const QVector<ParsedBid> &parsed, QVector<ConsumerBid> &out)
{
    out.reserve(out.size() + parsed.size());
    for (const ParsedBid &bid : parsed) {
        ConsumerBid item;
        item.id = bid.id;
        item.name = bid.name;
        item.period = bid.period;
        item.segment = bid.segment;
        item.price = bid.price;
        item.quantity = bid.quantity;
        out.push_back(item);
    }
}

// 识别长表表头并校验成对列结构
//   返回段对数（1~5）；不是长表返回 0；是长表意图但结构错误返回 -1
int parseLongHeader(
    const QStringList &header,
    const LongTableConfig &cfg,
    QStringList &errors)
{
    if (header.isEmpty() ||
        header[0] != QStringLiteral("period"))
    {
        return 0;
    }

    if (header.size() < 5 ||
        header[1] != cfg.nameHeader ||
        header[2] != cfg.idHeader)
    {
        addError(
            errors,
            cfg.prefix,
            QString(
                "长表表头应为：period,%1,%2,第N段…成对列")
                .arg(cfg.nameHeader,
                     cfg.idHeader));

        return -1;
    }

    const int pairCount =
        (header.size() - 3) / 2;

    if ((header.size() - 3) % 2 != 0 ||
        pairCount < 1 ||
        pairCount > 5)
    {
        addError(
            errors,
            cfg.prefix,
            "长表表头段列不成对：第N段出力/第N段报价必须成对出现，且最多 5 段");

        return -1;
    }

    for (int n = 1; n <= pairCount; ++n)
    {
        const QString expectedQty =
            cfg.qtyHeaderFmt.arg(n);

        const QString expectedPrice =
            QStringLiteral("第%1段报价(元/MWh)").arg(n);

        if (header[3 + 2 * (n - 1)] != expectedQty ||
            header[4 + 2 * (n - 1)] != expectedPrice)
        {
            addError(
                errors,
                cfg.prefix,
                QString(
                    "长表表头第 %1 段列名应为「%2,%3」")
                    .arg(n)
                    .arg(expectedQty,
                         expectedPrice));

            return -1;
        }
    }

    return pairCount;
}

// 长表行解析：一行 = 主体在某时段的全部段申报
bool parseLongBidRows(
    const QVector<CsvRow> &rows,
    int pairCount,
    const LongTableConfig &cfg,
    QVector<ParsedBid> &out,
    QStringList &errors)
{
    out.clear();

    QSet<QString> rowKeys;

    QHash<QString, QSet<int>>
        periodsByEntity;

    for (const CsvRow &row : rows)
    {
        const QStringList &c =
            row.columns;

        int period = 0;

        bool rowValid = true;

        if (!parsePositiveInt(
                c[0],
                period,
                row.lineNumber,
                "period",
                cfg.prefix,
                errors))
        {
            rowValid = false;
        }
        else if (period > 96)
        {
            addError(
                errors,
                cfg.prefix,
                QString(
                    "第 %1 行 period 必须在 1~96 范围内")
                    .arg(row.lineNumber));

            rowValid = false;
        }

        if (!checkNotEmpty(
                c[1],
                row.lineNumber,
                cfg.nameHeader,
                cfg.prefix,
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                c[2],
                row.lineNumber,
                cfg.idHeader,
                cfg.prefix,
                errors))
        {
            rowValid = false;
        }

        if (!rowValid)
        {
            continue;
        }

        // ---- 段成对列：从段 1 起连续，空对之后不得再有数据 ----
        double prevPrice = 0.0;
        bool hasPrev = false;
        bool seenAbsentPair = false;

        for (int n = 1; n <= pairCount; ++n)
        {
            const QString &qtyCell =
                c[3 + 2 * (n - 1)];

            const QString &priceCell =
                c[4 + 2 * (n - 1)];

            const bool absent =
                qtyCell.isEmpty() &&
                priceCell.isEmpty();

            if (absent)
            {
                seenAbsentPair = true;
                continue;
            }

            if (seenAbsentPair)
            {
                addError(
                    errors,
                    cfg.prefix,
                    QString(
                        "第 %1 行 %2 %3 时段 %4：申报段必须从 1 连续编号（第 %5 段为空但其后有数据）")
                        .arg(row.lineNumber)
                        .arg(c[1], c[2])
                        .arg(period)
                        .arg(n));

                rowValid = false;
                break;
            }

            if (qtyCell.isEmpty())
            {
                addError(
                    errors,
                    cfg.prefix,
                    QString(
                        "第 %1 行第 %2 段有报价但缺少%3")
                        .arg(row.lineNumber)
                        .arg(n)
                        .arg(cfg.qtyFieldFmt.arg(n)));

                rowValid = false;
                continue;
            }

            double quantity = 0.0;
            double price = 0.0;

            if (!parseBidQuantity(
                    qtyCell,
                    quantity,
                    row.lineNumber,
                    cfg.qtyFieldFmt.arg(n),
                    cfg.prefix,
                    errors))
            {
                rowValid = false;
                continue;
            }

            if (!parseBidPrice(
                    priceCell,
                    price,
                    row.lineNumber,
                    QStringLiteral("第%1段报价").arg(n),
                    cfg.prefix,
                    errors))
            {
                rowValid = false;
                continue;
            }

            if (hasPrev &&
                cfg.priceMonotonicUp &&
                price < prevPrice)
            {
                addError(
                    errors,
                    cfg.prefix,
                    QString(
                        "第 %1 行 %2 %3 时段 %4：申报电价必须随申报段单调不减")
                        .arg(row.lineNumber)
                        .arg(c[1], c[2])
                        .arg(period));

                rowValid = false;
            }

            if (hasPrev &&
                !cfg.priceMonotonicUp &&
                price > prevPrice)
            {
                addError(
                    errors,
                    cfg.prefix,
                    QString(
                        "第 %1 行 %2 %3 时段 %4：申报电价必须随申报段单调不增")
                        .arg(row.lineNumber)
                        .arg(c[1], c[2])
                        .arg(period));

                rowValid = false;
            }

            prevPrice = price;
            hasPrev = true;

            ParsedBid bid;
            bid.id = c[2];
            bid.name = c[1];
            bid.period = period;
            bid.segment = n;
            bid.price = price;
            bid.quantity = quantity;

            out.push_back(bid);
        }

        if (!rowValid)
        {
            continue;
        }

        // ---- 主体×时段 去重 ----
        const QString entityKey =
            c[1] + "|" + c[2];

        const QString rowKey =
            entityKey + "|" +
            QString::number(period);

        if (rowKeys.contains(rowKey))
        {
            addError(
                errors,
                cfg.prefix,
                QString(
                    "第 %1 行主体 %2 %3 在时段 %4 重复申报")
                    .arg(row.lineNumber)
                    .arg(c[1], c[2])
                    .arg(period));

            continue;
        }

        rowKeys.insert(rowKey);

        periodsByEntity[entityKey]
            .insert(period);
    }

    if (!errors.isEmpty())
    {
        out.clear();
        return false;
    }

    // ---- 时段覆盖检查：每个主体必须覆盖 96 个时段 ----
    for (auto it =
         periodsByEntity.cbegin();
         it != periodsByEntity.cend();
         ++it)
    {
        const QString entityKey =
            it.key();

        const QSet<int> &periods =
            it.value();

        if (periods.size() != 96)
        {
            QString displayKey =
                entityKey;

            displayKey.replace(
                QLatin1Char('|'),
                QLatin1Char(' '));

            addError(
                errors,
                cfg.prefix,
                QString(
                    "主体 %1 必须包含 96 个时段的申报，实际为 %2 个")
                    .arg(displayKey)
                    .arg(periods.size()));

            continue;
        }
    }

    if (!errors.isEmpty())
    {
        out.clear();
        return false;
    }

    return true;
}

// 窄表（V1.1/V1.2 兼容）列映射
struct NarrowColumnMap
{
    int id = 0;
    int name = 1;
    int segment = 2;
    int price = 3;
    int quantity = 4;
};

// 窄表行解析：一行 = 主体×段（日内一份，无 period 维度）
//   校验沿用 V1.1 规则；「机组类型」列（若存在）忽略不校验（D7）
bool parseNarrowBidRows(
    const QVector<CsvRow> &rows,
    const NarrowColumnMap &columns,
    const QString &prefix,
    bool priceMonotonicUp,
    QVector<ParsedBid> &out,
    QStringList &errors)
{
    out.clear();

    QSet<QString> segmentKeys;

    QHash<QString, QString> idToName;

    QMap<QString, QMap<int, double>>
        pricesByEntity;

    for (const CsvRow &row : rows)
    {
        const QStringList &c =
            row.columns;

        ParsedBid item;

        item.id = c[columns.id];
        item.name = c[columns.name];

        bool rowValid = true;

        if (!checkNotEmpty(
                item.id,
                row.lineNumber,
                "主体 ID",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!checkNotEmpty(
                item.name,
                row.lineNumber,
                "主体名称",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!parsePositiveInt(
                c[columns.segment],
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

        if (!parseBidPrice(
                c[columns.price],
                item.price,
                row.lineNumber,
                "申报电价",
                prefix,
                errors))
        {
            rowValid = false;
        }

        if (!parseBidQuantity(
                c[columns.quantity],
                item.quantity,
                row.lineNumber,
                "申报电量",
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
                    "第 %1 行主体 %2 的名称与前面不一致")
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
                    "第 %1 行主体 %2 的申报段 %3 重复")
                    .arg(row.lineNumber)
                    .arg(item.id)
                    .arg(item.segment));

            continue;
        }

        segmentKeys.insert(segmentKey);

        idToName[item.id] =
            item.name;

        pricesByEntity[item.id]
            .insert(
                item.segment,
                item.price);

        out.push_back(item);
    }

    if (!errors.isEmpty())
    {
        out.clear();
        return false;
    }

    // ---- 段连续性与段价单调（按主体） ----
    for (auto it =
         pricesByEntity.cbegin();
         it != pricesByEntity.cend();
         ++it)
    {
        const QString entityId =
            it.key();

        const QMap<int, double> &segments =
            it.value();

        const int segmentCount =
            segments.size();

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
                        "主体 %1 的申报段必须从 1 连续编号")
                        .arg(entityId));

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

            const bool violation =
                priceMonotonicUp
                    ? segments.value(segment) <
                          segments.value(segment - 1)
                    : segments.value(segment) >
                          segments.value(segment - 1);

            if (violation)
            {
                addError(
                    errors,
                    prefix,
                    QString(
                        "主体 %1 的申报电价必须随申报段单调%2")
                        .arg(entityId)
                        .arg(priceMonotonicUp
                                 ? QStringLiteral("不减")
                                 : QStringLiteral("不增")));

                break;
            }
        }
    }

    if (!errors.isEmpty())
    {
        out.clear();
        return false;
    }

    return true;
}

} // namespace


// 发电侧申报读取（V1.3：长表优先，窄表兼容展开）
bool DataReader::readGeneratorBids(
    const QString &filePath,
    QVector<GeneratorBid> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    const QString prefix =
        "发电";

    QStringList header;

    QVector<CsvRow> rows;

    if (!readCsvRowsRaw(
            filePath,
            prefix,
            header,
            rows,
            errors))
    {
        return false;
    }

    LongTableConfig longConfig;
    longConfig.prefix = prefix;
    longConfig.nameHeader =
        QStringLiteral("电厂名称");
    longConfig.idHeader =
        QStringLiteral("机组编号");
    longConfig.qtyHeaderFmt =
        QStringLiteral("第%1段出力(MW)");
    longConfig.qtyFieldFmt =
        QStringLiteral("第%1段出力");
    longConfig.priceMonotonicUp = true;

    QVector<ParsedBid> parsed;

    const int pairCount =
        parseLongHeader(
            header,
            longConfig,
            errors);

    if (pairCount > 0)
    {
        // ---- V1.3 长表 ----
        if (!parseLongBidRows(
                rows,
                pairCount,
                longConfig,
                parsed,
                errors))
        {
            return false;
        }
    }
    else if (pairCount == 0)
    {
        // ---- 窄表兼容（含/不含「机组类型」列，类型列忽略不校验） ----
        const QStringList legacyWithType =
            {
                QStringLiteral("机组ID"),
                QStringLiteral("机组名称"),
                QStringLiteral("机组类型"),
                QStringLiteral("申报段"),
                QStringLiteral("申报电价(元/MWh)"),
                QStringLiteral("申报电量(MWh)")
            };

        const QStringList legacyNoType =
            {
                QStringLiteral("机组ID"),
                QStringLiteral("机组名称"),
                QStringLiteral("申报段"),
                QStringLiteral("申报电价(元/MWh)"),
                QStringLiteral("申报电量(MWh)")
            };

        NarrowColumnMap columns;

        if (header == legacyWithType)
        {
            columns.id = 0;
            columns.name = 1;
            columns.segment = 3;
            columns.price = 4;
            columns.quantity = 5;
        }
        else if (header == legacyNoType)
        {
            columns.id = 0;
            columns.name = 1;
            columns.segment = 2;
            columns.price = 3;
            columns.quantity = 4;
        }
        else
        {
            addError(
                errors,
                prefix,
                "CSV 表头不匹配：应为 V1.3 长表"
                "（period,电厂名称,机组编号,段成对列）或窄表"
                "（机组ID,机组名称,申报段,申报电价,申报电量）");

            return false;
        }

        if (!parseNarrowBidRows(
                rows,
                columns,
                prefix,
                true,
                parsed,
                errors))
        {
            return false;
        }

        // ---- 窄表展开：96 个时段同量同价（等价性对拍锚点） ----
        expandParsedBidsTo96Periods(parsed);
    }
    else
    {
        return false;
    }

    materializeToGeneratorBids(parsed, data);
    return true;
}


// 用户侧申报读取（V1.3：长表优先，窄表兼容展开）
bool DataReader::readConsumerBids(
    const QString &filePath,
    QVector<ConsumerBid> &data,
    QStringList &errors)
{
    data.clear();
    errors.clear();

    const QString prefix =
        "购电";

    QStringList header;

    QVector<CsvRow> rows;

    if (!readCsvRowsRaw(
            filePath,
            prefix,
            header,
            rows,
            errors))
    {
        return false;
    }

    LongTableConfig longConfig;
    longConfig.prefix = prefix;
    longConfig.nameHeader =
        QStringLiteral("用户名称");
    longConfig.idHeader =
        QStringLiteral("负荷编号");
    longConfig.qtyHeaderFmt =
        QStringLiteral("第%1段申报量(MW)");
    longConfig.qtyFieldFmt =
        QStringLiteral("第%1段申报量");
    longConfig.priceMonotonicUp = false;

    QVector<ParsedBid> parsed;

    const int pairCount =
        parseLongHeader(
            header,
            longConfig,
            errors);

    if (pairCount > 0)
    {
        // ---- V1.3 长表 ----
        if (!parseLongBidRows(
                rows,
                pairCount,
                longConfig,
                parsed,
                errors))
        {
            return false;
        }
    }
    else if (pairCount == 0)
    {
        // ---- 窄表兼容 ----
        const QStringList legacyHeader =
            {
                QStringLiteral("用户ID"),
                QStringLiteral("用户名称"),
                QStringLiteral("申报段"),
                QStringLiteral("申报电价(元/MWh)"),
                QStringLiteral("申报电量(MWh)")
            };

        if (header != legacyHeader)
        {
            addError(
                errors,
                prefix,
                "CSV 表头不匹配：应为 V1.3 长表"
                "（period,用户名称,负荷编号,段成对列）或窄表"
                "（用户ID,用户名称,申报段,申报电价,申报电量）");

            return false;
        }

        NarrowColumnMap columns;
        columns.id = 0;
        columns.name = 1;
        columns.segment = 2;
        columns.price = 3;
        columns.quantity = 4;

        if (!parseNarrowBidRows(
                rows,
                columns,
                prefix,
                false,
                parsed,
                errors))
        {
            return false;
        }

        // ---- 窄表展开：96 个时段同量同价 ----
        expandParsedBidsTo96Periods(parsed);
    }
    else
    {
        return false;
    }

    materializeToConsumerBids(parsed, data);
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


// 跨文件一致性校验（V1.3 口径）
//   - 新能源不再出现在申报表（D4），原「新能源机组需在 generator_bids 定义、
//     类型一致」检查删除；
//   - 购电申报与负荷曲线的平衡偏差改为 P1 非阻断提示（契约 §3.2，
//     允许老师刻意做供需失衡场景），不再作为读取/校验错误。
bool DataReader::validateRelations(
    const MarketData &data,
    QStringList &errors)
{
    errors.clear();

    Q_UNUSED(data);

    return true;
}