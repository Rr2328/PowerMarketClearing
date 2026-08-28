#include "mainwindow.h"

#include <algorithm>
#include <cmath>

#include <QPainter>

#include <QComboBox>
#include <QDate>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTime>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("电力现货市场出清仿真平台"));
    resize(1180, 760);

    // ---------- 顶部栏：标题 + 一键演示按钮（创新点④） ----------
    auto *titleLabel = new QLabel(QStringLiteral("电力现货市场出清仿真平台"));
    titleLabel->setObjectName("titleLabel");

    auto *demoBtn = new QPushButton(QStringLiteral("⚡ 一键演示"));
    demoBtn->setObjectName("demoBtn");
    demoBtn->setToolTip(QStringLiteral("加载内置基准案例，自动完成一次完整出清演示（当前为骨架版：内置假数据）"));
    connect(demoBtn, &QPushButton::clicked, this, &MainWindow::onStartDemo);

    auto *topBar = new QHBoxLayout();
    topBar->addWidget(titleLabel);
    topBar->addStretch();
    topBar->addWidget(demoBtn);

    // ---------- 左侧导航：5 个页面（与《平台说明文档》编号一致） ----------
    m_nav = new QListWidget();
    m_nav->setObjectName("navList");
    m_nav->setFixedWidth(180);
    m_nav->addItems({
        QStringLiteral("①  数据导入"),
        QStringLiteral("②  仿真控制"),
        QStringLiteral("③  出清结果"),
        QStringLiteral("④  图表分析"),
        QStringLiteral("⑤  结果导出"),
    });

    // ---------- 右侧内容区：QStackedWidget 切页 ----------
    m_stack = new QStackedWidget();
    m_stack->addWidget(buildImportPage());
    m_stack->addWidget(buildControlPage());
    m_stack->addWidget(buildResultPage());
    m_stack->addWidget(buildChartPage());
    m_stack->addWidget(buildExportPage());

    connect(m_nav, &QListWidget::currentRowChanged,
            m_stack, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    // ---------- 整体布局 ----------
    auto *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->addWidget(m_nav);
    body->addSpacing(10);
    body->addWidget(m_stack, 1);

    auto *central = new QWidget();
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 12, 16, 8);
    root->addLayout(topBar);
    root->addSpacing(8);
    root->addLayout(body, 1);

    setCentralWidget(central);

    statusBar()->showMessage(QStringLiteral("就绪 · 界面骨架版（内置示例数据 · 出清为假数据）"));

    applyStyle();
}

