#include "mainwindow.h"

#include <algorithm>
#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

#include <QComboBox>
#include <QDate>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
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

    // ---------- 顶部栏：页面标题 banner（随导航切换）+ 数据就绪灯 + 一键演示 ----------
    m_pageTitle = new QLabel(QStringLiteral("数据导入"));
    m_pageTitle->setObjectName("pageTitle");
    m_pageSub = new QLabel(QStringLiteral("机组申报 · 数据集状态 · 申报校验"));
    m_pageSub->setObjectName("pageSub");

    // 标题 banner：浅蓝底 + 左侧蓝色竖条，视觉上独立成块
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
    demoBtn->setToolTip(QStringLiteral("加载内置基准案例，自动完成一次完整出清演示（当前为骨架版：内置假数据）"));
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
    topBar->addLayout(topRight);

    // ---------- 左侧导航：5 个页面（编号圆圈风格 + 底部版本标签） ----------
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
    navCol->addWidget(m_nav);   // 自然高度：白色卡片只包住 5 项，不再拉到底

    auto *navFooter = new QLabel(QStringLiteral("V 1.0 · 演示数据模式"));
    navFooter->setObjectName("navFooter");
    navCol->addSpacing(6);
    navCol->addWidget(navFooter);
    navCol->addStretch();       // 下方留白露出浅灰底，不显示白色长条

    // ---------- 右侧内容区：QStackedWidget 切页 ----------
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

    // ---------- 整体布局（主界面 = 顶栏 + 左导航 + 右内容区） ----------
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

    // ---------- 顶层页面栈：0 = 封面（导航隐藏） / 1 = 主界面 ----------
    m_rootStack = new QStackedWidget();
    m_rootStack->addWidget(buildCoverPage());
    m_rootStack->addWidget(mainWidget);
    m_rootStack->setCurrentIndex(0);   // 启动先显示封面

    setCentralWidget(m_rootStack);

    statusBar()->showMessage(QStringLiteral("就绪 · 界面骨架版（内置示例数据 · 出清为假数据）"));

    applyStyle();
}

// ============================================================
// 封面底部装饰：淡淡的负荷/新能源曲线底纹（电力行业符号）
// 无 Q_OBJECT 的局部小控件，只在封面页使用
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

        // 净负荷曲线（墨绿）
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

        // 光伏出力曲线（浅蓝，午间隆起）
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

