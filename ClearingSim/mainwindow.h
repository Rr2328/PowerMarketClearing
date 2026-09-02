#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>

#include "core/app_session.h"

class QComboBox;
class QLabel;
class QListWidget;
class QLineSeries;
class QScatterSeries;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class QButtonGroup;
class QValueAxis;

// 主窗口：深色科技风封面页（隐藏导航）→ 进入平台 → 左导航（5 页）+ 右侧内容区
// + 顶栏（页面标题/副标题 + 三视角切换条 + 数据就绪灯 + 一键演示）+ 底部状态栏
// 对应《界面清单与设计系统 v1.3》与《平台说明文档》5.6 导航可达规则：
//   导航永不锁死；P3/P4/P5 无出清结果时显示空态引导卡片
// 接线状态（feature/ui-wiring 分支）：
//   - P1 数据导入：接入 A 模块 DataReader::readAll / validateRelations（真实校验）
//   - P2 仿真控制：接入 A 模块 buildPeriodScenarios + FakeEngine（B 位算法待替换）
//   - P3/P4/P5：全部改读 AppSession.result，支持三视角过滤
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onStartDemo();     // 一键演示：加载内置基准例 → 单时段对拍出清
    void onRunSim();        // 开始仿真：场景构建 → 逐时段出清
    void onExportDaily();   // 导出结算日报 CSV（按当前视角）
    void onExportCurve();   // 导出电价曲线数据 CSV
    void onLoadSamples();   // P1：一键加载内置样例（benchmark 或 scenario）
    void onImportCsv();     // P1：选择 CSV 文件（按文件名自动识别四张表）
    void onClearData();     // P1：清空数据

private:
    // 封面页 + 五个页面 + 空态卡片
    QWidget *buildCoverPage();
    QWidget *buildImportPage();   // 1 数据导入（真实申报表 + 数据集状态 + 校验汇总条）
    QWidget *buildControlPage();  // 2 仿真控制（MCP/PAB 模式卡 + 参数摘要 + 就绪灯）
    QWidget *buildResultPage();   // 3 出清结果（指标卡 + 明细表 + 空态）
    QWidget *buildChartPage();    // 4 图表分析（供需交叉 / 分时电价 双页签 + 空态）
    QWidget *buildExportPage();   // 5 结果导出（摘要 + 双导出 + 导出记录 + 空态）
    QWidget *buildEmptyCard();    // 统一空态引导卡片
    QWidget *buildPerspectiveBar(); // 顶栏三视角切换条（免登录 · 互斥）

    // 数据与出清
    bool loadDataFiles(const QString &genFile, const QString &conFile,
                       const QString &loadFile, const QString &renewFile,
                       const QString &sourceName);   // 读取 + 校验 → m_session
    QString locateSamplesDir() const;                // 定位仓库 data/samples 目录
    void runClearing();                              // 场景构建 + FakeEngine 出清

    // 三视角
    void setPerspective(Perspective p);              // 切换视角（不重算）
    void applyPerspective();                         // 按当前视角刷新各页

    // 状态与刷新
    void setHasResult(bool on);
    void refreshImportPage();      // P1：按视角填充申报表 + 状态卡
    void refreshResultPage();      // P3：指标卡 + 明细表（视角化）
    void refreshChartPage();       // P4：供需阶梯（真申报）+ 分时电价（真结果）
    void refreshExportPage();      // P5：结算摘要 + 文件名预览
    void updateFileNamePreviews();
    void updatePageHeader(int row);
    void addExportRecord(const QString &fileName);
    void applyStyle();             // 全局 QSS

    // 控件指针
    QStackedWidget *m_rootStack   = nullptr;   // 顶层页面栈：0=封面 1=主界面
    QListWidget    *m_nav          = nullptr;
    QStackedWidget *m_stack        = nullptr;
    QStackedWidget *m_resultStack  = nullptr;   // 0=空态卡片 1=内容
    QStackedWidget *m_chartStack   = nullptr;
    QStackedWidget *m_exportStack  = nullptr;
    QPushButton    *m_btnMcp       = nullptr;   // MCP 模式卡（checkable，与 PAB 互斥）
    QPushButton    *m_btnPab       = nullptr;
    QComboBox      *m_granCombo    = nullptr;
    QLabel         *m_pageTitle    = nullptr;
    QLabel         *m_pageSub      = nullptr;
    QLabel         *m_paramSummary = nullptr;
    QLabel         *m_readyLabel   = nullptr;   // P2：数据就绪灯（随导入状态更新）
    QTableWidget   *m_resultTable  = nullptr;
    QListWidget    *m_exportLog    = nullptr;
    QLineSeries    *m_priceSeries  = nullptr;   // 分时电价曲线（电力蓝）
    QLineSeries    *m_renewSeries  = nullptr;   // 新能源出力曲线（墨绿）
    QLineSeries    *m_supplySeries = nullptr;   // 供需交叉图：供给阶梯（真申报）
    QLineSeries    *m_demandSeries = nullptr;   // 供需交叉图：需求阶梯（真申报）
    QLineSeries    *m_clearingLine = nullptr;   // 供需交叉图：出清价水平线
    QScatterSeries *m_clearPoint   = nullptr;   // 供需交叉图：出清点标记（成交×出清价）
    QValueAxis     *m_axisSupplyX  = nullptr;   // 供需图 X（累计电量）
    QValueAxis     *m_axisSupplyY  = nullptr;   // 供需图 Y（报价）
    QValueAxis     *m_axisPriceX   = nullptr;   // 分时电价 X（时段）

    // 三视角切换条
    QButtonGroup   *m_perspGroup   = nullptr;
    QPushButton    *m_btnPerspGen  = nullptr;
    QPushButton    *m_btnPerspCon  = nullptr;
    QPushButton    *m_btnPerspPlat = nullptr;

    // P1：申报表（发电/购电双页签）+ 状态卡 + 校验汇总条
    QTabWidget     *m_importTabs   = nullptr;
    QTableWidget   *m_genTable     = nullptr;
    QTableWidget   *m_conTable     = nullptr;
    QLabel         *m_statusBadges[4] = {nullptr, nullptr, nullptr, nullptr};
    QLabel         *m_checkText    = nullptr;

    // 指标卡（P3）
    QLabel *m_kpiAvg    = nullptr;
    QLabel *m_kpiVol    = nullptr;
    QLabel *m_kpiFee    = nullptr;
    QLabel *m_kpiSpread = nullptr;
    QLabel *m_kpiNames[4] = {nullptr, nullptr, nullptr, nullptr};   // 指标名称（随视角切换）
    // 结算摘要（P5）
    QLabel *m_sumAvg    = nullptr;
    QLabel *m_sumMax    = nullptr;
    QLabel *m_sumMin    = nullptr;
    QLabel *m_sumVol    = nullptr;
    QLabel *m_sumFee    = nullptr;
    QLabel *m_sumSpread = nullptr;
    QLabel *m_fileDaily = nullptr;
    QLabel *m_fileCurve = nullptr;

    // 数据会话：真实数据 + 真实场景 + 出清结果 + 当前视角（取代假数据成员）
    AppSession m_session;

    bool m_hasResult = false;
};

#endif // MAINWINDOW_H