// ============================================================
// 页面 1：数据导入（机组申报表 + 数据集状态 + 校验反馈区）
// ============================================================
QWidget *MainWindow::buildImportPage()
{
    auto *page = new QWidget();

    // --- 上：机组申报表（内置示例 · 假数据） ---
    auto *bidBox = new QGroupBox(QStringLiteral("机组申报数据（内置示例 · 假数据）"), page);

    auto *table = new QTableWidget(8, 4, bidBox);
    table->setHorizontalHeaderLabels({
        QStringLiteral("机组名称"),
        QStringLiteral("申报容量 (MW)"),
        QStringLiteral("申报报价 (元/MWh)"),
        QStringLiteral("机组类型"),
    });
    table->horizontalHeader()->setStretchLastSection(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    const QList<QStringList> rows = {
                                      {QStringLiteral("火电 G1"), QStringLiteral("300"), QStringLiteral("310"), QStringLiteral("火电")},
                                      {QStringLiteral("火电 G2"), QStringLiteral("250"), QStringLiteral("335"), QStringLiteral("火电")},
                                      {QStringLiteral("火电 G3"), QStringLiteral("200"), QStringLiteral("360"), QStringLiteral("火电")},
                                      {QStringLiteral("水电 H1"), QStringLiteral("150"), QStringLiteral("280"), QStringLiteral("水电")},
                                      {QStringLiteral("风电 W1"), QStringLiteral("120"), QStringLiteral("0"),   QStringLiteral("风电")},
                                      {QStringLiteral("风电 W2"), QStringLiteral("80"),  QStringLiteral("0"),   QStringLiteral("风电")},
                                      {QStringLiteral("光伏 S1"), QStringLiteral("80"),  QStringLiteral("0"),   QStringLiteral("光伏")},
                                      {QStringLiteral("储能 E1"), QStringLiteral("50"),  QStringLiteral("90"),  QStringLiteral("储能")},
                                      };
    for (int i = 0; i < rows.size(); ++i)
        for (int j = 0; j < 4; ++j)
            table->setItem(i, j, new QTableWidgetItem(rows[i][j]));

    auto *importBtn = new QPushButton(QStringLiteral("导入机组参数 CSV"), bidBox);
    auto *clearBtn  = new QPushButton(QStringLiteral("清空数据"), bidBox);
    importBtn->setObjectName("secondaryBtn");
    clearBtn->setObjectName("secondaryBtn");
    connect(importBtn, &QPushButton::clicked, this, [this] {
        statusBar()->showMessage(
            QStringLiteral("骨架版提示：CSV 导入将在数据模块（A）完成后接入 data/samples/ 样例"), 5000);
    });
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        statusBar()->showMessage(QStringLiteral("骨架版提示：清空功能待实现"), 5000);
    });

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(importBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();

    auto *bidLay = new QVBoxLayout(bidBox);
    bidLay->addWidget(table);
    bidLay->addLayout(btnRow);

    // --- 中：数据集状态表（四张表 · 数据契约 V1.0） ---
    auto *setBox = new QGroupBox(QStringLiteral("数据集状态（对应数据契约的四张表）"), page);

    auto *setTable = new QTableWidget(4, 3, setBox);
    setTable->setHorizontalHeaderLabels({
        QStringLiteral("数据集"),
        QStringLiteral("内容"),
        QStringLiteral("状态"),
    });
    setTable->horizontalHeader()->setStretchLastSection(true);
    setTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    setTable->verticalHeader()->setVisible(false);
    const QList<QStringList> setRows = {
                                         {QStringLiteral("generator_bids 机组申报"), QStringLiteral("8 台机组（5 火 2 风 1 光 + 储能）"), QStringLiteral("✓ 内置示例")},
                                         {QStringLiteral("consumer_bids 用户申报"),   QStringLiteral("双边申报（基准例）"),             QStringLiteral("✓ 内置示例")},
                                         {QStringLiteral("load_curve 负荷曲线"),      QStringLiteral("96 时段 · 双驼峰"),              QStringLiteral("○ 待导入（A 样例）")},
                                         {QStringLiteral("renewable_output 新能源出力"), QStringLiteral("风 / 光逐时段出力"),          QStringLiteral("○ 待导入（A 样例）")},
                                         };
    for (int i = 0; i < setRows.size(); ++i)
        for (int j = 0; j < 3; ++j)
            setTable->setItem(i, j, new QTableWidgetItem(setRows[i][j]));
    setTable->setMaximumHeight(150);

    auto *setLay = new QVBoxLayout(setBox);
    setLay->addWidget(setTable);

    // --- 下：校验反馈区（申报校验五规则 · 静态展示） ---
    auto *checkBox = new QGroupBox(QStringLiteral("校验反馈（申报校验五规则）"), page);
    auto *checkLabel = new QLabel(checkBox);
    checkLabel->setText(QStringLiteral(
        "<span style='color:#2FA84F;'>✓</span> 报价区间 0 ~ 540 元/MWh（细则第 92 条）&nbsp;&nbsp;&nbsp;"
        "<span style='color:#2FA84F;'>✓</span> 申报段数 ≤ 5 段<br>"
        "<span style='color:#2FA84F;'>✓</span> 分段报价单调不减&nbsp;&nbsp;&nbsp;"
        "<span style='color:#2FA84F;'>✓</span> 报价单位 1 元/MWh · 出力单位 1 MW&nbsp;&nbsp;&nbsp;"
        "<span style='color:#2FA84F;'>✓</span> 校验不通过不许提交<br>"
        "<span style='color:#8A8A93;'>骨架版：以上为内置示例数据的静态展示，逐行真实校验由 A 模块实现"
        "（依据《电力现货市场基本规则（试行）》4.2.3 条）</span>"));
    checkLabel->setTextFormat(Qt::RichText);
    auto *checkLay = new QVBoxLayout(checkBox);
    checkLay->addWidget(checkLabel);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);
    outer->addWidget(bidBox, 3);
    outer->addWidget(setBox, 2);
    outer->addWidget(checkBox, 1);
    return page;
}

