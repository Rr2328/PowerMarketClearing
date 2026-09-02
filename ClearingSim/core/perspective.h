#ifndef PERSPECTIVE_H
#define PERSPECTIVE_H

#include <QString>

// 三视角（免登录自由切换）：视角 = 过滤器，不是权限。
//   Gen      —— 发电侧：只看到发电侧申报 / 本机组中标与收入
//   Con      —— 购电侧：只看到购电侧申报 / 本用户中标与账单
//   Platform —— 平台：  双侧申报 / 全局出清价与全场明细
enum class Perspective { Gen, Con, Platform };

inline QString perspectiveName(Perspective p)
{
    switch (p) {
    case Perspective::Gen:      return QStringLiteral("发电侧视角");
    case Perspective::Con:      return QStringLiteral("购电侧视角");
    case Perspective::Platform: return QStringLiteral("平台视角");
    }
    return QStringLiteral("平台视角");
}

inline QString perspectiveShort(Perspective p)
{
    switch (p) {
    case Perspective::Gen:      return QStringLiteral("发电侧");
    case Perspective::Con:      return QStringLiteral("购电侧");
    case Perspective::Platform: return QStringLiteral("平台");
    }
    return QStringLiteral("平台");
}

#endif // PERSPECTIVE_H
