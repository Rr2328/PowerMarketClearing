#include "data_reader.h"
#include "csv_utils.h"
#include "data_validator.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace
{

// 检查发电侧表头
bool checkGeneratorHeader(const QStringList &header,QStringList &errors)
{
    constexpr int expectedColumns =24;
    if (header.size() !=expectedColumns)
    {
        errors.append(QString("CSV 表头列数错误：应为 %1 列，实际为 %2 列").arg(expectedColumns).arg(header.size()));
        return false;
    }
    if (!matchesAny(header[0],{"period","时段"}))
    {
        errors.append("发电侧第 1 列必须为 period 或 时段");
        return false;
    }
    if (!matchesAny(header[1],{"电厂名称","机组名称"}))
    {
        errors.append("发电侧第 2 列必须为电厂名称");
        return false;
    }
    if (!matchesAny(header[2],{"机组编号","机组ID","机组Id","id"}))
    {
        errors.append("发电侧第 3 列必须为机组编号");
        return false;
    }
    if (!matchesAny(header[3],{"机组类型","type"}))
    {
        errors.append("发电侧第 4 列必须为机组类型");
        return false;
    }
    for (int segment =1;segment <=10;++segment)
    {
        const int pIndex =4 +(segment -1) *2;
        const int cIndex =pIndex +1;
        const QString pToken =QString("P%1").arg(segment);
        const QString cToken =QString("C%1").arg(segment);
        if (!header[pIndex].contains(pToken,Qt::CaseInsensitive))
        {
            errors.append(QString("发电侧第 %1 列应为第 %2 段 P 列").arg(pIndex +1).arg(segment));
            return false;
        }
        if (!header[cIndex].contains(cToken,Qt::CaseInsensitive))
        {
            errors.append(QString("发电侧第 %1 列应为第 %2 段 C 列").arg(cIndex +1).arg(segment));
            return false;
        }
    }
    return true;
}

// 检查用户侧表头
bool checkConsumerHeader(const QStringList &header,QStringList &errors)
{
    constexpr int expectedColumns =23;
    if (header.size() !=expectedColumns)
    {
        errors.append(QString("CSV 表头列数错误：应为 %1 列，实际为 %2 列").arg(expectedColumns).arg(header.size()));
        return false;
    }
    if (!matchesAny(header[0],{"period","时段"}))
    {
        errors.append("用户侧第 1 列必须为 period 或 时段");
        return false;
    }
    if (!matchesAny(header[1],{"用户名称","名称"}))
    {
        errors.append("用户侧第 2 列必须为用户名称");
        return false;
    }
    if (!matchesAny(header[2],{"用户编号","用户ID","用户Id","id"}))
    {
        errors.append("用户侧第 3 列必须为用户编号");
        return false;
    }
    for (int segment =1;segment <=10;++segment)
    {
        const int pIndex =3 +(segment -1) *2;
        const int cIndex =pIndex +1;
        const QString pToken =QString("P%1").arg(segment);
        const QString cToken =QString("C%1").arg(segment);
        if (!header[pIndex].contains(pToken,Qt::CaseInsensitive))
        {
            errors.append(QString("用户侧第 %1 列应为第 %2 段 P 列").arg(pIndex +1).arg(segment));
            return false;
        }
        if (!header[cIndex].contains(cToken,Qt::CaseInsensitive))
        {
            errors.append(QString("用户侧第 %1 列应为第 %2 段 C 列").arg(cIndex +1).arg(segment));
            return false;
        }
    }
    return true;
}

// 检查负荷曲线表头
bool checkLoadHeader(const QStringList &header,QStringList &errors)
{
    if (header.size() !=3)
    {
        errors.append(QString("CSV 表头列数错误：应为 3 列，实际为 %1 列").arg(header.size()));
        return false;
    }
    if (!matchesAny(header[0],{"period","时段"}))
    {
        errors.append("负荷曲线第 1 列必须为 period 或 时段");
        return false;
    }
    if (!matchesAny(header[1],{"时刻","时间"}))
    {
        errors.append("负荷曲线第 2 列必须为时刻");
        return false;
    }
    if (!matchesAny(header[2],{"系统负荷(MW)","负荷(MW)"}))
    {
        errors.append("负荷曲线第 3 列必须为负荷(MW)");
        return false;
    }
    return true;
}

// 检查新能源表头
bool checkRenewableHeader(const QStringList &header,QStringList &errors)
{
    if (header.size() !=6)
    {
        errors.append(QString("CSV 表头列数错误：应为 6 列，实际为 %1 列").arg(header.size()));
        return false;
    }
    if (!matchesAny(header[0],{"period","时段"}))
    {
        errors.append("新能源第 1 列必须为 period 或 时段");
        return false;
    }
    if (!matchesAny(header[1],{"时刻","时间"}))
    {
        errors.append("新能源第 2 列必须为时刻");
        return false;
    }
    if (!matchesAny(header[2],{"机组编号","机组ID","机组Id"}))
    {
        errors.append("新能源第 3 列必须为机组编号");
        return false;
    }
    if (!matchesAny(header[3],{"机组名称","名称"}))
    {
        errors.append("新能源第 4 列必须为机组名称");
        return false;
    }
    if (!matchesAny(header[4],{"机组类型","类型"}))
    {
        errors.append("新能源第 5 列必须为机组类型");
        return false;
    }
    if (!matchesAny(header[5],{"可用出力(MW)","出力(MW)","基准出力(MW)"}))
    {
        errors.append("新能源第 6 列必须为出力(MW)");
        return false;
    }
    return true;
}

}