// ============================================================
// 页面 2：仿真控制（数据就绪灯 + 模式 + 颗粒度 + 渗透率）
// ============================================================
QWidget *MainWindow::buildControlPage()
{
    auto *page = new QWidget();
    auto *box = new QGroupBox(QStringLiteral("仿真参数设置"), page);

    // 数据就绪状态灯（v1.1：P2 状态灯；数据未就绪时开始仿真按钮将置灰）
    auto *readyLabel = new QLabel();
    readyLabel->setText(QStringLiteral(
        "数据状态：<span style='color:#2FA84F;'>●</span> <b>已就绪</b>（内置示例数据）"));
    readyLabel->setTextFormat(Qt::RichText);
    readyLabel->setObjectName("readyLabel");

    auto *form = new QFormLayout();

    m_modeCombo = new QComboBox();
    m_modeCombo->addItems({
        QStringLiteral("统一出清价（MCP）"),
        QStringLiteral("按报价结算（PAB）"),
    });
    m_modeCombo->setToolTip(QStringLiteral("创新点②：双模式报价出清，依据细则第 63 条"));
    form->addRow(QStringLiteral("报价模式："), m_modeCombo);

    m_granCombo = new QComboBox();
    m_granCombo->addItems({
        QStringLiteral("24 时段（每 1 小时）"),
        QStringLiteral("96 时段（每 15 分钟）"),
    });
    m_granCombo->setToolTip(QStringLiteral("创新点③：96 时段连续出清，依据规则 4.1 条"));
    form->addRow(QStringLiteral("时段颗粒度："), m_granCombo);

    // 模式 / 颗粒度变化 → P5 文件名预览联动
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateFileNamePreviews();
    });
    connect(m_granCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateFileNamePreviews();
    });

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 50);
    slider->setValue(20);
    auto *valLabel = new QLabel(QStringLiteral("20 %"));
    auto *sliderRow = new QHBoxLayout();
    sliderRow->addWidget(slider, 1);
    sliderRow->addWidget(valLabel);
    connect(slider, &QSlider::valueChanged, valLabel, [valLabel](int v) {
        valLabel->setText(QStringLiteral("%1 %").arg(v));
    });
    form->addRow(QStringLiteral("新能源渗透率："), sliderRow);

    auto *runBtn = new QPushButton(QStringLiteral("▶  开始仿真"));
    runBtn->setToolTip(QStringLiteral(
        "数据未就绪时此按钮置灰并提示「请先保存申报数据」（当前内置示例已就绪）"));
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::onRunSim);

    auto *hint = new QLabel(QStringLiteral(
        "骨架版说明：点击「开始仿真」用内置假数据生成一次出清结果并跳转查看。\n"
        "真实出清引擎（B 模块：MCP 边际电价法 / PAB 报价撮合法）接入后，这里将驱动一次完整计算。"));
    hint->setObjectName("hintLabel");

    auto *lay = new QVBoxLayout(box);
    lay->addWidget(readyLabel);
    lay->addSpacing(6);
    lay->addLayout(form);
    lay->addSpacing(12);
    lay->addWidget(runBtn, 0, Qt::AlignLeft);
    lay->addSpacing(8);
    lay->addWidget(hint);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(box);
    outer->addStretch();
    return page;
}

// ============================================================
// 页面 3：出清结果（空态卡片 / 指标卡 + 24 时段表）
// ============================================================
QWidget *MainWindow::buildResultPage()
{
    auto *page = new QWidget();
    m_resultStack = new QStackedWidget(page);

    // --- 0：空态引导卡片（无出清结果时显示，对应平台说明 5.6） ---
    m_resultStack->addWidget(buildEmptyCard());

    // --- 1：内容页：指标卡 + 结果表 ---
    auto *content = new QWidget();

    // 指标卡一行四张（v1.1：P3 指标卡）
    auto *kpiRow = new QWidget(content);
    auto *kpiLay = new QHBoxLayout(kpiRow);
    kpiLay->setContentsMargins(0, 0, 0, 0);
    struct Kpi { QLabel **value; const char *name; };
    const Kpi kpis[4] = {
                          {&m_kpiAvg,    "日均出清电价"},
                          {&m_kpiMax,    "最高出清电价"},
                          {&m_kpiMin,    "最低出清电价"},
                          {&m_kpiSpread, "峰谷价差比"},
                          };
    for (const auto &k : kpis) {
        auto *card = new QWidget(kpiRow);
        card->setObjectName("kpiCard");
        auto *v = new QLabel(QStringLiteral("--"), card);
        v->setObjectName("kpiValue");
        auto *n = new QLabel(QString::fromUtf8(k.name), card);
        n->setObjectName("kpiName");
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(14, 10, 14, 10);
        l->addWidget(v);
        l->addWidget(n);
        *k.value = v;
        kpiLay->addWidget(card, 1);
    }

    // 24 时段结果表
    auto *box = new QGroupBox(
        QStringLiteral("24 时段出清结果（骨架版 · 假数据 · 双驼峰 + 午间光伏压价）"), content);

    m_resultTable = new QTableWidget(24, 5, box);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("时段"),
        QStringLiteral("出清电价 (元/MWh)"),
        QStringLiteral("出清电量 (MW)"),
        QStringLiteral("新能源出力 (MW)"),
        QStringLiteral("备注"),
    });
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *boxLay = new QVBoxLayout(box);
    boxLay->addWidget(m_resultTable);

    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    lay->addWidget(kpiRow);
    lay->addWidget(box, 1);

    m_resultStack->addWidget(content);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_resultStack);
    return page;
}

