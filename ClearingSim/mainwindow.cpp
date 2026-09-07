#include "mainwindow.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTime>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QTime>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

#include "core/clearing_facade.h"
#include "core/market_view.h"
#include "data/data_reader.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("电力现货市场出清仿真平台"));
    resize(1180, 760);

    // ---------- 顶部栏：页面标题 banner + 三视角切换条 + 数据就绪灯 + 一键演示 ----------
    m_pageTitle = new QLabel(QStringLiteral("数据导入"));
    m_pageTitle->setObjectName("pageTitle");
    m_pageSub = new QLabel(QStringLiteral("申报数据 · 数据集状态 · 真实校验"));
    m_pageSub->setObjectName("pageSub");

    auto *headerBox = new QFrame();
    headerBox->setObjectName("pageHeader");
    auto *headCol = new QVBoxLayout(headerBox);
    headCol->setContentsMargins(18, 12, 24, 12);
    headCol->setSpacing(2);
    headCol->addWidget(m_pageTitle);
    headCol->addWidget(m_pageSub);

    auto *readyDot = new QLabel(QStringLiteral(
        "<span style='color:#2FA84F;'>●</span> 数据就绪"));
    readyDot->setObjectName("readyDot");
    readyDot->setTextFormat(Qt::RichText);

    auto *demoBtn = new QPushButton(QStringLiteral("⚡ 一键演示"));
    demoBtn->setObjectName("outlineDemoBtn");
    demoBtn->setToolTip(QStringLiteral("加载内置基准例（G1 100/40 · G2 250/60），单时段对拍出清：250 元/MWh / 50 MWh / 12500 元"));
    connect(demoBtn, &QPushButton::clicked, this, &MainWindow::onStartDemo);

    auto *topRight = new QVBoxLayout();
    topRight->setSpacing(10);
    topRight->addStretch();
    topRight->addWidget(readyDot, 0, Qt::AlignRight);
    topRight->addWidget(demoBtn, 0, Qt::AlignRight);
    topRight->addStretch();

    auto *topBar = new QHBoxLayout();
    topBar->setContentsMargins(0, 0, 0, 0);
    topBar->setSpacing(12);
    topBar->addWidget(headerBox, 1);
    topBar->addWidget(buildPerspectiveBar());   // 三视角切换条（免登录 · 互斥）
    topBar->addLayout(topRight);

    // ---------- 左侧导航：5 个页面 ----------
    auto *navCol = new QVBoxLayout();
    navCol->setContentsMargins(0, 0, 0, 0);
    navCol->setSpacing(6);

    m_nav = new QListWidget();
    m_nav->setObjectName("navList");
    m_nav->setFixedWidth(210);
    m_nav->addItems({
        QStringLiteral("①  数据导入"),
        QStringLiteral("②  仿真控制"),
        QStringLiteral("③  出清结果"),
        QStringLiteral("④  图表分析"),
        QStringLiteral("⑤  结果导出"),
    });
    navCol->addWidget(m_nav);

    auto *navFooter = new QLabel(QStringLiteral("V 1.1 · 真实数据模式"));
    navFooter->setObjectName("navFooter");
    navCol->addSpacing(6);
    navCol->addWidget(navFooter);
    navCol->addStretch();

    // ---------- 右侧内容区 ----------
    m_stack = new QStackedWidget();
    m_stack->addWidget(buildImportPage());
    m_stack->addWidget(buildControlPage());
    m_stack->addWidget(buildResultPage());
    m_stack->addWidget(buildChartPage());
    m_stack->addWidget(buildExportPage());

    connect(m_nav, &QListWidget::currentRowChanged,
            m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_nav, &QListWidget::currentRowChanged,
            this, &MainWindow::updatePageHeader);
    m_nav->setCurrentRow(0);

    // ---------- 整体布局 ----------
    auto *body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->addLayout(navCol);
    body->addSpacing(10);
    body->addWidget(m_stack, 1);

    auto *mainWidget = new QWidget();
    auto *root = new QVBoxLayout(mainWidget);
    root->setContentsMargins(16, 12, 16, 8);
    root->addLayout(topBar);
    root->addSpacing(8);
    root->addLayout(body, 1);

    // ---------- 顶层页面栈：0 = 封面 / 1 = 主界面 ----------
    m_rootStack = new QStackedWidget();
    m_rootStack->addWidget(buildCoverPage());
    m_rootStack->addWidget(mainWidget);
    m_rootStack->setCurrentIndex(0);

    setCentralWidget(m_rootStack);

    statusBar()->showMessage(QStringLiteral("就绪 · 数据接入 A 模块 · 出清已接 B 位真实算法（MCP/PAB 双模式）"));

    applyStyle();
}

// ============================================================
// 封面底部装饰：淡淡的电价/新能源曲线底纹
// ============================================================
namespace {
class CurveBackdrop : public QWidget
{
public:
    explicit CurveBackdrop(QWidget *parent = nullptr) : QWidget(parent) {}
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = width(), h = height();

        p.setPen(QPen(QColor(0x5D, 0xCA, 0xA5, 60), 2.0));
        p.drawLine(0, h * 0.85, w, h * 0.85);
        QPainterPath load;
        load.moveTo(0, h * 0.80);
        for (int x = 0; x <= w; x += 6) {
            const double t = double(x) / w;
            const double y = 0.72
                             - 0.16 * std::exp(-std::pow((t - 0.42) * 4.2, 2))
                             - 0.20 * std::exp(-std::pow((t - 0.79) * 4.6, 2))
                             + 0.10 * std::exp(-std::pow((t - 0.55) * 3.2, 2));
            load.lineTo(x, h * y);
        }
        p.drawPath(load);

        p.setPen(QPen(QColor(0x85, 0xB7, 0xEB, 50), 1.5));
        QPainterPath pv;
        pv.moveTo(0, h * 0.95);
        for (int x = 0; x <= w; x += 6) {
            const double t = double(x) / w;
            const double y = 0.93 - 0.14 * std::exp(-std::pow((t - 0.55) * 4.0, 2));
            pv.lineTo(x, h * y);
        }
        p.drawPath(pv);
    }
};

class CoverPage : public QWidget
{
public:
    explicit CoverPage(QWidget *parent = nullptr) : QWidget(parent) {}
    void setBackdrop(QWidget *b) { m_backdrop = b; }
protected:
    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        if (m_backdrop)
            m_backdrop->setGeometry(0, height() / 2, width(), height() / 2);
    }
private:
    QWidget *m_backdrop = nullptr;
};
} // namespace

// ============================================================
// 页面 0：启动封面
// ============================================================
QWidget *MainWindow::buildCoverPage()
{
    auto *page = new CoverPage();
    page->setObjectName("coverPage");

    auto *backdrop = new CurveBackdrop(page);
    page->setBackdrop(backdrop);
    backdrop->lower();

    auto *brandIcon = new QLabel(QStringLiteral("⚡"), page);
    brandIcon->setObjectName("coverBrandIcon");
    auto *brandText = new QLabel(QStringLiteral("SEU · 电力市场课程设计"), page);
    brandText->setObjectName("coverBrandText");

    auto *version = new QLabel(QStringLiteral("V 1.1"), page);
    version->setObjectName("coverVersion");
    version->setAlignment(Qt::AlignCenter);

    auto *brandRow = new QHBoxLayout();
    brandRow->addWidget(brandIcon);
    brandRow->addSpacing(8);
    brandRow->addWidget(brandText);
    brandRow->addStretch();
    brandRow->addWidget(version);

    auto *eyebrow = new QLabel(QStringLiteral("SPOT MARKET · CLEARING SIMULATION"), page);
    eyebrow->setObjectName("coverEyebrow");
    eyebrow->setAlignment(Qt::AlignHCenter);

    auto *title = new QLabel(QStringLiteral("电力现货市场出清仿真平台"), page);
    title->setObjectName("coverTitle");
    title->setAlignment(Qt::AlignHCenter);

    auto *sub = new QLabel(
        QStringLiteral("Spot Electricity Market Clearing Simulation Platform"), page);
    sub->setObjectName("coverSub");
    sub->setAlignment(Qt::AlignHCenter);

    auto *rule = new QFrame(page);
    rule->setObjectName("coverRule");
    rule->setFixedSize(56, 3);

    auto *tagline = new QLabel(
        QStringLiteral("三视角切换 · MCP / PAB 双模式 · 96 时段逐时仿真 · 单窗口五页面闭环"), page);
    tagline->setObjectName("coverTagline");
    tagline->setAlignment(Qt::AlignHCenter);

    struct Feature { const char *top; const char *name; const char *desc; };
    const Feature feats[3] = {
                               {"featBlue",  "MCP / PAB 双模式", "边际出清与按报价结算，覆盖两类典型出清机制"},
                               {"featGreen", "96 时段逐时出清",  "日前现货 15 分钟颗粒度，全日 96 点连续仿真"},
                               {"featAmber", "一键演示数据",      "内置基准案例，答辩演示无需现场准备数据"},
                               };
    auto *featRow = new QHBoxLayout();
    featRow->setSpacing(14);
    for (const auto &f : feats) {
        auto *card = new QWidget(page);
        card->setObjectName("coverFeatCard");
        auto *bar = new QFrame(card);
        bar->setObjectName(QString::fromUtf8(f.top));
        bar->setFixedHeight(3);
        bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *name = new QLabel(QString::fromUtf8(f.name), card);
        name->setObjectName("coverFeatName");
        auto *desc = new QLabel(QString::fromUtf8(f.desc), card);
        desc->setObjectName("coverFeatDesc");
        desc->setWordWrap(true);
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(16, 12, 16, 12);
        l->setSpacing(6);
        l->addWidget(bar);
        l->addWidget(name);
        l->addWidget(desc);
        featRow->addWidget(card, 1);
    }

    auto *enterBtn = new QPushButton(QStringLiteral("进入平台 →"), page);
    enterBtn->setObjectName("enterBtn");
    connect(enterBtn, &QPushButton::clicked, this, [this] {
        m_rootStack->setCurrentIndex(1);
        m_nav->setCurrentRow(0);
        statusBar()->showMessage(
            QStringLiteral("已进入平台 · 可在①数据导入加载申报数据（内置样例或 CSV）"), 5000);
    });

    auto *enterHint = new QLabel(
        QStringLiteral("首次使用可点击主界面右上角「一键演示」快速体验对拍基准例"), page);
    enterHint->setObjectName("coverEnterHint");
    enterHint->setAlignment(Qt::AlignHCenter);

    auto *footer = new QLabel(QStringLiteral("东南大学 · C++ 课程设计 · 2026"), page);
    footer->setObjectName("coverFooter");
    footer->setAlignment(Qt::AlignHCenter);

    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(48, 28, 48, 20);
    lay->addLayout(brandRow);
    lay->addStretch();
    lay->addWidget(eyebrow);
    lay->addSpacing(8);
    lay->addWidget(title);
    lay->addSpacing(10);
    lay->addWidget(sub);
    lay->addSpacing(16);
    lay->addWidget(rule, 0, Qt::AlignHCenter);
    lay->addSpacing(16);
    lay->addWidget(tagline);
    lay->addSpacing(26);
    lay->addLayout(featRow);
    lay->addSpacing(32);
    lay->addWidget(enterBtn, 0, Qt::AlignHCenter);
    lay->addSpacing(10);
    lay->addWidget(enterHint);
    lay->addStretch();
    lay->addWidget(footer, 0, Qt::AlignHCenter);
    return page;
}

