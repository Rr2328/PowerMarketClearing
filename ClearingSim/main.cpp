#include "mainwindow.h"
#include"clearing_engine.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QVector<Generator>generators;
    QVector<Consumer>consumers;
    generators.append({"G1",150,100});
    generators.append({"G2",150,80});
    generators.append({"G3",180,100});
    consumers.append({"C1",150,50});
    consumers.append({"C2",180,100});
    consumers.append({"C3",200,80});
    ClearResult clearrulst=ClearMarket(generators,consumers);
    qDebug()<<"出清测试结果如下：";
    qDebug()<<"出清价格："<<clearrulst.clearingprice;
    qDebug()<<"成交总电量："<<clearrulst.totalvolume;
    MainWindow w;
    w.show();
    return QApplication::exec();
}
