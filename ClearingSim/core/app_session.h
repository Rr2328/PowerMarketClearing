#ifndef APP_SESSION_H
#define APP_SESSION_H

#include "core/clearing_facade.h"
#include "core/perspective.h"

// ------------------------------------------------------------------
// 数据会话：一次实验的全部状态（取代 mainwindow 里 5 个假数据成员）
//   - 视角只存枚举，不做权限：切换视角 = 改一个值 + 刷新显示
//   - 同一份数据、同一次出清，三个视角只是三种"看法"
// ------------------------------------------------------------------
struct AppSession
{
    MarketData market;                 // A 读入的申报/曲线数据
    QVector<PeriodScenario> scenarios; // A 构建的逐时段场景
    ClearingResult result;             // 出清结果（ClearingFacade 外观 → 真引擎）

    bool hasData = false;              // 申报数据是否已导入（驱动 P2 就绪灯）
    bool hasResult = false;            // 出清是否已完成（驱动 P3/P4/P5 空态卡）

    double renewPercent = 20.0;        // P2 渗透率滑块：0~100%，默认 20%（V1.3 §5.3，
                                      // P_re(t)=渗透率×负荷(t)，撮合与供需图同式换算）

    Perspective perspective = Perspective::Platform;   // 当前视角

    QString dataSource;                // 数据源描述（内置基准例/内置场景/自定义）

    void resetResult()
    {
        scenarios.clear();
        result = ClearingResult();
        hasResult = false;
    }
};

#endif // APP_SESSION_H
