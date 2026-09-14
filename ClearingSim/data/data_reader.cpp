#include "data_reader.h"

#include "csv_utils.h"
#include "data_validator.h"

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
using teammate::CsvRow;
void addError(QStringList &errors,const QString &prefix,const QString &message)
{if (prefix.isEmpty())
    {errors.append(message);
    }
    else
    {errors.append(prefix + " · " + message);
    }
}

// CSV 公共读取函数
bool readCsvRows(const QString &filePath,const QStringList &expectedHeader,const QString &prefix,QVector<CsvRow> &rows,QStringList &errors)
{rows.clear();
    QFile file(filePath);
    if (!file.exists())
    {addError(errors,prefix,"文件不存在：" + filePath);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {addError(errors,prefix,"文件无法打开：" + filePath);
        return false;
    }
    QTextStream in(&file);
    if (in.atEnd())
    {addError(errors,prefix,"CSV 文件为空：" + filePath);
        return false;
    }
    const QStringList header = teammate::splitCsvLine(in.readLine().trimmed());
    if (header != expectedHeader)
    {addError(errors,prefix,"CSV 表头不匹配，应为：" + expectedHeader.join(','));
        return false;
    }
    int lineNumber = 1;
    while (!in.atEnd())
    {++lineNumber;
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
        {continue;
        }
        const QStringList columns = teammate::splitCsvLine(line);
        if (columns.size() != expectedHeader.size())
        {addError(errors,prefix,QString("第 %1 行列数错误：应为 %2 列，实际为 %3 列").arg(lineNumber).arg(expectedHeader.size()).arg(columns.size()));
            continue;
        }
        CsvRow row;
        row.lineNumber = lineNumber;
        row.columns = columns;
        rows.push_back(row);
    }
    if (rows.isEmpty())
    {if (errors.isEmpty())
        {addError(errors,prefix,"CSV 文件中没有有效数据：" + filePath);
        }
        return false;
    }
    return errors.isEmpty();
}

// 空字段检查（数据模块：陈美伊 data_validator）
bool checkNotEmpty(const QString &value,int lineNumber,const QString &fieldName,const QString &prefix,QStringList &errors)
{QStringList local;
    const bool ok = teammate::checkNotEmpty(value,lineNumber,fieldName,local);
    for (const QString &error : local)
    {addError(errors,prefix,error);
    }
    return ok;
}

// 正整数检查（数据模块：陈美伊 data_validator）
bool parsePositiveInt(const QString &text,int &value,int lineNumber,const QString &fieldName,const QString &prefix,QStringList &errors)
{QStringList local;
    const bool ok = teammate::parsePositiveInt(text,value,lineNumber,fieldName,local);
    for (const QString &error : local)
    {addError(errors,prefix,error);
    }
    return ok;
}

// 小数位数检查
bool checkPrecision(const QString &text,int decimals,int lineNumber,const QString &fieldName,const QString &prefix,QStringList &errors)
{const QRegularExpression expression(QString("^\\d+\\.\\d{%1}$").arg(decimals));
    if (!expression.match(text).hasMatch())
    {addError(errors,prefix,QString("第 %1 行 %2 应保留 %3 位小数：%4").arg(lineNumber).arg(fieldName).arg(decimals).arg(text));
        return false;
    }
    return true;
}

// 浮点数检查（数据模块：陈美伊 data_validator）
bool parseDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,const QString &prefix,QStringList &errors)
{QStringList local;
    const bool ok = teammate::parseDouble(text,value,lineNumber,fieldName,local);
    for (const QString &error : local)
    {addError(errors,prefix,error);
    }
    return ok;
}

// 申报价格检查（3 位小数、0~1500 元/MWh；字段名随格式可变）
bool parseBidPrice(const QString &text,double &value,int lineNumber,const QString &fieldName,const QString &prefix,QStringList &errors)
{if (!parseDouble(text,value,lineNumber,fieldName,prefix,errors))
    {return false;
    }
    // 报价范围（0~1500 元/MWh，数据模块：陈美伊 data_validator）
    {QStringList local;
        if (!teammate::checkPriceRange(value,lineNumber,fieldName,local))
        {for (const QString &error : local)
            {addError(errors,prefix,error);
            }
            return false;
        }
    }
    if (!checkPrecision(text,3,lineNumber,fieldName,prefix,errors))
    {return false;
    }
    return true;
}

// 申报量检查（1 位小数、非负；V1.3 规则⑥：允许 0 = 该时段不申报/停机）
bool parseBidQuantity(const QString &text,double &value,int lineNumber,const QString &fieldName,const QString &prefix,QStringList &errors)
{if (!parseDouble(text,value,lineNumber,fieldName,prefix,errors))
    {return false;
    }
    if (value < 0.0)
    {addError(errors,prefix,QString("第 %1 行 %2 不能为负数").arg(lineNumber).arg(fieldName));
        return false;
    }
    if (!checkPrecision(text,1,lineNumber,fieldName,prefix,errors))
    {return false;
    }
    return true;
}

