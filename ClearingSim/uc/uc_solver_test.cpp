// ============================================================
// SCUC 求解器测试：S2 LP 对拍 + S3 MILP 启停/爬坡/最小开停机
//
//   验证标准：
//   S2——λ(t) 必须等于人工推算的边际机组报价（对偶价对拍）；
//   S3——开停机计划、λ、总成本必须等于按经济逻辑手推的答案。
//   期望值全部手推：便宜的先发满、低谷停掉 pMin 底数亏的机组、
//   刚启动要付启动费、最小开/停机时间强制连续段、爬坡顶住时贵机组顶上。
// ============================================================

#include "uc_solver.h"

#include <QtGlobal>
#include <QCoreApplication>
#include <QDebug>

static int totalChecks = 0;
static int failedTests = 0;

static void check(bool cond, const QString &what)
{
    ++totalChecks;
    if (!cond) {
        ++failedTests;
        qWarning().noquote() << QStringLiteral("  ✗ %1").arg(what);
    }
}

// 浮点比较
static bool near(double a, double b, double tol = 1e-4)
{
    return std::fabs(a - b) < tol;
}

// ① 单机组：λ = 该机组报价
static void testSingleUnit()
{
    GeneratorMeta g;
    g.name = QStringLiteral("测试电厂");
    g.id = QStringLiteral("#1机组");
    g.pMin = 0.0;
    g.pMax = 150.0;
    g.marginalCost = 250.0;

    const UcSolution s = solveUcLp({g}, {100.0});
    check(s.ok, QStringLiteral("① 求解成功"));
    check(near(s.p[0][0], 100.0), QStringLiteral("① 出力 = 需求 100（实际 %1）").arg(s.p[0][0]));
    check(near(s.lambda[0], 250.0), QStringLiteral("① λ = 报价 250（实际 %1）").arg(s.lambda[0]));
}

// ② 经济调度排序（merit order）：便宜机组顶格，λ = 边际机组报价
static void testMeritOrder()
{
    GeneratorMeta a;  // 便宜大机组
    a.id = QStringLiteral("A");
    a.pMin = 0.0; a.pMax = 240.0; a.marginalCost = 150.0;
    GeneratorMeta b;  // 贵机组
    b.id = QStringLiteral("B");
    b.pMin = 0.0; b.pMax = 230.0; b.marginalCost = 210.0;

    // D=300 > A 的 240：A 顶格，B 补 60 → λ = B 的报价 210
    {
        const UcSolution s = solveUcLp({a, b}, {300.0});
        check(s.ok, QStringLiteral("②a 求解成功"));
        check(near(s.p[0][0], 240.0), QStringLiteral("②a A 顶格 240（实际 %1）").arg(s.p[0][0]));
        check(near(s.p[1][0], 60.0), QStringLiteral("②a B 补 60（实际 %1）").arg(s.p[1][0]));
        check(near(s.lambda[0], 210.0), QStringLiteral("②a λ = 边际报价 210（实际 %1）").arg(s.lambda[0]));
    }
    // D=100 < A 的容量：只有 A 发 → λ = A 的报价 150
    {
        const UcSolution s = solveUcLp({a, b}, {100.0});
        check(s.ok, QStringLiteral("②b 求解成功"));
        check(near(s.p[0][0], 100.0), QStringLiteral("②b A 独发 100（实际 %1）").arg(s.p[0][0]));
        check(near(s.p[1][0], 0.0), QStringLiteral("②b B 零出力（实际 %1）").arg(s.p[1][0]));
        check(near(s.lambda[0], 150.0), QStringLiteral("②b λ = 边际报价 150（实际 %1）").arg(s.lambda[0]));
    }
}

// ③ 最小技术出力生效：便宜机组有 pMin 底数，仍按经济逻辑出力
static void testPMinFloor()
{
    GeneratorMeta a;  // 便宜但有 pMin 底数 180（"启动了就有基础量"）
    a.id = QStringLiteral("A");
    a.pMin = 180.0; a.pMax = 300.0; a.marginalCost = 150.0;
    GeneratorMeta b;
    b.id = QStringLiteral("B");
    b.pMin = 0.0; b.pMax = 300.0; b.marginalCost = 210.0;

    // D=200 ≥ pMin_A：A 发 200 即可（在 [180,300] 内），B 零出力 → λ=150
    {
        const UcSolution s = solveUcLp({a, b}, {200.0});
        check(s.ok, QStringLiteral("③a 求解成功"));
        check(near(s.p[0][0], 200.0), QStringLiteral("③a A 出力 200（实际 %1）").arg(s.p[0][0]));
        check(near(s.lambda[0], 150.0), QStringLiteral("③a λ = 150（实际 %1）").arg(s.lambda[0]));
    }
    // D 落在 pMin 之下 → 全机组运行假设下不可行（S3 里才是 u=0 停机的场景）
    {
        const UcSolution s = solveUcLp({a, b}, {100.0});
        check(!s.ok, QStringLiteral("③b D=100 < ΣpMin=180 → 无可行解"));
    }
}