// ============================================================
// 页面 1：数据导入（A 模块真实读取 + 校验 + 三视角过滤）
// ============================================================
QWidget *MainWindow::buildImportPage()
{
    auto *page = new QWidget();

    // --- 上：申报数据双页签（发电侧 / 购电侧，随视角隐藏） ---
    auto *bidBox = new QGroupBox(QStringLiteral("申报数据（A 模块 · 真实读取与校验）"), page);

    m_importTabs = new QTabWidget(bidBox);

    // #89：交易时段选择行（96 期原生粒度，编辑/预填都针对单时段）
    auto *periodRow = new QHBoxLayout();
    auto *periodLbl = new QLabel(QStringLiteral("交易时段："), bidBox);
    m_periodCombo = new QComboBox(bidBox);
    for (int t = 1; t <= 96; ++t) {
        const int from = (t - 1) * 15, to = t * 15;
        m_periodCombo->addItem(QStringLiteral("%1 · %2–%3")
                                   .arg(t, 2, 10, QChar('0'))
                                   .arg(QTime(0, 0).addSecs(from * 60).toString(QStringLiteral("HH:mm")))
                                   .arg(QTime(0, 0).addSecs(to * 60).toString(QStringLiteral("HH:mm"))));
    }
    m_periodCombo->setToolTip(QStringLiteral(
        "V1.3 原生粒度 96 期（15 分钟/期）：选择时段后，下方申报表只显示该时段的申报，编辑即写回该时段"));
    auto *prefillBtn = new QPushButton(QStringLiteral("⚡ 按负荷预填本时段"), bidBox);
    prefillBtn->setObjectName("secondaryBtn");
    prefillBtn->setToolTip(QStringLiteral(
        "按负荷曲线形状预填当前时段：购电/发电申报量同乘 负荷(t)/平均负荷（保持供需结构，需已加载负荷曲线）"));
    m_periodHint = new QLabel(bidBox);
    m_periodHint->setObjectName("paramSummary");
    periodRow->addWidget(periodLbl);
    periodRow->addWidget(m_periodCombo, 0, Qt::AlignVCenter);
    periodRow->addWidget(prefillBtn);
    periodRow->addStretch();
    periodRow->addWidget(m_periodHint);
    connect(m_periodCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onEditPeriodChanged);
    connect(prefillBtn, &QPushButton::clicked,
            this, &MainWindow::onPrefillByLoad);

    // #90：P1 申报表改为横向段展开（与 CSV 格式一致：一机组一行，段1出力/段1报价/…）
    // 列数在 refreshImportPage 中按当前数据最大段数动态设置
    m_genTable = new QTableWidget(0, 0, bidBox);
    m_genTable->horizontalHeader()->setStretchLastSection(true);
    m_genTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                | QAbstractItemView::EditKeyPressed
                                | QAbstractItemView::AnyKeyPressed);
    m_genTable->setAlternatingRowColors(true);

    m_conTable = new QTableWidget(0, 0, bidBox);
    m_conTable->horizontalHeader()->setStretchLastSection(true);
    m_conTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                | QAbstractItemView::EditKeyPressed
                                | QAbstractItemView::AnyKeyPressed);
    m_conTable->setAlternatingRowColors(true);

    m_importTabs->addTab(m_genTable, QStringLiteral("发电侧申报"));
    m_importTabs->addTab(m_conTable, QStringLiteral("购电侧申报"));
    // #82：申报表编辑写回（电价/电量两列，见 onBidItemChanged）
    connect(m_genTable, &QTableWidget::itemChanged,
            this, &MainWindow::onBidItemChanged);
    connect(m_conTable, &QTableWidget::itemChanged,
            this, &MainWindow::onBidItemChanged);

    auto *loadBtn = new QPushButton(QStringLiteral("📂 加载内置样例"), bidBox);
    auto *csvBtn  = new QPushButton(QStringLiteral("📁 选择 CSV 文件…"), bidBox);
    auto *clearBtn= new QPushButton(QStringLiteral("清空"), bidBox);
    loadBtn->setObjectName("secondaryBtn");
    csvBtn->setObjectName("secondaryBtn");
    clearBtn->setObjectName("secondaryBtn");
    loadBtn->setToolTip(QStringLiteral("一键加载仓库内置样例（8 机组 + 3 用户，可跑 24/96 时段连续出清）"));
    csvBtn->setToolTip(QStringLiteral("选择 2 个 CSV（发电申报 / 购电申报），按文件名自动识别"));
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::onLoadSamples);
    connect(csvBtn,  &QPushButton::clicked, this, &MainWindow::onImportCsv);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearData);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(loadBtn);
    btnRow->addWidget(csvBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();

    auto *bidLay = new QVBoxLayout(bidBox);
    bidLay->addLayout(periodRow);
    bidLay->addWidget(m_importTabs);
    bidLay->addLayout(btnRow);

    // --- 中：数据集状态卡（真实导入状态；新能源由 P2 滑块控制，不占导入表） ---
    struct DataSet { const char *name; const char *desc; };
    const DataSet sets[2] = {
                              {"发电申报",   "generator_bids.csv"},
                              {"购电申报",   "consumer_bids.csv"},
                              };
    auto *setRow = new QHBoxLayout();
    setRow->setSpacing(10);
    for (int i = 0; i < 2; ++i) {
        auto *card = new QWidget(page);
        card->setObjectName("statusCard");
        auto *name = new QLabel(QString::fromUtf8(sets[i].name), card);
        name->setObjectName("statusCardName");
        auto *badge = new QLabel(QStringLiteral("未导入"), card);
        badge->setObjectName("statusBadgeWait");
        badge->setAlignment(Qt::AlignCenter);
        auto *desc = new QLabel(QString::fromUtf8(sets[i].desc), card);
        desc->setObjectName("statusCardDesc");
        auto *top = new QHBoxLayout();
        top->addWidget(name);
        top->addStretch();
        top->addWidget(badge);
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(14, 12, 14, 12);
        l->setSpacing(6);
        l->addLayout(top);
        l->addWidget(desc);
        setRow->addWidget(card, 1);
        m_statusBadges[i] = badge;
    }

    // --- 下：校验反馈汇总条（A 模块真实校验结果） ---
    auto *checkBar = new QWidget(page);
    checkBar->setObjectName("checkBar");
    auto *checkIcon = new QLabel(QStringLiteral("?"), checkBar);
    checkIcon->setObjectName("checkBarIcon");
    checkIcon->setAlignment(Qt::AlignCenter);
    m_checkText = new QLabel(checkBar);
    m_checkText->setObjectName("checkBarText");
    m_checkText->setTextFormat(Qt::RichText);
    m_checkText->setText(QStringLiteral(
        "<b>申报校验：尚未导入数据</b>　点击「加载内置样例」或「选择 CSV 文件」导入申报数据"));
    auto *checkLay = new QHBoxLayout(checkBar);
    checkLay->setContentsMargins(14, 10, 14, 10);
    checkLay->setSpacing(12);
    checkLay->addWidget(checkIcon);
    checkLay->addWidget(m_checkText, 1);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);
    outer->addWidget(bidBox, 3);
    outer->addLayout(setRow);
    outer->addWidget(checkBar);
    return page;
}

