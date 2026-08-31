#include "mainwindow.h"
#include"clearing_engine.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QVector<Generator> generators = {
        {"G1", 100.0, 50.0, 1},
        {"G1", 180.0, 40.0, 2},
        {"G2", 150.0, 60.0, 1}
    };

    QVector<Consumer> consumers = {
        {"U1", 300.0, 70.0, 1},
        {"U1", 220.0, 50.0, 2}
    };

    ClearResult clearrulst=ClearMarket(generators,consumers);
    qDebug()<<"出清测试结果如下：";
    qDebug()<<"出清价格："<<clearrulst.clearingprice;
    qDebug()<<"成交总电量："<<clearrulst.totalvolume;
    qDebug()<<"出清过程如下：";
    for (int i=0;i<clearrulst.trade.size(); ++i) {
        qDebug()<<clearrulst.trade[i].generatorID<<"("<<clearrulst.trade[i].generatorseg<<"):"
                 <<clearrulst.trade[i].generatorprice<<"-->"<<clearrulst.trade[i].consumerID<<"("<<clearrulst.trade[i].consumerseg<<"):"
                 <<clearrulst.trade[i].consumerprice;
        qDebug()<<"出清容量为："<<clearrulst.trade[i].volume;
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