// ============================================================
// 页面 4：图表分析（空态卡片 / 双页签：供需交叉 + 分时电价）
// ============================================================
QWidget *MainWindow::buildChartPage()
{
    auto *page = new QWidget();
    m_chartStack = new QStackedWidget(page);

    // --- 0：空态引导卡片 ---
    m_chartStack->addWidget(buildEmptyCard());

    // --- 1：内容页：QTabWidget 双页签 ---
    auto *content = new QWidget();
    auto *tabs = new QTabWidget(content);

    // 页签 1：供需交叉图（供给红 × 需求蓝，交点即出清价）
    {
        auto *box = new QGroupBox(QStringLiteral("供需曲线与出清点（示意 · 假数据）"));
        auto *chart = new QChart();

        auto *supply = new QLineSeries();
        supply->setName(QStringLiteral("供给曲线"));
        supply->setColor(QColor(0xE2, 0x4B, 0x4A));   // 供给红 #E24B4A

        auto *demand = new QLineSeries();
        demand->setName(QStringLiteral("需求曲线"));
        demand->setColor(QColor(0x37, 0x8A, 0xDD));   // 需求蓝 #378ADD

        for (int t = 1; t <= 24; ++t) {
            supply->append(t, 300.0 + t * 6.0);       // 供给随报价递增
            demand->append(t, 570.0 - t * 8.0);       // 需求随价格递减
        }
        chart->addSeries(supply);
        chart->addSeries(demand);

        auto *axisX = new QValueAxis();
        axisX->setRange(1, 24);
        axisX->setTitleText(QStringLiteral("申报电量排序"));
        auto *axisY = new QValueAxis();
        axisY->setRange(200, 700);
        axisY->setTitleText(QStringLiteral("报价 (元/MWh)"));
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        supply->attachAxis(axisX);
        supply->attachAxis(axisY);
        demand->attachAxis(axisX);
        demand->attachAxis(axisY);

        chart->setTitle(QStringLiteral("供需曲线交叉 → 出清电价（机制可视化）"));
        chart->legend()->setAlignment(Qt::AlignBottom);

        auto *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);

        auto *lay = new QVBoxLayout(box);
        lay->addWidget(view);
        tabs->addTab(box, QStringLiteral("供需交叉图"));
    }

    // 页签 2：分时电价曲线（电价蓝 + 新能源出力绿，双 Y 轴）
    {
        auto *box = new QGroupBox(QStringLiteral("分时电价与新能源出力（骨架版 · 假数据）"));
        auto *chart = new QChart();

        m_priceSeries = new QLineSeries();
        m_priceSeries->setName(QStringLiteral("出清电价 (元/MWh)"));
        m_priceSeries->setColor(QColor(0x18, 0x5F, 0xA5));   // 电力蓝 #185FA5

        m_renewSeries = new QLineSeries();
        m_renewSeries->setName(QStringLiteral("新能源出力 (MW)"));
        m_renewSeries->setColor(QColor(0x2F, 0xA8, 0x4F));   // 新能源绿 #2FA84F

        chart->addSeries(m_priceSeries);
        chart->addSeries(m_renewSeries);

        auto *axisX = new QValueAxis();
        axisX->setRange(1, 24);
        axisX->setTitleText(QStringLiteral("时段 (h)"));
        axisX->setTickCount(13);

        auto *axisLeft = new QValueAxis();
        axisLeft->setRange(0, 600);
        axisLeft->setTitleText(QStringLiteral("电价 (元/MWh)"));

        auto *axisRight = new QValueAxis();
        axisRight->setRange(0, 400);
        axisRight->setTitleText(QStringLiteral("新能源出力 (MW)"));

        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisLeft, Qt::AlignLeft);
        chart->addAxis(axisRight, Qt::AlignRight);
        m_priceSeries->attachAxis(axisX);
        m_priceSeries->attachAxis(axisLeft);
        m_renewSeries->attachAxis(axisX);
        m_renewSeries->attachAxis(axisRight);

        chart->setTitle(QStringLiteral("早晚高峰电价抬升 · 午间光伏压价 —— 典型日形态"));
        chart->legend()->setAlignment(Qt::AlignBottom);

        auto *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);

        auto *lay = new QVBoxLayout(box);
        lay->addWidget(view);
        tabs->addTab(box, QStringLiteral("分时电价曲线"));
    }

    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(tabs);

    m_chartStack->addWidget(content);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_chartStack);
    return page;
}