// ============================================================
// 页面 2：仿真控制
// ============================================================
QWidget *MainWindow::buildControlPage()
{
    auto *page = new QWidget();
    auto *box = new QGroupBox(QStringLiteral("仿真参数设置"), page);

    m_readyLabel = new QLabel();
    m_readyLabel->setTextFormat(Qt::RichText);
    m_readyLabel->setObjectName("readyLabel");
    m_readyLabel->setText(QStringLiteral(
        "数据状态：<span style='color:#C0392B;'>●</span> <b>待导入</b>（请先在①数据导入加载申报数据）"));

    // --- 出清模式：MCP / PAB 两张模式卡 ---
    m_btnMcp = new QPushButton();
    m_btnMcp->setObjectName("modeCard");
    m_btnMcp->setCheckable(true);
    m_btnMcp->setChecked(true);
    m_btnMcp->setAutoExclusive(true);
    m_btnMcp->setToolTip(QStringLiteral("创新点②：双模式报价出清，依据细则第 63 条"));
    m_btnMcp->setText(QStringLiteral(
        "MCP · 边际出清（统一出清价）\n"
        "统一按边际机组的报价出清，全部中标机组同价结算。\n"
        "规则最直观，适合首轮验证与教学演示。"));

    m_btnPab = new QPushButton();
    m_btnPab->setObjectName("modeCard");
    m_btnPab->setCheckable(true);
    m_btnPab->setAutoExclusive(true);
    m_btnPab->setText(QStringLiteral(
        "PAB · 按报价结算\n"
        "中标机组按各自报价结算，激励如实申报。\n"
        "与 MCP 对比可观察报价策略对收益的影响。"));

    auto *modeRow = new QHBoxLayout();
    modeRow->setSpacing(12);
    modeRow->addWidget(m_btnMcp, 1);
    modeRow->addWidget(m_btnPab, 1);

    // --- 其余参数 ---
    auto *form = new QFormLayout();

    m_granCombo = new QComboBox();
    m_granCombo->addItems({
        QStringLiteral("24 时段（每 1 小时）"),
        QStringLiteral("96 时段（每 15 分钟）"),
    });
    m_granCombo->setToolTip(QStringLiteral("创新点③：96 时段连续出清，依据规则 4.1 条"));
    form->addRow(QStringLiteral("时段颗粒度："), m_granCombo);

    connect(m_btnMcp, &QPushButton::toggled, this, [this] {
        updateFileNamePreviews();
    });
    connect(m_btnPab, &QPushButton::toggled, this, [this] {
        updateFileNamePreviews();
    });
    connect(m_granCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateFileNamePreviews();
        if (m_paramSummary) {
            const bool gran96 = m_granCombo->currentIndex() == 1;
            const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
            m_paramSummary->setText(QStringLiteral(
                                        "仿真日期 %1　·　时段颗粒度 %2　·　出清时段 %3 点　·　限价区间 0 ~ 540 元/MWh")
                                        .arg(today,
                                             gran96 ? QStringLiteral("15 分钟") : QStringLiteral("1 小时"),
                                             gran96 ? QStringLiteral("96") : QStringLiteral("24")));
        }
    });

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);
    slider->setValue(static_cast<int>(m_session.renewPercent));
    slider->setToolTip(QStringLiteral("新能源渗透率（0–100%，默认 20%）：每时段出力 = 渗透率 × 负荷，以 0 价供给优先中标，拖动后即时重新出清"));
    auto *valLabel = new QLabel(QStringLiteral("%1 %").arg(m_session.renewPercent, 0, 'f', 0));
    auto *sliderRow = new QHBoxLayout();
    sliderRow->addWidget(slider, 1);
    sliderRow->addWidget(valLabel);
    connect(slider, &QSlider::valueChanged, this, [this, valLabel](int v) {
        m_session.renewPercent = double(v);
        valLabel->setText(QStringLiteral("%1 %").arg(v));
        if (m_paramSummary) {
            const bool gran96 = m_granCombo && m_granCombo->currentIndex() == 1;
            const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
            m_paramSummary->setText(QStringLiteral(
                                        "仿真日期 %1　·　时段颗粒度 %2　·　出清时段 %3 点　·　新能源渗透率 %4 %")
                                        .arg(today,
                                             gran96 ? QStringLiteral("15 分钟") : QStringLiteral("1 小时"),
                                             gran96 ? QStringLiteral("96") : QStringLiteral("24"))
                                        .arg(v));
        }
        // #82：拖动滑块即时重算（与申报表编辑行为一致）
        if (m_session.hasResult && m_session.hasData)
            rerunIfReady();
    });
    form->addRow(QStringLiteral("新能源渗透率："), sliderRow);

    m_paramSummary = new QLabel();
    m_paramSummary->setObjectName("paramSummary");
    auto refreshSummary = [this] {
        const bool gran96 = m_granCombo && m_granCombo->currentIndex() == 1;
        const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        m_paramSummary->setText(QStringLiteral(
                                    "仿真日期 %1　·　时段颗粒度 %2　·　出清时段 %3 点　·　新能源渗透率 %4 %")
                                    .arg(today,
                                         gran96 ? QStringLiteral("15 分钟") : QStringLiteral("1 小时"),
                                         gran96 ? QStringLiteral("96") : QStringLiteral("24"))
                                    .arg(m_session.renewPercent, 0, 'f', 0));
    };
    refreshSummary();

    auto *runBtn = new QPushButton(QStringLiteral("▶  开始仿真"));
    runBtn->setObjectName("runBtn");
    runBtn->setToolTip(QStringLiteral("数据未就绪时按钮点击会提示先到①导入数据"));
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::onRunSim);

    auto *hint = new QLabel(QStringLiteral(
        "说明：点击「开始仿真」→ 引擎恒跑 96 时段连续出清（每时段需求 = 购电申报总量，新能源 = 渗透率 × 负荷）→ B 位真实引擎（MCP/PAB）；24 时段为聚合视图。\n"
        "①页申报表的电价/电量双击即可修改，改后自动重新出清；拖动滑块同样即时重算。\n"
        "视角切换不重算、不重置数据，只改变各页面的显示口径（工作台语义）。"));
    hint->setObjectName("hintLabel");

    auto *lay = new QVBoxLayout(box);
    lay->addWidget(m_readyLabel);
    lay->addSpacing(8);
    lay->addLayout(modeRow);
    lay->addSpacing(10);
    lay->addLayout(form);
    lay->addSpacing(10);
    lay->addWidget(m_paramSummary);
    lay->addSpacing(14);
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
// 页面 3：出清结果（视角化指标卡 + 明细表）
// ============================================================
QWidget *MainWindow::buildResultPage()
{
    auto *page = new QWidget();
    m_resultStack = new QStackedWidget(page);

    m_resultStack->addWidget(buildEmptyCard());

    auto *content = new QWidget();

    auto *kpiRow = new QWidget(content);
    auto *kpiLay = new QHBoxLayout(kpiRow);
    kpiLay->setContentsMargins(0, 0, 0, 0);
    kpiLay->setSpacing(10);
    struct Kpi { QLabel **value; QLabel **nameLabel; const char *name; };
    const Kpi kpis[4] = {
                          {&m_kpiAvg, &m_kpiNames[0], "出清均价 (元/MWh)"},
                          {&m_kpiVol, &m_kpiNames[1], "全天总出清电量 (MWh)"},
                          {&m_kpiFee, &m_kpiNames[2], "全天结算总额 (元)"},
                          {&m_kpiSpread, &m_kpiNames[3], "峰谷价差比"},
                          };
    for (const auto &k : kpis) {
        auto *card = new QWidget(kpiRow);
        card->setObjectName("kpiCard");
        auto *n = new QLabel(QString::fromUtf8(k.name), card);
        n->setObjectName("kpiName");
        auto *v = new QLabel(QStringLiteral("--"), card);
        v->setObjectName("kpiValue");
        auto *l = new QVBoxLayout(card);
        l->setContentsMargins(14, 12, 14, 12);
        l->setSpacing(4);
        l->addWidget(n);
        l->addWidget(v);
        *k.nameLabel = n;
        *k.value = v;
        kpiLay->addWidget(card, 1);
    }

    auto *box = new QGroupBox(
        QStringLiteral("出清明细（随视角切换口径）"), content);

    m_resultTable = new QTableWidget(0, 5, box);
    m_resultTable->setHorizontalHeaderLabels({
        QStringLiteral("时段"),
        QStringLiteral("出清电价 (元/MWh)"),
        QStringLiteral("出清电量 (MW)"),
        QStringLiteral("新能源出力 (MW)"),
        QStringLiteral("常规出力 (MW)"),
    });
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);

    auto *boxLay = new QVBoxLayout(box);
    boxLay->addWidget(m_resultTable);

    // --- 出力构成环形图（新能源 vs 常规，平台视角；新能源归平台口径） ---
    m_mixBox = new QGroupBox(
        QStringLiteral("出力构成 · 新能源 vs 常规机组（全天电量，平台视角）"), content);
    m_mixPie = new QPieSeries();
    m_mixPie->setHoleSize(0.42);
    auto *mixChart = new QChart();
    mixChart->addSeries(m_mixPie);
    mixChart->legend()->setVisible(true);
    mixChart->legend()->setAlignment(Qt::AlignRight);
    mixChart->legend()->setLabelColor(QColor(0x3A, 0x3A, 0x42));
    mixChart->setBackgroundVisible(false);
    mixChart->setMargins(QMargins(4, 4, 4, 4));
    m_mixView = new QChartView(mixChart, content);
    m_mixView->setRenderHint(QPainter::Antialiasing);
    m_mixView->setMinimumHeight(128);
    auto *mixLay = new QVBoxLayout(m_mixBox);
    mixLay->setContentsMargins(6, 6, 6, 6);
    mixLay->addWidget(m_mixView);

    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);
    lay->addWidget(kpiRow);
    lay->addWidget(m_mixBox);
    lay->addWidget(box, 1);

    m_resultStack->addWidget(content);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_resultStack);
    return page;
}

// ============================================================
// 页面 4：图表分析（供需交叉·真申报 / 分时电价·真结果）
// ============================================================
QWidget *MainWindow::buildChartPage()
{
    auto *page = new QWidget();
    m_chartStack = new QStackedWidget(page);

    m_chartStack->addWidget(buildEmptyCard());

    auto *content = new QWidget();
    auto *tabs = new QTabWidget(content);

    // 页签 1：供需交叉图（真申报阶梯 + 出清价水平线）
    {
        auto *box = new QGroupBox(QStringLiteral("供需曲线与出清点（真实申报数据 · 视角过滤）"));
        auto *chart = new QChart();

        m_supplySeries = new QLineSeries();
        m_supplySeries->setName(QStringLiteral("供给曲线（发电侧）"));
        m_supplySeries->setColor(QColor(0xE2, 0x4B, 0x4A));

        m_demandSeries = new QLineSeries();
        m_demandSeries->setName(QStringLiteral("需求曲线（购电侧）"));
        m_demandSeries->setColor(QColor(0x37, 0x8A, 0xDD));

        m_clearingLine = new QLineSeries();
        m_clearingLine->setName(QStringLiteral("出清价"));
        m_clearingLine->setColor(QColor(0xEF, 0x9F, 0x27));
        {
            QPen pen(QColor(0xEF, 0x9F, 0x27));
            pen.setWidth(2);
            pen.setStyle(Qt::DashLine);
            m_clearingLine->setPen(pen);
        }

        m_clearPoint = new QScatterSeries();
        m_clearPoint->setName(QStringLiteral("出清点"));
        m_clearPoint->setColor(QColor(0xC0, 0x39, 0x2B));
        m_clearPoint->setMarkerSize(11.0);

        chart->addSeries(m_supplySeries);
        chart->addSeries(m_demandSeries);
        chart->addSeries(m_clearingLine);
        chart->addSeries(m_clearPoint);

        m_axisSupplyX = new QValueAxis();
        m_axisSupplyX->setRange(0, 400);
        m_axisSupplyX->setTitleText(QStringLiteral("累计申报电量 (MWh)"));
        m_axisSupplyY = new QValueAxis();
        m_axisSupplyY->setRange(0, 600);
        m_axisSupplyY->setTitleText(QStringLiteral("报价 (元/MWh)"));

        chart->addAxis(m_axisSupplyX, Qt::AlignBottom);
        chart->addAxis(m_axisSupplyY, Qt::AlignLeft);
        m_supplySeries->attachAxis(m_axisSupplyX);
        m_supplySeries->attachAxis(m_axisSupplyY);
        m_demandSeries->attachAxis(m_axisSupplyX);
        m_demandSeries->attachAxis(m_axisSupplyY);
        m_clearingLine->attachAxis(m_axisSupplyX);
        m_clearingLine->attachAxis(m_axisSupplyY);
        m_clearPoint->attachAxis(m_axisSupplyX);
        m_clearPoint->attachAxis(m_axisSupplyY);

        chart->setTitle(QStringLiteral("供需曲线交叉 → 出清电价（机制可视化）"));
        chart->legend()->setAlignment(Qt::AlignBottom);

        auto *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);

        // #89：交易时段下拉（96 期原生粒度；与 P2 渗透率滑块联动，拖滑块后即时重绘）
        m_chartPeriodCombo = new QComboBox(box);
        for (int t = 1; t <= 96; ++t) {
            const int from = (t - 1) * 15, to = t * 15;
            m_chartPeriodCombo->addItem(QStringLiteral("%1 · %2–%3")
                                            .arg(t, 2, 10, QChar('0'))
                                            .arg(QTime(0, 0).addSecs(from * 60).toString(QStringLiteral("HH:mm")))
                                            .arg(QTime(0, 0).addSecs(to * 60).toString(QStringLiteral("HH:mm"))));
        }
        m_chartPeriodCombo->setToolTip(QStringLiteral(
            "选择要查看的时段截面：供给阶梯（含渗透率换算的新能源 0 价段）× 需求阶梯 → 边际出清点"));
        auto *chartPeriodRow = new QHBoxLayout();
        auto *cpLbl = new QLabel(QStringLiteral("交易时段："), box);
        chartPeriodRow->addWidget(cpLbl);
        chartPeriodRow->addWidget(m_chartPeriodCombo);
        chartPeriodRow->addStretch();
        connect(m_chartPeriodCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            refreshChartPage();   // 截面切换只重绘图表，不重算
        });

        auto *lay = new QVBoxLayout(box);
        lay->addLayout(chartPeriodRow);
        lay->addWidget(view);
        tabs->addTab(box, QStringLiteral("供需交叉图"));
    }

    // 页签 2：分时电价曲线（真实出清结果）
    {
        auto *box = new QGroupBox(QStringLiteral("分时电价与新能源出力（真实出清结果）"));
        auto *chart = new QChart();

        m_priceSeries = new QLineSeries();
        m_priceSeries->setName(QStringLiteral("出清电价 (元/MWh)"));
        m_priceSeries->setColor(QColor(0x18, 0x5F, 0xA5));

        m_renewSeries = new QLineSeries();
        m_renewSeries->setName(QStringLiteral("新能源出力 (MW)"));
        m_renewSeries->setColor(QColor(0x0F, 0x6E, 0x56));

        chart->addSeries(m_priceSeries);
        chart->addSeries(m_renewSeries);

        m_axisPriceX = new QValueAxis();
        m_axisPriceX->setRange(1, 24);
        m_axisPriceX->setTitleText(QStringLiteral("时段"));
        m_axisPriceX->setTickCount(13);

        auto *axisLeft = new QValueAxis();
        axisLeft->setRange(0, 600);
        axisLeft->setTitleText(QStringLiteral("电价 (元/MWh)"));

        auto *axisRight = new QValueAxis();
        axisRight->setRange(0, 400);
        axisRight->setTitleText(QStringLiteral("新能源出力 (MW)"));

        chart->addAxis(m_axisPriceX, Qt::AlignBottom);
        chart->addAxis(axisLeft, Qt::AlignLeft);
        chart->addAxis(axisRight, Qt::AlignRight);
        m_priceSeries->attachAxis(m_axisPriceX);
        m_priceSeries->attachAxis(axisLeft);
        m_renewSeries->attachAxis(m_axisPriceX);
        m_renewSeries->attachAxis(axisRight);

        chart->setTitle(QStringLiteral("真实出清：早晚高峰电价抬升 · 午间光伏压价"));
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
// 页面 5：结果导出
// ============================================================
QWidget *MainWindow::buildExportPage()
{
    auto *page = new QWidget();
    m_exportStack = new QStackedWidget(page);

    m_exportStack->addWidget(buildEmptyCard());

    auto *content = new QWidget();

    auto *box = new QGroupBox(QStringLiteral("结算摘要（真实出清结果 · 随视角口径）"), content);
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

    m_fileDaily = new QLabel(box);
    m_fileCurve = new QLabel(box);
    m_fileDaily->setObjectName("fileLabel");
    m_fileCurve->setObjectName("fileLabel");
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    m_fileDaily->setFont(mono);
    m_fileCurve->setFont(mono);
    auto *fileBox = new QGroupBox(QStringLiteral("导出文件名预览（自动生成，可改名）"), content);
    auto *fileLay = new QVBoxLayout(fileBox);
    fileLay->addWidget(m_fileDaily);
    fileLay->addWidget(m_fileCurve);

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
        "说明：结算日报按当前视角导出——平台视角为逐时段全局表；发电侧/购电侧视角为逐主体明细表。"));
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
// 空态引导卡片
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
// 三视角切换条（顶栏 · 免登录 · 互斥按钮组）
// ============================================================
QWidget *MainWindow::buildPerspectiveBar()
{
    auto *bar = new QWidget();
    bar->setObjectName("perspBar");
    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(2);

    m_perspGroup = new QButtonGroup(this);
    m_perspGroup->setExclusive(true);

    m_btnPerspGen  = new QPushButton(QStringLiteral("发电侧"));
    m_btnPerspCon  = new QPushButton(QStringLiteral("购电侧"));
    m_btnPerspPlat = new QPushButton(QStringLiteral("平台"));
    for (auto *b : {m_btnPerspGen, m_btnPerspCon, m_btnPerspPlat}) {
        b->setObjectName("perspBtn");
        b->setCheckable(true);
        m_perspGroup->addButton(b);
        lay->addWidget(b);
    }
    m_btnPerspPlat->setChecked(true);   // 默认平台视角

    connect(m_btnPerspGen, &QPushButton::clicked, this, [this] {
        setPerspective(Perspective::Gen);
    });
    connect(m_btnPerspCon, &QPushButton::clicked, this, [this] {
        setPerspective(Perspective::Con);
    });
    connect(m_btnPerspPlat, &QPushButton::clicked, this, [this] {
        setPerspective(Perspective::Platform);
    });
    return bar;
}

