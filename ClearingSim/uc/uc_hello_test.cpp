// ============================================================
// S1 Hello-World：验证 HiGHS 求解器接入成功（方案 §5.1）
//
//   解一个最小的线性规划：
//       min  x + y
//       s.t. x + y >= 1
//            x >= 0, y >= 0
//   最优目标值 = 1（x、y 各取一点即可，总和恰好 1）。
//
//   建模套路（S2/S3 的 SCUC 也用同一套 API，先在这里练手）：
//     addVar(下界, 上界)                 —— 加决策变量
//     changeColCost(变量号, 系数)         —— 目标函数系数
//     addRow(下界, 上界, 非零数, 列号, 系数) —— 加一条约束
//     run()                              —— 求解
// ============================================================

#include <highs/Highs.h>

#include <cmath>
#include <cstdio>
#include <limits>

int main()
{
    const double kInf = std::numeric_limits<double>::infinity();

    Highs h;
    h.setOptionValue("output_flag", false);   // 关掉求解器的刷屏日志

    // 1) 决策变量 x, y：取值 [0, +inf)
    h.addVar(0.0, kInf);   // 列 0 = x
    h.addVar(0.0, kInf);   // 列 1 = y

    // 2) 目标函数：min x + y
    h.changeColCost(0, 1.0);
    h.changeColCost(1, 1.0);

    // 3) 约束：x + y >= 1
    //    addRow(约束下界, 约束上界, 非零系数个数, 变量号数组, 系数数组)
    const HighsInt cols[2]   = {0, 1};
    const double coefs[2]    = {1.0, 1.0};
    h.addRow(1.0, kInf, 2, cols, coefs);

    // 4) 求解
    h.run();

    const HighsModelStatus status = h.getModelStatus();
    const double obj = h.getObjectiveValue();
    std::printf("status=%d  objective=%.6f\n", static_cast<int>(status), obj);

    if (status != HighsModelStatus::kOptimal || std::fabs(obj - 1.0) > 1e-6) {
        std::printf("FAIL: HiGHS hello-world 未得到预期最优值 1\n");
        return 1;
    }
    std::printf("PASS: HiGHS 接入成功（S1）\n");
    return 0;
}
