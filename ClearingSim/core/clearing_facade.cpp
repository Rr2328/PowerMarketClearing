#include "core/clearing_facade.h"
#include "engine/clearing_engine.h"
#include "../uc/uc_solver.h"
#include <QHash>
#include <QDebug>
#include <algorithm>
#include <utility>
namespace
{
//单时段需求
double totalDemandAt(const MarketData&market, int period)
{
    double sum=0.0;
    for (const auto&c:market.consumerBids)
    {
        if(c.period==period||c.period<=0)//period<=0的申报视为全天恒量，计入每个时段
        {
            sum+=c.quantity;
        }
    }
    return sum;
}
//负荷曲线在指定时段的取值，无该时段数据返回-1
double loadAt(const MarketData&market, int period)
{
    for (const auto&l:market.loadCurve)
    {
        if (l.period==period)
        {
            return l.load;
        }
    }
    return -1.0;
}
}

//目标新能源用量=渗透率×负荷(t)，无负荷曲线时回退，购电申报总量(t)，0价RENEW供给段总量恒为P_re(t)。
double ClearingFacade::renewCapacityAt(const MarketData&market, int period,
                                       double penetration)
{
    if(penetration<=0.0)
    {
        return 0.0;
    }
    const double base=loadAt(market,period);
    const double demand=(base>0.0)?base:totalDemandAt(market,period);
    return penetration*demand;
}
//单时段基准出清（不含新能源）
ClearingResult ClearingFacade::clearBenchmark(const MarketData&market, const QString&mode)
{
    const double demand=totalDemandAt(market,1);
    ClearingResult result;
    result.mode=mode;
    result.sourceName=QStringLiteral("内置基准例 · 真引擎出清");
    result.periods.append(clearOne(market,1,QStringLiteral("全日"),demand,0.0,1.0,1.0,mode));
    return result;
}
//基础模式：96多时段循环，之后在进行24小时聚合形成24小时图
ClearingResult ClearingFacade::clearPeriods(const MarketData&market, int periodCount,
                                            double penetration, const QString&mode)
{
    ClearingResult result;
    result.mode=mode;
    result.sourceName=QStringLiteral("连续仿真 · 真引擎出清");
    double anyDemand=0.0;
    for(const auto&c:market.consumerBids)
    {
        anyDemand += c.quantity;
    }
    if(anyDemand<=0.0)return result;
    QVector<PeriodResult>raw;
    raw.reserve(96);
    for(int period=1;period<=96;++period)
    {
        const QString time=periodTime(period,96);
        const double demand=totalDemandAt(market,period);
        const double renewCap=renewCapacityAt(market,period,penetration);
        raw.append(clearOne(market,period,time,demand,renewCap,1.0,0.25,mode));
    }
    if(periodCount==24)
    {
        for(int hour=1;hour<=24;++hour)
        {
            PeriodResult agg;
            agg.period=hour;
            agg.time=periodTime(hour,24);
            int peakIdx=-1;//该小时出清价最高的 15 分钟截面
            double priceSum=0.0,loadSum=0.0,renewSum=0.0,volSum=0.0;
            for(int q=0;q<4;++q)
            {
                const PeriodResult&pr=raw[(hour-1)*4+q];
                priceSum+=pr.clearingPrice;
                loadSum+=pr.loadMW;
                renewSum+=pr.renewMW;
                volSum+=pr.clearedMW;
                agg.genFee+=pr.genFee;
                agg.conFee+=pr.conFee;
                if (peakIdx<0||pr.clearingPrice>raw[(hour - 1)*4+peakIdx].clearingPrice)
                {
                    peakIdx=q;
                }
            }
            agg.clearingPrice=priceSum/4.0;
            agg.loadMW=loadSum/4.0;//小时平均功率
            agg.renewMW=renewSum/4.0;
            agg.clearedMW=volSum/4.0;
            agg.genDetails=raw[(hour-1)*4+peakIdx].genDetails;
            agg.conDetails=raw[(hour-1)*4+peakIdx].conDetails;
            result.periods.append(agg);
        }
    }
    else
    {
        result.periods=raw;
    }
    return result;
}
//时段转化成时间
QString ClearingFacade::periodTime(int period, int periodCount)
{
    if(periodCount==96)
    {
        const int totalMinutes=period*15;
        if(totalMinutes==24*60)return QStringLiteral("24:00");
        return QStringLiteral("%1:%2")
            .arg(totalMinutes / 60, 2, 10, QChar('0'))
            .arg(totalMinutes % 60, 2, 10, QChar('0'));
    }
    if(period==24)
        return QStringLiteral("24:00");
    return QStringLiteral("%1:00").arg(period, 2, 10, QChar('0'));
}
//单个时段的出清核心
PeriodResult ClearingFacade::clearOne(const MarketData&market, int period,
                                       const QString&time, double demandMW, double renewMW,
                                       double scale, double durationHours,
                                       const QString&mode)
{
    PeriodResult out;
    out.period=period;
    out.time=time;
    out.loadMW=demandMW;
    out.renewMW=renewMW;
    //发电侧申报
    QVector<Generator> generators;
    if(renewMW>0.0)
    {
        Generator r;
        r.id=QStringLiteral("RENEW");
        r.name=QStringLiteral("新能源出力");
        r.type=QStringLiteral("NEW");
        r.price=0.0;
        r.capacity=renewMW;
        r.segment=0;
        generators.append(r);
    }
    for(const auto&g:market.generatorBids)
    {
        if(g.period>0&&g.period!=period)
            continue;//只取当前时段
        if(g.quantity<=0.0)
            continue;
        Generator e;
        e.id=g.id;
        e.name=g.name;
        e.price=g.price;
        e.capacity=g.quantity*scale;
        e.segment=g.segment;
        generators.append(e);
    }
    //购电侧申报
    QVector<Consumer> consumers;
    for(const auto&c:market.consumerBids)
    {
        if(c.period>0&&c.period!=period)
            continue;
        if (c.quantity <= 0.0)
            continue;
        Consumer e;
        e.id=c.id;
        e.name=c.name;
        e.price=c.price;
        e.demand=c.quantity*scale;
        e.segment=c.segment;
        consumers.append(e);
    }
    const ClearResult cr=ClearMarket(generators,consumers);
    const bool pab=(mode==QStringLiteral("PAB"));
    out.clearingPrice=cr.clearingprice;
    out.clearedMW=cr.totalvolume;
    //单个主体申报明细
    QHash<QString,EntityCleared>genMap, conMap;
    for(const auto&t:cr.trade)
    {
        if(t.volume<=0.0) continue;
        const QString gk=t.generatorID+QLatin1Char('#')
                           +QString::number(t.generatorseg);
        EntityCleared &ge=genMap[gk];
        ge.id=t.generatorID;
        ge.segment=t.generatorseg;
        ge.bidPrice=t.generatorprice;
        ge.clearedMW+=t.volume;
        ge.money+=(pab?t.volume*t.generatorprice
                       :t.volume*cr.clearingprice)*durationHours;
        const QString ck=t.consumerID+QLatin1Char('#')
                           +QString::number(t.consumerseg);
        EntityCleared &ce=conMap[ck];
        ce.id=t.consumerID;
        ce.segment=t.consumerseg;
        ce.bidPrice=t.consumerprice;
        ce.clearedMW+=t.volume;
        ce.money+=t.volume*cr.clearingprice*durationHours;
    }
    for(auto&e:genMap)
    {
        if(e.id==QStringLiteral("RENEW"))
        {
            e.name=QStringLiteral("新能源出力");
            continue;
        }
        for(const auto&g:market.generatorBids)
        {
            if(g.id==e.id)
            {
                e.name=g.name;
                break;
            }
        }
    }
    for(auto&e:conMap)
    {
        for(const auto&c:market.consumerBids)
        {
            if (c.id==e.id)
            {
                e.name=c.name;
                break;
            }
        }
    }
    out.genDetails=genMap.values();
    out.conDetails=conMap.values();
    std::sort(out.genDetails.begin(), out.genDetails.end(),
              [](const EntityCleared&a, const EntityCleared&b) {
                  return a.bidPrice<b.bidPrice;
              });
    std::sort(out.conDetails.begin(), out.conDetails.end(),
              [](const EntityCleared&a, const EntityCleared&b) {
                  return a.bidPrice>b.bidPrice;
              });
    //新能源实际消纳量
    double renewActual=0.0;
    for (const auto& e : std::as_const(out.genDetails))
    {
        if(e.id==QStringLiteral("RENEW"))
            renewActual+=e.clearedMW;
    }
    out.renewMW=renewActual;
    for (const auto& e : std::as_const(out.genDetails))
        out.genFee+=e.money;
    for (const auto& e : std::as_const(out.conDetails))
        out.conFee+=e.money;
    return out;
}
//二次曲线模式
ClearingResult ClearingFacade::clearPeriodsQuadratic(const MarketData&market,
                                                     int periodCount, double penetration)
{
    ClearingResult result;
    result.mode=QStringLiteral("QUAD");
    result.sourceName=QStringLiteral("二次曲线出清 · 二分边际定价");
    if (market.quadraticGens.isEmpty())
        return result; //模式未启用
    //96期逐时段原始出清
    QVector<PeriodResult> raw;
    raw.reserve(96);
    for(int period=1;period<=96;++period)
    {
        double load=totalDemandAt(market,period);
        if(load<=0.0)load=loadAt(market,period);
        const double renewCap=renewCapacityAt(market,period,penetration);
        const double renewActual=std::min(renewCap,std::max(0.0,load));
        const double netLoad=std::max(0.0,load-renewActual);//净负荷
        const QuadraticClearResult qr=quadraticClearing(market.quadraticGens, netLoad);
        PeriodResult out;
        out.period=period;
        out.time=periodTime(period, 96);
        out.loadMW=load;
        out.renewMW=renewActual;
        out.clearingPrice=qr.clearingPrice;
        out.clearedMW=renewActual+qr.totalVolume;
        // 发电侧明细
        if(renewActual>0.0)
        {
            EntityCleared re;
            re.id=QStringLiteral("RENEW");
            re.name=QStringLiteral("新能源出力");
            re.segment=0;
            re.bidPrice=0.0;
            re.clearedMW=renewActual;
            re.money=renewActual*qr.clearingPrice*0.25;
            out.genDetails.append(re);
        }
        for(const auto&d:qr.dispatch)
        {
            EntityCleared e;
            e.id=d.id;
            e.name=d.name;
            e.segment=1;
            e.bidPrice=d.marginalCost;   // 边际成本申报口径
            e.clearedMW=d.output;
            e.money=d.output*qr.clearingPrice*0.25;
            out.genDetails.append(e);
        }
        // 购电侧，固定需求按申报量占比分摊
        double conSum=0.0;
        for(const auto&c:market.consumerBids)
        {
            if(c.period==period||c.period<= 0)
                conSum+=c.quantity;
        }
        if (conSum>0.0)
        {
            for(const auto&c:market.consumerBids)
            {
                if(!(c.period==period||c.period<= 0))
                    continue;
                EntityCleared e;
                e.id=c.id;
                e.name=c.name;
                e.segment=c.segment;
                e.bidPrice=qr.clearingPrice;   // 统一出清价结算
                e.clearedMW=load*(c.quantity/conSum);
                e.money=e.clearedMW*qr.clearingPrice*0.25;
                out.conDetails.append(e);
            }
        }
        for (const auto& e : std::as_const(out.genDetails))
            out.genFee+=e.money;
        for (const auto& e : std::as_const(out.conDetails))
            out.conFee+=e.money;
        raw.append(out);
    }
    if (periodCount==24)
    {
        for(int hour=1;hour<=24;++hour)
        {
            PeriodResult agg;
            agg.period=hour;
            agg.time=periodTime(hour,24);
            int peakIdx=-1;
            double priceSum=0.0,loadSum=0.0,renewSum=0.0,volSum=0.0;
            for(int q=0;q<4;++q)
            {
                const PeriodResult&pr=raw[(hour-1)*4+q];
                priceSum+=pr.clearingPrice;
                loadSum+=pr.loadMW;
                renewSum+=pr.renewMW;
                volSum+=pr.clearedMW;
                agg.genFee+=pr.genFee;
                agg.conFee+=pr.conFee;
                if (peakIdx<0||pr.clearingPrice>raw[(hour-1)*4+peakIdx].clearingPrice)
                    peakIdx=q;
            }
            agg.clearingPrice=priceSum/4.0;
            agg.loadMW=loadSum/4.0;
            agg.renewMW=renewSum/4.0;
            agg.clearedMW=volSum/4.0;
            agg.genDetails=raw[(hour-1)*4+peakIdx].genDetails;
            agg.conDetails=raw[(hour-1)*4+peakIdx].conDetails;
            result.periods.append(agg);
        }
    }
    else
    {
        result.periods=raw;
    }
    return result;
}
/*SCUC机组组合模式（HiGHS MILP求解器）：
     * 求解器决定全天96期的开停机与出力（基础量、爬坡、最小开/停机时间、启动费用），
     * 出清价=逐时段经济调度的功率平衡对偶变量。*/
