#include "clearing_engine.h"
#include<algorithm>
#include<QHash>
#include<QDebug>

ClearResult ClearMarket(QVector<Generator>generators,QVector<Consumer>consumers)
{   
    ClearResult clearresult;
    std::sort(consumers.begin(),consumers.end(),[](const Consumer &a, const Consumer &b){return a.price > b.price;});
    std::sort(generators.begin(),generators.end(),[](const Generator &a,const Generator &b){return a.price<b.price;});
    int gindex=0;
    int cindex=0;
    QVector<double>volumn;
    for (int i = 0; i < generators.size(); ++i) {
        volumn.append(generators[i].capacity);
    }
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
        /*if(generators[gindex].capacity+EPS<consumers[cindex].demand)
        {
            consumers[cindex].demand-=generators[gindex].capacity;
            clearresult.totalvolume+=generators[gindex].capacity;
            generators[gindex].capacity=0;
            gindex++;
        }
        else if(qAbs(generators[gindex].capacity-consumers[cindex].demand) <= EPS)
        {
            consumers[cindex].demand-=generators[gindex].capacity;
            clearresult.totalvolume+=generators[gindex].capacity;
            generators[gindex].capacity=0;
            gindex++;
            cindex++;
        }
        else
        {
            generators[gindex].capacity-=consumers[cindex].demand;
            clearresult.totalvolume+=consumers[cindex].demand;
            consumers[cindex].demand=0;
            cindex++;
        }*/
    }
    /*if(gindex<generators.size())
    {
        if(gindex==0&&cindex==0&&generators[gindex].capacity==volumn[gindex])
        {
            qDebug()<<"发电机报价均大于用户侧报价！";
        }
        else
        {
            if(generators[gindex].capacity!=volumn[gindex])clearresult.clearingprice=generators[gindex].price;
            else clearresult.clearingprice=generators[gindex-1].price;
        }
    }
    else
    {
        clearresult.clearingprice=generators[gindex-1].price;
        if(cindex<=consumers.size()-1&&consumers[cindex].demand>EPS)qDebug()<<"用户侧还需要电！";
    }*/
    clearresult.clearingprice=lastprice;
    return clearresult;
}
QVector<SettlementItem> settle(const ClearResult& clearresult,SettlementMode mode)
{
    QVector<SettlementItem>settlement;
    QHash<QString,SettlementItem>list;
    for(const auto& trade:clearresult.trade)
    {
        double money;
        if(mode==SettlementMode::MCP)
        {
            money =trade.volume*clearresult.clearingprice;
        }
        else
        {
            money =trade.volume*trade.generatorprice;
        }
        list[trade.generatorID].id=trade.generatorID;
        list[trade.generatorID].volume+=trade.volume;
        list[trade.generatorID].amount+=money;
        list[trade.consumerID].id=trade.consumerID;
        list[trade.consumerID].amount+=trade.volume*clearresult.clearingprice;
        list[trade.consumerID].volume+=trade.volume;
    }
    for(auto& settle:list)
    {
        settlement.append(settle);
    }
    return settlement;
}