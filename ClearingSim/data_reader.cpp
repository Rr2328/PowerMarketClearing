#include"data_reader.h"
#include<QFile>
#include<QDebug>
QVector<TimeMarketData>DataReader::readMarketData(const QString&filename1,const QString&filename2)
{
    QVector<TimeMarketData> result;
    QFile file1(filename1);
    QFile file2(filename2);
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
    qDebug() << "文件打开成功：" << filename1<<","<<filename2;
    QTextStream in(&file1);
    int linecount=0;
    bool firstline=true;
    while(!in.atEnd())
    {
        if(firstline)continue;
        QString line=in.readLine();
        QStringList data=line.split(",");
        linecount++;

    }
    qDebug()<<linecount;
    return result;
}