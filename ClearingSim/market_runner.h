#ifndef MARKET_RUNNER_H
#define MARKET_RUNNER_H
#include"clearing_engine.h"
#include"renewable_manager.h"
#include<QString>
#include<QVector>
struct TimeMarketData
{
    int period;
    QString time;
    double intervalHours = 0.0;
    double loadMW = 0.0;
    QVector<Generator>generators;
    QVector<Consumer>consumers;
    QVector<RenewableBase>renewbaleBase;
};
struct PeriodResult
{
    int period;
    ClearResult result;
    QVector<SettlementItem> settlement;
};
struct DayResult
{
    QVector<PeriodResult>result;
};

DayResult runmarket(QVector<TimeMarketData>daydata,SettlementMode mode,double penetration);
#endif // MARKET_RUNNER_H