// 读取发电侧申报数据
bool DataReader::readGeneratorBids(const QString &filePath,QVector<GeneratorBid> &data,QStringList &errors)
{
    data.clear();
    errors.clear();
    QStringList header;
    QVector<CsvRow> rows;
    if (!readCsvRows(filePath,header,rows,errors))
    {
        return false;
    }
    if (!checkGeneratorHeader(header,errors))
    {
        return false;
    }
    constexpr int baseColumns =4;
    constexpr int maxSegments =10;
    QVector<GeneratorBid> tempData;
    // 记录时段和机组组合，检查重复数据
    QSet<QString> rowKeys;
    // 记录所有出现过的时段
    QSet<int> allPeriods;
    // 记录每个机组对应的时段
    QHash<QString,QSet<int>> unitPeriods;
    // 记录机组类型
    QHash<QString,QString> unitTypes;
    for (const CsvRow &row : rows)
    {
        const QStringList &c =row.columns;
        int period =0;
        const QString plantName =c[1];
        const QString unitId =c[2];
        const QString unitType =c[3];
        bool rowValid =true;
        if (!parsePositiveInt(c[0],period,row.lineNumber,"period",errors))
        {
            rowValid =false;
        }
        if (period >96)
        {
            errors.append(QString("第 %1 行时段 %2 超出 1～96 范围").arg(row.lineNumber).arg(period));
            rowValid =false;
        }
        if (!checkNotEmpty(plantName,row.lineNumber,"电厂名称",errors))
        {
            rowValid =false;
        }
        if (!checkNotEmpty(unitId,row.lineNumber,"机组编号",errors))
        {
            rowValid =false;
        }
        if (!checkNotEmpty(unitType,row.lineNumber,"机组类型",errors))
        {
            rowValid =false;
        }
        if (!unitId.isEmpty())
        {
            if (unitTypes.contains(unitId) &&unitTypes.value(unitId) !=unitType)
            {
                errors.append(QString("第 %1 行机组 %2 的类型与其他时段不一致").arg(row.lineNumber).arg(unitId));
                rowValid =false;
            }
            else
            {
                unitTypes.insert(unitId,unitType);
            }
        }
        if (period >0 &&!unitId.isEmpty())
        {
            const QString key =QString::number(period) +"|" +unitId;
            if (rowKeys.contains(key))
            {
                errors.append(QString("第 %1 行时段 %2 的机组 %3 重复").arg(row.lineNumber).arg(period).arg(unitId));
                rowValid =false;
            }
            else
            {
                rowKeys.insert(key);
            }
        }
        QVector<GeneratorBid> rowBids;
        bool hasSegment =false;
        bool emptySegmentFound =false;
        bool hasPreviousPrice =false;
        double previousPrice =0.0;
        for (int segment =1;segment <=maxSegments;++segment)
        {
            const int quantityIndex =baseColumns +(segment -1) *2;
            const int priceIndex =quantityIndex +1;
            const QString quantityText =c[quantityIndex].trimmed();
            const QString priceText =c[priceIndex].trimmed();
            if (quantityText.isEmpty() &&priceText.isEmpty())
            {
                emptySegmentFound =true;
                continue;
            }
            hasSegment =true;
            if (quantityText.isEmpty() ||priceText.isEmpty())
            {
                errors.append(QString("第 %1 行第 %2 段报价不完整").arg(row.lineNumber).arg(segment));
                rowValid =false;
                emptySegmentFound =true;
                continue;
            }
            if (emptySegmentFound)
            {
                errors.append(QString("第 %1 行机组 %2 的报价段不连续").arg(row.lineNumber).arg(unitId));
                rowValid =false;
            }
            double quantity =0.0;
            double price =0.0;
            const QString quantityName =QString("P%1").arg(segment);
            const QString priceName =QString("C%1").arg(segment);
            const bool quantityOk =parseNonNegativeDouble(quantityText,quantity,row.lineNumber,quantityName,errors);
            const bool priceOk =parseDouble(priceText,price,row.lineNumber,priceName,errors);
            if (!quantityOk ||!priceOk)
            {
                rowValid =false;
                continue;
            }
            const bool renewable =unitType =="风电" ||unitType =="光伏";
            if (!renewable &&quantity <=0.0)
            {
                errors.append(QString("第 %1 行常规机组 P%2 必须大于 0").arg(row.lineNumber).arg(segment));
                rowValid =false;
            }
            if (!checkPriceRange(price,row.lineNumber,priceName,errors))
            {
                rowValid =false;
            }
            if (!checkPricePrecision(priceText,row.lineNumber,priceName,errors))
            {
                rowValid =false;
            }
            if (hasPreviousPrice &&price <previousPrice)
            {
                errors.append(QString("第 %1 行机组 %2 的报价必须单调非递减").arg(row.lineNumber).arg(unitId));
                rowValid =false;
            }
            GeneratorBid item;
            item.id =unitId;
            item.name =plantName;
            item.type =unitType;
            item.segment =segment;
            item.price =price;
            item.quantity =quantity;
            item.period =period;
            rowBids.push_back(item);
            previousPrice =price;
            hasPreviousPrice =true;
        }
        if (!hasSegment)
        {
            errors.append(QString("第 %1 行机组 %2 没有填写报价").arg(row.lineNumber).arg(unitId));
            rowValid =false;
        }
        if (rowValid)
        {
            allPeriods.insert(period);
            unitPeriods[unitId].insert(period);
            for (const GeneratorBid &bid : rowBids)
            {
                tempData.push_back(bid);
            }
        }
    }
    const int expectedPeriodCount =detectExpectedPeriodCount(allPeriods,"发电申报",errors);
    if (expectedPeriodCount ==24 ||expectedPeriodCount ==96)
    {
        validateParticipantPeriods(unitPeriods,expectedPeriodCount,"机组",errors);
    }
    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }
    std::sort(tempData.begin(),tempData.end(),[](const GeneratorBid &a,const GeneratorBid &b)
              {
                  if (a.period !=b.period)
                  {
                      return a.period <b.period;
                  }
                  if (a.id !=b.id)
                  {
                      return a.id <b.id;
                  }
                  return a.segment <b.segment;
              });
    data =tempData;
    return true;
}