// ④ 稀缺：D > ΣpMax → 不可行（S4 接口层再做稀缺封顶价）
static void testInfeasible()
{
    GeneratorMeta a; a.id = QStringLiteral("A");
    a.pMin = 0.0; a.pMax = 240.0; a.marginalCost = 150.0;
    GeneratorMeta b; b.id = QStringLiteral("B");
    b.pMin = 0.0; b.pMax = 230.0; b.marginalCost = 210.0;

    const UcSolution s = solveUcLp({a, b}, {700.0});
    check(!s.ok, QStringLiteral("④ D=700 > ΣpMax=470 → 无可行解"));
}

// ⑤ 多时段：96 期规模跑通，各时段 λ 随需求阶梯变化
static void testMultiPeriod()
{
    GeneratorMeta a; a.id = QStringLiteral("A");
    a.pMin = 0.0; a.pMax = 240.0; a.marginalCost = 150.0;
    GeneratorMeta b; b.id = QStringLiteral("B");
    b.pMin = 0.0; b.pMax = 230.0; b.marginalCost = 210.0;

    QVector<double> demand(96);
    demand.fill(100.0);
    for (int t = 23; t < 47; ++t) demand[t] = 300.0;   // 假想白天 300、其余 100
    const UcSolution s = solveUcLp({a, b}, demand);
    check(s.ok, QStringLiteral("⑤ 96 期求解成功"));
    check(s.lambda.size() == 96, QStringLiteral("⑤ 输出 96 个 λ"));
    check(near(s.lambda[0], 150.0), QStringLiteral("⑤ 低谷 λ=150（实际 %1）").arg(s.lambda[0]));
    check(near(s.lambda[30], 210.0), QStringLiteral("⑤ 高峰 λ=210（实际 %1）").arg(s.lambda[30]));
    check(near(s.totalCost, 72 * 100.0 * 150.0 + 24 * (240.0 * 150.0 + 60.0 * 210.0)),
          QStringLiteral("⑤ 总成本 = 分段手算（实际 %1）").arg(s.totalCost));
}

// ⑥ S3 MILP 启停：低谷 B 停机避开 pMin 底数，高峰 B 启动并计入启动费
static void testMilpStartup()
{
    GeneratorMeta a;  // 基荷：便宜、pMin 底数 100
    a.id = QStringLiteral("A");
    a.pMin = 100.0; a.pMax = 400.0; a.marginalCost = 150.0;
    a.startupCost = 5000.0;
    GeneratorMeta b;  // 调峰：贵、pMin 底数 200
    b.id = QStringLiteral("B");
    b.pMin = 200.0; b.pMax = 300.0; b.marginalCost = 210.0;
    b.startupCost = 8000.0;

    UcInitialState init;
    init.on = { true, false };       // A 初始开机，B 初始停机
    init.p = { 100.0, 0.0 };

    // 低谷 4 期只够 A 发；第 5 期 550 > 400，B 必须启动
    const QVector<double> demand = { 300.0, 350.0, 250.0, 150.0, 550.0 };
    const UcSolution s = solveUcMilp({a, b}, demand, init);
    check(s.ok, QStringLiteral("⑥ 求解成功：%1").arg(s.message));
    if (!s.ok) return;
    check(s.u[1] == QVector<int>({ 0, 0, 0, 0, 1 }),
          QStringLiteral("⑥ B 只在高峰启动（实际 %1 %2 %3 %4 %5)")
              .arg(s.u[1].value(0)).arg(s.u[1].value(1)).arg(s.u[1].value(2))
              .arg(s.u[1].value(3)).arg(s.u[1].value(4)));
    check(s.u[0] == QVector<int>({ 1, 1, 1, 1, 1 }),
          QStringLiteral("⑥ A 全程开机"));
    check(near(s.p[0][3], 150.0), QStringLiteral("⑥ 低谷 A 单独覆盖 150（实际 %1）").arg(s.p[0].value(3)));
    check(near(s.p[0][4], 350.0) && near(s.p[1][4], 200.0),
          QStringLiteral("⑥ 高峰 A=350 + B=pMin=200（实际 %1/%2）").arg(s.p[0].value(4)).arg(s.p[1].value(4)));
    // λ：A 单独运行时段（100<D<400 内点）= 150；高峰 A 未顶格 → 仍 150
    for (int t = 0; t < 5; ++t)
        check(near(s.lambda[t], 150.0, 1e-3),
              QStringLiteral("⑥ λ[%1] = 150（实际 %2）").arg(t).arg(s.lambda.value(t)));
    // 总成本 = 电量 150·1400 + 210·200 + 一次启动费 8000
    check(near(s.totalCost, 150.0 * (300 + 350 + 250 + 150 + 350) + 210.0 * 200 + 8000.0, 0.01),
          QStringLiteral("⑥ 总成本含启动费（实际 %1）").arg(s.totalCost));
}

