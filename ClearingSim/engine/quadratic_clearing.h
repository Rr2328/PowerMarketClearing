#ifndef QUADRATIC_CLEARING_H
#define QUADRATIC_CLEARING_H
#include <QString>
#include <QVector>
// 二次曲线版发电侧
struct QuadraticGenerator
{
    QString id;
    QString name;
    double a=0.0; //二次项系数
    double b=0.0; //一次项系数
    double c=0.0; //常数项
    double pMax=0.0;//最大出力（MW）
};
//单机出清明细
struct QuadraticDispatchItem
{
    QString id;
    QString name;
    double output=0.0;//成交出力
    double marginalCost=0.0;//边际成本
};
//单时段二次出清结果
struct QuadraticClearResult
{
    bool ok=false;
    double clearingPrice=0.0;//统一出清价
    double totalVolume=0.0;//发电侧成交总量
    double shortfall=0.0;//供给缺口
    QVector<QuadraticDispatchItem>dispatch;
};
QuadraticClearResult quadraticClearing(QVector<QuadraticGenerator>generators,double demandMW);
#endif // QUADRATIC_CLEARING_H