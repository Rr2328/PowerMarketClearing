#include "clearing_engine.h"
#include<algorithm>
#include<QDebug>
ClearResult ClearMarket(QVector<Generator>generators,QVector<Consumer>consumers)
{
    ClearResult clearresult;
    std::sort(consumers.begin(),consumers.end(),[](const Consumer &a, const Consumer &b){return a.price > b.price;});
    std::sort(generators.begin(),generators.end(),[](Generator &a,Generator &b){return a.price<b.price;});
    int gindex=0;
    int cindex=0;
    QVector<double>volumn;
    for (int i = 0; i < generators.size(); ++i) {
        volumn.append(generators[i].capacity);
    }
    while(gindex<generators.size()&&cindex<consumers.size()&&generators[gindex].price<=consumers[cindex].price)
    {
        if(generators[gindex].capacity<consumers[cindex].demand)
        {
            consumers[cindex].demand-=generators[gindex].capacity;
            clearresult.totalvolume+=generators[gindex].capacity;
            generators[gindex].capacity=0;
            gindex++;
        }
        else if(generators[gindex].capacity==consumers[cindex].demand)
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
        }
    }
    if(gindex<generators.size())
    {
        if(generators[gindex].capacity!=volumn[gindex])clearresult.clearingprice=generators[gindex].price;
        else clearresult.clearingprice=generators[gindex-1].price;
    }
    else
    {
        clearresult.clearingprice=generators[gindex-1].price;
        if(cindex<consumers.size()-1)qDebug()<<"用户侧还需要电！";
    }
    return clearresult;
}