// ⑦ S3 最小开/停机时间：低谷必须停机 → 最小停机 2 期；重启后最小开机 3 期
static void testMilpMinUpDown()
{
    GeneratorMeta a;  // 基荷：pMin=100，空载费 50/期，启动费 1000，最小开 3 / 停 2
    a.id = QStringLiteral("A");
    a.pMin = 100.0; a.pMax = 300.0; a.marginalCost = 100.0;
    a.noLoadCost = 50.0; a.startupCost = 1000.0;
    a.minUpTime = 3; a.minDownTime = 2;
    GeneratorMeta c;  // 调峰：随时启停、无空载/启动费
    c.id = QStringLiteral("C");
    c.pMin = 10.0; c.pMax = 300.0; c.marginalCost = 400.0;

    UcInitialState init;
    init.on = { true, false };
    init.p = { 200.0, 0.0 };

    // 低谷 50 < pMin_A=100：A 必须停 → 停满 2 期(t2,t3)；t4 重启
    const QVector<double> demand = { 200.0, 250.0, 50.0, 50.0, 200.0, 100.0 };
    const UcSolution s = solveUcMilp({a, c}, demand, init);
    check(s.ok, QStringLiteral("⑦ 求解成功：%1").arg(s.message));
    if (!s.ok) return;
    check(s.u[0] == QVector<int>({ 1, 1, 0, 0, 1, 1 }),
          QStringLiteral("⑦ A：低谷停满最小停机 2 期（实际 %1%2%3%4%5%6）")
              .arg(s.u[0].value(0)).arg(s.u[0].value(1)).arg(s.u[0].value(2))
              .arg(s.u[0].value(3)).arg(s.u[0].value(4)).arg(s.u[0].value(5)));
    check(s.u[1] == QVector<int>({ 0, 0, 1, 1, 0, 0 }),
          QStringLiteral("⑦ C：只在低谷顶上（实际 %1%2%3%4%5%6）")
              .arg(s.u[1].value(0)).arg(s.u[1].value(1)).arg(s.u[1].value(2))
              .arg(s.u[1].value(3)).arg(s.u[1].value(4)).arg(s.u[1].value(5)));
    // λ：A 运行时段 = 100；C 单独顶低谷 = 400
    const double lamExp[6] = { 100.0, 100.0, 400.0, 400.0, 100.0, 100.0 };
    for (int t = 0; t < 6; ++t)
        check(near(s.lambda[t], lamExp[t], 1e-3),
              QStringLiteral("⑦ λ[%1] = %2（实际 %3）").arg(t).arg(lamExp[t]).arg(s.lambda.value(t)));
    // 总成本 = A 电量 100·750 + 空载 50·4 + 启动 1000 + C 电量 400·100
    check(near(s.totalCost, 100.0 * (200 + 250 + 200 + 100) + 50.0 * 4 + 1000.0 + 400.0 * 100.0, 0.01),
          QStringLiteral("⑦ 总成本 = 电量+空载+启动（实际 %1）").arg(s.totalCost));
}