// ============================================================
// 页面 5：结果导出（空态卡片 / 摘要 + 双导出 + 导出记录）
// ============================================================
QWidget *MainWindow::buildExportPage()
{
    auto *page = new QWidget();
    m_exportStack = new QStackedWidget(page);

    // --- 0：空态引导卡片 ---
    m_exportStack->addWidget(buildEmptyCard());

    // --- 1：内容页 ---
    auto *content = new QWidget();

    // 上：结算日报摘要（六项指标，由假数据计算）
    auto *box = new QGroupBox(QStringLiteral("结算日报摘要（骨架版 · 由内置假数据计算）"), content);
    auto *grid = new QGridLayout();
    struct Sum { QLabel **value; const char *name; };
    const Sum sums[6] = {
                          {&m_sumAvg,    "日均出清电价"},
                          {&m_sumMax,    "最高出清电价"},
                          {&m_sumMin,    "最低出清电价"},
                          {&m_sumVol,    "全天总出清电量"},
                          {&m_sumFee,    "全天结算总额"},
                          {&m_sumSpread, "峰谷价差比"},
                          };
    for (int i = 0; i < 6; ++i) {
        auto *n = new QLabel(QString::fromUtf8(sums[i].name));
        auto *v = new QLabel(QStringLiteral("--"));
        n->setObjectName("sumName");
        v->setObjectName("sumValue");
        grid->addWidget(n, i / 2, (i % 2) * 2);
        grid->addWidget(v, i / 2, (i % 2) * 2 + 1);
        *sums[i].value = v;
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    auto *boxLay = new QVBoxLayout(box);
    boxLay->addLayout(grid);

    // 中：文件名预览（随模式 / 颗粒度联动）
    m_fileDaily = new QLabel(box);
    m_fileCurve = new QLabel(box);
    auto *fileBox = new QGroupBox(QStringLiteral("导出文件名预览（自动生成，可改名）"), content);
    auto *fileLay = new QVBoxLayout(fileBox);
    fileLay->addWidget(m_fileDaily);
    fileLay->addWidget(m_fileCurve);

    // 下：双导出按钮 + 导出记录
    auto *dailyBtn = new QPushButton(QStringLiteral("⬇ 导出结算日报 CSV"), content);
    auto *curveBtn = new QPushButton(QStringLiteral("⬇ 导出电价曲线数据 CSV"), content);
    dailyBtn->setObjectName("demoBtn");
    curveBtn->setObjectName("demoBtn");
    connect(dailyBtn, &QPushButton::clicked, this, &MainWindow::onExportDaily);
    connect(curveBtn, &QPushButton::clicked, this, &MainWindow::onExportCurve);

    auto *logBox = new QGroupBox(QStringLiteral("导出记录（本次会话）"), content);
    m_exportLog = new QListWidget(logBox);
    m_exportLog->setObjectName("exportLog");
    auto *logLay = new QVBoxLayout(logBox);
    logLay->addWidget(m_exportLog);
    logBox->setMaximumHeight(120);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(dailyBtn);
    btnRow->addWidget(curveBtn);
    btnRow->addStretch();

    auto *hint = new QLabel(QStringLiteral(
        "骨架版说明：导出内容为内置假数据；接入 B 模块真实结算数据后，"
        "日报字段将对标官方结算口径（时段 / 电价 / 中标电量 / 结算费用 + 日汇总行）。"));
    hint->setObjectName("hintLabel");

    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    lay->addWidget(box);
    lay->addWidget(fileBox);
    lay->addLayout(btnRow);
    lay->addWidget(logBox);
    lay->addWidget(hint);

    m_exportStack->addWidget(content);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_exportStack);
    return page;
}

// ============================================================
// 空态引导卡片（P3 / P4 / P5 共用：导航永不锁死，缺什么就引导什么）
// ============================================================
QWidget *MainWindow::buildEmptyCard()
{
    auto *card = new QWidget();
    card->setObjectName("emptyCard");

    auto *icon = new QLabel(QStringLiteral("◎"), card);
    icon->setObjectName("emptyIcon");
    auto *title = new QLabel(QStringLiteral("暂无出清结果"), card);
    title->setObjectName("emptyTitle");
    auto *hint = new QLabel(QStringLiteral(
                                "先完成一次仿真，或直接用内置案例体验完整流程"), card);
    hint->setObjectName("emptyHint");

    auto *demoBtn = new QPushButton(QStringLiteral("⚡ 一键演示"), card);
    demoBtn->setObjectName("demoBtn");
    connect(demoBtn, &QPushButton::clicked, this, &MainWindow::onStartDemo);

    auto *goBtn = new QPushButton(QStringLiteral("前往仿真控制"), card);
    goBtn->setObjectName("secondaryBtn");
    connect(goBtn, &QPushButton::clicked, this, [this] {
        m_nav->setCurrentRow(1);
    });

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(demoBtn);
    btnRow->addSpacing(12);
    btnRow->addWidget(goBtn);
    btnRow->addStretch();

    auto *lay = new QVBoxLayout(card);
    lay->addStretch();
    lay->addWidget(icon, 0, Qt::AlignHCenter);
    lay->addSpacing(6);
    lay->addWidget(title, 0, Qt::AlignHCenter);
    lay->addSpacing(4);
    lay->addWidget(hint, 0, Qt::AlignHCenter);
    lay->addSpacing(14);
    lay->addLayout(btnRow);
    lay->addStretch();
    return card;
}