// ============================================================
// 数据导入（A 模块真实读取 + 校验）
//   V1.3（#88）口径：申报两张长表必选；负荷曲线（渗透率基准）与
//   新能源形状曲线可选——缺失时渗透率回退按购电申报总量换算。
// ============================================================
bool MainWindow::loadDataFiles(const QString &genFile, const QString &conFile,
                               const QString &sourceName,
                               const QString &loadFile, const QString &renewFile)
{
    MarketData d;
    QStringList errs;
    bool ok = true;
    if (!genFile.isEmpty())
        ok = DataReader::readGeneratorBids(genFile, d.generatorBids, errs) && ok;
    if (!conFile.isEmpty())
        ok = DataReader::readConsumerBids(conFile, d.consumerBids, errs) && ok;

    // 可选曲线（存在才读）：负荷 = 渗透率换算基准，新能源 = 形状曲线
    if (!loadFile.isEmpty() && QFile::exists(loadFile))
        ok = DataReader::readLoadCurve(loadFile, d.loadCurve, errs) && ok;
    if (!renewFile.isEmpty() && QFile::exists(renewFile))
        ok = DataReader::readRenewableOutput(renewFile, d.renewableOutputs, errs) && ok;

    if (!ok) {
        m_checkErrors = errs;
        renderCheckBar();
        statusBar()->showMessage(QStringLiteral("数据读取失败，请检查 CSV 文件"), 8000);
        return false;
    }

    ok = DataReader::validateRelations(d, errs);
    if (!ok) {
        m_checkErrors = errs;
        renderCheckBar();
        statusBar()->showMessage(QStringLiteral("校验未通过：%1").arg(errs.join(QStringLiteral("；"))), 8000);
        return false;
    }

    m_session.market = d;
    m_session.marketBaseline = d;       // 拍快照：按负荷预填的重置基准（幂等）
    m_session.dataSource = sourceName;
    m_session.hasData = true;
    m_session.resetResult();
    m_checkErrors.clear();

    refreshImportPage();
    updateFileNamePreviews();
    statusBar()->showMessage(QStringLiteral("数据导入成功：%1（校验通过）").arg(sourceName), 6000);
    return true;
}

// 定位仓库内 data/samples 目录（Qt Creator 运行目录与源码目录不同）
QString MainWindow::locateSamplesDir() const
{
    QStringList candidates;
    candidates << QDir::currentPath() + QStringLiteral("/data/samples");
    candidates << QCoreApplication::applicationDirPath() + QStringLiteral("/data/samples");
    candidates << QCoreApplication::applicationDirPath() + QStringLiteral("/../data/samples");
    // 从当前目录向上找 4 层
    QDir d = QDir::currentPath();
    for (int i = 0; i < 4; ++i) {
        d.cdUp();
        candidates << d.path() + QStringLiteral("/data/samples");
    }
    for (const auto &c : candidates) {
        if (QFile::exists(c + QStringLiteral("/benchmark/generator_bids.csv")))
            return c;
    }
    return QString();
}

// P1：加载内置样例（场景 8 机组 + 3 用户申报，24/96 时段出清）
void MainWindow::onLoadSamples()
{
    const QString dir = locateSamplesDir();
    if (dir.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("未找到内置样例目录 data/samples，请确认已 clone 完整仓库，或改用「选择 CSV 文件」"), 8000);
        return;
    }
    loadDataFiles(dir + QStringLiteral("/scenario/generator_bids.csv"),
                  dir + QStringLiteral("/scenario/consumer_bids.csv"),
                  QStringLiteral("内置场景（8 机组 · 3 用户）"),
                  dir + QStringLiteral("/curves/load_curve.csv"),
                  dir + QStringLiteral("/curves/renewable_output.csv"));
}

// P1：选择 CSV 文件（按文件名自动识别两张申报表）
void MainWindow::onImportCsv()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择申报 CSV（可多选，按文件名自动识别）"),
        QString(), QStringLiteral("CSV 文件 (*.csv)"));
    if (files.isEmpty())
        return;

    const auto pick = [](const QStringList &names, const QStringList &keys) {
        for (const auto &n : names)
            for (const auto &k : keys)
                if (n.contains(k, Qt::CaseInsensitive))
                    return n;
        return QString();
    };

    const QString gen   = pick(files, {QStringLiteral("发电"), QStringLiteral("generator"), QStringLiteral("gen_")});
    const QString con   = pick(files, {QStringLiteral("用户"), QStringLiteral("购电"), QStringLiteral("consumer"), QStringLiteral("con_")});

    if (gen.isEmpty() && con.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("未识别到申报文件（文件名需含「发电/用户」等关键字）"), 8000);
        return;
    }

    // 曲线文件未选时，在申报文件同目录自动识别（可选，缺省回退购电申报总量）
    const QDir bidDir = QFileInfo(gen.isEmpty() ? con : gen).absoluteDir();
    const QString loadCurve = bidDir.filePath(QStringLiteral("load_curve.csv"));
    const QString renewCurve = bidDir.filePath(QStringLiteral("renewable_output.csv"));
    loadDataFiles(gen, con, QStringLiteral("自定义 CSV 导入"),
                  QFile::exists(loadCurve) ? loadCurve : QString(),
                  QFile::exists(renewCurve) ? renewCurve : QString());
}

// P1：清空数据
void MainWindow::onClearData()
{
    m_session.market = MarketData();
    m_session.marketBaseline = MarketData();   // #92：同步清空原始基准快照
    m_session.dataSource.clear();
    m_session.hasData = false;
    m_session.resetResult();
    m_checkErrors.clear();
    setHasResult(false);
    refreshImportPage();
    updateFileNamePreviews();
    statusBar()->showMessage(QStringLiteral("已清空数据"), 4000);
}

// ============================================================
// 出清流水线（P2 开始仿真共用）
//   V1.3（#88）口径：负荷退场，每时段需求 = 购电申报总量；
//   新能源 P_re(t) = 渗透率 × 负荷(t)（契约 §5.3）；
//   引擎恒跑 96 期，24 为聚合视图。
// ============================================================
void MainWindow::runClearing()
{
    m_session.resetResult();
    const bool gran96 = m_granCombo && m_granCombo->currentIndex() == 1;
    const int periodCount = gran96 ? 96 : 24;
    const QString mode = (m_btnPab && m_btnPab->isChecked())
                             ? QStringLiteral("PAB") : QStringLiteral("MCP");

    // 出清（B 位真实引擎外壳，每时段 = 购电申报总量 + 渗透率×负荷的新能源）
    m_session.result = ClearingFacade::clearPeriods(
        m_session.market, periodCount, m_session.renewPercent / 100.0, mode);
    m_session.hasResult = true;
}

// ============================================================
// 三视角：切换（不重算）→ 刷新各页
// ============================================================
void MainWindow::setPerspective(Perspective p)
{
    m_session.perspective = p;
    applyPerspective();
    statusBar()->showMessage(
        QStringLiteral("已切换：%1（免登录 · 视角=过滤器，不重算出清）").arg(perspectiveName(p)), 4000);
}

void MainWindow::applyPerspective()
{
    refreshImportPage();          // P1：申报表按侧过滤
    if (m_session.hasResult) {
        refreshResultPage();      // P3：指标卡/明细表换口径
        refreshChartPage();       // P4：曲线换视角
        refreshExportPage();      // P5：摘要/文件名换
    }
}

