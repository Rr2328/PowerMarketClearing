// data_pipeline_integration_test.cpp
// 电力现货市场出清仿真平台 · V1.3 数据流水线端到端集成测试
// 全链路：CSV → DataReader → ScenarioManager → ClearingFacade（→ 撮合引擎）
// 借鉴 origin/feature/data-model 的端到端集成测试思路（陈美伊 2137534），
// 但适配到 V1.3 长表结构（bid 不带 period 字段、按 name+id 复合主体标识）
//
// 用法：
//   - 放在与 ClearingSim.exe 同一构建目录运行
//   - QT_FORCE_STDERR_LOGGING=1 可看到 PASS/FAIL 输出（Windows GUI 模式默认走 OutputDebugString）

#include "data/data_reader.h"
#include "data/scenario_manager.h"
#include "core/clearing_facade.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

#include <cmath>

namespace
{

int failedTests = 0;

void check(
    bool condition,
    const QString &testName)
{
    if (condition)
    {
        qInfo().noquote()
            << "[PASS]" << testName;
    }
    else
    {
        qCritical().noquote()
            << "[FAIL]" << testName;

        ++failedTests;
    }
}

void checkClose(
    double actual,
    double expected,
    double eps,
    const QString &testName)
{
    const bool ok = std::fabs(actual - expected) <= eps;
    if (ok)
    {
        qInfo().noquote()
            << "[PASS]" << testName
            << QString("(actual=%1, expected=%2)")
                   .arg(actual, 0, 'f', 2)
                   .arg(expected, 0, 'f', 2);
    }
    else
    {
        qCritical().noquote()
            << "[FAIL]" << testName
            << QString("(actual=%1, expected=%2, eps=%3)")
                   .arg(actual, 0, 'f', 2)
                   .arg(expected, 0, 'f', 2)
                   .arg(eps, 0, 'f', 4);

        ++failedTests;
    }
}

// ============================================================
// 路径定位：找到仓库根（含 data/samples/ 目录）
// ============================================================

QString searchRepoRoot(const QString &startPath)
{
    QDir dir(startPath);
    while (true) {
        if (dir.exists("data/samples/scenario_balanced")
            && dir.exists("data/samples/benchmark"))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    return QString();
}

QString findRepoRoot()
{
    QString root = searchRepoRoot(QCoreApplication::applicationDirPath());
    if (!root.isEmpty())
        return root;
    root = searchRepoRoot(QDir::currentPath());
    if (!root.isEmpty())
        return root;
    return searchRepoRoot(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
}

// ============================================================
// V1.3 适配的主体识别辅助函数
//   V1.3 中 bid 不带 period 字段，但同一主体的各时段各段都是独立 bid
//   所以"主体"由 name + id 复合标识（跨厂重号兼容）
// ============================================================

QString entityKey(const QString &name, const QString &id)
{
    return name + QLatin1Char('#') + id;
}

// 模板：支持 GeneratorBid 和 ConsumerBid 两种类型
//   C++17 if constexpr 简化模板逻辑
template <typename BidT>
bool hasEntity(
    const QVector<BidT> &bids,
    const QString &name,
    const QString &id)
{
    const QString key = entityKey(name, id);
    for (const auto &b : bids) {
        if (entityKey(b.name, b.id) == key)
            return true;
    }
    return false;
}

template <typename BidT>
QSet<int> periodsForEntity(
    const QVector<BidT> &bids,
    const QString &name,
    const QString &id)
{
    QSet<int> periods;
    const QString key = entityKey(name, id);
    for (const auto &b : bids) {
        if (entityKey(b.name, b.id) == key)
            periods.insert(b.period);
    }
    return periods;
}

// ============================================================
// 场景 1：V1.3 长表 scenario 端到端出清
//   CSV → DataReader → ScenarioManager → ClearingFacade 96 期 → 全天均价 ≈ 229.7
// ============================================================

void testScenarioLongTableEndToEnd(const QString &repoRoot)
{
    qInfo().noquote()
        << "---------- 场景 1：V1.3 长表 scenario 端到端 ----------";

    // ---- 1) DataReader：读取 V1.3 长表 + 负荷 + 新能源 ----
    QStringList errors;
    MarketData market;
    DataFileSet files;
    files.generatorBidsFile = repoRoot + "/data/samples/scenario_balanced/generator_bids.csv";
    files.consumerBidsFile = repoRoot + "/data/samples/scenario_balanced/consumer_bids.csv";
    files.loadCurveFile = repoRoot + "/data/samples/curves/load_curve.csv";
    files.renewableOutputFile = repoRoot + "/data/samples/curves/renewable_output.csv";
    const bool okRead = DataReader::readAll(files, market, errors);
    check(okRead, "scenario 长表读取成功");
    check(errors.isEmpty(),
          QStringLiteral("scenario 读取无错误（实际=%1条）").arg(errors.size()));

    // ---- 2) 主体覆盖：发电 5 机组（按 name+id 区分）----
    check(hasEntity(market.generatorBids, "金陵电厂", "#1机组"),
          "scenario 发电覆盖 · 金陵电厂#1机组");
    check(hasEntity(market.generatorBids, "金陵电厂", "#2机组"),
          "scenario 发电覆盖 · 金陵电厂#2机组");
    check(hasEntity(market.generatorBids, "龙潭电厂", "#1机组"),
          "scenario 发电覆盖 · 龙潭电厂#1机组");
    check(hasEntity(market.generatorBids, "龙潭电厂", "#2机组"),
          "scenario 发电覆盖 · 龙潭电厂#2机组");
    check(hasEntity(market.generatorBids, "龙潭电厂", "#3机组"),
          "scenario 发电覆盖 · 龙潭电厂#3机组");

    // ---- 3) 主体覆盖：购电 3 用户 ----
    check(hasEntity(market.consumerBids, "一号工业园区", "L1"),
          "scenario 购电覆盖 · 一号工业园区L1");
    check(hasEntity(market.consumerBids, "二号商业综合体", "L2"),
          "scenario 购电覆盖 · 二号商业综合体L2");
    check(hasEntity(market.consumerBids, "三号居民小区", "L3"),
          "scenario 购电覆盖 · 三号居民小区L3");

    // ---- 4) 时段覆盖：发电 5 主体 × 96 期 = 480 行（V1.3 长表每段每期一行）----
    QSet<int> genPeriods;
    for (const auto &b : market.generatorBids)
        genPeriods.insert(b.period);
    check(genPeriods.size() == 96,
          QStringLiteral("scenario 发电覆盖 96 期（实际=%1）").arg(genPeriods.size()));

    QSet<int> conPeriods;
    for (const auto &b : market.consumerBids)
        conPeriods.insert(b.period);
    check(conPeriods.size() == 96,
          QStringLiteral("scenario 购电覆盖 96 期（实际=%1）").arg(conPeriods.size()));

    // ---- 5) 单一主体逐时段全覆盖（每主体 × 96 期）----
    for (const auto &pair : QList<QPair<QString, QString>>{
            {"金陵电厂", "#1机组"},
            {"金陵电厂", "#2机组"},
            {"龙潭电厂", "#1机组"},
            {"龙潭电厂", "#2机组"},
            {"龙潭电厂", "#3机组"}}) {
        const auto ps = periodsForEntity(market.generatorBids, pair.first, pair.second);
        check(ps.size() == 96,
              QStringLiteral("scenario 发电主体 %1#%2 覆盖 96 期（实际=%3）")
                  .arg(pair.first, pair.second)
                  .arg(ps.size()));
    }

    // ---- 6) ScenarioManager：构建 96 期场景 ----
    QVector<PeriodScenario> scenarios;
    QStringList sErr;
    const bool okScenario = ScenarioManager::buildPeriodScenarios(
        market, 96, scenarios, sErr);
    check(okScenario, "scenario 96 期场景构建成功");
    check(scenarios.size() == 96,
          QStringLiteral("scenario 96 期场景数量（实际=%1）").arg(scenarios.size()));
    check(sErr.isEmpty(),
          QStringLiteral("scenario 96 期场景构建无错误（实际=%1条）").arg(sErr.size()));

    // ---- 7) 负荷曲线 96 点 ----
    check(market.loadCurve.size() == 96,
          QStringLiteral("scenario 负荷曲线 96 点（实际=%1）").arg(market.loadCurve.size()));

    // ---- 8) ClearingFacade 端到端：96 期恒跑 + 全天均价 ≈ 229.7（V1.3 对拍锚点）----
    const auto dayResult = ClearingFacade::clearPeriods(
        market, 96, 0.20, QStringLiteral("MCP"));
    check(dayResult.periods.size() == 96,
          QStringLiteral("ClearingFacade 96 期出清（实际=%1）").arg(dayResult.periods.size()));

    double priceSum = 0.0;
    int clearedCount = 0;
    int fullClearCount = 0;
    double maxGap = 0.0;
    for (const auto &pr : dayResult.periods) {
        priceSum += pr.clearingPrice;
        if (pr.clearedMW > 0.0)
            ++clearedCount;
        // 供需满足 = 成交 ≥ 负荷（无缺口）；容差 0.5 MW 与 ClearingFacadeTest 一致
        const double gap = pr.loadMW - pr.clearedMW;
        if (gap > maxGap)
            maxGap = gap;
        if (pr.clearedMW > 0.0 && pr.clearedMW >= pr.loadMW - 0.5)
            ++fullClearCount;
    }
    const double avg = priceSum / dayResult.periods.size();
    checkClose(avg, 229.7, 0.5,
               "ClearingFacade 全天均价 ≈ 229.7（V1.3 长表对拍锚点）");
    check(clearedCount == 96,
          QStringLiteral("96 期全部时段均有成交（实际=%1）").arg(clearedCount));
    check(fullClearCount == 96,
          QStringLiteral("96 期供需全部满足（零缺口，实际=%1，最大缺口=%2 MW）")
              .arg(fullClearCount)
              .arg(maxGap, 0, 'f', 3));

    // ---- 9) 出清价落在五级阶梯内（210/220/230/240/260）----
    QSet<double> priceSet;
    for (const auto &pr : dayResult.periods)
        priceSet.insert(pr.clearingPrice);
    check(priceSet.size() <= 5,
          QStringLiteral("出清价档位 ≤ 5（实际=%1）").arg(priceSet.size()));
}

// ============================================================
// 场景 2：V1.1 窄表 benchmark 端到端（自动展开 96 期）
//   CSV → DataReader（窄表 → 96 期展开）→ ClearingFacade → 250/50/12500 锚点
// ============================================================

void testBenchmarkNarrowTableEndToEnd(const QString &repoRoot)
{
    qInfo().noquote()
        << "---------- 场景 2：窄表 benchmark 端到端 ----------";

    QStringList errors;
    MarketData market;
    // 窄表 benchmark 只有两张申报表，无负荷曲线/新能源曲线——
    // 逐张调 readGeneratorBids / readConsumerBids 避开 readAll 的全文件检查。
    const QString baseDir = repoRoot + "/data/samples/benchmark";
    const bool okGen = DataReader::readGeneratorBids(
        baseDir + "/generator_bids.csv",
        market.generatorBids, errors);
    const bool okCon = DataReader::readConsumerBids(
        baseDir + "/consumer_bids.csv",
        market.consumerBids, errors);
    const bool okRead = okGen && okCon;
    check(okRead, "benchmark 窄表读取成功");
    check(errors.isEmpty(),
          QStringLiteral("benchmark 读取无错误（实际=%1条）").arg(errors.size()));

    // 窄表基准例：G1 (100元, 40MWh) × 1 段, G2 (250元, 60MWh) × 1 段
    // 展开 96 期：每条 bid × 96 = 192 条
    check(market.generatorBids.size() == 192,
          QStringLiteral("benchmark 发电窄表展开 192 行（实际=%1）")
              .arg(market.generatorBids.size()));
    check(market.consumerBids.size() == 192,
          QStringLiteral("benchmark 购电窄表展开 192 行（实际=%1）")
              .arg(market.consumerBids.size()));

    // 时段覆盖：所有 bid 的 period 应覆盖 1–96
    QSet<int> periods;
    for (const auto &b : market.generatorBids)
        periods.insert(b.period);
    check(periods.size() == 96,
          QStringLiteral("benchmark 发电展开覆盖 96 期（实际=%1）").arg(periods.size()));

    // ClearingFacade：基准例（窄表）单时段锚点对拍
    // 默认模式 MCP、渗透率 0（无负荷曲线 → 回退到购电申报总量 × penetration）
    const auto dayResult = ClearingFacade::clearPeriods(
        market, 96, 0.0, QStringLiteral("MCP"));

    // 96 期逐时段结果 = 250 元 / 50 MWh / 12500 元（窄表等价性对拍）
    int matchCount = 0;
    for (const auto &pr : dayResult.periods) {
        if (std::fabs(pr.clearingPrice - 250.0) < 0.5
            && std::fabs(pr.clearedMW - 50.0) < 0.5)
            ++matchCount;
    }
    check(matchCount == 96,
          QStringLiteral("benchmark 96 期 = 250/50 锚点（实际命中=%1）").arg(matchCount));

    // 全天单边费用 = 1200000 元（genFee = conFee = 250元 × 50 MWh × 96 期）
    //   MCP 模式下发电侧与购电侧都按出清价结算（B 位 settle 等价）
    double genFeeSum = 0.0, conFeeSum = 0.0;
    for (const auto &pr : dayResult.periods) {
        genFeeSum += pr.genFee;
        conFeeSum += pr.conFee;
    }
    checkClose(genFeeSum, 1200000.0, 1.0,
               QStringLiteral("benchmark 发电侧全天费用 ≈ 1200000 元（250×50×96）"));
    checkClose(conFeeSum, 1200000.0, 1.0,
               QStringLiteral("benchmark 购电侧全天费用 ≈ 1200000 元（250×50×96）"));
}

// ============================================================
// 场景 3：24 期聚合（V1.3 §7.2）
//   96 期出清 → 24 期聚合：出清价 = 4 期均值、费用 = 4 期求和、电量 = 4 期均值（口径：MWh/h）
// ============================================================

void testAggregation24(const QString &repoRoot)
{
    qInfo().noquote()
        << "---------- 场景 3：24 期聚合（V1.3 §7.2）----------";

    QStringList errors;
    MarketData market;
    DataFileSet files;
    files.generatorBidsFile = repoRoot + "/data/samples/scenario_balanced/generator_bids.csv";
    files.consumerBidsFile = repoRoot + "/data/samples/scenario_balanced/consumer_bids.csv";
    files.loadCurveFile = repoRoot + "/data/samples/curves/load_curve.csv";
    files.renewableOutputFile = repoRoot + "/data/samples/curves/renewable_output.csv";
    DataReader::readAll(files, market, errors);

    const auto day96 = ClearingFacade::clearPeriods(market, 96, 0.20, QStringLiteral("MCP"));
    const auto day24 = ClearingFacade::clearPeriods(market, 24, 0.20, QStringLiteral("MCP"));

    check(day24.periods.size() == 24,
          QStringLiteral("24 期聚合视图时段数（实际=%1）").arg(day24.periods.size()));

    // 24 期聚合总费用应等于 96 期总费用（按口径聚合，不重复计算）
    double fee96 = 0.0, fee24 = 0.0;
    for (const auto &pr : day96.periods)
        fee96 += pr.genFee;
    for (const auto &pr : day24.periods)
        fee24 += pr.genFee;
    checkClose(fee24, fee96, 1.0,
               QStringLiteral("24 期聚合发电总费用 ≈ 96 期总费用（差=%1）")
                   .arg(fee24 - fee96, 0, 'f', 2));

    // 24 期聚合全天均价与 96 期全天均价的差应在合理范围（每小时一价）
    double p96 = 0.0, p24 = 0.0;
    for (const auto &pr : day96.periods) p96 += pr.clearingPrice;
    for (const auto &pr : day24.periods) p24 += pr.clearingPrice;
    const double avg96 = p96 / day96.periods.size();
    const double avg24 = p24 / day24.periods.size();
    checkClose(avg24, avg96, 1.0,
               QStringLiteral("24 期聚合全天均价 ≈ 96 期均价（差=%1）")
                   .arg(avg24 - avg96, 0, 'f', 2));
}

// ============================================================
// 场景 4：错误检测（CSV 表头错误 + 段数越界 + 跨文件一致性）
// ============================================================

void testErrorDetection(const QString &repoRoot)
{
    qInfo().noquote()
        << "---------- 场景 4：错误检测 ----------";

    // ---- 4.1 长表表头错误（缺关键列）----
    QTemporaryDir tmpDir;
    check(tmpDir.isValid(), "创建临时测试目录");
    const QString badLong = tmpDir.path() + "/bad_long.csv";
    QFile f(badLong);
    check(f.open(QIODevice::WriteOnly | QIODevice::Text), "打开临时文件写");
    f.write("period,电厂名称,机组编号,缺段列\n");
    f.write("1,某电厂,#1,100\n");
    f.close();

    QStringList errs;
    QVector<GeneratorBid> bids;
    const bool ok = DataReader::readGeneratorBids(badLong, bids, errs);
    check(!ok, "长表表头错误被识别（readGeneratorBids 返回 false）");
    check(!errs.isEmpty(),
          QStringLiteral("长表表头错误产生错误信息（实际=%1条）").arg(errs.size()));

    // ---- 4.2 跨文件校验：发电/购电同时缺失 ----
    QStringList errs2;
    MarketData empty;
    DataFileSet emptyFiles;   // 全空路径
    const bool ok2 = DataReader::readAll(emptyFiles, empty, errs2);
    check(!ok2, "空数据集被识别（readAll 返回 false）");
    check(!errs2.isEmpty(),
          QStringLiteral("空数据集产生错误信息（实际=%1条）").arg(errs2.size()));
}

}   // namespace


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qInfo().noquote() << "========== V1.3 数据流水线集成测试 ==========";

    const QString repoRoot = findRepoRoot();
    check(!repoRoot.isEmpty(), "定位项目根目录");
    if (repoRoot.isEmpty())
        return 1;

    qInfo().noquote() << "Repo root:" << repoRoot;

    testScenarioLongTableEndToEnd(repoRoot);
    testBenchmarkNarrowTableEndToEnd(repoRoot);
    testAggregation24(repoRoot);
    testErrorDetection(repoRoot);

    qInfo().noquote()
        << "========================================";
    if (failedTests == 0) {
        qInfo().noquote() << "All V1.3 data-pipeline integration tests passed.";
        return 0;
    }
    qCritical().noquote()
        << failedTests << "V1.3 data-pipeline integration tests failed.";
    return 1;
}