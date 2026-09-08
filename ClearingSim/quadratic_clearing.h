#ifndef QUADRATIC_CLEARING_H
#define QUADRATIC_CLEARING_H
#include<QString>
#include<QVector>
#include"market_runner.h"
struct QuadraticGenerator
{
    QString id;
    QString name;
    QString type;
    double a=0.0;
    double b=0.0;
    double c=0.0;
    double pMax = 0.0;
};
struct QuadraticDispatchItem
{
    QString generatorId;
    double output=0.0;
    double marginalCost=0.0;
};
struct QuadraticClearResult
{
    double clearingPrice=0.0;
    double totalVolume=0.0;
    QVector<QuadraticDispatchItem> dispatch;
};
struct QuadraticPeriodResult
{
    int period=0;
    double loadMW=0.0;
    double renewableOutput=0.0;
    double netLoadMW = 0.0;
    QuadraticClearResult result;
};
struct QuadraticDayResult
{
    QVector<QuadraticPeriodResult>periods;
};
QuadraticClearResult quadraticclearing(QVector<QuadraticGenerator> generators,double D);
QuadraticDayResult runQuadraticMarket(const QVector<TimeMarketData>& daydata,const QVector<QuadraticGenerator>& generators,double penetration);
#endif // QUADRATIC_CLEARING_H
