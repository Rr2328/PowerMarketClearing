#include"quadratic_clearing.h"
#include<QVector>
#include<QDebug>
QuadraticClearResult quadraticclearing(QVector<QuadraticGenerator> generators,double D)
{
    double price;
    double pricemax=0;
    double pricemin=0;
    for(const auto& gens:generators)
    {
        double maxMC=2*gens.a*gens.pMax+gens.b;
        pricemax=std::max(pricemax, maxMC);
    }
    price=(pricemin+pricemax)/2.0;
    double totalvolumn=0;
    double maxTotal=0;
    for(const auto& gens:generators)
    {
        maxTotal+=gens.pMax;
    }
    if(D>maxTotal)
    {
        qDebug()<<"出清失败：总需求超过所有机组最大容量";
        return{};
    }
    int count=0;
    while(count<1000)
    {
        totalvolumn=0;
        for(auto& gens:generators)
        {
            double pvolumn=(price-gens.b)/(2*gens.a);
            totalvolumn+=std::max(0.0,std::min(pvolumn,gens.pMax));
        }
        if(std::abs(totalvolumn-D)<1e-3)
        {
            break;
        }
        else
        {
            if(totalvolumn<D)
            {
                pricemin=price;
                price=(price+pricemax)/2;
            }
            else
            {
                pricemax=price;
                price=(price+pricemin)/2;
            }
        }
        count++;
    }
    QuadraticClearResult clearingresult;
    clearingresult.clearingPrice=price;
    clearingresult.totalVolume=totalvolumn;
    for(auto& gens:generators)
    {
        QuadraticDispatchItem dispatch;
        double pvolumn=(price-gens.b)/(2*gens.a);
        double output=std::max(0.0,std::min(pvolumn,gens.pMax));
        dispatch.generatorId=gens.id;
        dispatch.marginalCost=2*gens.a*output+gens.b;
        dispatch.output=output;
        clearingresult.dispatch.append(dispatch);
    }
    return clearingresult;
}
QuadraticDayResult runQuadraticMarket(const QVector<TimeMarketData>& daydata,const QVector<QuadraticGenerator>& generators,double penetration)
{
    QuadraticDayResult dayResult;
    for(const auto& data:daydata)
    {
        QuadraticPeriodResult periodResult;
        periodResult.period=data.period;
        periodResult.loadMW=data.loadMW;
        QVector<Generator>renewableGenerators=createRenewableGenerators(data.loadMW,penetration,data.renewbaleBase);
        double renewableOutput=0.0;
        for(const auto& renewable:renewableGenerators)
        {
            renewableOutput+=renewable.quantity;
        }
        periodResult.renewableOutput=renewableOutput;
        double netLoad=data.loadMW-renewableOutput;
        netLoad=std::max(0.0, netLoad);
        periodResult.netLoadMW=netLoad;
        periodResult.result=quadraticclearing(generators,netLoad);
        dayResult.periods.append(periodResult);
    }
    return dayResult;
}