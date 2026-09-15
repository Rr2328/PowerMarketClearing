#include "quadratic_clearing.h"
#include <QtGlobal>
#include <algorithm>
#include <limits>
namespace
{
constexpr double kSupplyTol=1e-3;//供给-需求平衡容差
constexpr double kCoefEps=1e-9;//a系数零判定阈值
constexpr int kMaxIter=1000;//二分最大迭代次数
//计算二次曲线出力
double unitSupply(const QuadraticGenerator &g, double price)
{
    if (g.a<kCoefEps)
    {
        return price>=g.b?g.pMax:0.0;//a≈0时边际成本恒为b的阶梯机组
    }
    const double p=(price-g.b)/(2.0*g.a);
    return std::max(0.0,std::min(p,g.pMax));
}
//单机边际成本
double unitMarginalCost(const QuadraticGenerator&g, double output)
{
    if(g.a<kCoefEps)return g.b;
    return 2.0*g.a*output+g.b;
}
}
//二次曲线模式出清
QuadraticClearResult quadraticClearing(QVector<QuadraticGenerator>generators,double demandMW)
{
    QuadraticClearResult result;
    if(demandMW<0.0)demandMW=0.0;
    //恒定供给下界：λ低于所有b时机组出力全0
    double minB=std::numeric_limits<double>::max();//机组里面最小的b
    bool hasGen=false;//是否有可用机组
    double capacity=0.0;//可以用的容量
    double priceMax=0.0;//最高边际价
    for (const auto&g:generators)
    {
        if(g.pMax<=0.0)continue;
        hasGen=true;
        capacity+=g.pMax;
        priceMax=std::max(priceMax,unitMarginalCost(g,g.pMax));
        minB=std::min(minB,g.b);
    }
    if(!hasGen||capacity<= 0.0)return result; //没有可用机组
    if (demandMW>capacity+kSupplyTol)//发电侧供电量小于需求
    {
        result.ok=true;
        result.shortfall=demandMW-capacity;
        result.clearingPrice=priceMax;
        result.totalVolume=capacity;
        for (const auto&g:generators)
        {
            QuadraticDispatchItem item;
            item.id=g.id;
            item.name=g.name;
            item.output=g.pMax;
            item.marginalCost=unitMarginalCost(g,g.pMax);
            result.dispatch.append(item);
        }
        return result;
    }
    if(demandMW<=kSupplyTol) //零需求，机组全部不出力，价格取开机门槛
    {
        result.ok=true;
        result.clearingPrice=minB;
        for(const auto&g:generators)
        {
            QuadraticDispatchItem item;
            item.id=g.id;
            item.name=g.name;
            result.dispatch.append(item);
        }
        return result;
    }
    // 二分搜索
    // 搜索“供给首次达到需求”的最低价格。阶梯机组在该价格可部分中标，
    // 因此不能在二分过程中用 0/满发的跳变结果直接作为最终出力。
    double priceLow=std::nextafter(minB,-std::numeric_limits<double>::infinity());
    double priceHigh=priceMax;
    for (int iter=0;iter<kMaxIter;++iter)
    {
        const double price=0.5*(priceLow+priceHigh);
        double total=0.0;
        for(const auto&g:generators)
        {
            total+=unitSupply(g,price);
        }
        if(total<demandMW)
        {
            priceLow=price;
        }
        else
        {
            priceHigh=price;
        }
        const double priceScale=std::max({1.0,std::abs(priceLow),std::abs(priceHigh)});
        if(priceHigh-priceLow<=1e-10*priceScale)break;
    }

    const double price=priceHigh;
    const double priceTol=1e-7*std::max(1.0,std::abs(price));
    QVector<double> outputs(generators.size(),0.0);
    QVector<int> marginalSteps;
    double total=0.0;
    for(int i=0;i<generators.size();++i)
    {
        const auto&g=generators.at(i);
        if(g.pMax<=0.0)continue;
        if(g.a<kCoefEps)
        {
            if(g.b<price-priceTol)
            {
                outputs[i]=g.pMax;
                total+=g.pMax;
            }
            else if(std::abs(g.b-price)<=priceTol)
            {
                marginalSteps.append(i);
            }
        }
        else
        {
            outputs[i]=unitSupply(g,price);
            total+=outputs[i];
        }
    }
    double remaining=std::max(0.0,demandMW-total);
    for(const int i:marginalSteps)
    {
        const double accepted=std::min(generators.at(i).pMax,remaining);
        outputs[i]=accepted;
        total+=accepted;
        remaining-=accepted;
    }

    result.ok=(std::abs(total-demandMW)<=kSupplyTol);
    result.clearingPrice=price;
    result.totalVolume=total;
    for(int i=0;i<generators.size();++i)
    {
        const auto&g=generators.at(i);
        QuadraticDispatchItem item;
        item.id=g.id;
        item.name=g.name;
        item.output=outputs.at(i);
        item.marginalCost=unitMarginalCost(g,item.output);
        result.dispatch.append(item);
    }
    return result;
}
