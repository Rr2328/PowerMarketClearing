#include "data_validator.h"

#include <cmath>

// 检查字段是否为空
bool checkNotEmpty(const QString &value,int lineNumber,const QString &fieldName,QStringList &errors)
{
    if (value.trimmed().isEmpty())
    {
        errors.append(QString("第 %1 行 %2 为空").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 解析正整数
bool parsePositiveInt(const QString &text,int &value,int lineNumber,const QString &fieldName,QStringList &errors)
{
    bool ok =false;
    value =text.toInt(&ok);
    if (!ok)
    {
        errors.append(QString("第 %1 行 %2 不是有效整数：%3").arg(lineNumber).arg(fieldName).arg(text));
        return false;
    }
    if (value <=0)
    {
        errors.append(QString("第 %1 行 %2 必须大于 0").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 解析浮点数
bool parseDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors)
{
    bool ok =false;
    value =text.toDouble(&ok);
    if (!ok ||!std::isfinite(value))
    {
        errors.append(QString("第 %1 行 %2 不是有效数字：%3").arg(lineNumber).arg(fieldName).arg(text));
        return false;
    }
    return true;
}

// 解析非负浮点数
bool parseNonNegativeDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors)
{
    if (!parseDouble(text,value,lineNumber,fieldName,errors))
    {
        return false;
    }
    if (value <0.0)
    {
        errors.append(QString("第 %1 行 %2 不能为负数").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 检查报价范围
bool checkPriceRange(double price,int lineNumber,const QString &fieldName,QStringList &errors)
{
    if (price <0.0 ||price >540.0)
    {
        errors.append(QString("第 %1 行 %2 必须在 0～540 元/MWh 范围内").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 检查报价小数位
bool checkPricePrecision(const QString &text,int lineNumber,const QString &fieldName,QStringList &errors)
{
    const QString value =text.trimmed();
    if (value.contains('e',Qt::CaseInsensitive))
    {
        errors.append(QString("第 %1 行 %2 不允许使用科学计数法").arg(lineNumber).arg(fieldName));
        return false;
    }
    const int dotIndex =value.indexOf('.');
    if (dotIndex <0)
    {
        return true;
    }
    const int decimals =value.size() -dotIndex -1;
    if (decimals >3)
    {
        errors.append(QString("第 %1 行 %2 最多保留 3 位小数").arg(lineNumber).arg(fieldName));
        return false;
    }
    return true;
}

// 判断数据采用24时段还是96时段
int detectExpectedPeriodCount(const QSet<int> &periods,const QString &name,QStringList &errors)
{
    if (periods.isEmpty())
    {
        errors.append(name +"没有有效时段");
        return 0;
    }
    int maxPeriod =0;
    for (int period : periods)
    {
        if (period <1 ||period >96)
        {
            errors.append(QString("%1 出现非法时段 %2").arg(name).arg(period));
            continue;
        }
        if (period >maxPeriod)
        {
            maxPeriod =period;
        }
    }
    const int expected =maxPeriod <=24 ?24 :96;
    for (int period =1;period <=expected;++period)
    {
        if (!periods.contains(period))
        {
            errors.append(QString("%1 缺少时段 %2").arg(name).arg(period));
        }
    }
    return expected;
}

// 检查参与者是否包含完整时段
bool validateParticipantPeriods(const QHash<QString,QSet<int>> &periodMap,int expectedPeriodCount,const QString &name,QStringList &errors)
{
    bool valid =true;
    for (auto it =periodMap.constBegin();it !=periodMap.constEnd();++it)
    {
        const QString id =it.key();
        const QSet<int> &periods =it.value();
        for (int period =1;period <=expectedPeriodCount;++period)
        {
            if (!periods.contains(period))
            {
                errors.append(QString("%1 %2 缺少时段 %3").arg(name).arg(id).arg(period));
                valid =false;
            }
        }
    }
    return valid;
}