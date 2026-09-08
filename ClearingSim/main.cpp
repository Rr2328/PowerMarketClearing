/*#include <QApplication>
#include <QDebug>
#include <QVector>

#include "mainwindow.h"
#include "data_reader.h"
#include "market_runner.h"
#include "quadratic_clearing.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    qDebug() << "==========================================";
    qDebug() << "24时段 二次成本 + 新能源 出清测试开始";
    qDebug() << "==========================================";


    // =====================================================
    // 1. 读取24时段市场数据
    // =====================================================

    DataReader datareader;

    QVector<TimeMarketData> daydata =
        datareader.readMarketData(
            "generator_bids_24period.csv",
            "consumer_bids_24period.csv",
            "renewable_output_24period.csv",
            "load_curve_24period.csv"
            );

    qDebug() << "读取到的时段数量：" << daydata.size();


    // =====================================================
    // 2. 创建二次成本常规机组
    // =====================================================

    QVector<QuadraticGenerator> generators;


    // ---------- G1 ----------
    // C1(P) = 0.05P^2 + 80P + 500
    // MC1(P) = 0.1P + 80

    QuadraticGenerator g1;

    g1.id = "G1";
    g1.name = "火电机组1";
    g1.type = "火电";

    g1.a = 0.05;
    g1.b = 80;
    g1.c = 500;

    g1.pMax = 400;

    generators.append(g1);


    // ---------- G2 ----------
    // C2(P) = 0.04P^2 + 90P + 600
    // MC2(P) = 0.08P + 90

    QuadraticGenerator g2;

    g2.id = "G2";
    g2.name = "火电机组2";
    g2.type = "火电";

    g2.a = 0.04;
    g2.b = 90;
    g2.c = 600;

    g2.pMax = 350;

    generators.append(g2);


    // ---------- G3 ----------
    // C3(P) = 0.03P^2 + 110P + 800
    // MC3(P) = 0.06P + 110

    QuadraticGenerator g3;

    g3.id = "G3";
    g3.name = "燃气机组";
    g3.type = "燃气";

    g3.a = 0.03;
    g3.b = 110;
    g3.c = 800;

    g3.pMax = 300;

    generators.append(g3);


    // =====================================================
    // 3. 设置新能源渗透率
    // =====================================================

    double penetration = 0.30;

    qDebug()
        << "新能源目标渗透率："
        << penetration * 100
        << "%";


    // =====================================================
    // 4. 运行24时段二次成本出清
    // =====================================================

    QuadraticDayResult dayResult =
        runQuadraticMarket(
            daydata,
            generators,
            penetration
            );


    // =====================================================
    // 5. 输出24时段结果
    // =====================================================

    for(const auto& period : dayResult.periods)
    {
        qDebug() << "";
        qDebug() << "==========================================";

        qDebug()
            << "时段：" << period.period;

        qDebug()
            << "系统负荷："
            << period.loadMW
            << "MW";

        qDebug()
            << "新能源实际出力："
            << period.renewableOutput
            << "MW";

        qDebug()
            << "常规机组净负荷："
            << period.netLoadMW
            << "MW";


        // -------------------------------------------------
        // 检查：新能源 + 净负荷 = 原始负荷
        // -------------------------------------------------

        double balance1 =
            period.renewableOutput
            + period.netLoadMW;

        qDebug()
            << "负荷平衡检查："
            << balance1
            << "/"
            << period.loadMW
            << "MW";


        // -------------------------------------------------
        // 输出二次成本出清结果
        // -------------------------------------------------

        qDebug()
            << "出清价格 MCP："
            << period.result.clearingPrice
            << "元/MWh";

        qDebug()
            << "常规机组总出力："
            << period.result.totalVolume
            << "MW";


        // -------------------------------------------------
        // 输出每台常规机组
        // -------------------------------------------------

        double conventionalTotal = 0.0;

        for(const auto& dispatch : period.result.dispatch)
        {
            qDebug()
            << "机组：" << dispatch.generatorId
            << "出力：" << dispatch.output << "MW"
            << "边际成本："
            << dispatch.marginalCost
            << "元/MWh";

            conventionalTotal += dispatch.output;
        }


        // -------------------------------------------------
        // 检查常规机组出力是否等于净负荷
        // -------------------------------------------------

        qDebug()
            << "常规机组平衡检查："
            << conventionalTotal
            << "/"
            << period.netLoadMW
            << "MW";


        // -------------------------------------------------
        // 最终系统功率平衡
        // -------------------------------------------------

        double systemTotal =
            conventionalTotal
            + period.renewableOutput;

        qDebug()
            << "系统总供给："
            << systemTotal
            << "/"
            << period.loadMW
            << "MW";
    }


    qDebug() << "";
    qDebug() << "==========================================";
    qDebug() << "24时段 二次成本 + 新能源 出清测试结束";
    qDebug() << "==========================================";


    // =====================================================
    // 6. 启动Qt主窗口
    // =====================================================

    MainWindow w;
    w.show();

    return a.exec();
}*/

#include "mainwindow.h"
#include"clearing_engine.h"
#include"data_reader.h"
#include"market_runner.h"
#include"quadratic_clearing.h"
#include <QApplication>
#include <QDebug>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QVector<TimeMarketData> daydata;
    DataReader datareader;
    daydata=datareader.readMarketData("generator_bids_24period.csv","consumer_bids_24period.csv","renewable_output_24period.csv","load_curve_24period.csv");
    SettlementMode mode=SettlementMode::MCP;
    double penetration=0.3;
    DayResult dayresult=runmarket(daydata,mode,penetration);
    for(auto& result:dayresult.result)
    {
        qDebug()<<"————————————第"<<result.period<<"时段出清测试结果——————————";
        qDebug()<<"出清价格："<<result.result.clearingprice;
        qDebug()<<"成交总电量："<<result.result.totalvolume;
        qDebug()<<"渗透率为："<<penetration;
        qDebug()<<"当前时间为："<<daydata[result.period-1].time<<"当前负荷为："<<daydata[result.period-1].loadMW;
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
