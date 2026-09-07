#ifndef RENEWABLE_MANAGER_H
#define RENEWABLE_MANAGER_H
#include <QString>
#include <QVector>
#include "clearing_engine.h"
struct RenewableBase
{
    QString id;        // 机组ID
    QString name;      // 机组名称
    QString type;      // 风电 / 光伏
    double output = 0; // 当前时段基准出力
    int period;
};
QVector<Generator> createRenewableGenerators(double load,double penetration,
    const QVector<RenewableBase>& renewableBase);
#endif // RENEWABLE_MANAGER_H
