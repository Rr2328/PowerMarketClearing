#include "mainwindow.h"

#include <QApplication>

// 程序入口：启动三视角电力现货市场出清仿真平台
// （B 位引擎的单体测试演示已移除——引擎已接入界面，
//   可通过 P1「一键演示」或 mainwindow 的仿真控制验证。）
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
