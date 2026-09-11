#include "data_reader.h"
#include "scenario_manager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <cmath>
// ScenarioManager V1.4 单元测试：校验时段场景构建与 96→24 聚合
namespace
{
// 失败用例计数
int failedTests = 0;
// 断言并打印结果，失败时累加失败数
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
// 自指定路径上溯查找仓库根目录
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
// 依次从程序目录、工作目录、源码目录尝试定位仓库根目录
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
// 按时段数生成一组四文件路径
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
// 输出错误明细
void printErrors(const QStringList &errors)
{
    for (const QString &error : errors)
    {
        qInfo().noquote()<< "   "<< error;
    }
}
// 按机组 ID 与时段查找新能源出力记录
bool findRenewable(const QVector<RenewableOutput> &data,const QString &generatorId,int period,RenewableOutput &result)
{
    for (const RenewableOutput &item : data)
    {
        if (item.generatorId ==generatorId &&item.period ==period)
        {
            result = item;
            return true;
        }
    }
    return false;
}
// 提取新能源机组 ID 集合
QSet<QString> renewableIds(const QVector<RenewableOutput> &data)
{
    QSet<QString> ids;
    for (const RenewableOutput &item : data)
    {
        ids.insert(item.generatorId);
    }
    return ids;
}
// 统计指定时段的发电申报条数
int countGeneratorBids(const QVector<GeneratorBid> &data,int period)
{
    int count = 0;
    for (const GeneratorBid &bid : data)
    {
        if (bid.period == period)
        {
            ++count;
        }
    }
    return count;
}
// 统计指定时段的用户申报条数
int countConsumerBids(const QVector<ConsumerBid> &data,int period)
{
    int count = 0;
    for (const ConsumerBid &bid : data)
    {
        if (bid.period == period)
        {
            ++count;
        }
    }
    return count;
}
// 统计指定时段的新能源出力条数
int countRenewables(const QVector<RenewableOutput> &data,int period)
{
    int count = 0;
    for (const RenewableOutput &item : data)
    {
        if (item.period == period)
        {
            ++count;
        }
    }
    return count;
}
// 校验场景中的发电申报全部属于本时段
bool allGeneratorBidsBelongToPeriod(const PeriodScenario &scenario)
{
    if (scenario.generatorBids.isEmpty())
    {
        return false;
    }
    for (const GeneratorBid &bid : scenario.generatorBids)
    {
        if (bid.period !=scenario.period)
        {
            return false;
        }
    }
    return true;
}
// 校验场景中的购电申报全部属于本时段
bool allConsumerBidsBelongToPeriod(const PeriodScenario &scenario)
{
    if (scenario.consumerBids.isEmpty())
    {
        return false;
    }
    for (const ConsumerBid &bid : scenario.consumerBids)
    {
        if (bid.period !=scenario.period)
        {
            return false;
        }
    }
    return true;
}
// 校验场景中的新能源数据全部属于本时段
bool allRenewablesBelongToPeriod(const PeriodScenario &scenario)
{
    for (const RenewableOutput &item : scenario.renewableBase)
    {
        if (item.period !=scenario.period)
        {
            return false;
        }
    }
    return true;
}
// 比较两条发电申报的关键字段是否一致
bool sameGeneratorBid(const GeneratorBid &a,const GeneratorBid &b)
{
    return a.id == b.id &&a.name == b.name &&a.segment == b.segment &&std::abs(a.price -b.price) <0.000001 &&std::abs(a.quantity -b.quantity) <0.000001 &&a.period == b.period;
}
// 比较两条购电申报的关键字段是否一致
bool sameConsumerBid(const ConsumerBid &a,const ConsumerBid &b)
{
    return a.id == b.id &&a.name == b.name &&a.segment == b.segment &&std::abs(a.price -b.price) <0.000001 &&std::abs(a.quantity -b.quantity) <0.000001 &&a.period == b.period;
}
// 判断发电申报集合中是否包含目标申报
bool containsGeneratorBid(const QVector<GeneratorBid> &data,const GeneratorBid &target)
{
    for (const GeneratorBid &bid : data)
    {
        if (sameGeneratorBid(bid,target))
        {
            return true;
        }
    }
    return false;
}
// 判断购电申报集合中是否包含目标申报
bool containsConsumerBid(const QVector<ConsumerBid> &data,const ConsumerBid &target)
{
    for (const ConsumerBid &bid : data)
    {
        if (sameConsumerBid(bid,target))
        {
            return true;
        }
    }
    return false;
}
} // namespace
int main(int argc,char *argv[])
{
    QCoreApplication app(argc,argv);
    qInfo().noquote()<< "========== ScenarioManager V1.4 Test ==========";
    // 定位仓库根目录，失败则直接终止
    const QString repoRoot =findRepoRoot();
    check(!repoRoot.isEmpty(),"定位仓库根目录");
    if (repoRoot.isEmpty())
    {
        return 1;
    }
    qInfo().noquote()<< "Repository root:"<< QDir::toNativeSeparators(repoRoot);
    // 定位 scenario 场景数据目录，不存在则终止
    const QString scenarioDir =QDir(repoRoot).filePath("data/samples/scenario");
    check(QDir(scenarioDir).exists(),"定位 scenario 场景数据目录");
    if (!QDir(scenarioDir).exists())
    {
        return 1;
    }
    qInfo().noquote()<< "Scenario directory:"<< QDir::toNativeSeparators(scenarioDir);
    // 读取 96 时段数据（V1.3 契约：96 为唯一原生粒度，样例为长表）
    MarketData data96;
    QStringList errors;
    bool ok =DataReader::readAll(makeFileSet(scenarioDir,96),data96,errors);
    check(ok,"读取 96 时段基础数据");
    if (!ok)
    {
        printErrors(errors);
        return 1;
    }
    // 24 时段视图：V1.3 契约 §7.2 下 24 不再是原生粒度，
    //   由 96 期数据经 ScenarioManager 聚合合成
    //   （合并适配：原实现读取 24 时段原生样例，该路径已随 V1.3 契约移除）
    MarketData data24;
    ok =aggregateMarketDataTo24(data96,data24,errors);
    check(ok,"96 → 24 聚合合成 24 时段视图");
    if (!ok)
    {
        printErrors(errors);
        return 1;
    }
    // 96 → 24 负荷聚合
    QVector<LoadPoint> load24From96;
    errors.clear();
    ok =ScenarioManager::aggregateLoadTo24(data96.loadCurve,load24From96,errors);
    check(ok,"96→24 负荷聚合工具正常");
    if (!ok)
    {
        printErrors(errors);
    }
    check(load24From96.size() == 24,"聚合后负荷数量为 24");
    if (ok &&data96.loadCurve.size() >= 4 &&!load24From96.isEmpty())
    {
        const double expected =(data96.loadCurve[0].load +data96.loadCurve[1].load +data96.loadCurve[2].load +data96.loadCurve[3].load) / 4.0;
        check(std::abs(load24From96[0].load -expected) <0.000001,"96→24 负荷采用相邻 4 点平均");
        check(load24From96[0].period == 1 &&load24From96[23].period == 24,"96→24 负荷聚合后的 period 正确");
    }
    // 96 → 24 新能源聚合
    QVector<RenewableOutput> renewable24From96;
    errors.clear();
    ok =ScenarioManager::aggregateRenewableTo24(data96.renewableOutputs,renewable24From96,errors);
    check(ok,"96→24 新能源聚合工具正常");
    if (!ok)
    {
        printErrors(errors);
    }
    const QSet<QString> renewableIds96 =renewableIds(data96.renewableOutputs);
    check(renewable24From96.size() ==renewableIds96.size() * 24,"聚合后新能源数量正确");
    if (ok &&!renewableIds96.isEmpty())
    {
        const QString id =*renewableIds96.constBegin();
        double totalOutput = 0.0;
        bool sourceComplete = true;
        QString expectedType;
        for (int period = 1;period <= 4;++period)
        {
            RenewableOutput item;
            if (!findRenewable(data96.renewableOutputs,id,period,item))
            {
                sourceComplete = false;
                break;
            }
            totalOutput +=item.output;
            expectedType =item.generatorType;
        }
        RenewableOutput aggregated;
        const bool found =findRenewable(renewable24From96,id,1,aggregated);
        check(sourceComplete &&found &&std::abs(aggregated.output -totalOutput / 4.0) <0.000001,"96→24 新能源按机组采用相邻 4 点平均");
        check(found &&aggregated.generatorId ==id &&aggregated.generatorType ==expectedType &&aggregated.period == 1,"新能源聚合后机组信息保持正确");
    }
    // 构建 24 时段场景
    QVector<PeriodScenario> scenarios24;
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(data24,TimeGranularity::Hourly24,scenarios24,errors);
    check(ok,"直接构建 24 时段场景");
    if (!ok)
    {
        printErrors(errors);
    }
    check(scenarios24.size() == 24,"24 时段场景数量正确");
    if (ok &&scenarios24.size() == 24)
    {
        const PeriodScenario &first =scenarios24.first();
        const PeriodScenario &last =scenarios24.last();
        check(first.period == 1 &&last.period == 24,"24 时段场景 period 范围正确");
        check(std::abs(first.intervalHours -1.0) <0.000001,"24 时段长度为 1 小时");
        check(std::abs(first.loadMW -data24.loadCurve[0].load) <0.000001,"24 时段场景 loadMW 与负荷数据一致");
        check(first.time ==data24.loadCurve[0].time,"24 时段场景 time 与负荷数据一致");
        check(first.generatorBids.size() ==countGeneratorBids(data24.generatorBids,1),"24 时段场景发电申报数量正确");
        check(first.consumerBids.size() ==countConsumerBids(data24.consumerBids,1),"24 时段场景购电申报数量正确");
        check(first.renewableBase.size() ==countRenewables(data24.renewableOutputs,1),"24 时段场景新能源数量正确");
        check(allGeneratorBidsBelongToPeriod(first),"24 时段场景只包含本时段发电申报");
        check(allConsumerBidsBelongToPeriod(first),"24 时段场景只包含本时段购电申报");
        check(allRenewablesBelongToPeriod(first),"24 时段场景只包含本时段新能源数据");
        if (!first.generatorBids.isEmpty())
        {
            check(containsGeneratorBid(data24.generatorBids,first.generatorBids.first()),"24 时段发电申报字段未在场景构建中丢失");
        }
        if (!first.consumerBids.isEmpty())
        {
            check(containsConsumerBid(data24.consumerBids,first.consumerBids.first()),"24 时段购电申报字段未在场景构建中丢失");
        }
    }
    // 构建 96 时段场景
    QVector<PeriodScenario> scenarios96;
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(data96,TimeGranularity::QuarterHourly96,scenarios96,errors);
    check(ok,"直接构建 96 时段场景");
    if (!ok)
    {
        printErrors(errors);
    }
    check(scenarios96.size() == 96,"96 时段场景数量正确");
    if (ok &&scenarios96.size() == 96)
    {
        const PeriodScenario &first =scenarios96.first();
        const PeriodScenario &middle =scenarios96[36];
        const PeriodScenario &last =scenarios96.last();
        check(first.period == 1 &&middle.period == 37 &&last.period == 96,"96 时段场景 period 范围正确");
        check(std::abs(first.intervalHours -0.25) <0.000001,"96 时段长度为 0.25 小时");
        check(std::abs(middle.loadMW -data96.loadCurve[36].load) <0.000001,"第 37 时段 loadMW 正确");
        check(middle.time ==data96.loadCurve[36].time,"第 37 时段 time 正确");
        check(middle.generatorBids.size() ==countGeneratorBids(data96.generatorBids,37),"第 37 时段发电申报数量正确");
        check(middle.consumerBids.size() ==countConsumerBids(data96.consumerBids,37),"第 37 时段购电申报数量正确");
        check(middle.renewableBase.size() ==countRenewables(data96.renewableOutputs,37),"第 37 时段新能源数量正确");
        check(allGeneratorBidsBelongToPeriod(middle),"第 37 时段只包含 period=37 的发电申报");
        check(allConsumerBidsBelongToPeriod(middle),"第 37 时段只包含 period=37 的购电申报");
        check(allRenewablesBelongToPeriod(middle),"第 37 时段只包含 period=37 的新能源数据");
    }
    // 异常用例复用的场景输出容器
    QVector<PeriodScenario> invalidScenarios;
    // 24 / 96 数据模式不能混用
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(data24,TimeGranularity::QuarterHourly96,invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别 24 时段数据误用 96 时段模式");
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(data96,TimeGranularity::Hourly24,invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别 96 时段数据误用 24 时段模式");
    // 非法颗粒度
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(data96,static_cast<TimeGranularity>(48),invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别非法时段颗粒度");
    // 构建场景时负荷缺失
    MarketData brokenLoad =data96;
    if (!brokenLoad.loadCurve.isEmpty())
    {
        brokenLoad.loadCurve.removeLast();
    }
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(brokenLoad,TimeGranularity::QuarterHourly96,invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别 96 时段负荷缺失");
    // 构建场景时新能源数据非法
    //   （合并适配：V1.3 下新能源形状为可选数据，缺行不报错——
    //     渗透率换算可兜底；故改为破坏时段合法性，仍须被识别拒绝）
    MarketData brokenRenewable =data96;
    if (!brokenRenewable.renewableOutputs.isEmpty())
    {
        brokenRenewable.renewableOutputs.last().period =0;
    }
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(brokenRenewable,TimeGranularity::QuarterHourly96,invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别 96 时段新能源缺失");
    // 构建场景时发电申报缺失一个时段
    MarketData brokenGenerator =data96;
    for (int i =brokenGenerator.generatorBids.size() - 1;i >= 0;--i)
    {
        if (brokenGenerator.generatorBids[i].period == 96)
        {
            brokenGenerator.generatorBids.removeAt(i);
        }
    }
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(brokenGenerator,TimeGranularity::QuarterHourly96,invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别发电侧申报时段缺失");
    // 构建场景时购电申报缺失一个时段
    MarketData brokenConsumer =data96;
    for (int i =brokenConsumer.consumerBids.size() - 1;i >= 0;--i)
    {
        if (brokenConsumer.consumerBids[i].period == 96)
        {
            brokenConsumer.consumerBids.removeAt(i);
        }
    }
    errors.clear();
    ok =ScenarioManager::buildPeriodScenarios(brokenConsumer,TimeGranularity::QuarterHourly96,invalidScenarios,errors);
    check(!ok &&!errors.isEmpty(),"识别购电侧申报时段缺失");
    // 96→24 负荷工具拒绝数量不足
    QVector<LoadPoint> shortLoad =data96.loadCurve;
    if (!shortLoad.isEmpty())
    {
        shortLoad.removeLast();
    }
    QVector<LoadPoint> invalidLoad24;
    errors.clear();
    ok =ScenarioManager::aggregateLoadTo24(shortLoad,invalidLoad24,errors);
    check(!ok &&!errors.isEmpty(),"96→24 负荷聚合识别时段数量不足");
    // 96→24 负荷工具拒绝重复 period
    QVector<LoadPoint> duplicateLoad =data96.loadCurve;
    if (duplicateLoad.size() >= 2)
    {
        duplicateLoad.last().period =duplicateLoad[duplicateLoad.size() - 2].period;
    }
    errors.clear();
    ok =ScenarioManager::aggregateLoadTo24(duplicateLoad,invalidLoad24,errors);
    check(!ok &&!errors.isEmpty(),"96→24 负荷聚合识别重复时段");
    // 96→24 新能源工具拒绝缺失时段
    QVector<RenewableOutput> missingRenewable =data96.renewableOutputs;
    if (!missingRenewable.isEmpty())
    {
        missingRenewable.removeLast();
    }
    QVector<RenewableOutput> invalidRenewable24;
    errors.clear();
    ok =ScenarioManager::aggregateRenewableTo24(missingRenewable,invalidRenewable24,errors);
    check(!ok &&!errors.isEmpty(),"96→24 新能源聚合识别机组时段缺失");
    // 96→24 新能源工具拒绝重复数据
    QVector<RenewableOutput> duplicateRenewable =data96.renewableOutputs;
    if (!duplicateRenewable.isEmpty())
    {
        duplicateRenewable.push_back(duplicateRenewable.first());
    }
    errors.clear();
    ok =ScenarioManager::aggregateRenewableTo24(duplicateRenewable,invalidRenewable24,errors);
    check(!ok &&!errors.isEmpty(),"96→24 新能源聚合识别重复机组时段");
    // 96→24 新能源工具拒绝类型不一致
    QVector<RenewableOutput> wrongTypeRenewable =data96.renewableOutputs;
    if (!wrongTypeRenewable.isEmpty())
    {
        const QString targetId =wrongTypeRenewable.first().generatorId;
        const QString originalType =wrongTypeRenewable.first().generatorType;
        const QString wrongType =originalType == "风电"? "光伏": "风电";
        for (int i = 1;i < wrongTypeRenewable.size();++i)
        {
            if (wrongTypeRenewable[i].generatorId ==targetId)
            {
                wrongTypeRenewable[i].generatorType =wrongType;
                break;
            }
        }
    }
    errors.clear();
    ok =ScenarioManager::aggregateRenewableTo24(wrongTypeRenewable,invalidRenewable24,errors);
    check(!ok &&!errors.isEmpty(),"96→24 新能源聚合识别机组类型不一致");
    // 输出汇总，失败数决定退出码
    qInfo().noquote()<< "==========================================";
    if (failedTests == 0)
    {
        qInfo().noquote()<< "All ScenarioManager V1.4 tests passed.";
        return 0;
    }
    qCritical().noquote()<< failedTests<< "test(s) failed.";
    return 1;
}