#include "mainwindow.h"
#include"clearing_engine.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QVector<Generator> generators = {
        {"G1","一号风","风", 150.0, 100.0, 1},
        {"G1", "一号风","风",180.0, 60.0, 2},
        {"G2","2号风","风", 200.0, 40.0, 1}
    };

    QVector<Consumer> consumers = {
        {"U1", "用户1",300.0, 80.0, 1},
        {"U1","用户1", 250.0, 60.0, 2}
    };

    ClearResult clearresult=ClearMarket(generators,consumers);
    qDebug()<<"出清测试结果如下：";
    qDebug()<<"出清价格："<<clearresult.clearingprice;
    qDebug()<<"成交总电量："<<clearresult.totalvolume;
    qDebug()<<"出清过程如下：";
    for (int i=0;i<clearresult.trade.size(); ++i) {
        qDebug()<<clearresult.trade[i].generatorID<<"("<<clearresult.trade[i].generatorseg<<"):"
                 <<clearresult.trade[i].generatorprice<<"-->"<<clearresult.trade[i].consumerID<<"("<<clearresult.trade[i].consumerseg<<"):"
                 <<clearresult.trade[i].consumerprice;
        qDebug()<<"出清容量为："<<clearresult.trade[i].volume;
    }
    SettlementMode mode=SettlementMode::PAB;
    QVector<SettlementItem>settlement=settle(clearresult,mode);
    qDebug()<<"不同ID的交易总量如下";
    for(auto& settle:settlement)
    {
        qDebug()<<settle.id<<"的成交量为："<<settle.volume;
        if(settle.id[0]=="G")
        {
            qDebug()<<settle.id<<"的收入为："<<settle.amount;
        }
        else
        {
            qDebug()<<settle.id<<"的支出为："<<settle.amount;
        }
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