ClearingResult ClearingFacade::clearPeriodsUc(const MarketData&market,
                                              int periodCount, double penetration)
{
    ClearingResult result;
    result.mode=QStringLiteral("UC");
    result.sourceName=QStringLiteral("SCUC 机组组合 · 求解器边际定价");
    if(market.generatorMeta.isEmpty())
        return result; // 模式未启用
    //96 期净负荷向量（求解器一次解全天——爬坡/最小开停机是跨期约束）
    QVector<double> demand;
    QVector<double> renewOf;//各期新能源消纳
    QVector<double> loadOf;
    demand.reserve(96);
    renewOf.reserve(96);
    loadOf.reserve(96);
    for(int period=1;period<=96;++period)
    {
        double load=totalDemandAt(market,period);
        if(load<=0.0)
            load=loadAt(market,period);
        const double renewCap=renewCapacityAt(market,period,penetration);
        const double renewActual=std::min(renewCap,std::max(0.0,load));
        demand.append(std::max(0.0,load-renewActual));
        renewOf.append(renewActual);
        loadOf.append(load);
    }

    const UcSolution s=solveUcMilp(market.generatorMeta,demand,{},0.25);
    if (!s.ok)
    {
        //定位应用内求解失败原因
        double dmin=std::numeric_limits<double>::max(),dmax=-dmin,dsum=0.0;
        for (double v : std::as_const(demand))
        {
            dmin=std::min(dmin,v);
            dmax=std::max(dmax,v);
            dsum+=v;
        }
        double pMaxSum=0.0,pMinSum=0.0;
        for (const auto& g : std::as_const(market.generatorMeta))
        {
            pMaxSum+=g.pMax;
            pMinSum+=g.pMin;
        }
        qWarning()<<"[UC-DIAG] solve failed:"<<s.message
                   <<"units ="<<market.generatorMeta.size()
                   <<"demand min/max/sum ="<<dmin<<dmax<<dsum
                   <<"pMinSum/pMaxSum ="<<pMinSum<<pMaxSum
                   <<"genBids ="<<market.generatorBids.size()
                   <<"conBids ="<<market.consumerBids.size()
                   <<"loadCurve ="<<market.loadCurve.size()
                   <<"penetration ="<<penetration;
        result.sourceName=QStringLiteral("SCUC 求解失败：%1").arg(s.message);
        return result;//periods为空
    }
    //逐期聚合为PeriodResult
    QVector<PeriodResult>raw;
    raw.reserve(96);
    for(int period=1;period<=96;++period)
    {
        const int t=period-1;
        const double lam=s.lambda[t];
        double genSum=0.0;
        for (int g=0;g<market.generatorMeta.size();++g)
            genSum+=s.p[g][t];
        PeriodResult out;
        out.period=period;
        out.time=periodTime(period,96);
        out.loadMW=loadOf[t];
        out.renewMW=renewOf[t];
        out.clearingPrice=lam;
        out.clearedMW=renewOf[t]+genSum;
        //发电侧明细
        if(renewOf[t]>0.0)
        {
            EntityCleared re;
            re.id=QStringLiteral("RENEW");
            re.name=QStringLiteral("新能源出力");
            re.segment=0;
            re.bidPrice=0.0;
            re.clearedMW=renewOf[t];
            re.money=renewOf[t]*lam*0.25;
            out.genDetails.append(re);
        }
        for(int g=0;g<market.generatorMeta.size();++g)
        {
            const auto&m=market.generatorMeta[g];
            EntityCleared e;
            e.id=m.id;
            e.name=m.name;
            e.segment=0;
            e.bidPrice=m.marginalCost;//电量成本申报口径
            e.clearedMW=s.p[g][t];
            e.money=s.p[g][t]*lam*0.25;
            e.ucOn=s.u[g][t];//启停状态与技术出力区间
            e.ucPMin=m.pMin;
            e.ucPMax=m.pMax;
            out.genDetails.append(e);
        }
        //购电侧明细
        double conSum=0.0;
        for(const auto&c:market.consumerBids)
        {
            if(c.period==period||c.period<=0)
                conSum+=c.quantity;
        }
        if(conSum>0.0)
        {
            for(const auto&c:market.consumerBids)
            {
                if(!(c.period==period||c.period<=0))
                    continue;
                EntityCleared e;
                e.id=c.id;
                e.name=c.name;
                e.segment=c.segment;
                e.bidPrice=lam;//统一出清价结算
                e.clearedMW=loadOf[t]*(c.quantity/conSum);
                e.money=e.clearedMW*lam*0.25;
                out.conDetails.append(e);
            }
        }
        for (const auto& e : std::as_const(out.genDetails))
            out.genFee+=e.money;
        for (const auto& e : std::as_const(out.conDetails))
            out.conFee+=e.money;
        raw.append(out);
    }
    //UC汇总，启动次数/启动成本（初始状态=全部开机，t=0不计启动）
    for(int g=0;g<market.generatorMeta.size();++g)
    {
        for(int t=1;t<96;++t)
        {
            if(s.u[g][t]==1&&s.u[g][t-1]==0)
            {
                result.startupCount+=1;
                result.startupCostTotal+=market.generatorMeta[g].startupCost;
            }
        }
        for(int t=0;t<96;++t)
            result.noLoadCostTotal+=market.generatorMeta[g].noLoadCost*s.u[g][t];
    }
    result.totalCost=s.totalCost;
    //UC机组启停/出力计划用于P3状态列/P4甘特图/P5启停计划导出）
    result.ucUnitNames.clear();
    result.ucUnitStartupCost.clear();
    result.ucUnitOn.clear();
    result.ucUnitP.clear();
    for(int g=0;g<market.generatorMeta.size();++g)
    {
        const auto&m=market.generatorMeta[g];
        result.ucUnitNames.append(QStringLiteral("%1 %2").arg(m.name,m.id));
        result.ucUnitStartupCost.append(m.startupCost);
        result.ucUnitOn.append(s.u[g]);
        result.ucUnitP.append(s.p[g]);
    }
    //需求超出可开机容量的时段由失负荷松弛放行、λ封顶1500
    double maxShed=0.0;
    for(double v:s.shed)maxShed=std::max(maxShed,v);
    if(maxShed>0.5)
        result.sourceName += QStringLiteral(" · 峰时段缺供 %1 MW（稀缺出清价 1500）")
                                 .arg(maxShed,0,'f',1);
    //96归为24
    if(periodCount==24)
    {
        for(int hour=1;hour<=24;++hour)
        {
            PeriodResult agg;
            agg.period=hour;
            agg.time=periodTime(hour,24);
            int peakIdx=-1;
            double priceSum=0.0,loadSum=0.0,renewSum=0.0,volSum=0.0;
            for(int q=0;q<4;++q)
            {
                const PeriodResult&pr=raw[(hour-1)*4+q];
                priceSum+=pr.clearingPrice;
                loadSum+=pr.loadMW;
                renewSum+=pr.renewMW;
                volSum+=pr.clearedMW;
                agg.genFee+=pr.genFee;
                agg.conFee+=pr.conFee;
                if(peakIdx<0||pr.clearingPrice>raw[(hour-1)*4+peakIdx].clearingPrice)
                    peakIdx=q;
            }
            agg.clearingPrice=priceSum/4.0;
            agg.loadMW=loadSum/4.0;
            agg.renewMW=renewSum/4.0;
            agg.clearedMW=volSum/4.0;
            agg.genDetails=raw[(hour-1)*4+peakIdx].genDetails;
            agg.conDetails=raw[(hour-1)*4+peakIdx].conDetails;
            result.periods.append(agg);
        }
    }
    else
    {
        result.periods=raw;
    }
    return result;
}
