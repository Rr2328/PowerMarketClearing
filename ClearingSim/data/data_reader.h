#ifndef DATA_READER_H
#define DATA_READER_H

#include <QString>
#include <QStringList>
#include <QVector>

// 发电侧申报数据
struct GeneratorBid
{
    QString id;
    QString name;
    QString type;

    int segment = 0;

    double price = 0.0;
    double quantity = 0.0;

    int period = 0;
};

// 用户侧申报数据
struct ConsumerBid
{
    QString id;
    QString name;

    int segment = 0;

    double price = 0.0;
    double quantity = 0.0;

    int period = 0;
};

// 系统负荷曲线
struct LoadPoint
{
    int period = 0;
    QString time;

    double load = 0.0;
};

// 新能源基准出力
struct RenewableOutput
{
    QString generatorId;
    QString generatorType;

    int period = 0;

    double output = 0.0;
};

// 一套场景对应的数据文件
struct DataFileSet
{
    QString generatorBidsFile;
    QString consumerBidsFile;
    QString loadCurveFile;
    QString renewableOutputFile;
};

// 数据管理层统一市场数据
struct MarketData
{
    QVector<GeneratorBid> generatorBids;
    QVector<ConsumerBid> consumerBids;
    QVector<LoadPoint> loadCurve;
    QVector<RenewableOutput> renewableOutputs;

    void clear()
    {
        generatorBids.clear();
        consumerBids.clear();
        loadCurve.clear();
        renewableOutputs.clear();
    }

    bool isEmpty() const
    {
        return generatorBids.isEmpty()
        && consumerBids.isEmpty()
            && loadCurve.isEmpty()
            && renewableOutputs.isEmpty();
    }
};

class DataReader
{
public:
    static bool readGeneratorBids(
        const QString &filePath,
        QVector<GeneratorBid> &data,
        QStringList &errors);

    static bool readConsumerBids(
        const QString &filePath,
        QVector<ConsumerBid> &data,
        QStringList &errors);

    static bool readLoadCurve(
        const QString &filePath,
        QVector<LoadPoint> &data,
        QStringList &errors);

    static bool readRenewableOutput(
        const QString &filePath,
        QVector<RenewableOutput> &data,
        QStringList &errors);

    static bool readAll(
        const DataFileSet &files,
        MarketData &data,
        QStringList &errors);

    static bool validateRelations(
        const MarketData &data,
        QStringList &errors);
};

#endif // DATA_READER_H