// ============================================================
// 假出清数据：负荷双驼峰 + 午间光伏压价 → 24 时段出清结果
// （三页共用这一份数据源；接入 B 模块后由真实出清引擎替换）
// ============================================================
void MainWindow::generateResults()
{
    const int T = 24;
    m_price.clear(); m_vol.clear(); m_load.clear(); m_renew.clear(); m_net.clear();
    m_price.reserve(T); m_vol.reserve(T); m_load.reserve(T);
    m_renew.reserve(T); m_net.reserve(T);

    for (int t = 1; t <= T; ++t) {
        const double hump1 = std::exp(-std::pow(t - 10, 2) / 8.0);   // 早高峰
        const double hump2 = std::exp(-std::pow(t - 19, 2) / 6.0);   // 晚高峰
        const double pv    = 200.0 * std::exp(-std::pow(t - 13, 2) / 6.0);  // 午间光伏
        const double wind  = 90.0 + 60.0 * std::sin(t * 0.8);        // 风电波动

        const double load = 700.0 + 350.0 * (0.6 * hump1 + hump2);   // 双驼峰负荷
        const double renew = pv + wind;
        const double net = load - renew;                              // 净负荷
        double price = 260.0 + 0.30 * (net - 550.0);                 // 净负荷定价
        price = std::max(180.0, std::min(520.0, price));

        m_load.append(load);
        m_renew.append(renew);
        m_net.append(net);
        m_price.append(price);
        m_vol.append(net);   // 骨架版：出清电量 = 净负荷
    }
}

void MainWindow::setHasResult(bool on)
{
    if (on && !m_hasResult) {
        generateResults();
        refreshResultPage();
        refreshChartPage();
        refreshExportPage();
    }
    m_hasResult = on;
    m_resultStack->setCurrentIndex(on ? 1 : 0);
    m_chartStack->setCurrentIndex(on ? 1 : 0);
    m_exportStack->setCurrentIndex(on ? 1 : 0);
}

void MainWindow::refreshResultPage()
{
    // 指标卡
    double sum = 0.0, mx = -1e9, mn = 1e9;
    int iMax = 0, iMin = 0;
    for (int i = 0; i < m_price.size(); ++i) {
        sum += m_price[i];
        if (m_price[i] > mx) { mx = m_price[i]; iMax = i; }
        if (m_price[i] < mn) { mn = m_price[i]; iMin = i; }
    }
    const double avg = sum / m_price.size();
    m_kpiAvg->setText(QStringLiteral("%1 元/MWh").arg(avg, 0, 'f', 1));
    m_kpiMax->setText(QStringLiteral("%1（%2 时段）").arg(mx, 0, 'f', 1).arg(iMax + 1));
    m_kpiMin->setText(QStringLiteral("%1（%2 时段）").arg(mn, 0, 'f', 1).arg(iMin + 1));
    m_kpiSpread->setText(QStringLiteral("%1").arg(mx / mn, 0, 'f', 2));

    // 结果表
    for (int t = 1; t <= 24; ++t) {
        const int row = t - 1;
        QString note;
        if (t == 10)      note = QStringLiteral("早高峰");
        else if (t == 13) note = QStringLiteral("午间光伏压价");
        else if (t == 19) note = QStringLiteral("晚高峰");

        m_resultTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("%1").arg(t)));
        m_resultTable->setItem(row, 1, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(m_price[row], 0, 'f', 1)));
        m_resultTable->setItem(row, 2, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(m_vol[row], 0, 'f', 1)));
        m_resultTable->setItem(row, 3, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(m_renew[row], 0, 'f', 1)));
        m_resultTable->setItem(row, 4, new QTableWidgetItem(note));
    }
}

void MainWindow::refreshChartPage()
{
    if (!m_priceSeries || !m_renewSeries)
        return;
    m_priceSeries->clear();
    m_renewSeries->clear();
    for (int t = 1; t <= 24; ++t) {
        m_priceSeries->append(t, m_price[t - 1]);
        m_renewSeries->append(t, m_renew[t - 1]);
    }
}