// 正功率检查
bool parsePositivePower(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors)
{if (!checkPrecision(text,1,lineNumber,fieldName,QString(),errors))
    {return false;
    }
    if (!parseDouble(text,value,lineNumber,fieldName,QString(),errors))
    {return false;
    }
    if (value <= 0.0)
    {errors.append(QString("第 %1 行 %2 必须大于 0").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 非负功率检查
bool parseNonNegativePower(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors)
{if (!checkPrecision(text,1,lineNumber,fieldName,QString(),errors))
    {return false;
    }
    if (!parseDouble(text,value,lineNumber,fieldName,QString(),errors))
    {return false;
    }
    if (value < 0.0)
    {errors.append(QString("第 %1 行 %2 不能为负数").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 时段对应时间
QString expectedTime(int period)
{const int totalMinutes = period * 15;
    if (totalMinutes == 24 * 60)
    {return "24:00";
    }
    const int hour = totalMinutes / 60;
    const int minute = totalMinutes % 60;
    return QString("%1:%2").arg(hour,2,10,QChar('0')).arg(minute,2,10,QChar('0'));
}

// ------------------------------------------------------------------
// V1.3 申报表读取：长表（period 行 × 段成对列）+ 窄表兼容展开
// ------------------------------------------------------------------

// 无表头校验的 CSV 读取（表头由调用方按格式识别后自行校验）
bool readCsvRowsRaw(const QString &filePath,const QString &prefix,QStringList &header,QVector<CsvRow> &rows,QStringList &errors)
{rows.clear();
    header.clear();
    QFile file(filePath);
    if (!file.exists())
    {addError(errors,prefix,"文件不存在：" + filePath);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {addError(errors,prefix,"文件无法打开：" + filePath);
        return false;
    }
    QTextStream in(&file);
    if (in.atEnd())
    {addError(errors,prefix,"CSV 文件为空：" + filePath);
        return false;
    }
    header = teammate::splitCsvLine(in.readLine().trimmed());
    int lineNumber = 1;
    while (!in.atEnd())
    {++lineNumber;
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
        {continue;
        }
        const QStringList columns = teammate::splitCsvLine(line);
        if (columns.size() != header.size())
        {addError(errors,prefix,QString("第 %1 行列数错误：应为 %2 列，实际为 %3 列").arg(lineNumber).arg(header.size()).arg(columns.size()));
            continue;
        }
        CsvRow row;
        row.lineNumber = lineNumber;
        row.columns = columns;
        rows.push_back(row);
    }
    if (rows.isEmpty())
    {if (errors.isEmpty())
        {addError(errors,prefix,"CSV 文件中没有有效数据：" + filePath);
        }
        return false;
    }
    return true;
}

// 长表解析的中间结构（发电/购电共用，字段同构）
struct ParsedBid
{QString id;
    QString name;
    int period = 0;
    int segment = 0;
    double price = 0.0;
    double quantity = 0.0;
};

// 长表格式描述（发电/购电各一份）
struct LongTableConfig
{QString prefix;// "发电" / "购电"
    QString nameHeader;// "电厂名称" / "用户名称"
    QString idHeader;// "机组编号" / "负荷编号"
    QString qtyHeaderFmt;// "第%1段出力(MW)" / "第%1段申报量(MW)"
    QString qtyFieldFmt;// "第%1段出力" / "第%1段申报量"
    bool priceMonotonicUp;// 发电：段价单调不减；购电：单调不增
};

// #96：长/窄表展开共享 helper（消除 readGeneratorBids / readConsumerBids 重复）
//   窄表解析无 period 字段，需要展开为 96 期同量同价（作对拍锚点）；
//   长表已带 period 字段，调用本函数是 no-op（period != 0 不展开）。
void expandParsedBidsTo96Periods(QVector<ParsedBid> &bids)
{QVector<ParsedBid> expanded;
    for (const ParsedBid &bid : bids)
    {if (bid.period > 0)
        {expanded.push_back(bid);// 长表自带 period，不复制
            continue;
        }
        for (int period = 1; period <= 96; ++period)
        {ParsedBid item = bid;
            item.period = period;
            expanded.push_back(item);
        }
    }
    bids = std::move(expanded);
}

// #96：ParsedBid → GeneratorBid 转换（共享）
void materializeToGeneratorBids(const QVector<ParsedBid> &parsed,QVector<GeneratorBid> &out)
{out.reserve(out.size() + parsed.size());
    for (const ParsedBid &bid : parsed)
    {GeneratorBid item;
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
void materializeToConsumerBids(const QVector<ParsedBid> &parsed,QVector<ConsumerBid> &out)
{out.reserve(out.size() + parsed.size());
    for (const ParsedBid &bid : parsed)
    {ConsumerBid item;
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
int parseLongHeader(const QStringList &header,const LongTableConfig &cfg,QStringList &errors)
{if (header.isEmpty() || header[0] != QStringLiteral("period"))
    {return 0;
    }
    if (header.size() < 5 || header[1] != cfg.nameHeader || header[2] != cfg.idHeader)
    {addError(errors,cfg.prefix,QString("长表表头应为：period,%1,%2,第N段…成对列").arg(cfg.nameHeader,cfg.idHeader));
        return -1;
    }
    const int pairCount = (header.size() - 3) / 2;
    if ((header.size() - 3) % 2 != 0 || pairCount < 1 || pairCount > 5)
    {addError(errors,cfg.prefix,"长表表头段列不成对：第N段出力/第N段报价必须成对出现，且最多 5 段");
        return -1;
    }
    for (int n = 1; n <= pairCount; ++n)
    {const QString expectedQty = cfg.qtyHeaderFmt.arg(n);
        const QString expectedPrice = QStringLiteral("第%1段报价(元/MWh)").arg(n);
        if (header[3 + 2 * (n - 1)] != expectedQty || header[4 + 2 * (n - 1)] != expectedPrice)
        {addError(errors,cfg.prefix,QString("长表表头第 %1 段列名应为「%2,%3」").arg(n).arg(expectedQty,expectedPrice));
            return -1;
        }
    }
    return pairCount;
}

// 长表行解析：一行 = 主体在某时段的全部段申报
bool parseLongBidRows(const QVector<CsvRow> &rows,int pairCount,const LongTableConfig &cfg,QVector<ParsedBid> &out,QStringList &errors)
{out.clear();
    QSet<QString> rowKeys;
    QHash<QString,QSet<int>> periodsByEntity;
    for (const CsvRow &row : rows)
    {const QStringList &c = row.columns;
        int period = 0;
        bool rowValid = true;
        if (!parsePositiveInt(c[0],period,row.lineNumber,"period",cfg.prefix,errors))
        {rowValid = false;
        }
        else if (period > 96)
        {addError(errors,cfg.prefix,QString("第 %1 行 period 必须在 1~96 范围内").arg(row.lineNumber));
            rowValid = false;
        }
        if (!checkNotEmpty(c[1],row.lineNumber,cfg.nameHeader,cfg.prefix,errors))
        {rowValid = false;
        }
        if (!checkNotEmpty(c[2],row.lineNumber,cfg.idHeader,cfg.prefix,errors))
        {rowValid = false;
        }
        if (!rowValid)
        {continue;
        }
        // ---- 段成对列：从段 1 起连续，空对之后不得再有数据 ----
        double prevPrice = 0.0;
        bool hasPrev = false;
        bool seenAbsentPair = false;
        for (int n = 1; n <= pairCount; ++n)
        {const QString &qtyCell = c[3 + 2 * (n - 1)];
            const QString &priceCell = c[4 + 2 * (n - 1)];
            const bool absent = qtyCell.isEmpty() && priceCell.isEmpty();
            if (absent)
            {seenAbsentPair = true;
                continue;
            }
            if (seenAbsentPair)
            {addError(errors,cfg.prefix,QString("第 %1 行 %2 %3 时段 %4：申报段必须从 1 连续编号（第 %5 段为空但其后有数据）").arg(row.lineNumber).arg(c[1],c[2]).arg(period).arg(n));
                rowValid = false;
                break;
            }
            if (qtyCell.isEmpty())
            {addError(errors,cfg.prefix,QString("第 %1 行第 %2 段有报价但缺少%3").arg(row.lineNumber).arg(n).arg(cfg.qtyFieldFmt.arg(n)));
                rowValid = false;
                continue;
            }
            double quantity = 0.0;
            double price = 0.0;
            if (!parseBidQuantity(qtyCell,quantity,row.lineNumber,cfg.qtyFieldFmt.arg(n),cfg.prefix,errors))
            {rowValid = false;
                continue;
            }
            if (!parseBidPrice(priceCell,price,row.lineNumber,QStringLiteral("第%1段报价").arg(n),cfg.prefix,errors))
            {rowValid = false;
                continue;
            }
            if (hasPrev && cfg.priceMonotonicUp && price < prevPrice)
            {addError(errors,cfg.prefix,QString("第 %1 行 %2 %3 时段 %4：申报电价必须随申报段单调不减").arg(row.lineNumber).arg(c[1],c[2]).arg(period));
                rowValid = false;
            }
            if (hasPrev && !cfg.priceMonotonicUp && price > prevPrice)
            {addError(errors,cfg.prefix,QString("第 %1 行 %2 %3 时段 %4：申报电价必须随申报段单调不增").arg(row.lineNumber).arg(c[1],c[2]).arg(period));
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
        {continue;
        }
        // ---- 主体×时段 去重 ----
        const QString entityKey = c[1] + "|" + c[2];
        const QString rowKey = entityKey + "|" + QString::number(period);
        if (rowKeys.contains(rowKey))
        {addError(errors,cfg.prefix,QString("第 %1 行主体 %2 %3 在时段 %4 重复申报").arg(row.lineNumber).arg(c[1],c[2]).arg(period));
            continue;
        }
        rowKeys.insert(rowKey);
        periodsByEntity[entityKey].insert(period);
    }
    if (!errors.isEmpty())
    {out.clear();
        return false;
    }
    // ---- 时段覆盖检查：每个主体必须覆盖 96 个时段 ----
    for (auto it = periodsByEntity.cbegin(); it != periodsByEntity.cend(); ++it)
    {const QString entityKey = it.key();
        const QSet<int> &periods = it.value();
        if (periods.size() != 96)
        {QString displayKey = entityKey;
            displayKey.replace(QLatin1Char('|'),QLatin1Char(' '));
            addError(errors,cfg.prefix,QString("主体 %1 必须包含 96 个时段的申报，实际为 %2 个").arg(displayKey).arg(periods.size()));
            continue;
        }
    }
    if (!errors.isEmpty())
    {out.clear();
        return false;
    }
    return true;
}

// 窄表（V1.1/V1.2 兼容）列映射
struct NarrowColumnMap
{int id = 0;
    int name = 1;
    int segment = 2;
    int price = 3;
    int quantity = 4;
};

// 窄表行解析：一行 = 主体×段（日内一份，无 period 维度）
//   校验沿用 V1.1 规则；「机组类型」列（若存在）忽略不校验（D7）
bool parseNarrowBidRows(const QVector<CsvRow> &rows,const NarrowColumnMap &columns,const QString &prefix,bool priceMonotonicUp,QVector<ParsedBid> &out,QStringList &errors)
{out.clear();
    QSet<QString> segmentKeys;
    QHash<QString,QString> idToName;
    QMap<QString,QMap<int,double>> pricesByEntity;
    for (const CsvRow &row : rows)
    {const QStringList &c = row.columns;
        ParsedBid item;
        item.id = c[columns.id];
        item.name = c[columns.name];
        bool rowValid = true;
        if (!checkNotEmpty(item.id,row.lineNumber,"主体 ID",prefix,errors))
        {rowValid = false;
        }
        if (!checkNotEmpty(item.name,row.lineNumber,"主体名称",prefix,errors))
        {rowValid = false;
        }
        if (!parsePositiveInt(c[columns.segment],item.segment,row.lineNumber,"申报段",prefix,errors))
        {rowValid = false;
        }
        else if (item.segment > 5)
        {addError(errors,prefix,QString("第 %1 行申报段不能超过 5").arg(row.lineNumber));
            rowValid = false;
        }
        if (!parseBidPrice(c[columns.price],item.price,row.lineNumber,"申报电价",prefix,errors))
        {rowValid = false;
        }
        if (!parseBidQuantity(c[columns.quantity],item.quantity,row.lineNumber,"申报电量",prefix,errors))
        {rowValid = false;
        }
        if (!rowValid)
        {continue;
        }
        if (idToName.contains(item.id) && idToName.value(item.id) != item.name)
        {addError(errors,prefix,QString("第 %1 行主体 %2 的名称与前面不一致").arg(row.lineNumber).arg(item.id));
            continue;
        }
        const QString segmentKey = item.id + "|" + QString::number(item.segment);
        if (segmentKeys.contains(segmentKey))
        {addError(errors,prefix,QString("第 %1 行主体 %2 的申报段 %3 重复").arg(row.lineNumber).arg(item.id).arg(item.segment));
            continue;
        }
        segmentKeys.insert(segmentKey);
        idToName[item.id] = item.name;
        pricesByEntity[item.id].insert(item.segment,item.price);
        out.push_back(item);
    }
    if (!errors.isEmpty())
    {out.clear();
        return false;
    }
    // ---- 段连续性与段价单调（按主体） ----
    for (auto it = pricesByEntity.cbegin(); it != pricesByEntity.cend(); ++it)
    {const QString entityId = it.key();
        const QMap<int,double> &segments = it.value();
        const int segmentCount = segments.size();
        for (int segment = 1; segment <= segmentCount; ++segment)
        {if (!segments.contains(segment))
            {addError(errors,prefix,QString("主体 %1 的申报段必须从 1 连续编号").arg(entityId));
                break;
            }
        }
        for (int segment = 2; segment <= segmentCount; ++segment)
        {if (!segments.contains(segment - 1) || !segments.contains(segment))
            {continue;
            }
            const bool violation = priceMonotonicUp ? segments.value(segment) < segments.value(segment - 1) : segments.value(segment) > segments.value(segment - 1);
            if (violation)
            {addError(errors,prefix,QString("主体 %1 的申报电价必须随申报段单调%2").arg(entityId).arg(priceMonotonicUp ? QStringLiteral("不减") : QStringLiteral("不增")));
                break;
            }
        }
    }
    if (!errors.isEmpty())
    {out.clear();
        return false;
    }
    return true;
}

} // namespace

// 发电侧申报读取（V1.3：长表优先，窄表兼容展开）
bool DataReader::readGeneratorBids(const QString &filePath,QVector<GeneratorBid> &data,QStringList &errors)
{data.clear();
    errors.clear();
    const QString prefix = "发电";
    QStringList header;
    QVector<CsvRow> rows;
    if (!readCsvRowsRaw(filePath,prefix,header,rows,errors))
    {return false;
    }
    LongTableConfig longConfig;
    longConfig.prefix = prefix;
    longConfig.nameHeader = QStringLiteral("电厂名称");
    longConfig.idHeader = QStringLiteral("机组编号");
    longConfig.qtyHeaderFmt = QStringLiteral("第%1段出力(MW)");
    longConfig.qtyFieldFmt = QStringLiteral("第%1段出力");
    longConfig.priceMonotonicUp = true;
    QVector<ParsedBid> parsed;
    const int pairCount = parseLongHeader(header,longConfig,errors);
    if (pairCount > 0)
    {
        // ---- V1.3 长表 ----
        if (!parseLongBidRows(rows,pairCount,longConfig,parsed,errors))
        {return false;
        }
    }
    else if (pairCount == 0)
    {
        // ---- 窄表兼容（含/不含「机组类型」列，类型列忽略不校验） ----
        const QStringList legacyWithType = { QStringLiteral("机组ID"),QStringLiteral("机组名称"),QStringLiteral("机组类型"),QStringLiteral("申报段"),QStringLiteral("申报电价(元/MWh)"),QStringLiteral("申报电量(MWh)") };
        const QStringList legacyNoType = { QStringLiteral("机组ID"),QStringLiteral("机组名称"),QStringLiteral("申报段"),QStringLiteral("申报电价(元/MWh)"),QStringLiteral("申报电量(MWh)") };
        NarrowColumnMap columns;
        if (header == legacyWithType)
        {columns.id = 0;
            columns.name = 1;
            columns.segment = 3;
            columns.price = 4;
            columns.quantity = 5;
        }
        else if (header == legacyNoType)
        {columns.id = 0;
            columns.name = 1;
            columns.segment = 2;
            columns.price = 3;
            columns.quantity = 4;
        }
        else
        {addError(errors,prefix,"CSV 表头不匹配：应为 V1.3 长表" "（period,电厂名称,机组编号,段成对列）或窄表" "（机组ID,机组名称,申报段,申报电价,申报电量）");
            return false;
        }
        if (!parseNarrowBidRows(rows,columns,prefix,true,parsed,errors))
        {return false;
        }
        // ---- 窄表展开：96 个时段同量同价（等价性对拍锚点） ----
        expandParsedBidsTo96Periods(parsed);
    }
    else
    {return false;
    }
    materializeToGeneratorBids(parsed,data);
    return true;
}

// 用户侧申报读取（V1.3：长表优先，窄表兼容展开）
bool DataReader::readConsumerBids(const QString &filePath,QVector<ConsumerBid> &data,QStringList &errors)
{data.clear();
    errors.clear();
    const QString prefix = "购电";
    QStringList header;
    QVector<CsvRow> rows;
    if (!readCsvRowsRaw(filePath,prefix,header,rows,errors))
    {return false;
    }
    LongTableConfig longConfig;
    longConfig.prefix = prefix;
    longConfig.nameHeader = QStringLiteral("用户名称");
    longConfig.idHeader = QStringLiteral("负荷编号");
    longConfig.qtyHeaderFmt = QStringLiteral("第%1段申报量(MW)");
    longConfig.qtyFieldFmt = QStringLiteral("第%1段申报量");
    longConfig.priceMonotonicUp = false;
    QVector<ParsedBid> parsed;
    const int pairCount = parseLongHeader(header,longConfig,errors);
    if (pairCount > 0)
    {
        // ---- V1.3 长表 ----
        if (!parseLongBidRows(rows,pairCount,longConfig,parsed,errors))
        {return false;
        }
    }
    else if (pairCount == 0)
    {
        // ---- 窄表兼容 ----
        const QStringList legacyHeader = { QStringLiteral("用户ID"),QStringLiteral("用户名称"),QStringLiteral("申报段"),QStringLiteral("申报电价(元/MWh)"),QStringLiteral("申报电量(MWh)") };
        if (header != legacyHeader)
        {addError(errors,prefix,"CSV 表头不匹配：应为 V1.3 长表" "（period,用户名称,负荷编号,段成对列）或窄表" "（用户ID,用户名称,申报段,申报电价,申报电量）");
            return false;
        }
        NarrowColumnMap columns;
        columns.id = 0;
        columns.name = 1;
        columns.segment = 2;
        columns.price = 3;
        columns.quantity = 4;
        if (!parseNarrowBidRows(rows,columns,prefix,false,parsed,errors))
        {return false;
        }
        // ---- 窄表展开：96 个时段同量同价 ----
        expandParsedBidsTo96Periods(parsed);
    }
    else
    {return false;
    }
    materializeToConsumerBids(parsed,data);
    return true;
}

// 负荷曲线读取
bool DataReader::readLoadCurve(const QString &filePath,QVector<LoadPoint> &data,QStringList &errors)
{data.clear();
    errors.clear();
    const QStringList expectedHeader = { "时段","时刻","负荷(MW)" };
    QVector<CsvRow> rows;
    if (!readCsvRows(filePath,expectedHeader,QString(),rows,errors))
    {return false;
    }
    QVector<LoadPoint> tempData;
    QSet<int> periods;
    for (const CsvRow &row : rows)
    {const QStringList &c = row.columns;
        LoadPoint item;
        item.time = c[1];
        bool rowValid = true;
        if (!parsePositiveInt(c[0],item.period,row.lineNumber,"时段",QString(),errors))
        {rowValid = false;
        }
        else if (item.period > 96)
        {errors.append(QString("第 %1 行时段必须在 1~96 范围内").arg(row.lineNumber));
            rowValid = false;
        }
        if (!checkNotEmpty(item.time,row.lineNumber,"时刻",QString(),errors))
        {rowValid = false;
        }
        if (!parsePositivePower(c[2],item.load,row.lineNumber,"负荷",errors))
        {rowValid = false;
        }
        if (!rowValid)
        {continue;
        }
        if (item.time != expectedTime(item.period))
        {errors.append(QString("第 %1 行时刻应为 %2，实际为 %3").arg(row.lineNumber).arg(expectedTime(item.period)).arg(item.time));
            continue;
        }
        if (periods.contains(item.period))
        {errors.append(QString("第 %1 行时段 %2 重复").arg(row.lineNumber).arg(item.period));
            continue;
        }
        periods.insert(item.period);
        tempData.push_back(item);
    }
    if (!errors.isEmpty())
    {return false;
    }
    if (tempData.size() != 96)
    {errors.append(QString("负荷曲线必须包含 96 个时段，实际为 %1 个").arg(tempData.size()));
    }
    for (int period = 1; period <= 96; ++period)
    {if (!periods.contains(period))
        {errors.append(QString("负荷曲线缺少时段 %1").arg(period));
        }
    }
    if (!errors.isEmpty())
    {data.clear();
        return false;
    }
    data = tempData;
    return true;
}

// 统一读取接口
bool DataReader::readAll(const DataFileSet &files,MarketData &data,QStringList &errors)
{data.clear();
    errors.clear();
    MarketData tempData;
    QStringList fileErrors;
    bool allOk = true;
    if (!readGeneratorBids(files.generatorBidsFile,tempData.generatorBids,fileErrors))
    {teammate::appendErrors("generator_bids.csv",fileErrors,errors);
        allOk = false;
    }
    fileErrors.clear();
    if (!readConsumerBids(files.consumerBidsFile,tempData.consumerBids,fileErrors))
    {teammate::appendErrors("consumer_bids.csv",fileErrors,errors);
        allOk = false;
    }
    fileErrors.clear();
    if (!readLoadCurve(files.loadCurveFile,tempData.loadCurve,fileErrors))
    {teammate::appendErrors("load_curve.csv",fileErrors,errors);
        allOk = false;
    }
    if (!allOk)
    {data.clear();
        return false;
    }
    data = tempData;
    return true;
}

// 跨文件一致性校验 + 供需平衡提示（契约 §3.2-3，负荷曲线职责②「平衡校验锚点」）
//   - 新能源不再出现在申报表（D4），原「新能源机组需在 generator_bids 定义、
//     类型一致」检查删除；
//   - 平衡校验为 P1 非阻断提示（契约 §3.2，允许老师刻意做供需失衡场景）：
//     ① 逐时段口径：Σ用户申报量(t) vs 负荷(t)，汇总偏差时段数与最大缺/过剩；
//     ② 总量口径：ΣΣ申报量 vs Σ负荷曲线。
//   提示写入 hints（琥珀色展示），errors 仅承载阻断性错误（当前无）。
bool DataReader::validateRelations(const MarketData &data,QStringList &errors,QStringList &hints)
{errors.clear();
    hints.clear();
    // 无负荷曲线（锚点缺失）或无购电申报（无可比对象）→ 静默通过
    if (data.loadCurve.isEmpty() || data.consumerBids.isEmpty()) return true;
    // 逐时段聚合购电申报量：period → Σquantity
    QHash<int,double> bidByPeriod;
    for (const auto &c : data.consumerBids) bidByPeriod[c.period] += c.quantity;
    int periodsChecked = 0;
    int deviatingCount = 0;// 偏差超容差的时段数
    double maxShortMW = 0.0;// 最大缺口（负荷 > 申报）
    int maxShortT = 0;
    double maxSurplusMW = 0.0;// 最大过剩（申报 > 负荷）
    int maxSurplusT = 0;
    double totalLoad = 0.0;
    double totalBid = 0.0;
    double worstDevAbs = 0.0;// 全时段最大绝对偏差（用于"基本平衡"提示）
    int worstDevT = 0;
    for (const auto &lp : data.loadCurve)
    {const double bid = bidByPeriod.value(lp.period,0.0);
        ++periodsChecked;
        totalLoad += lp.load;
        totalBid += bid;
        const double gap = lp.load - bid;// >0 缺口，<0 过剩
        const double devAbs = std::abs(gap);
        if (devAbs > 1.0)// 容差 1 MW：忽略申报取整尾差
            ++deviatingCount;
        if (devAbs > worstDevAbs)
        {worstDevAbs = devAbs;
            worstDevT = lp.period;
        }
        if (gap > maxShortMW) { maxShortMW = gap; maxShortT = lp.period; }
        if (-gap > maxSurplusMW) { maxSurplusMW = -gap; maxSurplusT = lp.period; }
    }
    // 时段 → 时刻标注（负荷曲线带 time 列时附上，方便对图读数）
    auto timeTag = [&data](int period) -> QString
    {if (period <= 0) return QString();
        for (const auto &lp : data.loadCurve) if (lp.period == period && !lp.time.isEmpty()) return QStringLiteral("（%1）").arg(lp.time);
        return QString();
    };
    // ① 逐时段提示
    if (deviatingCount == 0)
    {hints << QStringLiteral("逐时段平衡：全部 %1 个时段申报量与负荷预测偏差均 ≤1 MW").arg(periodsChecked);
    }
    else
    {QString line = QStringLiteral("逐时段平衡：%1/%2 个时段申报与负荷偏差超 1 MW").arg(deviatingCount).arg(periodsChecked);
        if (maxShortMW > 0.0) line += QStringLiteral("；最大缺口 时段%1%2 申报较负荷少 %3 MW").arg(maxShortT).arg(timeTag(maxShortT)).arg(maxShortMW,0,'f',1);
        if (maxSurplusMW > 0.0) line += QStringLiteral("；最大过剩 时段%1%2 申报较负荷多 %3 MW").arg(maxSurplusT).arg(timeTag(maxSurplusT)).arg(maxSurplusMW,0,'f',1);
        hints << line;
    }
    // ② 总量提示（带正负号与百分比）
    if (totalLoad > 0.0)
    {const double dev = totalBid - totalLoad;
        const double devPct = dev / totalLoad * 100.0;
        hints << QStringLiteral("总量平衡：申报合计 %1 MW · 负荷预测合计 %2 MW · 偏差 %3%4 MW（%5%6%）").arg(totalBid,0,'f',1).arg(totalLoad,0,'f',1).arg(dev >= 0 ? QStringLiteral("+") : QStringLiteral("-")).arg(std::abs(dev),0,'f',1).arg(dev >= 0 ? QStringLiteral("+") : QStringLiteral("-")).arg(std::abs(devPct),0,'f',1);
    }
    return true;
}

// ------------------------------------------------------------------
// 二次成本机组参数表（generator_quadratic.csv，可选文件）
//   表头 name,id,a,b,c,pMax；行 = 机组（二次参数是机组物理属性，
//   全天一条曲线，非逐时段申报）。选题 2026v2 (10) 问配套。
//   文件不存在 → 返回 false 且不记错误（调用方据此判定二次模式未启用）。
// ------------------------------------------------------------------
bool DataReader::readQuadraticGenerators(const QString &filePath,QVector<QuadraticGenerator> &data,QStringList &errors)
{data.clear();
    QFile file(filePath);
    if (!file.exists())
    {
        // 可选文件：不存在不是错误
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {addError(errors,QStringLiteral("二次机组参数"),"文件无法打开：" + filePath);
        return false;
    }
    QTextStream in(&file);
    if (in.atEnd())
    {addError(errors,QStringLiteral("二次机组参数"),"CSV 文件为空：" + filePath);
        return false;
    }
    const QStringList header = teammate::splitCsvLine(in.readLine().trimmed());
    const QStringList expectedHeader = { QStringLiteral("name"),QStringLiteral("id"),QStringLiteral("a"),QStringLiteral("b"),QStringLiteral("c"),QStringLiteral("pMax") };
    if (header != expectedHeader)
    {addError(errors,QStringLiteral("二次机组参数"),"表头不符，期望 name,id,a,b,c,pMax：" + filePath);
        return false;
    }
    while (!in.atEnd())
    {const QString line = in.readLine().trimmed();
        if (line.isEmpty())
        {continue;
        }
        const QStringList columns = teammate::splitCsvLine(line);
        if (columns.size() < 6)
        {addError(errors,QStringLiteral("二次机组参数"),QString("行 %1 列数不足（需 6 列）").arg(line));
            continue;
        }
        QuadraticGenerator gen;
        gen.name = columns[0];
        gen.id = columns[1];
        bool ok = true;
        gen.a = columns[2].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("二次机组参数"),"a 读取失败：" + line); continue; }
        gen.b = columns[3].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("二次机组参数"),"b 读取失败：" + line); continue; }
        gen.c = columns[4].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("二次机组参数"),"c 读取失败：" + line); continue; }
        gen.pMax = columns[5].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("二次机组参数"),"pMax 读取失败：" + line); continue; }
        if (gen.a < 0.0 || gen.pMax < 0.0)
        {addError(errors,QStringLiteral("二次机组参数"),"a / pMax 不可为负：" + line);
            continue;
        }
        data.append(gen);
    }
    return !data.isEmpty();
}

// ------------------------------------------------------------------
// SCUC 机组技术经济参数表（可选文件 generator_meta.csv，求解器方案 S4）
//   表头 name,id,pMin,pMax,rampUp,rampDown,minUpTime,minDownTime,
//        startupCost,noLoadCost,marginalCost；行 = 机组（全天一套参数）。
//   校验：pMax>0、0 ≤ pMin ≤ pMax、minUp/minDown ≥ 1、费用非负；
//   爬坡 <0 视为不限制（存 0，与 uc_solver 口径一致）。
// ------------------------------------------------------------------
bool DataReader::readGeneratorMeta(const QString &filePath,QVector<GeneratorMeta> &data,QStringList &errors)
{data.clear();
    QFile file(filePath);
    if (!file.exists())
    {
        // 可选文件：不存在不是错误
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {addError(errors,QStringLiteral("机组技术参数"),"文件无法打开：" + filePath);
        return false;
    }
    QTextStream in(&file);
    if (in.atEnd())
    {addError(errors,QStringLiteral("机组技术参数"),"CSV 文件为空：" + filePath);
        return false;
    }
    const QStringList header = teammate::splitCsvLine(in.readLine().trimmed());
    const QStringList expectedHeader = { QStringLiteral("name"),QStringLiteral("id"),QStringLiteral("pMin"),QStringLiteral("pMax"),QStringLiteral("rampUp"),QStringLiteral("rampDown"),QStringLiteral("minUpTime"),QStringLiteral("minDownTime"),QStringLiteral("startupCost"),QStringLiteral("noLoadCost"),QStringLiteral("marginalCost") };
    if (header != expectedHeader)
    {addError(errors,QStringLiteral("机组技术参数"),"表头不符，期望 name,id,pMin,pMax,rampUp,rampDown,minUpTime," "minDownTime,startupCost,noLoadCost,marginalCost：" + filePath);
        return false;
    }
    while (!in.atEnd())
    {const QString line = in.readLine().trimmed();
        if (line.isEmpty())
        {continue;
        }
        const QStringList columns = teammate::splitCsvLine(line);
        if (columns.size() < 11)
        {addError(errors,QStringLiteral("机组技术参数"),QString("行 %1 列数不足（需 11 列）").arg(line));
            continue;
        }
        GeneratorMeta m;
        m.name = columns[0];
        m.id = columns[1];
        bool ok = true;
        m.pMin = columns[2].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"pMin 读取失败：" + line); continue; }
        m.pMax = columns[3].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"pMax 读取失败：" + line); continue; }
        m.rampUp = columns[4].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"rampUp 读取失败：" + line); continue; }
        m.rampDown = columns[5].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"rampDown 读取失败：" + line); continue; }
        m.minUpTime = columns[6].toInt(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"minUpTime 读取失败：" + line); continue; }
        m.minDownTime = columns[7].toInt(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"minDownTime 读取失败：" + line); continue; }
        m.startupCost = columns[8].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"startupCost 读取失败：" + line); continue; }
        m.noLoadCost = columns[9].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"noLoadCost 读取失败：" + line); continue; }
        m.marginalCost = columns[10].toDouble(&ok);
        if (!ok) { addError(errors,QStringLiteral("机组技术参数"),"marginalCost 读取失败：" + line); continue; }
        if (m.rampUp < 0.0) m.rampUp = 0.0;// 负数 = 不限制（uc_solver 口径）
        if (m.rampDown < 0.0) m.rampDown = 0.0;
        if (m.pMax <= 0.0 || m.pMin < 0.0 || m.pMin > m.pMax || m.minUpTime < 1 || m.minDownTime < 1 || m.startupCost < 0.0 || m.noLoadCost < 0.0)
        {addError(errors,QStringLiteral("机组技术参数"),"参数越界（需 pMax>0、0≤pMin≤pMax、minUp/minDown≥1、费用非负）：" + line);
            continue;
        }
        data.append(m);
    }
    return !data.isEmpty();
}