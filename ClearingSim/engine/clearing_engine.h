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
};
// 注：B 位仅暴露撮合（ClearMarket）。结算由 ClearingFacade 自管（C 接口 EntityCleared），
//     历史遗留的 settle()/SettlementItem/SettlementMode 已在 #94 清理（全项目无 caller）。
ClearResult ClearMarket(QVector<Generator>generator,QVector<Consumer>consumer);
#endif // CLEARING_ENGINE_H
