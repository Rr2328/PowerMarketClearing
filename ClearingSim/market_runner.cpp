#include"market_runner.h"
#include"clearing_engine.h"
DayResult run96market(QVector<TimeMarketData>daydata,SettlementMode mode)
{
    DayResult dayresult;
    for(auto& data:daydata)
    {
        PeriodResult periodresult;
        periodresult.period=data.period;
        ClearResult clearresult=ClearMarket(data.generators,data.consumers);
        periodresult.result= clearresult;
        periodresult.settlement=settle(clearresult,mode);
        dayresult.result.append(periodresult);
    }
    return dayresult;
}