void MainWindow::refreshExportPage()
{
    double sum = 0.0, mx = -1e9, mn = 1e9, volSum = 0.0;
    for (int i = 0; i < m_price.size(); ++i) {
        sum += m_price[i];
        volSum += m_vol[i];
        if (m_price[i] > mx) mx = m_price[i];
        if (m_price[i] < mn) mn = m_price[i];
    }
    const double avg = sum / m_price.size();
    const double fee = [this] {
        double f = 0.0;
        for (int i = 0; i < m_price.size(); ++i)
            f += m_price[i] * m_vol[i];
        return f;
    }();

    m_sumAvg->setText(QStringLiteral("%1 元/MWh").arg(avg, 0, 'f', 1));
    m_sumMax->setText(QStringLiteral("%1 元/MWh").arg(mx, 0, 'f', 1));
    m_sumMin->setText(QStringLiteral("%1 元/MWh").arg(mn, 0, 'f', 1));
    m_sumVol->setText(QStringLiteral("%1 MWh").arg(volSum, 0, 'f', 0));
    m_sumFee->setText(QStringLiteral("¥ %1").arg(fee, 0, 'f', 0));
    m_sumSpread->setText(QStringLiteral("%1").arg(mx / mn, 0, 'f', 2));

    updateFileNamePreviews();
}

void MainWindow::updateFileNamePreviews()
{
    const QString date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    const QString mode = (m_modeCombo && m_modeCombo->currentIndex() == 1)
                             ? QStringLiteral("PAB") : QStringLiteral("MCP");
    const QString gran = (m_granCombo && m_granCombo->currentIndex() == 1)
                             ? QStringLiteral("96时段") : QStringLiteral("24时段");
    if (m_fileDaily)
        m_fileDaily->setText(QStringLiteral("结算日报_%1_%2_%3.csv").arg(date, mode, gran));
    if (m_fileCurve)
        m_fileCurve->setText(QStringLiteral("电价曲线_%1_%2_%3.csv").arg(date, mode, gran));
}

void MainWindow::addExportRecord(const QString &fileName)
{
    if (!m_exportLog)
        return;
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    m_exportLog->insertItem(0, QStringLiteral("[%1]  %2").arg(time, fileName));
}

// ============================================================
// 槽函数
// ============================================================
void MainWindow::onStartDemo()
{
    setHasResult(true);
    m_nav->setCurrentRow(2);   // 跳到出清结果页
    statusBar()->showMessage(
        QStringLiteral("一键演示：已加载内置基准案例并完成一次出清（骨架版 · 假数据）。"
                       "真实一键演示（内置基准例 200/140/56000）待 A 样例 + B 算法接入。"), 6000);
}

void MainWindow::onRunSim()
{
    setHasResult(true);
    m_nav->setCurrentRow(2);
    statusBar()->showMessage(
        QStringLiteral("骨架版：仿真完成（假数据）。真实出清引擎待 B 模块接入。"), 5000);
}

void MainWindow::onExportDaily()
{
    if (m_fileDaily == nullptr)
        return;
    const QString suggested = m_fileDaily->text();
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出结算日报"), suggested, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(QStringLiteral("导出失败：无法打开文件 %1").arg(path), 6000);
        return;
    }
    QTextStream out(&f);
    out << QChar(0xFEFF);   // UTF-8 BOM：保证 Excel 直接打开不乱码
    out << QStringLiteral("时段,出清电价(元/MWh),中标电量(MW),结算费用(元)\n");
    double volSum = 0.0, feeSum = 0.0, priceSum = 0.0;
    for (int t = 1; t <= m_price.size(); ++t) {
        const double fee = m_price[t - 1] * m_vol[t - 1];
        out << t << ',' << QString::number(m_price[t - 1], 'f', 1) << ','
            << QString::number(m_vol[t - 1], 'f', 1) << ','
            << QString::number(fee, 'f', 1) << '\n';
        volSum += m_vol[t - 1];
        feeSum += fee;
        priceSum += m_price[t - 1];
    }
    out << QStringLiteral("汇总,,%1,%2\n").arg(QString::number(volSum, 'f', 1),
                                               QString::number(feeSum, 'f', 1));
    out << QStringLiteral("日均电价(元/MWh),%1\n").arg(QString::number(priceSum / m_price.size(), 'f', 1));
    f.close();

    addExportRecord(QFileInfo(path).fileName());
    statusBar()->showMessage(QStringLiteral("已导出：%1").arg(path), 8000);
}

void MainWindow::onExportCurve()
{
    if (m_fileCurve == nullptr)
        return;
    const QString suggested = m_fileCurve->text();
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出电价曲线数据"), suggested, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(QStringLiteral("导出失败：无法打开文件 %1").arg(path), 6000);
        return;
    }
    QTextStream out(&f);
    out << QChar(0xFEFF);   // UTF-8 BOM
    out << QStringLiteral("时段,出清电价(元/MWh),负荷(MW),新能源出力(MW),净负荷(MW)\n");
    for (int t = 1; t <= m_price.size(); ++t) {
        out << t << ',' << QString::number(m_price[t - 1], 'f', 1) << ','
            << QString::number(m_load[t - 1], 'f', 1) << ','
            << QString::number(m_renew[t - 1], 'f', 1) << ','
            << QString::number(m_net[t - 1], 'f', 1) << '\n';
    }
    f.close();

    addExportRecord(QFileInfo(path).fileName());
    statusBar()->showMessage(QStringLiteral("已导出：%1").arg(path), 8000);
}