// 封面页容器：窗口缩放时让曲线底纹始终铺满下半屏
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
// 页面 0：启动封面（深色科技风，参考电力交易平台启动页）
// 品牌行 / 大标题区 / 三张特性卡 / 进入平台 / 落款
// 点「进入平台」后切换到主界面并跳到 ①数据导入
// ============================================================
QWidget *MainWindow::buildCoverPage()
{
    auto *page = new CoverPage();
    page->setObjectName("coverPage");

    // 底部曲线底纹（置于最底层，随窗口缩放铺满下半屏）
    auto *backdrop = new CurveBackdrop(page);
    page->setBackdrop(backdrop);
    backdrop->lower();

    // 品牌行：左上电力标识 + 单位，右上版本徽章
    auto *brandIcon = new QLabel(QStringLiteral("⚡"), page);
    brandIcon->setObjectName("coverBrandIcon");
    auto *brandText = new QLabel(QStringLiteral("SEU · 电力市场课程设计"), page);
    brandText->setObjectName("coverBrandText");

    auto *version = new QLabel(QStringLiteral("V 1.0"), page);
    version->setObjectName("coverVersion");
    version->setAlignment(Qt::AlignCenter);

    auto *brandRow = new QHBoxLayout();
    brandRow->addWidget(brandIcon);
    brandRow->addSpacing(8);
    brandRow->addWidget(brandText);
    brandRow->addStretch();
    brandRow->addWidget(version);

    // 标题区：绿色小字 + 中文大标题 + 英文副标题 + 墨绿短线 + 定位语
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
        QStringLiteral("MCP / PAB 双模式出清 · 96 时段逐时仿真 · 单窗口五页面闭环"), page);
    tagline->setObjectName("coverTagline");
    tagline->setAlignment(Qt::AlignHCenter);

    // 三张特性卡：彩色顶边 + 标题 + 一行说明
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

    // 进入平台按钮：切到主界面并跳到 ①数据导入
    auto *enterBtn = new QPushButton(QStringLiteral("进入平台 →"), page);
    enterBtn->setObjectName("enterBtn");
    enterBtn->setToolTip(QStringLiteral("进入单窗口五页面主界面"));
    connect(enterBtn, &QPushButton::clicked, this, [this] {
        m_rootStack->setCurrentIndex(1);
        m_nav->setCurrentRow(0);
        statusBar()->showMessage(
            QStringLiteral("已进入平台 · 当前为界面骨架版（内置示例数据）"), 5000);
    });

    auto *enterHint = new QLabel(
        QStringLiteral("首次使用可点击主界面右上角「一键演示」快速体验"), page);
    enterHint->setObjectName("coverEnterHint");
    enterHint->setAlignment(Qt::AlignHCenter);

    // 底部落款
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
    table->setAlternatingRowColors(true);   // 斑马纹

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

    // --- 中：数据集状态卡（四张卡 · 数据契约 V1.0） ---
    struct DataSet { const char *name; const char *desc; bool ready; };
    const DataSet sets[4] = {
                              {"机组申报数据",   "8 台机组（5 火 2 风 1 光 + 储能）", true},
                              {"用户申报数据",   "双边申报（基准例）",               true},
                              {"负荷曲线",       "96 时段 · 双驼峰典型曲线",          false},
                              {"新能源出力",     "风 / 光逐时段出力曲线",             false},
                              };
    auto *setRow = new QHBoxLayout();
    setRow->setSpacing(10);
    for (const auto &s : sets) {
        auto *card = new QWidget(page);
        card->setObjectName("statusCard");
        auto *name = new QLabel(QString::fromUtf8(s.name), card);
        name->setObjectName("statusCardName");
        auto *badge = new QLabel(
            s.ready ? QStringLiteral("✓ 已就绪") : QStringLiteral("待导入（A 样例）"), card);
        badge->setObjectName(s.ready ? "statusBadgeOk" : "statusBadgeWait");
        badge->setAlignment(Qt::AlignCenter);
        auto *desc = new QLabel(QString::fromUtf8(s.desc), card);
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
    }

    // --- 下：校验反馈汇总条（申报校验五规则 · 静态展示） ---
    auto *checkBar = new QWidget(page);
    checkBar->setObjectName("checkBar");
    auto *checkIcon = new QLabel(QStringLiteral("✓"), checkBar);
    checkIcon->setObjectName("checkBarIcon");
    checkIcon->setAlignment(Qt::AlignCenter);
    auto *checkText = new QLabel(checkBar);
    checkText->setObjectName("checkBarText");
    checkText->setText(QStringLiteral(
        "<b>申报校验：5 项规则全部通过</b>　"
        "段数 ≤5 · 单调不减 · 报价单位 1 元/MWh · 出力单位 1 MW · 报价区间 0 ~ 540 元/MWh"
        "<br><span style='color:#6A8F75;'>骨架版：内置示例数据的静态展示，逐行真实校验由 A 模块实现"
        "（依据《电力现货市场基本规则（试行）》4.2.3 条）</span>"));
    checkText->setTextFormat(Qt::RichText);
    auto *checkLay = new QHBoxLayout(checkBar);
    checkLay->setContentsMargins(14, 10, 14, 10);
    checkLay->setSpacing(12);
    checkLay->addWidget(checkIcon);
    checkLay->addWidget(checkText, 1);

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);
    outer->addWidget(bidBox, 3);
    outer->addLayout(setRow);
    outer->addWidget(checkBar);
    return page;
}