// ============================================================
// P1 刷新：真实申报表 + 状态卡 + 校验条 + 视角过滤
// ============================================================
void MainWindow::refreshImportPage()
{
    // #82：锁定列（ID/名称/类型/段）不可编辑；电价/电量列保持默认可编辑
    const auto lockedItem = [](const QString &text) {
        auto *it = new QTableWidgetItem(text);
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        return it;
    };

    // #90：P1 申报表改为横向段展开（与 CSV 格式一致：一机组一行，段1出力/段1报价/…）
    const auto fillBidTable = [&](QTableWidget *table, const auto &bids,
                                   const QString &nameHeader, const QString &idHeader) {
        // 统计当前时段最大段数，动态确定列数
        int maxSeg = 0;
        for (int i = 0; i < bids.size(); ++i) {
            const auto &b = bids[i];
            if (b.period > 0 && b.period != m_editPeriod)
                continue;
            maxSeg = std::max(maxSeg, b.segment);
        }
        const int segColCount = maxSeg * 2;
        const int colCount = 2 + segColCount;
        table->setColumnCount(colCount);
        QStringList headers;
        headers << nameHeader << idHeader;
        for (int s = 1; s <= maxSeg; ++s) {
            headers << QStringLiteral("第%1段出力(MW)").arg(s)
                    << QStringLiteral("第%1段报价(元/MWh)").arg(s);
        }
        table->setHorizontalHeaderLabels(headers);

        m_loadingBids = true;                 // 填充期间屏蔽 itemChanged（防递归）
        table->setRowCount(0);
        // #92+：用 (name, id) 复合 key 做行号索引——同一机组编号跨电厂存在时（如金陵#1/龙潭#1 都是"#1机组"）
        //      不能只用 id 撞行，必须按 (电厂名称 + 机组编号) 唯一定位
        const auto rowKey = [](const QString &name, const QString &id) {
            return name + QStringLiteral("\x1f") + id;   // \x1f 作分隔符（CSV 单元格里不会出现的字符）
        };
        QHash<QString, int> idToRow;
        int row = 0;
        for (int i = 0; i < bids.size(); ++i) {
            const auto &b = bids[i];
            if (b.period > 0 && b.period != m_editPeriod)
                continue;
            const QString key = rowKey(b.name, b.id);
            auto it = idToRow.find(key);
            if (it == idToRow.end()) {
                table->insertRow(row);
                table->setItem(row, 0, lockedItem(b.name));
                table->setItem(row, 1, lockedItem(b.id));
                it = idToRow.insert(key, row);
                ++row;
            }
            const int r = *it;
            const int qtyCol   = 2 + (b.segment - 1) * 2;       // 出力列
            const int priceCol = 2 + (b.segment - 1) * 2 + 1;   // 报价列
            auto *qtyItem = new QTableWidgetItem(QString::number(b.quantity, 'f', 1));
            qtyItem->setData(Qt::UserRole, i);
            auto *priceItem = new QTableWidgetItem(QString::number(b.price, 'f', 1));
            priceItem->setData(Qt::UserRole, i);
            table->setItem(r, qtyCol, qtyItem);
            table->setItem(r, priceCol, priceItem);
        }
        m_loadingBids = false;
    };

    if (m_genTable)
        fillBidTable(m_genTable, m_session.market.generatorBids,
                     QStringLiteral("电厂名称"), QStringLiteral("机组编号"));
    if (m_conTable)
        fillBidTable(m_conTable, m_session.market.consumerBids,
                     QStringLiteral("用户名称"), QStringLiteral("负荷编号"));

    // #89：本时段供需概览（购电申报总量 / 负荷曲线 / 发电可用）
    if (m_periodHint) {
        if (!m_session.hasData) {
            m_periodHint->setText(QString());
        } else {
            const auto sums = MarketView::periodSums(m_session.market, m_editPeriod);
            const QString loadTxt = sums.loadFound
                ? QStringLiteral("%1 MW").arg(sums.loadMW, 0, 'f', 1)
                : QStringLiteral("—");

            // #92：当前时段相对原始基准的累计缩放因子
            //   ratio = 当前电量 / 原始基准电量（按主体累加取均值）
            //   1.0 = 未缩放，< 1 = 已缩小，> 1 = 已放大
            const double ratio = MarketView::scaleRatio(
                m_session.market, m_session.marketBaseline, m_editPeriod);
            const QString scaleTxt = (ratio < 0.0)
                ? QStringLiteral("—")
                : QStringLiteral("×%1").arg(ratio, 0, 'f', 3);

            m_periodHint->setText(QStringLiteral(
                                      "第 %1 时段：购电 %2 MW · 发电 %3 MW · 负荷 %4 · 当前缩放 %5")
                                      .arg(m_editPeriod)
                                      .arg(sums.conMW, 0, 'f', 1)
                                      .arg(sums.genMW, 0, 'f', 1)
                                      .arg(loadTxt)
                                      .arg(scaleTxt));
        }
    }

    // 数据集状态卡（发电/购电两表；新能源由 P2 滑块控制）
    const bool ready[2] = {
        !m_session.market.generatorBids.isEmpty(),
        !m_session.market.consumerBids.isEmpty(),
    };
    for (int i = 0; i < 2; ++i) {
        if (m_statusBadges[i]) {
            m_statusBadges[i]->setText(ready[i] ? QStringLiteral("✓ 已就绪")
                                                : QStringLiteral("未导入"));
            m_statusBadges[i]->setObjectName(ready[i] ? QStringLiteral("statusBadgeOk")
                                                      : QStringLiteral("statusBadgeWait"));
            m_statusBadges[i]->style()->unpolish(m_statusBadges[i]);
            m_statusBadges[i]->style()->polish(m_statusBadges[i]);
        }
    }

    // 校验汇总条（#82：导入与编辑共用 renderCheckBar）
    renderCheckBar();

    // P2 就绪灯联动
    if (m_readyLabel) {
        if (m_session.hasData) {
            m_readyLabel->setText(QStringLiteral(
                "数据状态：<span style='color:#2FA84F;'>●</span> <b>已就绪</b>（%1）")
                                      .arg(m_session.dataSource));
        } else {
            m_readyLabel->setText(QStringLiteral(
                "数据状态：<span style='color:#C0392B;'>●</span> <b>待导入</b>（请先在①数据导入加载申报数据）"));
        }
    }

    // 视角过滤：发电侧只显示发电 tab，购电侧只显示购电 tab，平台都显示
    if (m_importTabs) {
        const Perspective p = m_session.perspective;
        m_importTabs->setTabVisible(0, p != Perspective::Con);
        m_importTabs->setTabVisible(1, p != Perspective::Gen);
        if (!m_importTabs->isTabVisible(m_importTabs->currentIndex()))
            m_importTabs->setCurrentIndex(p == Perspective::Gen ? 0 : 1);
    }
}

// ============================================================
// #82 申报表编辑：写回会话数据 + 即时校验 + 自动重算
// ============================================================
void MainWindow::onBidItemChanged(QTableWidgetItem *item)
{
    if (m_loadingBids || !item)
        return;
    QTableWidget *tbl = item->tableWidget();
    const bool isGen = (tbl == m_genTable);
    if (!isGen && tbl != m_conTable)
        return;
    const int col = item->column();
    // #90：横向段展开布局，列结构为 名称/编号/段1出力/段1报价/段2出力/段2报价/…
    //   col<2 为锁定列；偶数列（从2开始）为出力，奇数列为报价
    if (col < 2)
        return;
    const bool isQty   = ((col - 2) % 2 == 0);     // 出力列
    const bool isPrice = !isQty;                    // 报价列
    // 按 UserRole 源索引定位（表格行/列 ≠ bids 下标）
    const int srcIdx = item->data(Qt::UserRole).toInt();

    // 两种申报结构都有 price/quantity 字段，用泛型 lambda 统一处理
    const auto handle = [&](auto &bids) {
        if (srcIdx < 0 || srcIdx >= bids.size())
            return;
        auto &bid = bids[srcIdx];

        const double oldVal = isPrice ? bid.price : bid.quantity;
        const auto revert = [this, item, oldVal]() {
            m_loadingBids = true;
            item->setText(QString::number(oldVal, 'f', 1));
            m_loadingBids = false;
        };

        bool ok = false;
        const double val = item->text().toDouble(&ok);
        if (!ok) {
            revert();
            statusBar()->showMessage(QStringLiteral("输入无效：请填写数字（如 260 或 260.5）"), 6000);
            return;
        }
        if (isPrice && (val < 0.0 || val > 540.0)) {
            revert();
            statusBar()->showMessage(QStringLiteral("申报电价需在 0~540 元/MWh 之间（限价规则）"), 6000);
            return;
        }
        if (isQty && val < 0.0) {
            revert();
            statusBar()->showMessage(QStringLiteral("申报电量需为非负数（0 = 该时段该段不申报/停机）"), 6000);
            return;
        }
        if (qFuzzyCompare(val + 1.0, oldVal + 1.0))   // 数值未变（仅文本格式差异）
            return;

        if (isPrice)
            bid.price = val;
        else
            bid.quantity = val;

        // 编辑后即时重跑跨文件校验（结果只影响提示条，不阻断出清）
        QStringList errs;
        DataReader::validateRelations(m_session.market, errs);
        m_checkErrors = errs;
        renderCheckBar();

        // 已有出清结果 → 立即重算并刷新各页
        if (m_session.hasResult && m_session.hasData) {
            rerunIfReady();
            // 需求总量按当前编辑时段统计（#89 单时段视图）
            double demand = 0.0;
            for (const auto &c : m_session.market.consumerBids)
                if (c.period == m_editPeriod || c.period <= 0)
                    demand += c.quantity;
            const auto &pr = m_session.result.periods.isEmpty()
                                 ? PeriodResult() : m_session.result.periods.first();
            statusBar()->showMessage(
                QStringLiteral("申报已修改并重算：需求总量 %1 MWh · 出清价 %2 元/MWh · 成交 %3 MWh")
                    .arg(demand, 0, 'f', 0)
                    .arg(pr.clearingPrice, 0, 'f', 0)
                    .arg(pr.clearedMW, 0, 'f', 0),
                6000);
        } else {
            statusBar()->showMessage(
                QStringLiteral("申报已修改（尚未出清，可在②仿真控制点击「开始仿真」）"), 5000);
        }
    };
    if (isGen)
        handle(m_session.market.generatorBids);
    else
        handle(m_session.market.consumerBids);
}

// ============================================================
// #89 P1：交易时段切换 → 单时段小表刷新（不重算，只换视图）
// ============================================================
void MainWindow::onEditPeriodChanged(int index)
{
    if (index < 0)
        return;
    m_editPeriod = index + 1;
    refreshImportPage();
}