// ============================================================
// 全局 QSS：电力蓝 #185FA5 + 琥珀橙 #EF9F27 设计系统
// ============================================================
void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #F1F1F4; }

        #titleLabel {
            color: #185FA5;
            font-size: 20px;
            font-weight: bold;
        }

        /* 左侧导航 */
        #navList {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 8px;
            font-size: 14px;
            padding: 6px;
            outline: 0;
        }
        #navList::item {
            height: 44px;
            border-radius: 6px;
            padding-left: 12px;
            color: #3A3A42;
        }
        #navList::item:hover { background: #E8F0FA; }
        #navList::item:selected {
            background: #185FA5;
            color: #FFFFFF;
            font-weight: bold;
        }

        /* 通用按钮（电力蓝） */
        QPushButton {
            background: #185FA5;
            color: #FFFFFF;
            border: none;
            border-radius: 6px;
            padding: 8px 18px;
            font-size: 14px;
        }
        QPushButton:hover  { background: #124A87; }
        QPushButton:pressed { background: #0E3D6F; }
        QPushButton:disabled { background: #B9C6D6; }

        /* 次级按钮（白底描边） */
        #secondaryBtn {
            background: #FFFFFF;
            color: #185FA5;
            border: 1px solid #185FA5;
        }
        #secondaryBtn:hover  { background: #E8F0FA; }
        #secondaryBtn:pressed { background: #D5E4F5; }

        /* 强调按钮（琥珀橙：一键演示 / 导出） */
        #demoBtn {
            background: #EF9F27;
            font-size: 15px;
            font-weight: bold;
            padding: 9px 22px;
        }
        #demoBtn:hover  { background: #D98E1B; }
        #demoBtn:pressed { background: #C07E14; }

        /* 分组容器 */
        QGroupBox {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 8px;
            margin-top: 14px;
            font-weight: bold;
            color: #185FA5;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 4px;
        }

        /* 表格 */
        QTableWidget {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            gridline-color: #EDEDF2;
            font-size: 13px;
            selection-background-color: #E8F0FA;
            selection-color: #1A1A1A;
        }
        QHeaderView::section {
            background: #185FA5;
            color: #FFFFFF;
            font-weight: bold;
            padding: 6px;
            border: none;
        }

        /* 页签（P4 图表分析） */
        QTabWidget::pane {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 6px;
        }
        QTabBar::tab {
            background: #F1F1F4;
            color: #3A3A42;
            padding: 7px 20px;
            border: 1px solid #E2E2E8;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-size: 13px;
            margin-right: 3px;
        }
        QTabBar::tab:selected {
            background: #FFFFFF;
            color: #185FA5;
            font-weight: bold;
        }

        /* 指标卡（P3） */
        #kpiCard {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 8px;
        }
        #kpiValue {
            color: #185FA5;
            font-size: 21px;
            font-weight: bold;
        }
        #kpiName { color: #6A6A73; font-size: 12px; }

        /* 空态引导卡片（P3/P4/P5 共用） */
        #emptyCard {
            background: #FFFFFF;
            border: 1px dashed #C9C9D2;
            border-radius: 12px;
        }
        #emptyIcon {
            color: #C9C9D2;
            font-size: 40px;
        }
        #emptyTitle {
            color: #3A3A42;
            font-size: 18px;
            font-weight: bold;
        }
        #emptyHint { color: #8A8A93; font-size: 13px; }

        /* 数据就绪灯 / 导出记录 */
        #readyLabel { font-size: 14px; color: #3A3A42; }
        #exportLog {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 6px;
            font-size: 12px;
            color: #3A3A42;
            padding: 4px;
            outline: 0;
        }

        /* 下拉框 / 滑条 / 状态栏 */
        QComboBox {
            background: #FFFFFF;
            border: 1px solid #C9C9D2;
            border-radius: 6px;
            padding: 6px 12px;
            font-size: 14px;
        }
        QComboBox::drop-down { border: none; width: 26px; }
        QSlider::groove:horizontal {
            height: 6px;
            background: #E2E2E8;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 16px; height: 16px;
            margin: -5px 0;
            border-radius: 8px;
            background: #185FA5;
        }
        QStatusBar {
            background: #FFFFFF;
            border-top: 1px solid #E2E2E8;
            color: #6A6A73;
            font-size: 12px;
        }

        /* 说明文字 / 摘要 */
        #hintLabel { color: #8A8A93; font-size: 12px; font-weight: normal; }
        #sumName   { color: #6A6A73; font-size: 14px; }
        #sumValue  { color: #185FA5; font-size: 14px; font-weight: bold; }
    )"));
}