// ============================================================
// 页面 2：仿真控制（模式卡 + 颗粒度 + 渗透率 + 参数摘要 + 就绪灯）
// ============================================================
QWidget *MainWindow::buildControlPage()
{
    auto *page = new QWidget();
    auto *box = new QGroupBox(QStringLiteral("仿真参数设置"), page);

    // 数据就绪状态灯（数据未就绪时开始仿真按钮将置灰）
    auto *readyLabel = new QLabel();
    readyLabel->setText(QStringLiteral(
        "数据状态：<span style='color:#2FA84F;'>●</span> <b>已就绪</b>（内置示例数据）"));
    readyLabel->setTextFormat(Qt::RichText);
    readyLabel->setObjectName("readyLabel");

    // --- 出清模式：两张可点选模式卡（替代下拉框，突出双模式创新点） ---
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

    // --- 其余参数：颗粒度 + 渗透率 ---
    auto *form = new QFormLayout();

    m_granCombo = new QComboBox();
    m_granCombo->addItems({
        QStringLiteral("24 时段（每 1 小时）"),
        QStringLiteral("96 时段（每 15 分钟）"),
    });
    m_granCombo->setToolTip(QStringLiteral("创新点③：96 时段连续出清，依据规则 4.1 条"));
    form->addRow(QStringLiteral("时段颗粒度："), m_granCombo);

    // 模式 / 颗粒度变化 → P5 文件名预览 + 参数摘要联动
    // 注：两个模式卡都要连 toggled，互斥切换时最后一次信号带出正确选中态
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

    // --- 参数摘要行（一眼看清本次仿真的关键口径，随颗粒度联动） ---
    m_paramSummary = new QLabel();
    m_paramSummary->setObjectName("paramSummary");
    auto refreshSummary = [this] {
        const bool gran96 = m_granCombo && m_granCombo->currentIndex() == 1;
        const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        m_paramSummary->setText(QStringLiteral(
                                    "仿真日期 %1　·　时段颗粒度 %2　·　出清时段 %3 点　·　限价区间 0 ~ 540 元/MWh")
                                    .arg(today,
                                         gran96 ? QStringLiteral("15 分钟") : QStringLiteral("1 小时"),
                                         gran96 ? QStringLiteral("96") : QStringLiteral("24")));
    };
    refreshSummary();

    auto *runBtn = new QPushButton(QStringLiteral("▶  开始仿真"));
    runBtn->setObjectName("runBtn");
    runBtn->setToolTip(QStringLiteral(
        "数据未就绪时此按钮置灰并提示「请先保存申报数据」（当前内置示例已就绪）"));
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::onRunSim);

    auto *hint = new QLabel(QStringLiteral(
        "骨架版说明：点击「开始仿真」用内置假数据生成一次出清结果并跳转查看。\n"
        "真实出清引擎（B 模块：MCP 边际电价法 / PAB 报价撮合法）接入后，这里将驱动一次完整计算。"));
    hint->setObjectName("hintLabel");

    auto *lay = new QVBoxLayout(box);
    lay->addWidget(readyLabel);
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

    // 指标卡一行四张：灰底大数字卡，一眼看到核心结果
    auto *kpiRow = new QWidget(content);
    auto *kpiLay = new QHBoxLayout(kpiRow);
    kpiLay->setContentsMargins(0, 0, 0, 0);
    kpiLay->setSpacing(10);
    struct Kpi { QLabel **value; const char *name; };
    const Kpi kpis[4] = {
                          {&m_kpiAvg,    "出清均价 (元/MWh)"},
                          {&m_kpiVol,    "全天总出清电量 (MWh)"},
                          {&m_kpiFee,    "全天结算总额 (元)"},
                          {&m_kpiSpread, "峰谷价差比"},
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
    m_resultTable->setAlternatingRowColors(true);   // 斑马纹

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
        m_renewSeries->setColor(QColor(0x0F, 0x6E, 0x56));   // 墨绿 #0F6E56（与封面呼应）

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

    // 中：文件名预览（随模式 / 颗粒度联动，等宽代码风格）
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
    // 指标卡：均价 / 总电量 / 结算总额 / 峰谷价差比
    double sum = 0.0, mx = -1e9, mn = 1e9, volSum = 0.0, fee = 0.0;
    for (int i = 0; i < m_price.size(); ++i) {
        sum += m_price[i];
        volSum += m_vol[i];
        fee += m_price[i] * m_vol[i];
        if (m_price[i] > mx) mx = m_price[i];
        if (m_price[i] < mn) mn = m_price[i];
    }
    const double avg = sum / m_price.size();
    m_kpiAvg->setText(QStringLiteral("%1").arg(avg, 0, 'f', 1));
    m_kpiVol->setText(QStringLiteral("%1").arg(volSum, 0, 'f', 0));
    m_kpiFee->setText(QStringLiteral("%1").arg(fee, 0, 'f', 0));
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
    const QString mode = (m_btnPab && m_btnPab->isChecked())
                             ? QStringLiteral("PAB") : QStringLiteral("MCP");
    const QString gran = (m_granCombo && m_granCombo->currentIndex() == 1)
                             ? QStringLiteral("96时段") : QStringLiteral("24时段");
    if (m_fileDaily)
        m_fileDaily->setText(QStringLiteral("结算日报_%1_%2_%3.csv").arg(date, mode, gran));
    if (m_fileCurve)
        m_fileCurve->setText(QStringLiteral("电价曲线_%1_%2_%3.csv").arg(date, mode, gran));
}

