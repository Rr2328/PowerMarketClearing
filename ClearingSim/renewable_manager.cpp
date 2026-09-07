#include"renewable_manager.h"
#include<QDebug>
QVector<Generator>createRenewableGenerators(double load,double penetration,
                                             const QVector<RenewableBase>& renewableBase)
{
    QVector<Generator>result;
    if(load<0)
    {
        qDebug()<<"新能源计算失败：负荷不可以小于0！";
        return result;
    }
    if(penetration<0||penetration>1)
    {
        qDebug()<<"新能源计算失败：渗透率应在0~1之间！";
        return result;
    }
    double totalbase=0;
    for(const auto& renewable:renewableBase)
    {
        if (renewable.output>0)
        {
            totalbase+=renewable.output;
        }
    }
    if (totalbase<=0.0)
    {
        qDebug()<<"新能源计算失败：新能源基准总出力为0";
        return result;
    }
    double targetRenewableOutput=load*penetration;
    double actualRenewableOutput=std::min(targetRenewableOutput, totalbase);
    for (const auto& renewable:renewableBase)
    {
        if (renewable.output<=0)continue;
        double ratio=renewable.output/totalbase;
        double actualOutput=targetRenewableOutput * ratio;
        Generator generator;
        generator.period=renewable.period;
        generator.id=renewable.id;
        generator.name=renewable.name;
        generator.type=renewable.type;
        generator.segment=1;
        generator.price=0.0;
        generator.quantity=actualOutput;
        result.append(generator);
    }
    return result;
}