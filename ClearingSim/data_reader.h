#ifndef DATA_READER_H
#define DATA_READER_H
#include"market_runner.h"
#include<QVector>
#include<QString>
class DataReader
{
public:
    QVector<TimeMarketData>readMarketData(const QString&filename1,const QString&filename2,const QString&filename3,const QString& filename4);
};

#endif // DATA_READER_H