// ============================================================
// #89 P1：按负荷曲线预填当前时段申报量
//   购电/发电申报量同乘 负荷(t)/平均负荷（供需结构保持，
//   逐时段供需平衡态不被破坏），申报价不动
//   #92：幂等设计——每次点击都基于 marketBaseline（原始基准），
//        而非当前已缩放值；重复点击结果一致
// ============================================================
void MainWindow::onPrefillByLoad()
{
    const auto &lc = m_session.market.loadCurve;
    if (lc.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("预填失败：未加载负荷曲线（内置场景样例自带 curves/；自定义导入请把 load_curve.csv 放在同目录）"), 8000);
        return;
    }
    double loadT = 0.0, sum = 0.0;
    int hit = 0;
    for (const auto &lp : lc) {
        sum += lp.load;
        if (lp.period == m_editPeriod) {
            loadT = lp.load;
            ++hit;
        }
    }
    if (hit == 0 || sum <= 0.0) {
        statusBar()->showMessage(
            QStringLiteral("预填失败：负荷曲线缺少第 %1 时段的数据").arg(m_editPeriod), 8000);
        return;
    }
    const double avg = sum / lc.size();
    const double factor = loadT / avg;

    int nCon = 0, nGen = 0;

    // #92：先用 baseline 恢复当前时段的电量，再 × factor（幂等）
    // 修复：匹配条件必须用 (name, id, period, segment) 四轴定位——
    //       跨电厂的同机组编号（"金陵#1"/"龙潭#1" 都 id="#1机组"）只用 id 会撞主体
    auto prefillSide = [&](QVector<GeneratorBid> &side,
                           const QVector<GeneratorBid> &baseline) {
        for (int i = 0; i < side.size(); ++i) {
            auto &b = side[i];
            if (b.period > 0 && b.period != m_editPeriod)
                continue;
            for (const auto &bb : baseline) {
                if (bb.name == b.name && bb.id == b.id
                    && bb.period == b.period && bb.segment == b.segment) {
                    b.quantity = bb.quantity * factor;
                    break;
                }
            }
        }
    };
    auto prefillCon = [&](QVector<ConsumerBid> &side,
                          const QVector<ConsumerBid> &baseline) {
        for (int i = 0; i < side.size(); ++i) {
            auto &b = side[i];
            if (b.period > 0 && b.period != m_editPeriod)
                continue;
            for (const auto &bb : baseline) {
                if (bb.name == b.name && bb.id == b.id
                    && bb.period == b.period && bb.segment == b.segment) {
                    b.quantity = bb.quantity * factor;
                    break;
                }
            }
        }
    };

    prefillSide(m_session.market.generatorBids, m_session.marketBaseline.generatorBids);
    prefillCon(m_session.market.consumerBids, m_session.marketBaseline.consumerBids);

    // 统计实际改了多少条
    for (const auto &c : m_session.market.consumerBids)
        if (c.period == m_editPeriod || c.period <= 0)
            ++nCon;
    for (const auto &g : m_session.market.generatorBids)
        if (g.period == m_editPeriod || g.period <= 0)
            ++nGen;

    refreshImportPage();
    if (m_session.hasResult && m_session.hasData)
        rerunIfReady();
    statusBar()->showMessage(
        QStringLiteral("已按负荷预填第 %1 时段：负荷 %2 MW / 日均 %3 MW → 缩放因子 ×%4，购电 %5 条 · 发电 %6 条申报已从原始基准重置并应用缩放（幂等：重复点击结果一致）")
            .arg(m_editPeriod)
            .arg(loadT, 0, 'f', 1)
            .arg(avg, 0, 'f', 1)
            .arg(factor, 0, 'f', 3)
            .arg(nCon)
            .arg(nGen),
        8000);
}

// 校验汇总条（导入与编辑共用）
void MainWindow::renderCheckBar()
{
    if (!m_checkText)
        return;
    if (!m_session.hasData) {
        m_checkText->setStyleSheet(QString());
        m_checkText->setText(QStringLiteral(
            "<b>申报校验：尚未导入数据</b>　点击「加载内置样例」或「选择 CSV 文件」导入申报数据"));
        return;
    }
    if (m_checkErrors.isEmpty()) {
        m_checkText->setStyleSheet(QString());
        m_checkText->setText(QStringLiteral(
            "<b>申报校验：规则全部通过</b>　数据来源：%1<br>"
            "<span style='color:#6A8F75;'>真实校验由 A 模块执行（段数≤5 · 报价单调性 · 0~540 限价 · 跨文件一致性），"
            "依据《电力现货市场基本规则（试行）》4.2.3 条；新能源不参与申报，出力由仿真控制页渗透率 × 负荷曲线自动生成</span>")
                .arg(m_session.dataSource));
    } else {
        m_checkText->setStyleSheet(QStringLiteral("color:#B3261E;"));
        m_checkText->setText(QStringLiteral(
                                 "<b>申报校验未通过：%1 项（出清仍按当前申报计算）</b><br>%2")
                                 .arg(m_checkErrors.size())
                                 .arg(m_checkErrors.join(QStringLiteral("<br>"))));
    }
}

// 改申报/滑块后：已有结果则按原路径即时重算
void MainWindow::rerunIfReady()
{
    if (!m_session.hasResult || !m_session.hasData)
        return;
    const QString mode = (m_btnPab && m_btnPab->isChecked())
                             ? QStringLiteral("PAB") : QStringLiteral("MCP");
    if (m_session.result.sourceName.contains(QStringLiteral("基准"))) {
        m_session.result = ClearingFacade::clearBenchmark(m_session.market, mode);
        m_session.hasResult = true;
    } else {
        runClearing();
    }
    if (!m_session.hasResult)
        return;
    refreshResultPage();
    refreshChartPage();
    refreshExportPage();
}

// ============================================================
// 结果状态切换
// ============================================================
void MainWindow::setHasResult(bool on)
{
    if (on && !m_hasResult) {
        refreshResultPage();
        refreshChartPage();
        refreshExportPage();
    }
    m_hasResult = on;
    m_resultStack->setCurrentIndex(on ? 1 : 0);
    m_chartStack->setCurrentIndex(on ? 1 : 0);
    m_exportStack->setCurrentIndex(on ? 1 : 0);
}

// ============================================================
// P3 刷新（视角化指标卡 + 明细表）
// ============================================================
void MainWindow::refreshResultPage()
{
    const auto &periods = m_session.result.periods;
    if (periods.isEmpty())
        return;
    const Perspective p = m_session.perspective;
    const int T = periods.size();

    // 96 期时 clearedMW 为 15 分钟功率（MW），×0.25 才是电量（MWh）；
    // 24 期为聚合视图（小时均值 MW = MWh/h），×1 即电量。环形图同口径。
    const double dh = (T == 96) ? 0.25 : 1.0;
    double priceSum = 0.0, volSum = 0.0, feeSum = 0.0;
    double mx = -std::numeric_limits<double>::max();
    double mn = std::numeric_limits<double>::max();
    for (const auto &pr : periods) {
        priceSum += pr.clearingPrice;
        volSum += pr.clearedMW * dh;
        feeSum += (p == Perspective::Gen) ? pr.genFee
                 : (p == Perspective::Con) ? pr.conFee
                                           : (pr.genFee + pr.conFee);
        mx = std::max(mx, pr.clearingPrice);
        mn = std::min(mn, pr.clearingPrice);
    }
    const double avg = priceSum / T;

    // 指标名称随视角切换
    const char *names[4] = {
        "出清均价 (元/MWh)", "全天总出清电量 (MWh)", "全天结算总额 (元)", "峰谷价差比",
    };
    double vals[4] = { avg, volSum, feeSum, mx / mn };
    if (p == Perspective::Gen) {
        names[0] = "发电总收入 (元)";
        names[1] = "发电总中标量 (MWh)";
        names[2] = "最高出清价 (元/MWh)";
        names[3] = "中标时段数";
        vals[0] = feeSum;
        vals[1] = volSum;
        vals[2] = mx;
        vals[3] = double(T);
    } else if (p == Perspective::Con) {
        names[0] = "购电总费用 (元)";
        names[1] = "购电总中标量 (MWh)";
        names[2] = "最高出清价 (元/MWh)";
        names[3] = "中标时段数";
        vals[0] = feeSum;
        vals[1] = volSum;
        vals[2] = mx;
        vals[3] = double(T);
    }
    for (int i = 0; i < 4; ++i) {
        if (m_kpiNames[i])
            m_kpiNames[i]->setText(QString::fromUtf8(names[i]));
    }
    m_kpiAvg->setText(QStringLiteral("%1").arg(vals[0], 0, 'f', 1));
    m_kpiVol->setText(QStringLiteral("%1").arg(vals[1], 0, 'f', 1));
    m_kpiFee->setText(QStringLiteral("%1").arg(vals[2], 0, 'f', 0));
    m_kpiSpread->setText(QStringLiteral("%1").arg(vals[3], 0, 'f', 2));

    // 明细表：平台=逐时段；发电/购电=逐主体
    if (p == Perspective::Platform) {
        m_resultTable->setColumnCount(5);
        m_resultTable->setHorizontalHeaderLabels({
            QStringLiteral("时段"), QStringLiteral("出清电价 (元/MWh)"),
            QStringLiteral("出清电量 (MW)"), QStringLiteral("新能源出力 (MW)"),
            QStringLiteral("常规出力 (MW)"),
        });
        m_resultTable->setRowCount(T);
        for (int i = 0; i < T; ++i) {
            const auto &pr = periods[i];
            m_resultTable->setItem(i, 0, new QTableWidgetItem(pr.time));
            m_resultTable->setItem(i, 1, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(pr.clearingPrice, 0, 'f', 1)));
            m_resultTable->setItem(i, 2, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(pr.clearedMW, 0, 'f', 1)));
            m_resultTable->setItem(i, 3, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(pr.renewMW, 0, 'f', 1)));
            m_resultTable->setItem(i, 4, new QTableWidgetItem(
                                           QStringLiteral("%1").arg(pr.clearedMW - pr.renewMW, 0, 'f', 1)));
        }
    } else {
        const bool genSide = (p == Perspective::Gen);
        m_resultTable->setColumnCount(6);
        m_resultTable->setHorizontalHeaderLabels({
            QStringLiteral("时段"), QStringLiteral("主体"), QStringLiteral("段"),
            QStringLiteral("报价 (元/MWh)"), QStringLiteral("中标量 (MWh)"),
            genSide ? QStringLiteral("收入 (元)") : QStringLiteral("费用 (元)"),
        });
        int rows = 0;
        for (const auto &pr : periods)
            rows += genSide ? pr.genDetails.size() : pr.conDetails.size();
        m_resultTable->setRowCount(rows);
        int r = 0;
        for (const auto &pr : periods) {
            const auto &list = genSide ? pr.genDetails : pr.conDetails;
            for (const auto &e : list) {
                m_resultTable->setItem(r, 0, new QTableWidgetItem(pr.time));
                m_resultTable->setItem(r, 1, new QTableWidgetItem(e.name));
                m_resultTable->setItem(r, 2, new QTableWidgetItem(QString::number(e.segment)));
                m_resultTable->setItem(r, 3, new QTableWidgetItem(
                                               QStringLiteral("%1").arg(e.bidPrice, 0, 'f', 1)));
                m_resultTable->setItem(r, 4, new QTableWidgetItem(
                                               QStringLiteral("%1").arg(e.clearedMW, 0, 'f', 1)));
                m_resultTable->setItem(r, 5, new QTableWidgetItem(
                                               QStringLiteral("%1").arg(e.money, 0, 'f', 1)));
                ++r;
            }
        }
    }

    // 出力构成环形图（新能源归平台视角：仅平台视角可见）
    if (m_mixPie && m_mixBox) {
        m_mixBox->setVisible(p == Perspective::Platform);
        if (p == Perspective::Platform) {
            const double dh = (T == 96) ? 0.25 : 1.0;
            double renewEnergy = 0.0, convEnergy = 0.0;
            for (const auto &pr : periods) {
                renewEnergy += pr.renewMW * dh;                 // 新能源实际消纳
                convEnergy += (pr.clearedMW - pr.renewMW) * dh; // 常规机组
            }
            m_mixPie->clear();
            const double total = renewEnergy + convEnergy;
            if (total > 0.0) {
                auto *rs = m_mixPie->append(QStringLiteral("新能源"), renewEnergy);
                rs->setColor(QColor(0x0F, 0x6E, 0x56));
                rs->setLabel(QStringLiteral("新能源 %1%")
                                 .arg(renewEnergy / total * 100.0, 0, 'f', 1));
                rs->setLabelVisible(true);
                auto *cs = m_mixPie->append(QStringLiteral("常规机组"), convEnergy);
                cs->setColor(QColor(0x18, 0x5F, 0xA5));
                cs->setLabel(QStringLiteral("常规 %1%")
                                 .arg(convEnergy / total * 100.0, 0, 'f', 1));
                cs->setLabelVisible(true);
            }
        }
    }
}

