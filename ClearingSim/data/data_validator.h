#ifndef DATA_VALIDATOR_H
#define DATA_VALIDATOR_H

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

bool checkNotEmpty(const QString &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool parsePositiveInt(const QString &text,int &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool parseDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool parseNonNegativeDouble(const QString &text,double &value,int lineNumber,const QString &fieldName,QStringList &errors);
bool checkPriceRange(double price,int lineNumber,const QString &fieldName,QStringList &errors);
bool checkPricePrecision(const QString &text,int lineNumber,const QString &fieldName,QStringList &errors);
int detectExpectedPeriodCount(const QSet<int> &periods,const QString &name,QStringList &errors);
bool validateParticipantPeriods(const QHash<QString,QSet<int>> &periodMap,int expectedPeriodCount,const QString &name,QStringList &errors);

#endif // DATA_VALIDATOR_H