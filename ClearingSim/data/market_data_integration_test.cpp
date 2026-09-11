#include "data_reader.h"
#include "scenario_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>
// MarketData 集成测试 V1.3：校验 CSV → MarketData → PeriodScenario 全链路数据流转。
namespace
{
// 失败用例计数（全局变量，由 check 累加）。
int failedTests = 0;
// 断言并打印结果，失败时累加失败数。
void check(bool condition,const QString &testName)
{
    if (condition)
    {
        qInfo().noquote()<< "[PASS]"<< testName;
    }
    else
    {
        qCritical().noquote()<< "[FAIL]"<< testName;
        ++failedTests;
    }
}
// 自指定路径上溯查找仓库根目录。
QString searchRepoRoot(const QString &startPath)
{
    QDir dir(startPath);
    while (true)
    {
        if (dir.exists("ClearingSim") &&dir.exists("data/samples/scenario"))
        {
            return dir.absolutePath();
        }
        if (!dir.cdUp())
        {
            break;
        }
    }
    return QString();
}
// 依次从程序目录、工作目录、源码目录尝试定位仓库根目录。
QString findRepoRoot()
{
    QString root =searchRepoRoot(QCoreApplication::applicationDirPath());
    if (!root.isEmpty())
    {
        return root;
    }
    root =searchRepoRoot(QDir::currentPath());
    if (!root.isEmpty())
    {
        return root;
    }
    const QFileInfo sourceFile(QString::fromUtf8(__FILE__));
    return searchRepoRoot(sourceFile.absolutePath());
}
// 按时段数生成一组四文件路径。
DataFileSet makeFileSet(const QString &scenarioDir,int periodCount)
{
    // V1.3 契约：96 为唯一原生粒度，样例仅一份长表；
    //   periodCount 参数保留兼容旧调用点，24 时段视图由聚合合成（见 main）
    Q_UNUSED(periodCount);
    DataFileSet files;
    files.generatorBidsFile =QDir(scenarioDir).filePath("generator_bids.csv");
    files.consumerBidsFile =QDir(scenarioDir).filePath("consumer_bids.csv");
    files.loadCurveFile =QDir(scenarioDir).filePath("load_curve.csv");
    files.renewableOutputFile =QDir(scenarioDir).filePath("renewable_output.csv");
    return files;
}

// （合并适配，V1.3 契约 §7.2）24 时段视图由 96 期数据聚合生成：
//   负荷/新能源用 ScenarioManager 的 96→24 聚合；发电/购电申报按
//   (t-1)/4+1 映射到小时，量与价取 4 期均值，主体身份 = (name,id)。
bool aggregateMarketDataTo24(const MarketData &src,MarketData &dst,QStringList &errors)
{
    dst =MarketData();
    if (!ScenarioManager::aggregateLoadTo24(src.loadCurve,dst.loadCurve,errors))
    {
        return false;
    }
    if (!ScenarioManager::aggregateRenewableTo24(src.renewableOutputs,dst.renewableOutputs,errors))
    {
        return false;
    }
    struct HourBid
    {
        QString name;
        QString id;
        int period =0;
        double price =0.0;
        double quantity =0.0;
        int count =0;
    };
    QVector<HourBid> genAcc;
    for (const GeneratorBid &bid :src.generatorBids)
    {
        if (bid.period <1 ||bid.period >96)
        {
            errors.append(QString("发电申报出现非法时段 %1").arg(bid.period));
            return false;
        }
        const int hour =(bid.period -1)/4 +1;
        HourBid *acc =nullptr;
        for (HourBid &item :genAcc)
        {
            if (item.name ==bid.name &&item.id ==bid.id &&item.period ==hour)
            {
                acc =&item;
                break;
            }
        }
        if (!acc)
        {
            HourBid fresh;
            fresh.name =bid.name;
            fresh.id =bid.id;
            fresh.period =hour;
            genAcc.append(fresh);
            acc =&genAcc.last();
        }
        acc->price +=bid.price;
        acc->quantity +=bid.quantity;
        ++acc->count;
    }
    for (const HourBid &item :genAcc)
    {
        GeneratorBid bid;
        bid.name =item.name;
        bid.id =item.id;
        bid.period =item.period;
        bid.price =item.count >0 ?item.price /item.count :0.0;
        bid.quantity =item.count >0 ?item.quantity /item.count :0.0;
        dst.generatorBids.append(bid);
    }
    QVector<HourBid> conAcc;
    for (const ConsumerBid &bid :src.consumerBids)
    {
        if (bid.period <1 ||bid.period >96)
        {
            errors.append(QString("购电申报出现非法时段 %1").arg(bid.period));
            return false;
        }
        const int hour =(bid.period -1)/4 +1;
        HourBid *acc =nullptr;
        for (HourBid &item :conAcc)
        {
            if (item.name ==bid.name &&item.id ==bid.id &&item.period ==hour)
            {
                acc =&item;
                break;
            }
        }
        if (!acc)
        {
            HourBid fresh;
            fresh.name =bid.name;
            fresh.id =bid.id;
            fresh.period =hour;
            conAcc.append(fresh);
            acc =&conAcc.last();
        }
        acc->price +=bid.price;
        acc->quantity +=bid.quantity;
        ++acc->count;
    }
    for (const HourBid &item :conAcc)
    {
        ConsumerBid bid;
        bid.name =item.name;
        bid.id =item.id;
        bid.period =item.period;
        bid.price =item.count >0 ?item.price /item.count :0.0;
        bid.quantity =item.count >0 ?item.quantity /item.count :0.0;
        dst.consumerBids.append(bid);
    }
    return true;
}
// 输出错误明细。
void printErrors(const QStringList &errors)
{
    for (const QString &error : errors)
    {
        qInfo().noquote()<< "   "<< error;
    }
}
// 提取发电侧机组 ID 集合。
QSet<QString> generatorIds(const QVector<GeneratorBid> &data)
{
    QSet<QString> ids;
    for (const GeneratorBid &bid : data)
    {
        ids.insert(bid.id);
    }
    return ids;
}
// 提取用户侧用户 ID 集合。
QSet<QString> consumerIds(const QVector<ConsumerBid> &data)
{
    QSet<QString> ids;
    for (const ConsumerBid &bid : data)
    {
        ids.insert(bid.id);
    }
    return ids;
}
// 提取新能源机组 ID 集合。
QSet<QString> renewableIds(const QVector<RenewableOutput> &data)
{
    QSet<QString> ids;
    for (const RenewableOutput &item : data)
    {
        ids.insert(item.generatorId);
    }
    return ids;
}
// 统计指定时段的发电申报条数。
int countGeneratorBids(const MarketData &data,int period)
{
    int count = 0;
    for (const GeneratorBid &bid : data.generatorBids)
    {
        if (bid.period ==period)
        {
            ++count;
        }
    }
    return count;
}
// 统计指定时段的用户申报条数。
int countConsumerBids(const MarketData &data,int period)
{
    int count = 0;
    for (const ConsumerBid &bid : data.consumerBids)
    {
        if (bid.period ==period)
        {
            ++count;
        }
    }
    return count;
}
// 校验单时段场景的申报数量与 MarketData 中该时段一致。
bool scenarioMatchesMarketData(const PeriodScenario &scenario,const MarketData &data)
{
    return scenario.generatorBids.size() ==countGeneratorBids(data,scenario.period) &&scenario.consumerBids.size() ==countConsumerBids(data,scenario.period);
}
// 跑通「MarketData → PeriodScenario」链路测试（数据由调用方准备：
//   96 期为 V1.3 原生长表；24 期为聚合合成视图，见 aggregateMarketDataTo24）。
bool pipelineFromData(const MarketData &data,int periodCount,TimeGranularity granularity)
{
    qInfo().noquote()<< "-----"<< periodCount<< "period pipeline -----";
    QStringList errors;
    bool ok =false;
    check(data.loadCurve.size() ==periodCount,QString("%1 时段负荷数量正确").arg(periodCount));
    const QSet<QString> genIds =generatorIds(data.generatorBids);
    const QSet<QString> conIds =consumerIds(data.consumerBids);
    const QSet<QString> renIds =renewableIds(data.renewableOutputs);
    check(!genIds.isEmpty(),QString("%1 时段包含发电主体").arg(periodCount));
    check(!conIds.isEmpty(),QString("%1 时段包含购电主体").arg(periodCount));
    check(!renIds.isEmpty(),QString("%1 时段包含新能源主体").arg(periodCount));
    QVector<PeriodScenario> scenarios;
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(data,granularity,scenarios,errors);
    check(ok,QString("%1 时段 MarketData → PeriodScenario").arg(periodCount));
    if (!ok)
    {
        printErrors(errors);
        return false;
    }
    check(scenarios.size() ==periodCount,QString("%1 时段场景数量正确").arg(periodCount));
    if (!scenarios.isEmpty())
    {
        const PeriodScenario &first =scenarios.first();
        const PeriodScenario &last =scenarios.last();
        check(first.period == 1 &&last.period == periodCount,QString("%1 时段场景编号完整").arg(periodCount));
        check(scenarioMatchesMarketData(first,data),QString("%1 时段第1场景申报数量与 MarketData 一致").arg(periodCount));
        check(scenarioMatchesMarketData(last,data),QString("%1 时段最后场景申报数量与 MarketData 一致").arg(periodCount));
        check(!first.generatorBids.isEmpty() &&!first.consumerBids.isEmpty(),QString("%1 时段单场景可直接提供给算法").arg(periodCount));
    }
    // 校验 MarketData 可完整拷贝。
    MarketData copiedData =data;
    check(copiedData.generatorBids.size() ==data.generatorBids.size() &&copiedData.consumerBids.size() ==data.consumerBids.size() &&copiedData.loadCurve.size() ==data.loadCurve.size() &&copiedData.renewableOutputs.size() ==data.renewableOutputs.size(),QString("%1 时段 MarketData 可完整复制").arg(periodCount));
    return true;
}
} // namespace
int main(int argc,char *argv[])
{
    QCoreApplication app(argc,argv);
    qInfo().noquote()<< "========== MarketData Integration V1.3 Test ==========";
    // 定位仓库根目录，失败则直接终止。
    const QString repoRoot =findRepoRoot();
    check(!repoRoot.isEmpty(),"定位仓库根目录");
    if (repoRoot.isEmpty())
    {
        return 1;
    }
    qInfo().noquote()<< "Repository root:"<< QDir::toNativeSeparators(repoRoot);
    // 定位 scenario 场景数据目录，不存在则终止。
    const QString scenarioDir =QDir(repoRoot).filePath("data/samples/scenario");
    check(QDir(scenarioDir).exists(),"定位 scenario 场景目录");
    if (!QDir(scenarioDir).exists())
    {
        return 1;
    }
    // 96 期原生链路（V1.3 长表：CSV → DataReader → 场景）。
    MarketData data96;
    QStringList errors;
    bool ok =DataReader::readAll(makeFileSet(scenarioDir,96),data96,errors);
    check(ok,"96 时段 CSV → MarketData");
    if (ok)
    {
        ok =pipelineFromData(data96,96,TimeGranularity::QuarterHourly96);
    }
    else
    {
        printErrors(errors);
    }
    // 24 时段视图链路：由 96 期聚合合成（V1.3 契约 §7.2，合并适配：
    //   原实现读取 24 时段原生样例，该路径已随 V1.3 契约移除）。
    if (ok)
    {
        MarketData data24;
        errors.clear();
        ok =aggregateMarketDataTo24(data96,data24,errors);
        check(ok,"96 → 24 聚合合成 24 时段视图");
        if (ok)
        {
            ok =pipelineFromData(data24,24,TimeGranularity::Hourly24);
        }
        else
        {
            printErrors(errors);
        }
    }
    // 输出汇总，失败数决定退出码。
    qInfo().noquote()<< "===============================================";
    if (failedTests == 0)
    {
        qInfo().noquote()<< "All MarketData Integration V1.3 tests passed.";
        return 0;
    }
    qCritical().noquote()<< failedTests<< "test(s) failed.";
    return 1;
}