// ============================================================
// P4 刷新（真申报阶梯 + 真出清曲线）
// ============================================================
void MainWindow::refreshChartPage()
{
    const auto &periods = m_session.result.periods;
    const int T = periods.size();
    const Perspective p = m_session.perspective;

    // 分时电价曲线（真实出清结果）
    if (m_priceSeries && m_renewSeries) {
        m_priceSeries->clear();
        m_renewSeries->clear();
        for (int i = 0; i < T; ++i) {
            m_priceSeries->append(i + 1, periods[i].clearingPrice);
            m_renewSeries->append(i + 1, periods[i].renewMW);
        }
        if (m_axisPriceX)
            m_axisPriceX->setRange(1, std::max(1, T));
    }

    // 供需交叉图（真实申报阶梯 + 出清价水平线）
    if (m_supplySeries && m_demandSeries && m_clearingLine) {
        m_supplySeries->clear();
        m_demandSeries->clear();
        m_clearingLine->clear();
        if (m_clearPoint)
            m_clearPoint->clear();

        // 时段截面（#89：下拉选择；默认 1）。0 量段（停机申报）不画——与引擎入参口径一致
        const int chartPeriod = m_chartPeriodCombo
                                    ? m_chartPeriodCombo->currentIndex() + 1 : 1;

        // #95：阶梯入参构造下沉到 MarketView，避免 UI 层直接遍历 generatorBids/consumerBids
        //   genSupplyCurve 已含渗透率换算的 RENEW 0 价段
        using CurvePoint = MarketView::CurvePoint;
        const QVector<CurvePoint> gen = MarketView::genSupplyCurve(
            m_session.market, chartPeriod, m_session.renewPercent / 100.0);
        const QVector<CurvePoint> con = MarketView::conDemandCurve(
            m_session.market, chartPeriod);

        double cumG = 0.0, cumC = 0.0, maxPrice = 0.0;
        for (const auto &s : gen) maxPrice = std::max(maxPrice, s.price);
        for (const auto &s : con) maxPrice = std::max(maxPrice, s.price);

        // 供给阶梯：每段画一条水平线（价格=该段申报价），段与段之间竖直跳变。
        //   从第一段报价起画（不从原点 0,0 出发）：每个申报段补两个点 = 水平线起点 + 终点，
        //   下一段的起点与前一段终点自动连成竖直跳变线，形成标准阶梯图。
        for (const auto &s : gen) {
            m_supplySeries->append(cumG, s.price);
            cumG += s.quantity;
            m_supplySeries->append(cumG, s.price);
        }
        // 需求阶梯：按报价降序，同样每段一条水平线
        for (const auto &s : con) {
            m_demandSeries->append(cumC, s.price);
            cumC += s.quantity;
            m_demandSeries->append(cumC, s.price);
        }
        const double maxX = std::max(cumG, cumC) * 1.05;

        // 出清价水平线 + 出清点标记（#89：按所选时段映射出清结果；
        //   24 期聚合视图时，15 分钟截面映射到所在小时的聚合时段）
        const PeriodResult *pr = nullptr;
        for (const auto &p : periods)
            if (p.period == chartPeriod) { pr = &p; break; }
        if (!pr && periods.size() == 24) {
            const int hourIdx = (chartPeriod - 1) / 4 + 1;
            for (const auto &p : periods)
                if (p.period == hourIdx) { pr = &p; break; }
        }
        if (!pr && !periods.isEmpty())
            pr = &periods.first();
        const double cp = pr ? pr->clearingPrice : 0.0;
        if (cp > 0.0) {
            m_clearingLine->append(0.0, cp);
            m_clearingLine->append(maxX, cp);
        }
        if (m_clearPoint) {
            const double cv = pr ? pr->clearedMW : 0.0;
            if (cp > 0.0 && cv > 0.0)
                m_clearPoint->append(cv, cp);
        }

        // 视角过滤：发电只看供给，购电只看需求，平台全看
        m_supplySeries->setVisible(p != Perspective::Con);
        m_demandSeries->setVisible(p != Perspective::Gen);
        m_clearingLine->setVisible(p == Perspective::Platform);
        if (m_clearPoint)
            m_clearPoint->setVisible(p == Perspective::Platform);

        if (m_axisSupplyX) {
            const double xMax = std::ceil(std::max(100.0, maxX) / 25.0) * 25.0;
            m_axisSupplyX->setRange(0.0, xMax);
            m_axisSupplyX->setTickCount(static_cast<int>(xMax / 25.0) + 1);
            m_axisSupplyX->setLabelFormat(QStringLiteral("%d"));
        }
        if (m_axisSupplyY) {
            const double yMax = std::ceil(std::max(100.0, maxPrice * 1.15) / 100.0) * 100.0;
            m_axisSupplyY->setRange(0.0, yMax);
            m_axisSupplyY->setTickCount(static_cast<int>(yMax / 100.0) + 1);
            m_axisSupplyY->setLabelFormat(QStringLiteral("%d"));
        }
    }
}

// ============================================================
// P5 刷新
// ============================================================
void MainWindow::refreshExportPage()
{
    const auto &periods = m_session.result.periods;
    if (periods.isEmpty())
        return;
    const Perspective p = m_session.perspective;

    double sum = 0.0, mx = -std::numeric_limits<double>::max(),
           mn = std::numeric_limits<double>::max(), volSum = 0.0;
    double fee = 0.0;
    for (const auto &pr : periods) {
        sum += pr.clearingPrice;
        volSum += pr.clearedMW;
        fee += (p == Perspective::Gen) ? pr.genFee
             : (p == Perspective::Con) ? pr.conFee
                                       : (pr.genFee + pr.conFee);
        mx = std::max(mx, pr.clearingPrice);
        mn = std::min(mn, pr.clearingPrice);
    }
    const double avg = sum / periods.size();

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
    const QString mode = (m_btnPab && m_btnPab->isChecked())
                             ? QStringLiteral("PAB") : QStringLiteral("MCP");
    const QString gran = (m_granCombo && m_granCombo->currentIndex() == 1)
                             ? QStringLiteral("96时段") : QStringLiteral("24时段");
    const QString view = perspectiveShort(m_session.perspective);
    if (m_fileDaily)
        m_fileDaily->setText(QStringLiteral("%1_结算日报_%2_%3_%4.csv").arg(view, date, mode, gran));
    if (m_fileCurve)
        m_fileCurve->setText(QStringLiteral("%1_电价曲线_%2_%3_%4.csv").arg(view, date, mode, gran));
}

// 顶栏页面标题 / 副标题
void MainWindow::updatePageHeader(int row)
{
    static const char *titles[5] = {
        "数据导入", "仿真控制", "出清结果", "图表分析", "结果导出",
    };
    static const char *subs[5] = {
        "申报数据 · 数据集状态 · 真实校验",
        "出清模式 · 时段颗粒度 · 新能源出力滑块",
        "核心指标 · 出力构成 · 出清明细（随视角切换）",
        "供需交叉 · 分时电价形态",
        "结算摘要 · 双 CSV 导出 · 导出记录",
    };
    if (row < 0 || row > 4)
        return;
    m_pageTitle->setText(QString::fromUtf8(titles[row]));
    m_pageSub->setText(QString::fromUtf8(subs[row]));
}

void MainWindow::addExportRecord(const QString &fileName)
{
    if (!m_exportLog)
        return;
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    auto *item = new QListWidgetItem(
        QStringLiteral("✓  [%1]  %2").arg(time, fileName));
    item->setForeground(QColor(0x27, 0x50, 0x0A));
    m_exportLog->insertItem(0, item);
}

// ============================================================
// 槽函数
// ============================================================
void MainWindow::onStartDemo()
{
    const QString dir = locateSamplesDir();
    if (dir.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("未找到内置样例目录 data/samples，请确认已 clone 完整仓库，或先在①导入数据"), 8000);
        return;
    }
    if (!loadDataFiles(dir + QStringLiteral("/benchmark/generator_bids.csv"),
                       dir + QStringLiteral("/benchmark/consumer_bids.csv"),
                       QStringLiteral("内置基准例（对拍锚点 250/50/12500）")))
        return;

    const QString mode = (m_btnPab && m_btnPab->isChecked())
                             ? QStringLiteral("PAB") : QStringLiteral("MCP");
    m_session.result = ClearingFacade::clearBenchmark(m_session.market, mode);
    m_session.hasResult = true;
    setHasResult(true);
    m_nav->setCurrentRow(2);

    // 状态栏展示实际模式与真实结算金额（PAB 下发电收入应明显低于 MCP）
    const auto &pr = m_session.result.periods.first();
    statusBar()->showMessage(
        QStringLiteral("一键演示完成：%1 模式 · 出清价 %2 元/MWh · 成交 %3 MWh · 发电收入 %4 元 / 购电费用 %5 元（购电侧统一按出清价结算；MCP 对拍锚点：250 / 50 / 12500）")
            .arg(mode)
            .arg(pr.clearingPrice, 0, 'f', 0)
            .arg(pr.clearedMW, 0, 'f', 0)
            .arg(pr.genFee, 0, 'f', 0)
            .arg(pr.conFee, 0, 'f', 0),
        8000);
}

void MainWindow::onRunSim()
{
    if (!m_session.hasData) {
        statusBar()->showMessage(
            QStringLiteral("请先在①数据导入加载申报数据（内置样例或 CSV）"), 6000);
        m_nav->setCurrentRow(0);
        return;
    }
    runClearing();
    if (!m_session.hasResult)
        return;
    setHasResult(true);
    m_nav->setCurrentRow(2);
    statusBar()->showMessage(
        QStringLiteral("仿真完成：%1 · %2 模式 · %3 时段")
            .arg(m_session.dataSource, m_session.result.mode)
            .arg(m_session.result.periods.size()),
        6000);
}

