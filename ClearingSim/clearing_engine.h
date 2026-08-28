#ifndef CLEARING_ENGINE_H
#define CLEARING_ENGINE_H
#include <QString>
#include <QVector>
struct Generator//发电机报价
{
    QString id;
    double price;
    double capacity;
};
struct Consumer//用户报价
{
    QString name;
    double price;
    double demand;
};
struct ClearResult//出清结果
{
    double clearingprice=0.0;//出清价格
    double totalvolume=0.0;//总成交电量
};
ClearResult ClearMarket(QVector<Generator>generator,QVector<Consumer>consumer);
#endif // CLEARING_ENGINE_H
