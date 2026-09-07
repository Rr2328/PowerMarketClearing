#include"market_runner.h"
#include"clearing_engine.h"
#include"renewable_manager.h"
DayResult runmarket(QVector<TimeMarketData>daydata,SettlementMode mode,double penetration)
{
    DayResult dayresult;
    for(auto& data:daydata)
    {
        PeriodResult periodresult;
        periodresult.period=data.period;
        QVector<Generator>renewablegenerators=createRenewableGenerators(data.loadMW,penetration,data.renewbaleBase);
        QVector<Generator>allgenerators=data.generators;
        for(auto& renew:renewablegenerators)
        {
            allgenerators.append(renew);
        }
        ClearResult clearresult=ClearMarket(allgenerators,data.consumers);
        periodresult.result= clearresult;
        periodresult.settlement=settle(clearresult,mode);
        dayresult.result.append(periodresult);
    }
    return dayresult;
}