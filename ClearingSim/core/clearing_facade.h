#ifndef CLEARING_FACADE_H
#define CLEARING_FACADE_H
#include <QString>
#include <QVector>
#include "data/data_reader.h"
//单主体单时段出清结果
struct EntityCleared
{
    QString id;
    QString name;
    int segment=0;
    double bidPrice=0.0;//该段申报价
    double clearedMW=0.0;//出清电量
    double money=0.0;//收入/费用
    //UC模式（SCUC）专用
    int ucOn=-1;//启停状态0/1（-1=非UC模式）
    double ucPMin=0.0;//最小出力
    double ucPMax=0.0;//额定容量
};
//多主体单时段出清结果
struct PeriodResult
{
    int period=0;
    QString time;
    double loadMW=0.0;//该时段总需求
    double renewMW=0.0;//该时段新能源实际消纳量
    double clearingPrice=0.0;//该时段出清价
    double clearedMW=0.0;//该时段总成交电量
    double genFee=0.0;//发电侧结算总额
    double conFee=0.0; //购电侧结算总额
    QVector<EntityCleared>genDetails;
    QVector<EntityCleared>conDetails;
};
//多主体多时段
struct ClearingResult
{
    QString mode;
    QString sourceName;
    QVector<PeriodResult>periods;
    //UC模式
    int startupCount=0;//全天启动次数
    double startupCostTotal=0.0;//全天启动成本
    double noLoadCostTotal=0.0;//全天空载成本
    double totalCost=0.0;//求解器总成本
    //用于界面设计，供P3明细状态列、P4启停甘特图、P5启停计划导出消费
    QStringList ucUnitNames;//机组显示名（电厂+机组号）
    QVector<double>ucUnitStartupCost;//每机组启动费用
    QVector<QVector<int>>ucUnitOn;//启停状态0/1
    QVector<QVector<double>> ucUnitP;//出力计划
};
//三个模式（基础、二次曲线、uc模式）走不同的函数。
class ClearingFacade
{
public:
    //目标新能源用量=渗透率×负荷(t)，无负荷曲线时回退，购电申报总量(t)。
    static double renewCapacityAt(const MarketData&market, int period,double penetration);
    //单时段基准出清（不含新能源）
    static ClearingResult clearBenchmark(const MarketData&market, const QString&mode);
    //基础模式：96多时段循环，之后在进行24小时聚合形成24小时图
    static ClearingResult clearPeriods(const MarketData&market, int periodCount,
                                       double penetration, const QString&mode);
    //二次曲线模式
    static ClearingResult clearPeriodsQuadratic(const MarketData&market,
                                                int periodCount, double penetration);
    /*SCUC机组组合模式（HiGHS MILP求解器）：
     * 求解器决定全天96期的开停机与出力（基础量、爬坡、最小开/停机时间、启动费用），
     * 出清价=逐时段经济调度的功率平衡对偶变量。*/
    static ClearingResult clearPeriodsUc(const MarketData&market,
                                         int periodCount, double penetration);
private:
    //单个时段的出清核心
    static PeriodResult clearOne(const MarketData&market, int period,
                                 const QString&time, double demandMW, double renewMW,
                                 double scale, const QString&mode);
    //时段转化成时间
    static QString periodTime(int period, int periodCount);
};
#endif//CLEARING_FACADE_H