// ⑧ S3 爬坡：需求陡增超过爬坡限速 → A 被限速卡住，贵机组 B 顶上，λ 跳到 B 报价
static void testMilpRamp()
{
    GeneratorMeta a;  // 便宜但爬坡慢：±50 MW/期
    a.id = QStringLiteral("A");
    a.pMin = 0.0; a.pMax = 300.0; a.marginalCost = 100.0;
    a.rampUp = 50.0; a.rampDown = 50.0;
    a.startupCost = 5000.0;          // 启动费>0 → su 不会虚增、爬坡约束不被钻空子
    GeneratorMeta b;  // 贵但爬坡快（不限）
    b.id = QStringLiteral("B");
    b.pMin = 0.0; b.pMax = 300.0; b.marginalCost = 500.0;
    b.startupCost = 5000.0;

    UcInitialState init;
    init.on = { true, true };
    init.p = { 100.0, 0.0 };         // A 初始出力 100

    // A 每期最多爬 50：t1 卡在 150、t2 卡在 200，缺口全靠 B
    const QVector<double> demand = { 100.0, 200.0, 290.0 };
    const UcSolution s = solveUcMilp({a, b}, demand, init);
    check(s.ok, QStringLiteral("⑧ 求解成功：%1").arg(s.message));
    if (!s.ok) return;
    check(near(s.p[0][1], 150.0) && near(s.p[1][1], 50.0),
          QStringLiteral("⑧ t1 A 被爬坡卡在 150、B 补 50（实际 %1/%2）").arg(s.p[0].value(1)).arg(s.p[1].value(1)));
    check(near(s.p[0][2], 200.0) && near(s.p[1][2], 90.0),
          QStringLiteral("⑧ t2 A=200、B=90（实际 %1/%2）").arg(s.p[0].value(2)).arg(s.p[1].value(2)));
    // λ：t0 A 内点 = 100；t1/t2 边际在 B = 500（爬坡约束的对偶吸收差额）
    const double lamExp[3] = { 100.0, 500.0, 500.0 };
    for (int t = 0; t < 3; ++t)
        check(near(s.lambda[t], lamExp[t], 1e-3),
              QStringLiteral("⑧ λ[%1] = %2（实际 %3）").arg(t).arg(lamExp[t]).arg(s.lambda.value(t)));
    check(near(s.totalCost, 100.0 * 100.0 + (150.0 * 100.0 + 50.0 * 500.0)
                                  + (200.0 * 100.0 + 90.0 * 500.0), 0.01),
          QStringLiteral("⑧ 总成本（实际 %1）").arg(s.totalCost));
}

// ⑨ S3 96 期规模跑通：结果与 S2 LP 一致（本组参数启停不触发，成本应相同）
static void testMilpMultiPeriod()
{
    GeneratorMeta a; a.id = QStringLiteral("A");
    a.pMin = 0.0; a.pMax = 240.0; a.marginalCost = 150.0; a.startupCost = 1000.0;
    GeneratorMeta b; b.id = QStringLiteral("B");
    b.pMin = 0.0; b.pMax = 230.0; b.marginalCost = 210.0; b.startupCost = 1000.0;

    QVector<double> demand(96);
    demand.fill(100.0);
    for (int t = 23; t < 47; ++t) demand[t] = 300.0;
    const UcSolution s = solveUcMilp({a, b}, demand);
    check(s.ok, QStringLiteral("⑨ 96 期 MILP 求解成功：%1").arg(s.message));
    if (!s.ok) return;
    check(s.lambda.size() == 96, QStringLiteral("⑨ 输出 96 个 λ"));
    check(near(s.lambda[0], 150.0, 1e-3), QStringLiteral("⑨ 低谷 λ=150（实际 %1）").arg(s.lambda.value(0)));
    check(near(s.lambda[30], 210.0, 1e-3), QStringLiteral("⑨ 高峰 λ=210（实际 %1）").arg(s.lambda.value(30)));
    check(near(s.totalCost, 72 * 100.0 * 150.0 + 24 * (240.0 * 150.0 + 60.0 * 210.0), 0.05),
          QStringLiteral("⑨ 总成本与 S2 LP 对拍一致（实际 %1）").arg(s.totalCost));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testSingleUnit();
    testMeritOrder();
    testPMinFloor();
    testInfeasible();
    testMultiPeriod();
    testMilpStartup();
    testMilpMinUpDown();
    testMilpRamp();
    testMilpMultiPeriod();

    qInfo().noquote() << QStringLiteral("UcSolverTest(S2+S3)：%1/%2 项通过")
                             .arg(totalChecks - failedTests)
                             .arg(totalChecks);
    return failedTests == 0 ? 0 : 1;
}