// 顶栏页面标题 / 副标题（随左侧导航切换）
void MainWindow::updatePageHeader(int row)
{
    static const char *titles[5] = {
        "数据导入", "仿真控制", "出清结果", "图表分析", "结果导出",
    };
    static const char *subs[5] = {
        "机组申报 · 数据集状态 · 申报校验",
        "出清模式 · 时段颗粒度 · 新能源渗透率",
        "核心指标 · 24 时段出清明细",
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
    item->setForeground(QColor(0x27, 0x50, 0x0A));   // 成功绿
    m_exportLog->insertItem(0, item);
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
// 全局 QSS：主界面电力蓝 #185FA5 + 墨绿 #0F6E56 设计系统
// 封面为深色科技风（深藏青 #042C53），与主界面浅色系形成
// 「封面 → 平台」两段式视觉结构（参考电力交易平台启动页）
// ============================================================
void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #F1F1F4; }

        /* ---------- 顶栏：页面标题 banner / 副标题 / 就绪灯 ---------- */
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

        /* 顶栏一键演示（描边款：不与主操作抢焦点） */
        #outlineDemoBtn {
            background: #FFFFFF;
            color: #185FA5;
            border: 1px solid #185FA5;
            font-size: 13px;
            padding: 6px 16px;
        }
        #outlineDemoBtn:hover  { background: #E8F0FA; }
        #outlineDemoBtn:pressed { background: #D5E4F5; }

        /* 强调按钮（琥珀橙：导出 / 空态卡片一键演示） */
        #demoBtn {
            background: #EF9F27;
            font-size: 15px;
            font-weight: bold;
            padding: 9px 22px;
        }
        #demoBtn:hover  { background: #D98E1B; }
        #demoBtn:pressed { background: #C07E14; }

        /* 开始仿真（墨绿大按钮） */
        #runBtn {
            background: #0F6E56;
            font-size: 16px;
            font-weight: bold;
            padding: 12px 40px;
        }
        #runBtn:hover  { background: #0D5E49; }
        #runBtn:pressed { background: #0A4C3B; }
        #runBtn:disabled { background: #B9CDC6; }

        /* MCP / PAB 模式卡（可点选，选中蓝描边） */
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

        /* 校验汇总条（P1 · 绿色横条） */
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

        /* 分组容器：标题位于框线上方的独立标题带（不压线） */
        QGroupBox {
            background: #FFFFFF;
            border: 1px solid #E2E2E8;
            border-radius: 8px;
            margin-top: 24px;   /* 标题带高度：标题与框线之间留出间距 */
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

        /* 表格（深蓝表头 + 斑马纹） */
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

        /* 指标卡（P3 · 灰底大数字） */
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

        /* 数据就绪灯 / 导出记录 / 文件名预览 */
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

        /* 说明文字 / 摘要 */
        #hintLabel { color: #8A8A93; font-size: 12px; font-weight: normal; }
        #sumName   { color: #6A6A73; font-size: 14px; }
        #sumValue  { color: #185FA5; font-size: 14px; font-weight: bold; }
    )"));
}
