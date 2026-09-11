#include "core/clearing_facade.h"
#include "data/data_reader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

#include <algorithm>
#include <cmath>

// ============================================================
// ClearingFacade 对拍测试（V1.3 · #88）
//   ① 窄表基准例等价性对拍：渗透率 0 时，96 期逐时段结果
//      与单时段基准出清（250 元 / 50 MWh / 12500 元）完全一致
//   ② 场景样例 + 20% 渗透率：逐时段全额成交、价格五级阶梯、
//      新能源按 P_re(t)=渗透率×负荷(t) 全额消纳（契约 §5.3）
//   ③ 24 期聚合视图：价 = 4 期均值、费用 = 求和、电量 = 均值（§7.2）
//   ④ renewCapacityAt：负荷曲线基准 / 无曲线回退购电申报总量
// ============================================================

namespace
{

int failedTests = 0;

void check(bool condition, const QString &testName)
{
    if (condition)
        qInfo().noquote() << "[PASS]" << testName;
    else {
        qCritical().noquote() << "[FAIL]" << testName;
        ++failedTests;
    }
}

bool near(double a, double b, double eps = 0.01)
{
    return std::fabs(a - b) <= eps;
}

// 从测试可执行文件位置向上定位仓库根（含 ClearingSim + data/samples）
QString searchRepoRoot()
{
    QDir dir = QCoreApplication::applicationDirPath();
    while (true) {
        if (dir.exists("ClearingSim") && dir.exists("data/samples"))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    return QString();
}

// 只读两张申报表（基准例无曲线文件）
bool loadBids(const QString &repoRoot, const QString &subDir,
              MarketData &market, QStringList &errors)
{
    const QString base = repoRoot + QStringLiteral("/data/samples/") + subDir;
    return DataReader::readGeneratorBids(base + QStringLiteral("/generator_bids.csv"),
                                         market.generatorBids, errors)
           && DataReader::readConsumerBids(base + QStringLiteral("/consumer_bids.csv"),
                                           market.consumerBids, errors);
}

// 读取场景样例（申报两张 + curves/ 曲线两张，四表齐备）
bool loadScenario(const QString &repoRoot, MarketData &market, QStringList &errors)
{
    const QString base = repoRoot + QStringLiteral("/data/samples");
    DataFileSet files;
    files.generatorBidsFile = base + QStringLiteral("/scenario_balanced/generator_bids.csv");
    files.consumerBidsFile = base + QStringLiteral("/scenario_balanced/consumer_bids.csv");
    files.loadCurveFile = base + QStringLiteral("/curves/load_curve.csv");
    files.renewableOutputFile = base + QStringLiteral("/curves/renewable_output.csv");
    return DataReader::readAll(files, market, errors);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString root = searchRepoRoot();
    if (root.isEmpty()) {
        qCritical().noquote() << "[FAIL] 未定位到仓库根目录（data/samples）";
        return 1;
    }

    // ---------------- ① 窄表基准例：渗透率 0 的等价性对拍 ----------------
    {
        MarketData m;
        QStringList errs;
        check(loadBids(root, QStringLiteral("benchmark"), m, errs),
              QStringLiteral("基准例读取（窄表自动展开 96 期）%1").arg(errs.join(QStringLiteral(";"))));

        const ClearingResult bench = ClearingFacade::clearBenchmark(m, QStringLiteral("MCP"));
        check(bench.periods.size() == 1
                  && near(bench.periods[0].clearingPrice, 250.0)
                  && near(bench.periods[0].clearedMW, 50.0)
                  && near(bench.periods[0].genFee, 12500.0),
              QStringLiteral("基准锚点 250 元 / 50 MWh / 12500 元"));

        const ClearingResult r96 = ClearingFacade::clearPeriods(m, 96, 0.0, QStringLiteral("MCP"));
        bool allEq = r96.periods.size() == 96;
        for (const auto &pr : r96.periods) {
            if (!near(pr.clearingPrice, 250.0)
                || !near(pr.clearedMW, 50.0)
                || !near(pr.genFee, 12500.0)
                || !near(pr.renewMW, 0.0)) {
                allEq = false;
                break;
            }
        }
        check(allEq, QStringLiteral("96 期逐时段 = 基准锚点（窄表等价性对拍）"));

        check(r96.periods.size() == 96
                  && r96.periods[0].time == QStringLiteral("00:15")
                  && r96.periods[95].time == QStringLiteral("24:00"),
              QStringLiteral("96 期时间标签 00:15 ~ 24:00"));
    }

    // ---------------- ② 场景样例 + 20% 渗透率 ----------------
    ClearingResult raw96;
    {
        MarketData m;
        QStringList errs;
        check(loadScenario(root, m, errs),
              QStringLiteral("场景样例读取（申报 + 负荷 + 新能源形状）%1").arg(errs.join(QStringLiteral(";"))));

        raw96 = ClearingFacade::clearPeriods(m, 96, 0.2, QStringLiteral("MCP"));
        check(raw96.periods.size() == 96, QStringLiteral("场景 96 期出清"));

        bool fullClear = true, priceLadder = true, renewAbsorbed = true;
        double priceSum = 0.0;
        for (const auto &pr : raw96.periods) {
            if (pr.clearedMW < pr.loadMW - 0.5)
                fullClear = false;                      // 逐时段全额成交（零缺口）
            const bool inLadder = near(pr.clearingPrice, 210.0, 0.5)
                                  || near(pr.clearingPrice, 220.0, 0.5)
                                  || near(pr.clearingPrice, 230.0, 0.5)
                                  || near(pr.clearingPrice, 240.0, 0.5)
                                  || near(pr.clearingPrice, 260.0, 0.5);
            if (!inLadder)
                priceLadder = false;                    // 五级阶梯价
            const double cap = ClearingFacade::renewCapacityAt(m, pr.period, 0.2);
            if (pr.renewMW < cap - 0.5 || pr.renewMW > cap + 0.5)
                renewAbsorbed = false;                  // P_re(t) 全额消纳
            priceSum += pr.clearingPrice;
        }
        check(fullClear, QStringLiteral("20% 渗透率下 96 期全额成交（零缺口）"));
        check(priceLadder, QStringLiteral("出清价落在 210/220/230/240/260 五级阶梯"));
        check(renewAbsorbed, QStringLiteral("新能源 P_re(t)=20%×负荷(t) 全额消纳"));
        check(near(priceSum / 96.0, 229.7, 0.6),
              QStringLiteral("全日均价 ≈ 229.7（实测 %1）").arg(priceSum / 96.0, 0, 'f', 1));

        // ---------------- ④ renewCapacityAt 两条换算路径 ----------------
        check(near(ClearingFacade::renewCapacityAt(m, 1, 0.2), 141.3, 0.05),
              QStringLiteral("有负荷曲线：P_re(1)=0.2×706.5=141.3"));
        check(near(ClearingFacade::renewCapacityAt(m, 1, 0.0), 0.0),
              QStringLiteral("渗透率 0 → 出力 0"));
    }

    // ---------------- ③ 24 期聚合视图（契约 §7.2） ----------------
    {
        MarketData m;
        QStringList errs;
        loadScenario(root, m, errs);
        const ClearingResult r24 = ClearingFacade::clearPeriods(m, 24, 0.2, QStringLiteral("MCP"));

        check(r24.periods.size() == 24
                  && r24.periods[0].time == QStringLiteral("01:00")
                  && r24.periods[23].time == QStringLiteral("24:00"),
              QStringLiteral("24 期聚合视图时间标签 01:00 ~ 24:00"));

        bool priceMean = true, feeSum = true, volMean = true, detailsKept = true;
        for (int h = 1; h <= 24; ++h) {
            const auto &agg = r24.periods[h - 1];
            double pSum = 0.0, fee = 0.0, vSum = 0.0;
            int peakQ = 0;
            for (int q = 0; q < 4; ++q) {
                const auto &pr = raw96.periods[(h - 1) * 4 + q];
                pSum += pr.clearingPrice;
                fee += pr.genFee;
                vSum += pr.clearedMW;
                if (pr.clearingPrice > raw96.periods[(h - 1) * 4 + peakQ].clearingPrice)
                    peakQ = q;
            }
            if (!near(agg.clearingPrice, pSum / 4.0, 0.05))
                priceMean = false;
            if (!near(agg.genFee, fee, 0.5))
                feeSum = false;
            if (!near(agg.clearedMW, vSum / 4.0, 0.05))
                volMean = false;
            if (agg.genDetails.size()
                    != raw96.periods[(h - 1) * 4 + peakQ].genDetails.size())
                detailsKept = false;                    // 明细不聚合：取峰值截面
        }
        check(priceMean, QStringLiteral("24 期聚合：出清价 = 4 期均值"));
        check(feeSum, QStringLiteral("24 期聚合：费用 = 4 期求和"));
        check(volMean, QStringLiteral("24 期聚合：电量 = 4 期均值（小时 MWh/h 口径）"));
        check(detailsKept, QStringLiteral("24 期聚合：明细取峰值截面（不聚合）"));

        // 无负荷曲线时回退购电申报总量（基准例需求 100 MW → 20% = 20）
        MarketData bm;
        loadBids(root, QStringLiteral("benchmark"), bm, errs);
        check(near(ClearingFacade::renewCapacityAt(bm, 1, 0.2), 20.0),
              QStringLiteral("无负荷曲线：P_re(1)=0.2×购电申报 100=20"));
    }

    // ---------------- ⑤ 稀缺封顶：供给量尽 → 出清价 = 限价 540（契约 V1.3.1） ----------------
    {
        // 场景 A：供给 30 MW @200，需求 50 MW @300 → 供给先尽、缺口 20 MW → 封顶 540
        MarketData m;
        GeneratorBid g;
        g.id = QStringLiteral("#1机组"); g.name = QStringLiteral("测试电厂");
        g.period = 1; g.segment = 1; g.price = 200.0; g.quantity = 30.0;
        m.generatorBids.append(g);
        ConsumerBid c;
        c.id = QStringLiteral("L1"); c.name = QStringLiteral("测试用户");
        c.period = 1; c.segment = 1; c.price = 300.0; c.quantity = 50.0;
        m.consumerBids.append(c);

        const ClearingResult r = ClearingFacade::clearBenchmark(m, QStringLiteral("MCP"));
        check(r.periods.size() == 1
                  && near(r.periods[0].clearingPrice, 540.0)
                  && near(r.periods[0].clearedMW, 30.0),
              QStringLiteral("稀缺封顶：供给 30 < 需求 50 → 出清价升至限价 540、成交 30"));

        // 场景 B：供给要价 350 > 需求愿付 300 → 价格不交叉、零成交，
        //   维持边际定价（lastprice=0），不触发封顶（双侧均有剩余，非量尽）
        MarketData m2;
        GeneratorBid g2 = g;
        g2.price = 350.0;
        m2.generatorBids.append(g2);
        m2.consumerBids.append(c);
        const ClearingResult r2 = ClearingFacade::clearBenchmark(m2, QStringLiteral("MCP"));
        check(r2.periods.size() == 1
                  && near(r2.periods[0].clearingPrice, 0.0)
                  && near(r2.periods[0].clearedMW, 0.0),
              QStringLiteral("价格不交叉：零成交、出清价 0（不触发封顶）"));
    }

    // ---------------- ⑥ UC 模式端到端：场景样例 + generator_meta.csv（S4） ----------------
    {
        MarketData m;
        QStringList errs;
        check(loadScenario(root, m, errs),
              QStringLiteral("UC：场景样例读取 %1").arg(errs.join(QStringLiteral(";"))));
        const QString metaFile = root + QStringLiteral("/data/samples/scenario/generator_meta.csv");
        check(DataReader::readGeneratorMeta(metaFile, m.generatorMeta, errs)
                  && m.generatorMeta.size() == 5,
              QStringLiteral("UC：读取 generator_meta.csv（5 机组）"));

        // meta 缺失 → 空结果守护
        MarketData noMeta = m;
        noMeta.generatorMeta.clear();
        const ClearingResult emptyR = ClearingFacade::clearPeriodsUc(noMeta, 96, 0.2);
        check(emptyR.periods.isEmpty() && emptyR.mode == QStringLiteral("UC"),
              QStringLiteral("UC：无 generator_meta.csv → 空结果（模式未启用）"));

        const ClearingResult uc = ClearingFacade::clearPeriodsUc(m, 96, 0.2);
        check(uc.periods.size() == 96, QStringLiteral("UC：96 期逐时段出清"));

        // 逐期供需平衡（缺口 < 0.5 MW）、λ 落在机组电量成本区间 [150, 260]
        bool balanced = true, priceInBand = true;
        for (const auto &pr : uc.periods) {
            if (pr.clearedMW < pr.loadMW - 0.5)
                balanced = false;
            if (pr.clearingPrice < 150.0 - 0.5 || pr.clearingPrice > 260.0 + 0.5)
                priceInBand = false;
        }
        check(balanced, QStringLiteral("UC：96 期供需平衡（缺口 < 0.5 MW）"));
        check(priceInBand, QStringLiteral("UC：出清价落在电量成本区间 [150, 260]"));

        // 逐期发电明细之和（含新能源段）= 成交量；结算 = Σ p×λ
        bool detailOk = true;
        for (const auto &pr : uc.periods) {
            double sum = 0.0, fee = 0.0;
            for (const auto &e : pr.genDetails) { sum += e.clearedMW; fee += e.money; }
            if (!near(sum, pr.clearedMW, 0.5) || !near(fee, pr.genFee, 0.5))
                detailOk = false;
        }
        check(detailOk, QStringLiteral("UC：发电明细/结算与汇总一致"));

        // 峰谷价差与 KPI 合理性：启动次数有限、成本非负
        check(uc.startupCount > 0 && uc.startupCount < 96
                  && uc.startupCostTotal > 0.0,
              QStringLiteral("UC：启动 %1 次、启动成本 %2 元（全天有启停、非逐期乱启）")
                  .arg(uc.startupCount).arg(uc.startupCostTotal, 0, 'f', 0));

        // 24 期聚合视图
        const ClearingResult uc24 = ClearingFacade::clearPeriodsUc(m, 24, 0.2);
        check(uc24.periods.size() == 24
                  && near(uc24.periods[0].clearingPrice,
                          (uc.periods[0].clearingPrice + uc.periods[1].clearingPrice
                           + uc.periods[2].clearingPrice + uc.periods[3].clearingPrice) / 4.0,
                          0.05),
              QStringLiteral("UC：24 期聚合价 = 4 期均值"));
    }

    // ---------------- ⑦ UC 建模行为：pMin 底数 / 停机避底数 / 启动计费 ----------------
    {
        // 3 机组：便宜基荷（pMin=100，成本 150）、中档（pMin=50，成本 210）、
        // 贵调峰（pMin=0，成本 260）。需求 120 → 只需基荷机组开（100 ≤ 120 ≤ 300），
        // 中档/调峰停机避底数；λ = 150（基荷边际）。
        QVector<GeneratorMeta> metas;
        GeneratorMeta base;
        base.name = QStringLiteral("基荷电厂"); base.id = QStringLiteral("#1机组");
        base.pMin = 100.0; base.pMax = 300.0; base.marginalCost = 150.0;
        base.startupCost = 5000.0; base.noLoadCost = 800.0;
        GeneratorMeta mid = base;
        mid.name = QStringLiteral("腰荷电厂"); mid.id = QStringLiteral("#2机组");
        mid.pMin = 50.0; mid.pMax = 200.0; mid.marginalCost = 210.0;
        GeneratorMeta peak = base;
        peak.name = QStringLiteral("调峰电厂"); peak.id = QStringLiteral("#3机组");
        peak.pMin = 0.0; peak.pMax = 150.0; peak.marginalCost = 260.0;
        metas << base << mid << peak;

        QVector<double> demand(96, 0.0);
        demand[0] = 120.0;
        UcInitialState init;
        init.on = { false, false, false };   // 初始全停 → 需求期启动基荷机组
        const UcSolution s = solveUcMilp(metas, demand, init);

        check(s.ok && s.u[0][0] == 1 && s.u[1][0] == 0 && s.u[2][0] == 0,
              QStringLiteral("UC 建模：需求 120 时仅基荷机组开机（其余停机避 pMin 底数）%1")
                  .arg(s.ok ? QStringLiteral("") : s.message));
        check(near(s.lambda[0], 150.0),
              QStringLiteral("UC 建模：λ = 基荷机组电量成本 150（实测 %1）").arg(s.lambda[0]));
        check(near(s.p[0][0], 120.0, 0.5),
              QStringLiteral("UC 建模：基荷机组出力 = 需求 120"));

        // 需求 380 → 基荷顶格 300 + 腰荷 80（80 < pMin=50? 不，80 ≥ 50 ✓）→ λ = 210
        QVector<double> demand2(96, 0.0);
        demand2[0] = 380.0;
        const UcSolution s2 = solveUcMilp(metas, demand2, init);
        check(s2.ok && s2.u[0][0] == 1 && s2.u[1][0] == 1 && s2.u[2][0] == 0
                  && near(s2.lambda[0], 210.0),
              QStringLiteral("UC 建模：需求 380 → 基荷+腰荷开机、λ = 210"));

        // 需求超出 ΣpMax → 不可行（稀缺需要另行封顶，UC 模式如实报告）
        // 需求超出 ΣpMax → 失负荷松弛放行、λ 封顶 540（V1.3.1 稀缺定价同口径，
        // 与分段引擎封顶行为一致——界面不会因无解而白屏）
        QVector<double> demand3(96, 0.0);
        demand3[0] = 1000.0;
        const UcSolution s3 = solveUcMilp(metas, demand3, init);
        check(s3.ok && near(s3.lambda[0], 540.0)
                  && near(s3.shed[0], 350.0, 0.5),
              QStringLiteral("UC 建模：需求 1000 > ΣpMax 650 → 缺口 350 失负荷、λ 封顶 540"));
    }

    // ---------------- ⑧ UC × 应用默认数据：scenario/（现实供需形态，峰时供不应求） ----------------
    //   用户在 GUI 里默认加载的就是这个目录——峰时净负荷 990 > ΣpMax 985，
    //   曾经因 MILP 无松弛直接无解 → 界面白屏。此用例保证"UI 默认数据恒可解"。
    {
        MarketData m;
        QStringList errs;
        const QString base = root + QStringLiteral("/data/samples/scenario");
        DataFileSet files;
        files.generatorBidsFile = base + QStringLiteral("/generator_bids.csv");
        files.consumerBidsFile = base + QStringLiteral("/consumer_bids.csv");
        files.loadCurveFile = root + QStringLiteral("/data/samples/curves/load_curve.csv");
        files.renewableOutputFile = root + QStringLiteral("/data/samples/curves/renewable_output.csv");
        check(DataReader::readAll(files, m, errs),
              QStringLiteral("UC 默认场景：读取 scenario/ 四表 %1").arg(errs.join(QStringLiteral(";"))));
        check(DataReader::readGeneratorMeta(
                  base + QStringLiteral("/generator_meta.csv"), m.generatorMeta, errs)
                  && m.generatorMeta.size() == 5,
              QStringLiteral("UC 默认场景：读取 generator_meta.csv（5 机组）"));

        // 稀缺断言用 96 期原生粒度（24 期小时均值会把单季度 540 稀释）
        const ClearingResult uc96 = ClearingFacade::clearPeriodsUc(m, 96, 0.2);
        const ClearingResult uc = ClearingFacade::clearPeriodsUc(m, 24, 0.2);
        check(uc.periods.size() == 24 && uc96.periods.size() == 96,
              QStringLiteral("UC 默认场景：24/96 期出清有解（不白屏）"));
        bool hasScarcity = false, priceCapped = true;
        for (const auto &pr : uc96.periods) {
            if (pr.clearingPrice >= 540.0 - 0.5)
                hasScarcity = true;
            if (pr.clearingPrice > 540.0 + 0.5)
                priceCapped = false;
        }
        check(hasScarcity,
              QStringLiteral("UC 默认场景：峰时段触发稀缺出清价 540（供不应求可见）"));
        check(priceCapped,
              QStringLiteral("UC 默认场景：出清价不超过限价 540"));
    }

    qInfo().noquote() << (failedTests == 0
                              ? QStringLiteral("All ClearingFacade V1.3 tests passed.")
                              : QStringLiteral("%1 test(s) FAILED.").arg(failedTests));
    return failedTests == 0 ? 0 : 1;
}
