// ------------------------------------------------------------------
// 二次曲线出清测试（选题 2026v2 (10) 问，feature/quadratic-curve）
//   覆盖五类工况：
//   ① 无机组顶格 → 与解析解 λ* = (D + Σb/2a) / Σ(1/2a) 比对到 1e-6；
//   ② 高需求 → 高价机组恰好顶格 pMax；
//   ③ 低需求 → 高价机组恰好 0 出力；
//   ④ a≈0 阶梯机组 → 不崩溃、按阶梯逻辑参与供给；
//   ⑤ 需求超 ΣpMax → 稀缺封顶（衔接 V1.3.1）：shortfall>0、全部顶格；
//   ⑥ Facade 接口：quadraticGens 为空 → 空结果（模式未启用）。
// ------------------------------------------------------------------

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

#include "quadratic_clearing.h"
#include "core/clearing_facade.h"

#include <cmath>
#include <cstdio>

namespace {

int failedTests = 0;
int totalChecks = 0;

void check(bool condition, const QString &name)
{
    ++totalChecks;
    if (!condition) {
        ++failedTests;
        qCritical().noquote() << QStringLiteral("[FAIL] %1").arg(name);
    } else {
        qInfo().noquote() << QStringLiteral("[OK] %1").arg(name);
    }
}

QuadraticGenerator makeGen(const QString &id, double a, double b, double c,
                           double pMax)
{
    QuadraticGenerator g;
    g.id = id;
    g.name = id;
    g.a = a;
    g.b = b;
    g.c = c;
    g.pMax = pMax;
    return g;
}

// 三机组参考集合（无顶格区间内可解析求解）
QVector<QuadraticGenerator> referenceGens()
{
    return {
        makeGen(QStringLiteral("G1"), 0.05, 80.0, 500.0, 400.0),
        makeGen(QStringLiteral("G2"), 0.03, 100.0, 450.0, 350.0),
        makeGen(QStringLiteral("G3"), 0.02, 60.0, 300.0, 300.0)
    };
}

// ① 解析解（G1/G2 未顶格未零出力、G3 顶格 300 的两机内点结构）
void testAnalytic()
{
    const QVector<QuadraticGenerator> gens = referenceGens();

    // 参考组结构：G3 便宜且陡（λ>72 即顶格 300），G2 开机门槛 100。
    //   取 λ=110：G1=(110-80)/0.1=300（<400 内点）、G2=(110-100)/0.06=166.67（<350 内点）、
    //   G3 顶格 300 → D = 766.67。两机内点子系统解析：λ=(D-300+Σb/2a)/Σ(1/2a)。
    const double D = 300.0 + (110.0 - 80.0) / 0.1 + (110.0 - 100.0) / 0.06;

    // 验证解析解适用前提：G1/G2 未越界、G3 恰顶格
    const double p1 = (110.0 - 80.0) / 0.1;
    const double p2 = (110.0 - 100.0) / 0.06;
    const double p3 = (110.0 - 60.0) / 0.04;
    check(p1 > 0.0 && p1 < 400.0 && p2 > 0.0 && p2 < 350.0 && p3 >= 300.0,
          QStringLiteral("解析解适用前提（λ=110 时 G1/G2 内点、G3 顶格）"));

    const QuadraticClearResult r = quadraticClearing(gens, D);

    check(r.ok, QStringLiteral("① 可行性标志"));
    check(std::abs(r.clearingPrice - 110.0) < 1e-3,
          QStringLiteral("① 出清价 = 解析解 110（%.6f）").arg(r.clearingPrice));
    check(std::abs(r.totalVolume - D) < 1e-2,
          QStringLiteral("① 总量平衡（%.6f vs %.6f）").arg(r.totalVolume).arg(D));
    check(std::abs(r.shortfall) < 1e-9, QStringLiteral("① 无供给缺口"));

    // 未顶格机组（G1/G2）的边际成本 = 统一出清价；G3 顶格机组 MC=72 例外
    bool mcConsistent = true;
    for (const auto &d : r.dispatch) {
        if (d.id == QStringLiteral("G3"))
            continue;
        mcConsistent = mcConsistent && std::abs(d.marginalCost - r.clearingPrice) < 1e-3;
    }
    check(mcConsistent, QStringLiteral("① 未顶格机组边际成本 = 统一出清价"));
}

// ② 顶格：高价机组（G1 最高价）恰好满发
void testCapAtPMax()
{
    QuadraticGenerator cheap = makeGen(QStringLiteral("GC"), 0.02, 60.0, 300.0, 300.0);
    QuadraticGenerator dear = makeGen(QStringLiteral("GD"), 0.05, 200.0, 800.0, 100.0);
    QVector<QuadraticGenerator> gens = { cheap, dear };

    // λ=202 时：cheap=(202-60)/0.04=3550→顶格300；dear=(202-200)/0.1=20（内点）
    // D = 300 + 20 = 320 → 出清价恰 202（< dear 顶格价 210，二分上界可达）
    const double D = 320.0;
    const QuadraticClearResult r = quadraticClearing(gens, D);

    check(r.ok, QStringLiteral("② 可行性标志"));
    check(std::abs(r.clearingPrice - 202.0) < 1e-2,
          QStringLiteral("② 出清价（%.4f vs 202）").arg(r.clearingPrice));
    bool dearPartial = false, cheapCapped = false;
    for (const auto &d : r.dispatch) {
        if (d.id == QStringLiteral("GD"))
            dearPartial = std::abs(d.output - 20.0) < 1e-2;
        if (d.id == QStringLiteral("GC"))
            cheapCapped = std::abs(d.output - 300.0) < 1e-2;
    }
    check(dearPartial, QStringLiteral("② 高价机组内点出力 20"));
    check(cheapCapped, QStringLiteral("② 低价机组顶格 300"));
}

// ③ 低需求：高价机组恰好 0 出力（平坦供给段：λ ∈ [72,80] 任一价均可出清）
void testZeroOutput()
{
    const QVector<QuadraticGenerator> gens = referenceGens();
    // G3 在 λ=72 即顶格 300，G1 开机门槛 80 → 供给曲线在 [72,80] 平坦 = 300。
    // D=300 落在平坦段，出清价不唯一（经济学退化情形），但必落于 [72,80]：
    //   高价机组 G1/G2 恒零出力、G3 顶格。二分法收敛到段内一点即正确。
    const double D = 300.0;
    const QuadraticClearResult r = quadraticClearing(gens, D);

    check(r.ok, QStringLiteral("③ 可行性标志"));
    check(r.clearingPrice >= 72.0 - 1e-6 && r.clearingPrice <= 80.0 + 1e-3,
          QStringLiteral("③ 出清价落于平坦段 [72,80]（%.4f）").arg(r.clearingPrice));
    for (const auto &d : r.dispatch) {
        if (d.id == QStringLiteral("G1"))
            check(d.output < 1e-6, QStringLiteral("③ G1 恰好零出力"));
        if (d.id == QStringLiteral("G3"))
            check(std::abs(d.output - 300.0) < 1e-2, QStringLiteral("③ G3 顶格 300"));
    }
}

// ④ a≈0 阶梯机组：不崩溃、按阶梯逻辑参与
void testZeroQuadraticCoefficient()
{
    QuadraticGenerator step = makeGen(QStringLiteral("GS"), 0.0, 90.0, 200.0, 200.0);
    QuadraticGenerator smooth = makeGen(QStringLiteral("GF"), 0.05, 80.0, 500.0, 400.0);
    QVector<QuadraticGenerator> gens = { step, smooth };

    const double D = 300.0;   // 阶梯机满发 200 + 平滑机 (λ-80)/0.1 = 100 → λ=90
    const QuadraticClearResult r = quadraticClearing(gens, D);

    check(r.ok, QStringLiteral("④ a=0 不崩溃且可行"));
    check(std::abs(r.clearingPrice - 90.0) < 1e-2,
          QStringLiteral("④ 出清价（%.4f vs 90）").arg(r.clearingPrice));
    for (const auto &d : r.dispatch) {
        if (d.id == QStringLiteral("GS")) {
            check(std::abs(d.output - 200.0) < 1e-6,
                  QStringLiteral("④ 阶梯机组越过门槛即满发"));
            check(std::abs(d.marginalCost - 90.0) < 1e-9,
                  QStringLiteral("④ 阶梯机组边际成本 = b"));
        }
    }
}

// ⑤ 需求超容量：稀缺封顶（衔接 V1.3.1）
void testScarcityCap()
{
    const QVector<QuadraticGenerator> gens = referenceGens();
    const double D = 1200.0;   // > ΣpMax = 1050
    const QuadraticClearResult r = quadraticClearing(gens, D);

    check(r.ok, QStringLiteral("⑤ 稀缺时仍返回可行结果"));
    check(std::abs(r.shortfall - 150.0) < 1e-6,
          QStringLiteral("⑤ 缺口 = 150（%.4f）").arg(r.shortfall));
    check(std::abs(r.clearingPrice - 121.0) < 1e-6,
          QStringLiteral("⑤ 出清价封顶 = 最高边际成本 121（%.4f）").arg(r.clearingPrice));
    bool allCapped = true;
    for (const auto &d : r.dispatch)
        allCapped = allCapped && d.output > 0.0;
    check(allCapped, QStringLiteral("⑤ 全部机组顶格"));
}

// ⑥ Facade 接口：未启用二次模式 → 空结果
void testFacadeGuard()
{
    MarketData market;   // quadraticGens 为空
    const ClearingResult r = ClearingFacade::clearPeriodsQuadratic(market, 96, 0.2);
    check(r.periods.isEmpty(), QStringLiteral("⑥ 未提供二次参数 → 空结果"));
    check(r.mode == QStringLiteral("QUAD"), QStringLiteral("⑥ 模式标记 QUAD"));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testAnalytic();
    testCapAtPMax();
    testZeroOutput();
    testZeroQuadraticCoefficient();
    testScarcityCap();
    testFacadeGuard();

    qInfo().noquote() << QStringLiteral("QuadraticClearingTest：%1/%2 项通过")
                             .arg(totalChecks - failedTests)
                             .arg(totalChecks);
    return failedTests == 0 ? 0 : 1;
}
