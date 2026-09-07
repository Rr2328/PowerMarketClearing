#include "clearing_engine.h"
#include<algorithm>
#include<QDebug>

ClearResult ClearMarket(QVector<Generator>generators,QVector<Consumer>consumers)
{
    ClearResult clearresult;
    std::sort(consumers.begin(),consumers.end(),[](const Consumer &a, const Consumer &b){return a.price > b.price;});
    std::sort(generators.begin(),generators.end(),[](const Generator &a,const Generator &b){return a.price<b.price;});
    int gindex=0;
    int cindex=0;
    constexpr double EPS = 1e-9;
    if (generators.isEmpty() || consumers.isEmpty()) {
        qDebug() << "出清失败：发电侧或购电侧为空";
        return clearresult;
    }
    double lastprice=0;
    while(gindex<generators.size()&&cindex<consumers.size()&&generators[gindex].price<=consumers[cindex].price)
    {
        Trade trade;
        double tradevolume=std::min(generators[gindex].capacity,consumers[cindex].demand);
        trade.consumerseg=consumers[cindex].segment;
        trade.generatorseg=generators[gindex].segment;
        trade.consumerID=consumers[cindex].id;
        trade.consumerprice=consumers[cindex].price;
        trade.generatorID=generators[gindex].id;
        trade.generatorprice=generators[gindex].price;
        trade.volume=tradevolume;
        clearresult.trade.append(trade);
        clearresult.totalvolume+=tradevolume;
        lastprice=generators[gindex].price;
        generators[gindex].capacity-=tradevolume;
        consumers[cindex].demand-=tradevolume;
        if(generators[gindex].capacity<=EPS)gindex++;
        if(consumers[cindex].demand<=EPS)cindex++;
    }
    clearresult.clearingprice=lastprice;
    return clearresult;
}