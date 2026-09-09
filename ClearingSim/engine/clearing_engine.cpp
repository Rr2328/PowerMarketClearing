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
    // V1.3.1 稀缺封顶（契约 §7.3-2，老师评审反馈）：
    //   仅当供给侧申报全部用尽（gindex 走到头）且未满足需求 > 0.5 MW 时，
    //   统一出清价升到限价 540——供给量尽 → 价格竖直升至限价的稀缺语义。
    //   若循环因"供给要价 > 剩余需求报价"终止（价格不交叉、双侧均有剩余），
    //   维持边际供给定价：此时需求封口线仍与供给水平段相交，图形自洽。
    //   容差 0.5 MW：默认样例逐时段供需差仅 ±0.02 MW（舍入级），不得误触发。
    double residualDemand = 0.0;
    for (const Consumer &c : consumers)
        residualDemand += c.demand;
    constexpr double kDemandTol = 0.5;    // MW
    constexpr double kPriceCap  = 540.0;  // 总则规则④：双侧统一限价
    if (gindex >= generators.size() && residualDemand > kDemandTol)
        clearresult.clearingprice = kPriceCap;
    else
        clearresult.clearingprice = lastprice;
    return clearresult;
}