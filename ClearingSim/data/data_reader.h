#ifndef DATA_READER_H
#define DATA_READER_H

#include <QString>
#include <QStringList>
#include <QVector>

// ============================================================
// A 数据管理模块
//
// 作用：
// 把 CSV 中的文本数据读取并转换为程序可以直接使用的数据结构。
// B 的算法模块、C 的界面模块以后可以直接使用这些结构。
// ============================================================


// ============================================================
// 1. 发电机组申报
//
// generator_bids.csv
//
// 机组ID,机组名称,机组类型,申报段,
// 申报电价(元/MWh),申报电量(MWh)
// ============================================================

struct GeneratorBid
{
    QString id;
    QString name;
    QString type;

    int segment;

    double price;
    double quantity;

    GeneratorBid()
        : segment(0),
        price(0.0),
        quantity(0.0)
    {
    }
};


// ============================================================
// 2. 用户申报
//
// consumer_bids.csv
// ============================================================

struct ConsumerBid
{
    QString id;
    QString name;

    int segment;

    double price;
    double quantity;

    ConsumerBid()
        : segment(0),
        price(0.0),
        quantity(0.0)
    {
    }
};


// ============================================================
// 3. 96 时段负荷
//
// load_curve.csv
// ============================================================

struct LoadPoint
{
    int period;

    QString time;

    double load;

    LoadPoint()
        : period(0),
        load(0.0)
    {
    }
};


// ============================================================
// 4. 新能源实际出力
//
// renewable_output.csv
// ============================================================

struct RenewableOutput
{
    QString generatorId;

    QString generatorType;

    int period;

    double output;

    RenewableOutput()
        : period(0),
        output(0.0)
    {
    }
};


// ============================================================
// DataReader
//
// 所有 CSV 的统一入口。
// ============================================================

class DataReader
{
public:

    // --------------------------------------------------------
    // 读取机组申报
    // --------------------------------------------------------

    static bool readGeneratorBids(
        const QString& filePath,
        QVector<GeneratorBid>& data,
        QStringList& errors);


    // --------------------------------------------------------
    // 读取用户申报
    // --------------------------------------------------------

    static bool readConsumerBids(
        const QString& filePath,
        QVector<ConsumerBid>& data,
        QStringList& errors);


    // --------------------------------------------------------
    // 读取96时段负荷
    // --------------------------------------------------------

    static bool readLoadCurve(
        const QString& filePath,
        QVector<LoadPoint>& data,
        QStringList& errors);


    // --------------------------------------------------------
    // 读取新能源实际出力
    // --------------------------------------------------------

    static bool readRenewableOutput(
        const QString& filePath,
        QVector<RenewableOutput>& data,
        QStringList& errors);
};


#endif // DATA_READER_H