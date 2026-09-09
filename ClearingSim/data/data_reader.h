#ifndef DATA_READER_H
#define DATA_READER_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "../quadratic_clearing.h"

// 发电侧申报数据（V1.3 长表口径）
//   每条 = 电厂×机组 在某交易时段某申报段的申报；
//   主体身份 = (name=电厂名称, id=机组编号) 组合（机组编号可跨厂重号）。
//   V1.3 起无机组类型字段（D7）；新能源不进申报表（D4）。
struct GeneratorBid
{
    QString id;      // 机组编号（如 "#1机组"；窄表导入时为机组ID）
    QString name;    // 电厂名称（窄表导入时为机组名称）
    int period = 0;  // 交易时段 1~96（窄表导入自动展开为 96 期同量同价）

    int segment = 0;

    double price = 0.0;
    double quantity = 0.0;   // ≥ 0，0 表示该时段该段不申报/停机（规则⑥）
};

// 用户侧申报数据（V1.3 长表口径，主体身份 = (name=用户名称, id=负荷编号)）
struct ConsumerBid
{
    QString id;      // 负荷编号（窄表导入时为用户ID）
    QString name;    // 用户名称
    int period = 0;  // 交易时段 1~96

    int segment = 0;

    double price = 0.0;
    double quantity = 0.0;
};

// 负荷曲线数据
struct LoadPoint
{
    int period = 0;

    QString time;

    double load = 0.0;
};

// 新能源出力数据
struct RenewableOutput
{
    QString generatorId;
    QString generatorType;

    int period = 0;

    double output = 0.0;
};

// 数据文件路径
struct DataFileSet
{
    QString generatorBidsFile;
    QString consumerBidsFile;
    QString loadCurveFile;
    QString renewableOutputFile;
};

// 统一市场输入数据
struct MarketData
{
    QVector<GeneratorBid> generatorBids;
    QVector<ConsumerBid> consumerBids;
    QVector<LoadPoint> loadCurve;
    QVector<RenewableOutput> renewableOutputs;

    // 二次成本机组参数（可选：generator_quadratic.csv 提供时启用二次模式，
    // 选题 2026v2 (10) 问；行 = 机组，全天一条曲线，非逐时段申报）
    QVector<QuadraticGenerator> quadraticGens;

    void clear()
    {
        generatorBids.clear();
        consumerBids.clear();
        loadCurve.clear();
        renewableOutputs.clear();
        quadraticGens.clear();
    }
};

// CSV 数据读取接口
//   V1.3：申报表支持两种格式，按表头自动识别——
//   ① 长表（V1.3）：period 行 × 段成对列（第N段出力/第N段报价），双侧逐时段申报；
//   ② 窄表（V1.1/V1.2 兼容）：无 period 列，一行 = 主体×段；导入时自动展开为
//      96 个时段同量同价（等价于旧引擎"每时段恒量"，作新旧对拍锚点）。
//   旧文件中的「机组类型」列忽略、不校验（D7）。
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

    // 二次成本机组参数表（可选文件 generator_quadratic.csv）：
    //   表头 name,id,a,b,c,pMax；行 = 机组（全天一条曲线）。
    //   文件不存在时返回 false 且不记错误（二次模式仅是未启用）。
    static bool readQuadraticGenerators(
        const QString &filePath,
        QVector<QuadraticGenerator> &data,
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