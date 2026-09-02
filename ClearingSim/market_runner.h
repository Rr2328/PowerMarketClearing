#ifndef MARKET_RUNNER_H
#define MARKET_RUNNER_H
#include"clearing_engine.h"
#include<QString>
#include<QVector>
struct TimeMarketData
{
    int period;
    QVector<Generator>generators;
    QVector<Consumer>consumers;
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

DayResult run96market(QVector<TimeMarketData>daydata,SettlementMode mode);
#endif // MARKET_RUNNER_H