void MainWindow::onExportDaily()
{
    if (!m_hasResult || !m_fileDaily)
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
    out << QChar(0xFEFF);   // UTF-8 BOM：Excel 直接打开不乱码

    const auto &periods = m_session.result.periods;
    const Perspective p = m_session.perspective;

    if (p == Perspective::Platform) {
        // 逐时段全局日报（负荷概念已退场：总需求 = 购电申报总量，新能源 = 滑块直给）
        out << QStringLiteral("时段,出清电价(元/MWh),总需求(MW),出清电量(MWh),新能源出力(MW),常规出力(MW)\n");
        double volSum = 0.0, feeSum = 0.0, priceSum = 0.0;
        for (const auto &pr : periods) {
            const double fee = pr.genFee + pr.conFee;
            out << pr.time << ',' << QString::number(pr.clearingPrice, 'f', 1) << ','
                << QString::number(pr.loadMW, 'f', 1) << ','
                << QString::number(pr.clearedMW, 'f', 1) << ','
                << QString::number(pr.renewMW, 'f', 1) << ','
                << QString::number(pr.clearedMW - pr.renewMW, 'f', 1) << '\n';
            volSum += pr.clearedMW;
            feeSum += fee;
            priceSum += pr.clearingPrice;
        }
        out << QStringLiteral("汇总（全日）,,,%1,,%2\n").arg(QString::number(volSum, 'f', 1),
                                                            QString::number(feeSum, 'f', 1));
        out << QStringLiteral("日均电价(元/MWh),%1\n")
                .arg(QString::number(priceSum / periods.size(), 'f', 1));
    } else {
        // 发电侧 / 购电侧逐主体账单
        const bool genSide = (p == Perspective::Gen);
        out << (genSide
                    ? QStringLiteral("时段,机组,段,报价(元/MWh),中标量(MWh),收入(元)\n")
                    : QStringLiteral("时段,用户,段,报价(元/MWh),中标量(MWh),费用(元)\n"));
        double volSum = 0.0, moneySum = 0.0;
        for (const auto &pr : periods) {
            const auto &list = genSide ? pr.genDetails : pr.conDetails;
            for (const auto &e : list) {
                out << pr.time << ',' << e.name << ',' << e.segment << ','
                    << QString::number(e.bidPrice, 'f', 1) << ','
                    << QString::number(e.clearedMW, 'f', 1) << ','
                    << QString::number(e.money, 'f', 1) << '\n';
                volSum += e.clearedMW;
                moneySum += e.money;
            }
        }
        out << QStringLiteral("汇总,,,%1,%2\n")
                .arg(QString::number(volSum, 'f', 1), QString::number(moneySum, 'f', 1));
    }
    f.close();

    addExportRecord(QFileInfo(path).fileName());
    statusBar()->showMessage(QStringLiteral("已导出：%1").arg(path), 8000);
}

void MainWindow::onExportCurve()
{
    if (!m_hasResult || !m_fileCurve)
        return;
    const QString suggested = m_fileCurve->text();
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出电价曲线数据（96 期原生粒度）"), suggested, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    // #89：无论当前显示 24/96 颗粒度，导出一律按 96 期原生粒度重算
    //   （同参数同模式，结果与界面显示口径自洽）
    const QString mode = m_session.result.mode.isEmpty()
                             ? QStringLiteral("MCP") : m_session.result.mode;
    const ClearingResult native = ClearingFacade::clearPeriods(
        m_session.market, 96, m_session.renewPercent / 100.0, mode);
    const auto &rows = native.periods.isEmpty()
                           ? m_session.result.periods : native.periods;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(QStringLiteral("导出失败：无法打开文件 %1").arg(path), 6000);
        return;
    }
    QTextStream out(&f);
    out << QChar(0xFEFF);
    out << QStringLiteral("period,时刻,负荷(MW),新能源出力(MW),常规出力(MW),出清电价(元/MWh),出清电量(MWh)\n");
    for (const auto &pr : rows) {
        out << pr.period << ',' << pr.time << ','
            << QString::number(pr.loadMW, 'f', 1) << ','
            << QString::number(pr.renewMW, 'f', 1) << ','
            << QString::number(pr.clearedMW - pr.renewMW, 'f', 1) << ','
            << QString::number(pr.clearingPrice, 'f', 1) << ','
            << QString::number(pr.clearedMW, 'f', 1) << '\n';
    }
    f.close();

    addExportRecord(QFileInfo(path).fileName());
    statusBar()->showMessage(
        QStringLiteral("已导出 %1 期电价曲线：%2").arg(rows.size()).arg(path), 8000);
}

// ============================================================
// 全局 QSS：电力蓝 #185FA5 + 墨绿 #0F6E56 设计系统
// ============================================================
void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #F1F1F4; }

        /* ---------- 顶栏 ---------- */
        #pageHeader {
            background: #F5F9FD;
            border-left: 4px solid #185FA5;
            border-radius: 6px;
        }
        #pageTitle {
            color: #0C447C;
            font-size: 26px;
            font-weight: bold;
        }
        #pageSub { color: #6A6A73; font-size: 13px; }
        #readyDot { color: #3A3A42; font-size: 13px; }

        /* ---------- 三视角切换条 ---------- */
        #perspBar {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 8px;
        }
        #perspBtn {
            background: transparent;
            color: #5F5E5A;
            border: none;
            border-radius: 6px;
            padding: 7px 14px;
            font-size: 13px;
        }
        #perspBtn:hover { background: #E8F0FA; }
        #perspBtn:checked {
            background: #185FA5;
            color: #FFFFFF;
            font-weight: bold;
        }

        /* ---------- 封面页（深色科技风） ---------- */
        #coverPage {
            background: qlineargradient(x1:0, y1:0, x2:0.3, y2:1,
                                        stop:0 #042C53, stop:1 #0A3A6B);
        }
        #coverBrandIcon {
            color: #FFFFFF;
            background: #0F6E56;
            border-radius: 6px;
            padding: 3px 6px;
            font-size: 14px;
        }
        #coverBrandText { color: #B5D4F4; font-size: 12px; }
        #coverVersion {
            color: #85B7EB;
            border: 1px solid #185FA5;
            border-radius: 4px;
            padding: 3px 12px;
            font-size: 12px;
        }
        #coverEyebrow {
            color: #5DCAA5;
            font-size: 13px;
            letter-spacing: 4px;
        }
        #coverTitle {
            color: #FFFFFF;
            font-size: 44px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        #coverSub { color: #85B7EB; font-size: 15px; }
        #coverRule { background: #1D9E75; border: none; }
        #coverTagline { color: #D3D1C7; font-size: 17px; }
        #coverFeatCard {
            background: #0C447C;
            border: 1px solid #185FA5;
            border-radius: 10px;
        }
        #featBlue  { background: #378ADD; border: none; border-radius: 2px; }
        #featGreen { background: #1D9E75; border: none; border-radius: 2px; }
        #featAmber { background: #EF9F27; border: none; border-radius: 2px; }
        #coverFeatName { color: #FFFFFF; font-size: 17px; font-weight: bold; }
        #coverFeatDesc { color: #B5D4F4; font-size: 13px; }
        #enterBtn {
            background: #0F6E56;
            color: #FFFFFF;
            border: none;
            border-radius: 8px;
            padding: 14px 52px;
            font-size: 18px;
            font-weight: bold;
        }
        #enterBtn:hover  { background: #0D5E49; }
        #enterBtn:pressed { background: #0A4C3B; }
        #coverEnterHint { color: #7E9BC4; font-size: 12px; }
        #coverFooter { color: #8FA8C4; font-size: 13px; }

        /* ---------- 左侧导航 ---------- */
        #navList {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 10px;
            font-size: 15px;
            padding: 8px 6px;
            outline: 0;
        }
        #navList::item {
            height: 50px;
            border-radius: 6px;
            padding-left: 16px;
            margin: 2px 0;
            color: #5F5E5A;
            border-left: 4px solid transparent;
        }
        #navList::item:hover { background: #E8F0FA; }
        #navList::item:selected {
            background: #E6F1FB;
            color: #0C447C;
            font-weight: bold;
            border-left: 4px solid #185FA5;
        }
        #navFooter {
            color: #8A8A93;
            font-size: 11px;
            padding-left: 14px;
        }

        /* 通用按钮 */
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

        #secondaryBtn {
            background: #FFFFFF;
            color: #185FA5;
            border: 1px solid #185FA5;
        }
        #secondaryBtn:hover  { background: #E8F0FA; }
        #secondaryBtn:pressed { background: #D5E4F5; }

        #outlineDemoBtn {
            background: #FFFFFF;
            color: #185FA5;
            border: 1px solid #185FA5;
            font-size: 13px;
            padding: 6px 16px;
        }
        #outlineDemoBtn:hover  { background: #E8F0FA; }
        #outlineDemoBtn:pressed { background: #D5E4F5; }

        #demoBtn {
            background: #EF9F27;
            font-size: 15px;
            font-weight: bold;
            padding: 9px 22px;
        }
        #demoBtn:hover  { background: #D98E1B; }
        #demoBtn:pressed { background: #C07E14; }

        #runBtn {
            background: #0F6E56;
            font-size: 16px;
            font-weight: bold;
            padding: 12px 40px;
        }
        #runBtn:hover  { background: #0D5E49; }
        #runBtn:pressed { background: #0A4C3B; }
        #runBtn:disabled { background: #B9CDC6; }

        /* MCP / PAB 模式卡 */
        #modeCard {
            background: #FFFFFF;
            color: #3A3A42;
            border: 1px solid #C9C9D2;
            border-radius: 10px;
            padding: 14px 16px;
            font-size: 14px;
            text-align: left;
        }
        #modeCard:hover { background: #F7FAFD; }
        #modeCard:checked {
            background: #E6F1FB;
            color: #0C447C;
            border: 2px solid #185FA5;
            font-weight: bold;
        }

        /* 数据集状态卡（P1） */
        #statusCard {
            background: #F1F1F4;
            border-radius: 8px;
        }
        #statusCardName { color: #444441; font-size: 13px; font-weight: bold; }
        #statusCardDesc { color: #8A8A93; font-size: 11px; }
        #statusBadgeOk {
            background: #EAF3DE;
            color: #27500A;
            border-radius: 4px;
            padding: 2px 8px;
            font-size: 11px;
        }
        #statusBadgeWait {
            background: #FAEEDA;
            color: #633806;
            border-radius: 4px;
            padding: 2px 8px;
            font-size: 11px;
        }

        /* 校验汇总条（P1） */
        #checkBar {
            background: #EAF3DE;
            border: 1px solid #C0DD97;
            border-radius: 8px;
        }
        #checkBarIcon {
            background: #3B6D11;
            color: #FFFFFF;
            border-radius: 9px;
            min-width: 18px;
            min-height: 18px;
            font-weight: bold;
            font-size: 12px;
        }
        #checkBarText { color: #27500A; font-size: 13px; }

        /* 参数摘要行（P2） */
        #paramSummary {
            background: #F1F1F4;
            border-radius: 6px;
            padding: 10px 14px;
            color: #444441;
            font-size: 13px;
        }

        /* 分组容器 */
        QGroupBox {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 8px;
            margin-top: 24px;
            font-weight: bold;
            color: #0C447C;
            font-size: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 2px;
            top: 0px;
            padding: 0;
        }

        /* 表格 */
        QTableWidget {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            gridline-color: #EDEDF2;
            font-size: 13px;
            alternate-background-color: #F7F9FB;
            selection-background-color: #E8F0FA;
            selection-color: #1A1A1A;
        }
        QHeaderView::section {
            background: #0C447C;
            color: #FFFFFF;
            font-weight: bold;
            padding: 6px;
            border: none;
        }

        /* 页签 */
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
            background: #F1F1F4;
            border-radius: 8px;
        }
        #kpiValue {
            color: #0C447C;
            font-size: 24px;
            font-weight: bold;
        }
        #kpiName { color: #6A6A73; font-size: 12px; }

        /* 空态引导卡片 */
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

        /* 就绪灯 / 导出记录 / 文件名预览 */
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
        #fileLabel {
            background: #F1F1F4;
            border-radius: 4px;
            padding: 6px 10px;
            color: #444441;
            font-size: 13px;
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

        #hintLabel { color: #8A8A93; font-size: 12px; font-weight: normal; }
        #sumName   { color: #6A6A73; font-size: 14px; }
        #sumValue  { color: #185FA5; font-size: 14px; font-weight: bold; }
    )"));
}