// 读取用户侧申报数据
bool DataReader::readConsumerBids(const QString &filePath,QVector<ConsumerBid> &data,QStringList &errors)
{
    data.clear();
    errors.clear();
    QStringList header;
    QVector<CsvRow> rows;
    if (!readCsvRows(filePath,header,rows,errors))
    {
        return false;
    }
    if (!checkConsumerHeader(header,errors))
    {
        return false;
    }
    constexpr int baseColumns =3;
    constexpr int maxSegments =10;
    QVector<ConsumerBid> tempData;
    QSet<QString> rowKeys;
    QSet<int> allPeriods;
    // 记录每个用户对应的时段
    QHash<QString,QSet<int>> consumerPeriods;
    for (const CsvRow &row : rows)
    {
        const QStringList &c =row.columns;
        int period =0;
        const QString consumerName =c[1];
        const QString consumerId =c[2];
        bool rowValid =true;
        if (!parsePositiveInt(c[0],period,row.lineNumber,"period",errors))
        {
            rowValid =false;
        }
        if (period >96)
        {
            errors.append(QString("第 %1 行时段 %2 超出 1～96 范围").arg(row.lineNumber).arg(period));
            rowValid =false;
        }
        if (!checkNotEmpty(consumerName,row.lineNumber,"用户名称",errors))
        {
            rowValid =false;
        }
        if (!checkNotEmpty(consumerId,row.lineNumber,"用户编号",errors))
        {
            rowValid =false;
        }
        if (period >0 &&!consumerId.isEmpty())
        {
            const QString key =QString::number(period) +"|" +consumerId;
            if (rowKeys.contains(key))
            {
                errors.append(QString("第 %1 行时段 %2 的用户 %3 重复").arg(row.lineNumber).arg(period).arg(consumerId));
                rowValid =false;
            }
            else
            {
                rowKeys.insert(key);
            }
        }
        QVector<ConsumerBid> rowBids;
        bool hasSegment =false;
        bool emptySegmentFound =false;
        bool hasPreviousPrice =false;
        double previousPrice =0.0;
        for (int segment =1;segment <=maxSegments;++segment)
        {
            const int quantityIndex =baseColumns +(segment -1) *2;
            const int priceIndex =quantityIndex +1;
            const QString quantityText =c[quantityIndex].trimmed();
            const QString priceText =c[priceIndex].trimmed();
            if (quantityText.isEmpty() &&priceText.isEmpty())
            {
                emptySegmentFound =true;
                continue;
            }
            hasSegment =true;
            if (quantityText.isEmpty() ||priceText.isEmpty())
            {
                errors.append(QString("第 %1 行第 %2 段报价不完整").arg(row.lineNumber).arg(segment));
                rowValid =false;
                emptySegmentFound =true;
                continue;
            }
            if (emptySegmentFound)
            {
                errors.append(QString("第 %1 行用户 %2 的报价段不连续").arg(row.lineNumber).arg(consumerId));
                rowValid =false;
            }
            double quantity =0.0;
            double price =0.0;
            const QString quantityName =QString("P%1").arg(segment);
            const QString priceName =QString("C%1").arg(segment);
            const bool quantityOk =parseNonNegativeDouble(quantityText,quantity,row.lineNumber,quantityName,errors);
            const bool priceOk =parseDouble(priceText,price,row.lineNumber,priceName,errors);
            if (!quantityOk ||!priceOk)
            {
                rowValid =false;
                continue;
            }
            if (quantity <=0.0)
            {
                errors.append(QString("第 %1 行 P%2 必须大于 0").arg(row.lineNumber).arg(segment));
                rowValid =false;
            }
            if (!checkPriceRange(price,row.lineNumber,priceName,errors))
            {
                rowValid =false;
            }
            if (!checkPricePrecision(priceText,row.lineNumber,priceName,errors))
            {
                rowValid =false;
            }
            if (hasPreviousPrice &&price >previousPrice)
            {
                errors.append(QString("第 %1 行用户 %2 的报价必须单调非递增").arg(row.lineNumber).arg(consumerId));
                rowValid =false;
            }
            ConsumerBid item;
            item.id =consumerId;
            item.name =consumerName;
            item.segment =segment;
            item.price =price;
            item.quantity =quantity;
            item.period =period;
            rowBids.push_back(item);
            previousPrice =price;
            hasPreviousPrice =true;
        }
        if (!hasSegment)
        {
            errors.append(QString("第 %1 行用户 %2 没有填写报价").arg(row.lineNumber).arg(consumerId));
            rowValid =false;
        }
        if (rowValid)
        {
            allPeriods.insert(period);
            consumerPeriods[consumerId].insert(period);
            for (const ConsumerBid &bid : rowBids)
            {
                tempData.push_back(bid);
            }
        }
    }
    const int expectedPeriodCount =detectExpectedPeriodCount(allPeriods,"用户申报",errors);
    if (expectedPeriodCount ==24 ||expectedPeriodCount ==96)
    {
        validateParticipantPeriods(consumerPeriods,expectedPeriodCount,"用户",errors);
    }
    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }
    std::sort(tempData.begin(),tempData.end(),[](const ConsumerBid &a,const ConsumerBid &b)
              {
                  if (a.period !=b.period)
                  {
                      return a.period <b.period;
                  }
                  if (a.id !=b.id)
                  {
                      return a.id <b.id;
                  }
                  return a.segment <b.segment;
              });
    data =tempData;
    return true;
}

