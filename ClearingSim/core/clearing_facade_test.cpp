#include "core/clearing_facade.h"
#include "data/data_reader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>

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
    files.generatorBidsFile = base + QStringLiteral("/scenario/generator_bids.csv");
    files.consumerBidsFile = base + QStringLiteral("/scenario/consumer_bids.csv");
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

    qInfo().noquote() << (failedTests == 0
                              ? QStringLiteral("All ClearingFacade V1.3 tests passed.")
                              : QStringLiteral("%1 test(s) FAILED.").arg(failedTests));
    return failedTests == 0 ? 0 : 1;
}
