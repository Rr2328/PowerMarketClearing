#ifndef CLEARING_ENGINE_H
#define CLEARING_ENGINE_H
#include <QString>
#include <QVector>
struct Generator//发电机报价
{
    QString id;
    QString name;
    QString type;
    double price;
    double capacity;
    int segment=1;
};
struct Consumer//用户报价
{
    QString id;
    QString name;
    double price;
    double demand;
    int segment=1;//用户报价段区分
};
struct Trade
{
    QString generatorID;
    int generatorseg=1;
    QString consumerID;
    int consumerseg=1;
    double volume=0;
    double generatorprice=0;
    double consumerprice=0;
};
struct ClearResult//出清结果
{
    double clearingprice=0.0;//出清价格
    double totalvolume=0.0;//总成交电量
    QVector<Trade>trade;
    //bool supplyShortage=false;
    //double unmetDemand=0.0;
    //QString message;
};
enum class SettlementMode
{
    MCP,
    PAB
};
struct SettlementItem
{
    QString id;
    double volume=0;
    double amount=0;
};
QVector<SettlementItem> settle(const ClearResult& clearresult,SettlementMode mode);

ClearResult ClearMarket(QVector<Generator>generator,QVector<Consumer>consumer);
#endif // CLEARING_ENGINE_H
