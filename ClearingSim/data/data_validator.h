#ifndef DATA_VALIDATOR_H
#define DATA_VALIDATOR_H

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// 数据校验器（数据模块：陈美伊）
//   独立 namespace 避免与主线 data_reader.cpp 内部同名 helper 产生
//   匿名命名空间注入歧义；文件尾部 using 声明保持全局直接可用。
namespace teammate
{

bool checkNotEmpty(const QString &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool parsePositiveInt(const QString &text,int &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool parseDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool parseNonNegativeDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool checkPriceRange(double price,int lineNumber,const QString &fieldName,QStringList &errors);
bool checkPricePrecision(const QString &text,int lineNumber,const QString &fieldName,QStringList &errors);
int detectExpectedPeriodCount(const QSet<int> &periods,const QString &name,QStringList &errors);
bool validateParticipantPeriods(const QHash<QString,QSet<int>> &periodMap,int expectedPeriodCount,const QString &name,QStringList &errors);

} // namespace teammate

#endif // DATA_VALIDATOR_H
