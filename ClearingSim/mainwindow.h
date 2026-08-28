#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>

class QComboBox;
class QLabel;
class QListWidget;
class QLineSeries;
class QStackedWidget;
class QTableWidget;

// 主窗口骨架：左导航（5 页）+ 右侧内容区 + 顶部标题栏（含一键演示）+ 底部状态栏
// 对应《界面清单与设计系统 v1.1》与《平台说明文档》5.6 导航可达规则：
//   导航永不锁死；P3/P4/P5 无出清结果时显示空态引导卡片（一键演示 / 前往仿真控制）
// 当前全部为假数据（内置示例），用于设计验证与开题 PPT 截图；
// 后续 A 模块（数据）/ B 模块（出清算法）接入后逐步替换为真实数据与真实计算
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onStartDemo();     // 一键演示（创新点④骨架）：加载内置示例并出清
    void onRunSim();        // 开始仿真（骨架版：假数据 + 跳页）
    void onExportDaily();   // 导出结算日报 CSV（创新点⑤骨架：可真实导出假数据）
    void onExportCurve();   // 导出电价曲线数据 CSV（P5 双导出）

private:
    // 五个页面 + 空态卡片
    QWidget *buildImportPage();   // 1 数据导入（申报表 + 数据集状态 + 校验反馈）
    QWidget *buildControlPage();  // 2 仿真控制（模式/颗粒度/渗透率 + 数据就绪灯）
    QWidget *buildResultPage();   // 3 出清结果（指标卡 + 24 时段表 + 空态）
    QWidget *buildChartPage();    // 4 图表分析（供需交叉 / 分时电价 双页签 + 空态）
    QWidget *buildExportPage();   // 5 结果导出（摘要 + 双导出 + 导出记录 + 空态）
    QWidget *buildEmptyCard();    // 统一空态引导卡片（P3/P4/P5 共用样式）

    // 假数据与状态
    void generateResults();       // 生成 24 时段假出清结果（双驼峰 + 午间光伏压价）
    void setHasResult(bool on);   // 切换「无结果 → 有结果」，驱动三个空态卡片
    void refreshResultPage();     // 刷新指标卡 + 结果表
    void refreshChartPage();      // 刷新分时电价曲线
    void refreshExportPage();     // 刷新结算摘要 + 文件名预览
    void updateFileNamePreviews();// 文件名随模式/颗粒度联动
    void addExportRecord(const QString &fileName);
    void applyStyle();            // 全局 QSS（电力蓝 + 琥珀橙 设计系统）

    // 控件指针
    QListWidget    *m_nav          = nullptr;
    QStackedWidget *m_stack        = nullptr;
    QStackedWidget *m_resultStack  = nullptr;   // 0=空态卡片 1=内容
    QStackedWidget *m_chartStack   = nullptr;
    QStackedWidget *m_exportStack  = nullptr;
    QComboBox      *m_modeCombo    = nullptr;
    QComboBox      *m_granCombo    = nullptr;
    QTableWidget   *m_resultTable  = nullptr;
    QListWidget    *m_exportLog    = nullptr;
    QLineSeries    *m_priceSeries  = nullptr;   // 分时电价曲线（电力蓝）
    QLineSeries    *m_renewSeries  = nullptr;   // 新能源出力曲线（绿色）

    // 指标卡（P3）
    QLabel *m_kpiAvg    = nullptr;
    QLabel *m_kpiMax    = nullptr;
    QLabel *m_kpiMin    = nullptr;
    QLabel *m_kpiSpread = nullptr;
    // 结算摘要（P5）
    QLabel *m_sumAvg    = nullptr;
    QLabel *m_sumMax    = nullptr;
    QLabel *m_sumMin    = nullptr;
    QLabel *m_sumVol    = nullptr;
    QLabel *m_sumFee    = nullptr;
    QLabel *m_sumSpread = nullptr;
    QLabel *m_fileDaily = nullptr;
    QLabel *m_fileCurve = nullptr;

    // 假出清结果数据（generateResults 填充，三页共用一份数据源）
    QVector<double> m_price;   // 各时段出清电价（元/MWh）
    QVector<double> m_vol;     // 各时段出清电量 = 净负荷（MW）
    QVector<double> m_load;    // 各时段负荷（MW）
    QVector<double> m_renew;   // 各时段新能源出力（MW）
    QVector<double> m_net;     // 净负荷 = 负荷 - 新能源（MW）
    bool m_hasResult = false;
};

#endif // MAINWINDOW_H
