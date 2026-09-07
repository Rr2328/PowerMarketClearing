#include"data_reader.h"
#include<QFile>
#include<QDebug>
QVector<TimeMarketData>DataReader::readMarketData(const QString&filename1,const QString&filename2,const QString&filename3,const QString& filename4)
{
    QVector<TimeMarketData> result;
    QFile file1(filename1);
    QFile file2(filename2);
    QFile file3(filename3);
    QFile file4(filename4);
    if(!file1.open(QIODevice::ReadOnly))
    {
        qDebug()<<filename1<<"文件打开失败！";
        return result;
    }
    if(!file2.open(QIODevice::ReadOnly))
    {
        qDebug()<<filename2<<"文件打开失败！";
        return result;
    }
    if(!file3.open(QIODevice::ReadOnly))
    {
        qDebug()<<filename3<<"文件打开失败！";
        return result;
    }
    if(!file4.open(QIODevice::ReadOnly))
    {
        qDebug() << filename4 << "文件打开失败！";
        return result;
    }
    qDebug() << "文件打开成功：" << filename1<<","<<filename2<<","<<filename3<<filename4;
    QTextStream genin(&file1);
    int genlinecount=0;
    QString genline=genin.readLine();
    while(!genin.atEnd())
    {
        QString genline=genin.readLine();
        QStringList gendata=genline.split(",");
        genlinecount++;
        TimeMarketData dataresult;
        bool ok=true;
        int period=gendata[0].toInt(&ok);
        if (!ok)
        {
            qDebug()<<"generator_bids.csv的period读取失败："<<gendata[0];
            continue;
        }
        QVector<Generator>generators;
        for(int i=4,j=1;i<gendata.size()&&gendata[i]!=nullptr;i+=2,j++)
        {
            Generator gens;
            bool oi=true;bool oj=true;
            gens.period=period;
            gens.id=gendata[2];
            gens.name=gendata[1];
            gens.type=gendata[3];
            gens.segment=j;
            gens.quantity=gendata[i].toDouble(&oi);
            gens.price=gendata[i+1].toDouble(&oj);
            if(!oi)
            {
                qDebug()<<"时段"<<gendata[0]<<"中的"<<gens.id<<"中的第"<<gens.segment<<"段的申报容量读取失败："<<gendata[i];
                continue;
            }
            if(!oj)
            {
                qDebug()<<"时段"<<gendata[0]<<"中的"<<gens.id<<"中的第"<<gens.segment<<"段的申报价格读取失败："<<gendata[i+1];
                continue;
            }
            generators.append(gens);
        }
        int k=0;
        while(k<result.size()&&result[k].period!=period)k++;
        if(k<result.size())result[k].generators+=generators;
        else
        {
            dataresult.period=period;
            dataresult.generators=generators;
            result.append(dataresult);
        }
    }
    qDebug()<<"generator_bid.csv真确读取的行数为："<<genlinecount;
    int conlinecount=0;
    QTextStream conin(&file2);
    QString conline=conin.readLine();
    while(!conin.atEnd())
    {
        QString conline=conin.readLine();
        QStringList condata=conline.split(",");
        conlinecount++;
        bool ok=true;
        int period=condata[0].toInt(&ok);
        if (!ok)
        {
            qDebug()<<"consumer_bids.csv的period读取失败："<<condata[0];
            continue;
        }
        QVector<Consumer>consumers;
        for(int i=3,j=1;i<condata.size()&&condata[i]!=nullptr;i+=2,j++)
        {
            Consumer cons;
            bool oi=true;bool oj=true;
            cons.period=period;
            cons.id=condata[2];
            cons.name=condata[1];
            cons.segment=j;
            cons.quantity=condata[i].toDouble(&oi);
            cons.price=condata[i+1].toDouble(&oj);
            if(!oi)
            {
                qDebug()<<"时段"<<condata[0]<<"中的"<<cons.id<<"中的第"<<cons.segment<<"段的需求容量读取失败："<<condata[i];
                continue;
            }
            if(!oj)
            {
                qDebug()<<"时段"<<condata[0]<<"中的"<<cons.id<<"中的第"<<cons.segment<<"段的价格读取失败："<<condata[i+1];
                continue;
            }
            consumers.append(cons);
        }
        int k=0;
        while(k<result.size()&&result[k].period!=period)k++;
        if(k<result.size())result[k].consumers+=consumers;
        else qDebug()<<"出现了用户测多了时段的情况！";
    }
    qDebug()<<"conerator_bid.csv真确读取的行数为："<<conlinecount;
    int renlinecount=0;
    QTextStream renin(&file3);
    QString renline=renin.readLine();
    while(!renin.atEnd())
    {
        QString renline=renin.readLine();
        QStringList rendata=renline.split(",");
        renlinecount++;
        bool ok=true;
        int period=rendata[0].toInt(&ok);
        if (!ok)
        {
            qDebug()<<"consumer_bids.csv的period读取失败："<<rendata[0];
            continue;
        }
        QVector<RenewableBase>renewablebase;
        for(int i=4;i<rendata.size()&&rendata[i]!=nullptr;i++)
        {
            RenewableBase rens;
            bool oi=true;bool oj=true;
            rens.period=period;
            rens.id=rendata[2];
            rens.name=rendata[1];
            rens.output=rendata[i].toDouble(&oi);
            rens.type=rendata[3];
            if(!oi)
            {
                qDebug()<<"时段"<<rendata[0]<<"中的"<<rens.id<<"需求容量读取失败："<<rendata[i];
                continue;
            }
            if(!oj)
            {
                qDebug()<<"时段"<<rendata[0]<<"中的"<<rens.id<<"价格读取失败："<<rendata[i+1];
                continue;
            }
            renewablebase.append(rens);
        }
        int k=0;
        while(k<result.size()&&result[k].period!=period)
        {
            k++;
        }
        if(k<result.size())result[k].renewbaleBase+=renewablebase;
        else qDebug()<<"出现了用户测多了时段的情况！";
    }
    qDebug()<<"renerator_bid.csv真确读取的行数为："<<renlinecount;
    QTextStream loadin(&file4);

    // 跳过表头
    QString loadline = loadin.readLine();

    int loadlinecount = 0;

    while(!loadin.atEnd())
    {
        loadline = loadin.readLine();

        if(loadline.trimmed().isEmpty())
            continue;

        QStringList loaddata = loadline.split(",");

        if(loaddata.size() < 3)
        {
            qDebug() << "负荷曲线数据格式错误：" << loadline;
            continue;
        }

        bool periodOk = false;
        bool loadOk = false;

        int period =
            loaddata[0].toInt(&periodOk);

        double loadMW =
            loaddata[2].toDouble(&loadOk);

        if(!periodOk)
        {
            qDebug()
            << "负荷曲线period读取失败："
            << loaddata[0];

            continue;
        }

        if(!loadOk)
        {
            qDebug()
            << "第" << period
            << "时段负荷读取失败："
            << loaddata[2];

            continue;
        }

        int k = 0;

        while(k < result.size() &&
               result[k].period != period)
        {
            k++;
        }

        if(k < result.size())
        {
            result[k].time = loaddata[1];
            result[k].loadMW = loadMW;
        }
        else
        {
            qDebug()
            << "负荷曲线存在额外时段："
            << period;
        }

        loadlinecount++;
    }

    qDebug()
        << "负荷曲线正确读取行数："
        << loadlinecount;
    return result;
}