// 读取系统负荷曲线
bool DataReader::readLoadCurve(const QString &filePath,QVector<LoadPoint> &data,QStringList &errors)
{
    data.clear();
    errors.clear();
    QStringList header;
    QVector<CsvRow> rows;
    if (!readCsvRows(filePath,header,rows,errors))
    {
        return false;
    }
    if (!checkLoadHeader(header,errors))
    {
        return false;
    }
    QVector<LoadPoint> tempData;
    QSet<int> periods;
    for (const CsvRow &row : rows)
    {
        const QStringList &c =row.columns;
        LoadPoint item;
        item.time =c[1];
        bool rowValid =true;
        if (!parsePositiveInt(c[0],item.period,row.lineNumber,"period",errors))
        {
            rowValid =false;
        }
        if (item.period >96)
        {
            errors.append(QString("第 %1 行时段 %2 超出 1～96 范围").arg(row.lineNumber).arg(item.period));
            rowValid =false;
        }
        if (!checkNotEmpty(item.time,row.lineNumber,"时刻",errors))
        {
            rowValid =false;
        }
        if (!parseNonNegativeDouble(c[2],item.load,row.lineNumber,"系统负荷",errors))
        {
            rowValid =false;
        }
        if (item.load <=0.0)
        {
            errors.append(QString("第 %1 行系统负荷必须大于 0").arg(row.lineNumber));
            rowValid =false;
        }
        if (rowValid)
        {
            if (periods.contains(item.period))
            {
                errors.append(QString("第 %1 行时段 %2 重复").arg(row.lineNumber).arg(item.period));
                rowValid =false;
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
    detectExpectedPeriodCount(periods,"负荷曲线",errors);
    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }
    std::sort(tempData.begin(),tempData.end(),[](const LoadPoint &a,const LoadPoint &b)
              {
                  return a.period <b.period;
              });
    data =tempData;
    return true;
}

// 读取新能源出力数据
bool DataReader::readRenewableOutput(const QString &filePath,QVector<RenewableOutput> &data,QStringList &errors)
{
    data.clear();
    errors.clear();
    QStringList header;
    QVector<CsvRow> rows;
    if (!readCsvRows(filePath,header,rows,errors))
    {
        return false;
    }
    if (!checkRenewableHeader(header,errors))
    {
        return false;
    }
    QVector<RenewableOutput> tempData;
    // 记录机组和时段组合，检查重复数据
    QSet<QString> keys;
    // 记录所有出现过的时段
    QSet<int> allPeriods;
    // 记录每个新能源机组对应的时段
    QHash<QString,QSet<int>> generatorPeriods;
    // 记录新能源机组类型
    QHash<QString,QString> generatorTypes;
    for (const CsvRow &row : rows)
    {
        const QStringList &c =row.columns;
        RenewableOutput item;
        item.generatorId =c[2];
        item.generatorType =c[4];
        bool rowValid =true;
        if (!parsePositiveInt(c[0],item.period,row.lineNumber,"period",errors))
        {
            rowValid =false;
        }
        if (item.period >96)
        {
            errors.append(QString("第 %1 行时段 %2 超出 1～96 范围").arg(row.lineNumber).arg(item.period));
            rowValid =false;
        }
        if (!checkNotEmpty(item.generatorId,row.lineNumber,"新能源机组编号",errors))
        {
            rowValid =false;
        }
        if (!checkNotEmpty(item.generatorType,row.lineNumber,"新能源机组类型",errors))
        {
            rowValid =false;
        }
        if (item.generatorType !="风电" &&item.generatorType !="光伏")
        {
            errors.append(QString("第 %1 行新能源类型必须为风电或光伏").arg(row.lineNumber));
            rowValid =false;
        }
        if (!parseNonNegativeDouble(c[5],item.output,row.lineNumber,"新能源出力",errors))
        {
            rowValid =false;
        }
        if (!item.generatorId.isEmpty())
        {
            if (generatorTypes.contains(item.generatorId) &&generatorTypes.value(item.generatorId) !=item.generatorType)
            {
                errors.append(QString("第 %1 行新能源机组 %2 类型不一致").arg(row.lineNumber).arg(item.generatorId));
                rowValid =false;
            }
            else
            {
                generatorTypes.insert(item.generatorId,item.generatorType);
            }
        }
        if (rowValid)
        {
            const QString key =item.generatorId +"|" +QString::number(item.period);
            if (keys.contains(key))
            {
                errors.append(QString("第 %1 行机组 %2 在时段 %3 的新能源出力重复").arg(row.lineNumber).arg(item.generatorId).arg(item.period));
                rowValid =false;
            }
            else
            {
                keys.insert(key);
            }
        }
        if (rowValid)
        {
            allPeriods.insert(item.period);
            generatorPeriods[item.generatorId].insert(item.period);
            tempData.push_back(item);
        }
    }
    const int expectedPeriodCount =detectExpectedPeriodCount(allPeriods,"新能源出力",errors);
    if (expectedPeriodCount ==24 ||expectedPeriodCount ==96)
    {
        validateParticipantPeriods(generatorPeriods,expectedPeriodCount,"新能源机组",errors);
    }
    if (!errors.isEmpty())
    {
        data.clear();
        return false;
    }
    std::sort(tempData.begin(),tempData.end(),[](const RenewableOutput &a,const RenewableOutput &b)
              {
                  if (a.period !=b.period)
                  {
                      return a.period <b.period;
                  }
                  return a.generatorId <b.generatorId;
              });
    data =tempData;
    return true;
}

// 读取全部市场数据
bool DataReader::readAll(const DataFileSet &files,MarketData &data,QStringList &errors)
{
    data.clear();
    errors.clear();
    MarketData tempData;
    QStringList fileErrors;
    bool allOk =true;
    if (!readGeneratorBids(files.generatorBidsFile,tempData.generatorBids,fileErrors))
    {
        appendErrors("generator_bids",fileErrors,errors);
        allOk =false;
    }
    fileErrors.clear();
    if (!readConsumerBids(files.consumerBidsFile,tempData.consumerBids,fileErrors))
    {
        appendErrors("consumer_bids",fileErrors,errors);
        allOk =false;
    }
    fileErrors.clear();
    if (!readLoadCurve(files.loadCurveFile,tempData.loadCurve,fileErrors))
    {
        appendErrors("load_curve",fileErrors,errors);
        allOk =false;
    }
    fileErrors.clear();
    if (!readRenewableOutput(files.renewableOutputFile,tempData.renewableOutputs,fileErrors))
    {
        appendErrors("renewable_output",fileErrors,errors);
        allOk =false;
    }
    if (!allOk)
    {
        data.clear();
        return false;
    }
    fileErrors.clear();
    if (!validateRelations(tempData,fileErrors))
    {
        appendErrors("relations",fileErrors,errors);
        data.clear();
        return false;
    }
    data =tempData;
    return true;
}

// 检查各类市场数据之间的关联关系
bool DataReader::validateRelations(const MarketData &data,QStringList &errors)
{
    errors.clear();
    if (data.generatorBids.isEmpty())
    {
        errors.append("发电侧申报为空");
    }
    if (data.consumerBids.isEmpty())
    {
        errors.append("用户侧申报为空");
    }
    if (data.loadCurve.isEmpty())
    {
        errors.append("负荷曲线为空");
    }
    if (!errors.isEmpty())
    {
        return false;
    }
    QSet<int> loadPeriods;
    for (const LoadPoint &point : data.loadCurve)
    {
        loadPeriods.insert(point.period);
    }
    const int expectedPeriodCount =detectExpectedPeriodCount(loadPeriods,"负荷曲线",errors);
    if (expectedPeriodCount !=24 &&expectedPeriodCount !=96)
    {
        return false;
    }
    QHash<QString,QString> generatorTypes;
    QHash<QString,QSet<int>> generatorPeriods;
    for (const GeneratorBid &bid : data.generatorBids)
    {
        if (!generatorTypes.contains(bid.id))
        {
            generatorTypes.insert(bid.id,bid.type);
        }
        else if (generatorTypes.value(bid.id) !=bid.type)
        {
            errors.append(QString("机组 %1 在不同记录中的类型不一致").arg(bid.id));
        }
        generatorPeriods[bid.id].insert(bid.period);
    }
    validateParticipantPeriods(generatorPeriods,expectedPeriodCount,"机组",errors);
    QHash<QString,QSet<int>> consumerPeriods;
    for (const ConsumerBid &bid : data.consumerBids)
    {
        consumerPeriods[bid.id].insert(bid.period);
    }
    validateParticipantPeriods(consumerPeriods,expectedPeriodCount,"用户",errors);
    QHash<QString,QSet<int>> renewablePeriods;
    QSet<QString> missingIds;
    QSet<QString> typeMismatchIds;
    for (const RenewableOutput &item : data.renewableOutputs)
    {
        renewablePeriods[item.generatorId].insert(item.period);
        if (!generatorTypes.contains(item.generatorId))
        {
            if (!missingIds.contains(item.generatorId))
            {
                errors.append(QString("新能源机组 %1 在发电侧申报中不存在").arg(item.generatorId));
                missingIds.insert(item.generatorId);
            }
            continue;
        }
        if (generatorTypes.value(item.generatorId) !=item.generatorType)
        {
            if (!typeMismatchIds.contains(item.generatorId))
            {
                errors.append(QString("新能源机组 %1 的机组类型不一致").arg(item.generatorId));
                typeMismatchIds.insert(item.generatorId);
            }
        }
    }
    validateParticipantPeriods(renewablePeriods,expectedPeriodCount,"新能源机组",errors);
    for (auto it =generatorTypes.constBegin();it !=generatorTypes.constEnd();++it)
    {
        const QString id =it.key();
        const QString type =it.value();
        if ((type =="风电" ||type =="光伏") &&!renewablePeriods.contains(id))
        {
            errors.append(QString("新能源机组 %1 缺少新能源出力数据").arg(id));
        }
    }
    return errors.isEmpty();
}