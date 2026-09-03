#include "mainwindow.h"
#include"clearing_engine.h"
#include"data_reader.h"
#include"market_runner.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QVector<TimeMarketData> daydata;
    DataReader datareader;
    daydata=datareader.readMarketData("generator_bids_24period.csv","consumer_bids_24period.csv");
    SettlementMode mode=SettlementMode::MCP;
    DayResult dayresult=run96market(daydata,mode);
    for(auto& result:dayresult.result)
    {
        qDebug()<<"————————————第"<<result.period<<"时段出清测试结果——————————";
        qDebug()<<"出清价格："<<result.result.clearingprice;
        qDebug()<<"成交总电量："<<result.result.totalvolume;
        qDebug()<<"出清过程如下：";
        for (auto& trade:result.result.trade) {
            qDebug()<<trade.generatorID<<"("<<trade.generatorseg<<"):"
                     <<trade.generatorprice<<"-->"<<trade.consumerID<<"("<<trade.consumerseg<<"):"
                     <<trade.consumerprice;
            qDebug()<<"出清容量为："<<trade.volume;
        }
        qDebug()<<"不同ID的交易总量如下";
        if(result.settlement.empty())
        {
            qDebug()<<"未形成任何交易。";
        }
        else
        {
            for(auto& settle:result.settlement)
            {
                qDebug()<<settle.id<<"的成交量为："<<settle.volume;
                if(settle.id[0]=="G"||settle.id[0]=="W"||settle.id[0]=="S")
                {
                    qDebug()<<settle.id<<"的收入为："<<settle.amount;
                }
                else
                {
                    qDebug()<<settle.id<<"的支出为："<<settle.amount;
                }
            }
        }
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
