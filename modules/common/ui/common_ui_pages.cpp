#include "mainwindow.h"
#include "modules/themes/fitness/calendar/fitness_calendar_mark_builder.h"
#include "modules/ui/profile/profile_interaction_helper.h"

#include <QAbstractItemView>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

using mouseplan::fitness::CalendarCellMark;

class PlanCalendarWidget : public QCalendarWidget {
public:
    explicit PlanCalendarWidget(QWidget *parent = nullptr)
        : QCalendarWidget(parent)
    {
    }

    void setMarkProvider(const std::function<CalendarCellMark(const QDate &)> &provider)
    {
        markProvider = provider;
        updateCells();
    }

protected:
    void paintCell(QPainter *painter, const QRect &rect, const QDate &date) const override
    {
        QCalendarWidget::paintCell(painter, rect, date);
        if (!markProvider) {
            return;
        }
        const CalendarCellMark mark = markProvider(date);
        painter->save();

        if (!mark.marker.isEmpty()) {
            QFont markerFont = painter->font();
            markerFont.setPointSize(qMax(8, markerFont.pointSize() - 1));
            markerFont.setBold(true);
            painter->setFont(markerFont);
            painter->setPen(mark.marker == QStringLiteral("休") ? QColor("#8fbca0") : QColor("#2b6a4e"));
            QRect markerRect = rect.adjusted(0, rect.height() / 2, 0, -2);
            painter->drawText(markerRect, Qt::AlignHCenter | Qt::AlignBottom, mark.marker);
        }

        if (mark.submitted) {
            QFont checkFont = painter->font();
            checkFont.setPointSize(qMax(10, checkFont.pointSize()));
            checkFont.setBold(true);
            painter->setFont(checkFont);
            painter->setPen(QColor("#d36b2c"));
            painter->drawText(rect.adjusted(0, 1, -4, 0), Qt::AlignRight | Qt::AlignTop, QStringLiteral("✓"));
        }

        painter->restore();
    }

private:
    std::function<CalendarCellMark(const QDate &)> markProvider;
};

void enableMobileSingleFingerScroll(QAbstractScrollArea *area)
{
    if (!area || !area->viewport()) {
        return;
    }
    QWidget *viewport = area->viewport();
    QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture);

    QScroller *scroller = QScroller::scroller(viewport);
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.012);
    props.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.22);
    props.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.32);
    props.setScrollMetric(QScrollerProperties::AcceleratingFlickSpeedupFactor, 1.0);
    props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.12);
    scroller->setScrollerProperties(props);

    if (QAbstractItemView *itemView = qobject_cast<QAbstractItemView *>(area)) {
        itemView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        itemView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
}

} // namespace

// 功能：初始化登录页面控件与交互。
void MainWindow::setupLoginPage()
{
    
    loginPage = new QWidget(rootWidget);
    loginPage->setObjectName(QStringLiteral("loginPage"));
    if (rootStackLayout) {
        rootStackLayout->addWidget(loginPage);
    }
    loginMainLayout = new QVBoxLayout(loginPage);
    loginMainLayout->setContentsMargins(24, 24, 24, 24);
    loginMainLayout->setSpacing(16);

    logoLabel = new QLabel(QStringLiteral("MOUSE PLAN"), loginPage);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setObjectName(QStringLiteral("logoLabel"));
    logoLabel->setMinimumHeight(92);

    loginCaptionLabel = new QLabel(QStringLiteral("学习记录 · 健身计划 · 自律管理"), loginPage);
    loginCaptionLabel->setAlignment(Qt::AlignCenter);
    loginCaptionLabel->setObjectName(QStringLiteral("loginCaptionLabel"));

    loginModeLabel = new QLabel(loginPage);
    loginModeLabel->setAlignment(Qt::AlignCenter);
    loginModeLabel->setObjectName(QStringLiteral("loginModeLabel"));

    loginCard = new QFrame(loginPage);
    loginCard->setObjectName(QStringLiteral("loginCard"));
    QVBoxLayout *cardLayout = new QVBoxLayout(loginCard);
    cardLayout->setContentsMargins(22, 22, 22, 20);
    cardLayout->setSpacing(18);

    usernameEdit = new QLineEdit(loginCard);
    usernameEdit->setPlaceholderText(QStringLiteral("用户名"));
    usernameEdit->setClearButtonEnabled(true);
    passwordEdit = new QLineEdit(loginCard);
    passwordEdit->setPlaceholderText(QStringLiteral("密码"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setClearButtonEnabled(true);

    agreementCheckBox = new QCheckBox(QStringLiteral("我已阅读并同意《用户服务协议与隐私说明》"), loginCard);
    agreementCheckBox->setStyleSheet("font-size:20px;color:#4e5f56;spacing:10px;");

    loginButton = new QPushButton(QStringLiteral("登录"), loginCard);
    loginButton->setObjectName(QStringLiteral("primaryLoginButton"));
    registerButton = new QPushButton(QStringLiteral("注册"), loginCard);
    registerButton->setFlat(true);
    loginServerConfigButton = new QPushButton(QStringLiteral("服务器设置"), loginCard);
    loginServerConfigButton->setFlat(true);
    loginServerConfigButton->setStyleSheet("font-size:18px;color:#5f6f66;padding:0 6px;");
    forgotButton = new QPushButton(QStringLiteral("忘记密码"), loginCard);
    forgotButton->setFlat(true);
    loginModeSwitchButton = new QPushButton(loginCard);
    loginModeSwitchButton->setFlat(true);

    loginTailLayout = new QHBoxLayout();
    loginTailLayout->addWidget(loginModeSwitchButton);
    loginTailLayout->addStretch();
    loginTailLayout->addWidget(loginServerConfigButton);
    loginTailLayout->addWidget(registerButton);
    loginTailLayout->addWidget(forgotButton);

    loginMainLayout->addStretch();
    loginMainLayout->addWidget(logoLabel);
    loginMainLayout->addWidget(loginCaptionLabel);
    loginMainLayout->addWidget(loginModeLabel);
    cardLayout->addWidget(usernameEdit);
    cardLayout->addSpacing(16);
    cardLayout->addWidget(passwordEdit);
    cardLayout->addWidget(agreementCheckBox);
    cardLayout->addWidget(loginButton);
    cardLayout->addLayout(loginTailLayout);
    loginMainLayout->addWidget(loginCard, 0, Qt::AlignHCenter);
    loginMainLayout->addStretch();

    /****************** 连接信号与槽 BEGIN********************/

    QObject::connect(loginButton, &QPushButton::clicked, this, &MainWindow::tryLogin);
    QObject::connect(agreementCheckBox, &QCheckBox::clicked, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        if (!promptAgreementDialog()) {
            agreementCheckBox->blockSignals(true);
            agreementCheckBox->setChecked(false);
            agreementCheckBox->blockSignals(false);
        }
    });
    QObject::connect(registerButton, &QPushButton::clicked, this, &MainWindow::runRegisterFlow);
    QObject::connect(loginServerConfigButton, &QPushButton::clicked, this, &MainWindow::openServerConfigDialog);
    QObject::connect(loginModeSwitchButton, &QPushButton::clicked, this, [this]() {
        if (loginLocalMode) {
            QMessageBox::information(this,
                                     QStringLiteral("切换为在线模式"),
                                     QStringLiteral("在线模式会启用云端同步能力,支持在线导入学习计划，支持密码数据找回\n"));
            loginLocalMode = false;
        } else {
            QMessageBox::information(this,
                                     QStringLiteral("切换为本地模式"),
                                     QStringLiteral("本地模式不依赖网络，数据仅保存在当前设备。\n 若忘记本地密码，当前版本无法找回，只能重新注册新本地账号!"));
            loginLocalMode = true;
        }
        if (agreementCheckBox) {
            agreementCheckBox->blockSignals(true);
            agreementCheckBox->setChecked(false);
            agreementCheckBox->blockSignals(false);
        }
        refreshLoginModeUi();
    });
    QObject::connect(forgotButton, &QPushButton::clicked, this, [this]() {
        if (loginLocalMode) {
            QMessageBox::information(this,
                                     QStringLiteral("忘记密码"),
                                     QStringLiteral("本地模式可直接重新注册新账号。\n若旧本地账号已忘记密码，当前版本暂未提供找回功能。"));
        } else {
            QMessageBox::information(this,
                                     QStringLiteral("忘记密码"),
                                     QStringLiteral("在线模式找回密码接口待实现"));
        }
    });

    /****************** 连接信号与槽 END********************/

    refreshLoginModeUi();
}

// 功能：初始化主页控件与布局。
void MainWindow::setupHomePage()
{
    homePage = new QWidget(rootWidget);
    homePage->setObjectName(QStringLiteral("homePage"));
    if (rootStackLayout) {
        rootStackLayout->addWidget(homePage);
    }
    homeMainLayout = new QVBoxLayout(homePage);
    homeMainLayout->setContentsMargins(0, 0, 0, 0);
    homeMainLayout->setSpacing(12);

    QWidget *contentHost = new QWidget(homePage);
    homeContentStack = new QStackedLayout(contentHost);
    homeContentStack->setContentsMargins(0, 0, 0, 0);
    homeContentStack->setSpacing(0);

    homeContentPanel = new QWidget(contentHost);
    QVBoxLayout *homeContentLayout = new QVBoxLayout(homeContentPanel);
    homeContentLayout->setContentsMargins(0, 0, 0, 0);
    homeContentLayout->setSpacing(12);

    calendarCard = new QFrame(homeContentPanel);
    calendarCard->setObjectName(QStringLiteral("calendarCard"));
    QVBoxLayout *calendarCardLayout = new QVBoxLayout(calendarCard);
    calendarCardLayout->setContentsMargins(12, 12, 12, 12);
    calendarCardLayout->setSpacing(8);

    PlanCalendarWidget *planCal = new PlanCalendarWidget(calendarCard);
    calendar = planCal;
    calendar->setObjectName(QStringLiteral("fitnessCalendar"));
    calendar->setSelectedDate(QDate::currentDate());
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setGridVisible(false);

    planCal->setMarkProvider([this](const QDate &date) {
        const QDate today = QDate::currentDate();
        const CalendarPlanInfo info = resolvePlanInfo(date);
        const TrainingRecord *record = recordForDate(date);
        // 健身主题子模块：日历标记判定逻辑下沉到独立模块，主窗口仅负责组装上下文。
        return mouseplan::fitness::FitnessCalendarMarkBuilder::buildMark(date,
                                                                         today,
                                                                         info.hasPlan,
                                                                         info.isRestDay,
                                                                         record);
    });
    calendarCardLayout->addWidget(calendar);

    middleContainer = new QWidget(homeContentPanel);
    middleSplitLayout = new QBoxLayout(QBoxLayout::LeftToRight, middleContainer);
    middleSplitLayout->setContentsMargins(0, 0, 0, 0);
    middleSplitLayout->setSpacing(12);

    QFrame *leftFrame = new QFrame(middleContainer);
    leftPlanContainer = leftFrame;
    leftFrame->setObjectName(QStringLiteral("todayPlanCard"));
    todayPlanCard = leftFrame;
    leftFrame->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftFrame);
    leftLayout->setContentsMargins(14, 14, 14, 14);
    leftLayout->setSpacing(10);
    dayTitleLabel = new QLabel(QStringLiteral("当日计划"), leftFrame);
    dayTitleLabel->setObjectName(QStringLiteral("dayTitleLabel"));
    dayHintLabel = new QLabel(leftFrame);
    dayHintLabel->setObjectName(QStringLiteral("dayHintLabel"));
    dayHintLabel->setWordWrap(true);

    itemsScrollArea = new QScrollArea(leftFrame);
    itemsScrollArea->setObjectName(QStringLiteral("itemsScrollArea"));
    itemsScrollArea->setWidgetResizable(true);
    itemsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    itemsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    enableMobileSingleFingerScroll(itemsScrollArea);
    itemsContainer = new QWidget(itemsScrollArea);
    itemsContainer->setObjectName(QStringLiteral("itemsContainer"));
    itemsLayout = new QVBoxLayout(itemsContainer);
    itemsLayout->setContentsMargins(0, 4, 0, 0);
    itemsLayout->setSpacing(10);
    itemsLayout->setAlignment(Qt::AlignTop);
    itemsScrollArea->setWidget(itemsContainer);

    leftLayout->addWidget(dayTitleLabel);
    leftLayout->addWidget(dayHintLabel);
    leftLayout->addWidget(itemsScrollArea);

    QFrame *rightFrame = new QFrame(middleContainer);
    rightPlanContainer = rightFrame;
    rightFrame->setObjectName(QStringLiteral("planQuickCard"));
    rightFrame->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightFrame);
    rightLayout->setContentsMargins(14, 14, 14, 14);
    rightLayout->setSpacing(8);
    QLabel *quickTitle = new QLabel(QStringLiteral("计划管理"), rightFrame);
    quickTitle->setStyleSheet("font-size: 12pt;");  //m
    quickTitle->setObjectName(QStringLiteral("quickTitleLabel"));
    planSettingButton = new QPushButton(QStringLiteral("总\n计\n划\n设\n置"), rightFrame);
    planSettingButton->setObjectName(QStringLiteral("planSettingButton"));
    planSettingButton->setMinimumWidth(80);
    planSettingButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightLayout->addWidget(quickTitle);
    rightLayout->addWidget(planSettingButton);

    middleSplitLayout->addWidget(leftFrame, 5);
    middleSplitLayout->addWidget(rightFrame, 1);

//    TrainingRecord *record = recordForDate(selectedDate);
    submitTodayButton = new QPushButton(QStringLiteral("提交当日记录"), homeContentPanel);
    submitTodayButton->setObjectName(QStringLiteral("submitTodayButton"));
    submitTodayButton->setMinimumHeight(44);

    homeContentLayout->addWidget(calendarCard);
    homeContentLayout->addWidget(middleContainer, 1);
    homeContentLayout->addWidget(submitTodayButton);

    profilePanel = new QFrame(homePage);
    profilePanel->setObjectName(QStringLiteral("profilePanel"));
    setupProfilePanelUi();

    themePanel = new QWidget(contentHost);
    themePanel->setObjectName(QStringLiteral("themePanel"));
    QVBoxLayout *themeLayout = new QVBoxLayout(themePanel);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(0);

    QWidget *themeTopBlank = new QWidget(themePanel);
    themeTopBlank->setObjectName(QStringLiteral("themeTopBlank"));
    themeTopBlank->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QWidget *themeCenter = new QWidget(themePanel);
    themeCenter->setObjectName(QStringLiteral("themeCenter"));
    QVBoxLayout *themeCenterLayout = new QVBoxLayout(themeCenter);
    themeCenterLayout->setContentsMargins(20, 18, 20, 18);
    themeCenterLayout->setSpacing(14);

    QLabel *themeTitle = new QLabel(QStringLiteral("切换主题"), themeCenter);
    themeTitle->setAlignment(Qt::AlignCenter);
    themeTitle->setStyleSheet("font-size:44px;font-weight:900;color:#284137;");

    QPushButton *fitThemeBtn = new QPushButton(QStringLiteral("健身主题"), themeCenter);
    QPushButton *studyThemeBtn = new QPushButton(QStringLiteral("学习主题"), themeCenter);
    QPushButton *normalThemeBtn = new QPushButton(QStringLiteral("普通计划主题"), themeCenter);
    const QString themeSwitchBtnStyle = "font-size:36px;min-height:140px;border-radius:20px;font-weight:800;";
    fitThemeBtn->setStyleSheet(themeSwitchBtnStyle + QStringLiteral("background:#ffd9b3;color:#7a3d14;"));
    studyThemeBtn->setStyleSheet(themeSwitchBtnStyle + QStringLiteral("background:#dcecff;color:#204a7a;"));
    normalThemeBtn->setStyleSheet(themeSwitchBtnStyle + QStringLiteral("background:#ffdadd;color:#7a2d2d;"));

    QWidget *themeBottomBlank = new QWidget(themePanel);
    themeBottomBlank->setObjectName(QStringLiteral("themeBottomBlank"));
    themeBottomBlank->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    themeCenterLayout->addStretch(1);
    themeCenterLayout->addWidget(themeTitle);
    themeCenterLayout->addWidget(fitThemeBtn);
    themeCenterLayout->addWidget(studyThemeBtn);
    themeCenterLayout->addWidget(normalThemeBtn);
    themeCenterLayout->addStretch(1);

    themeLayout->addWidget(themeTopBlank);
    themeLayout->addWidget(themeCenter, 1);
    themeLayout->addWidget(themeBottomBlank);

    auto applyThemeByKey = [this](const QString &key) {
        User *u = currentUser();
        if (!u) {
            return;
        }
        u->theme = key;
        u->themeChosen = true;
        store.save();
        applyThemeStyle();
        rebuildDayView();
    };

    QObject::connect(fitThemeBtn, &QPushButton::clicked, this, [applyThemeByKey]() { applyThemeByKey(QStringLiteral("fitness")); });
    QObject::connect(studyThemeBtn, &QPushButton::clicked, this, [applyThemeByKey]() { applyThemeByKey(QStringLiteral("study")); });
    QObject::connect(normalThemeBtn, &QPushButton::clicked, this, [applyThemeByKey]() { applyThemeByKey(QStringLiteral("normal")); });

    homeContentStack->addWidget(homeContentPanel);
    homeContentStack->addWidget(profilePanel);
    homeContentStack->addWidget(themePanel);
    homeContentStack->setCurrentWidget(homeContentPanel);

    navBarCard = new QFrame(homePage);
    navBarCard->setObjectName(QStringLiteral("navBarCard"));
    QWidget *nav = navBarCard;
    navLayout = new QHBoxLayout(nav);
    navLayout->setContentsMargins(10, 8, 10, 8);
    navLayout->setSpacing(4);
    // Navigation icon path interface: replace these 3 strings with your own resources later.
    const QString navHomeIconPath = QStringLiteral(":/img/MousePlan");
    const QString navThemeIconPath = QStringLiteral(":/img/dehead");
    const QString navProfileIconPath = QStringLiteral(":/img/MousePlan");

    homeNavButton = new QToolButton(nav);
    homeNavButton->setText(QStringLiteral("Home"));
    homeNavButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    homeNavButton->setIcon(QIcon(navHomeIconPath));
    if (homeNavButton->icon().isNull()) {
        homeNavButton->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
    }
    themeNavButton = new QToolButton(nav);
    themeNavButton->setText(QStringLiteral("主题色"));
    themeNavButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    themeNavButton->setIcon(QIcon(navThemeIconPath));
    if (themeNavButton->icon().isNull()) {
        themeNavButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    }
    profileNavButton = new QToolButton(nav);
    profileNavButton->setText(QStringLiteral("我的"));
    profileNavButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    profileNavButton->setIcon(QIcon(navProfileIconPath));
    if (profileNavButton->icon().isNull()) {
        profileNavButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));
    }
    homeNavButton->setObjectName(QStringLiteral("navHomeButton"));
    themeNavButton->setObjectName(QStringLiteral("navThemeButton"));
    profileNavButton->setObjectName(QStringLiteral("navProfileButton"));
    navLayout->addWidget(homeNavButton);
    navLayout->addSpacing(12);
    navLayout->addWidget(themeNavButton);
    navLayout->addSpacing(12);
    navLayout->addWidget(profileNavButton);

    homeMainLayout->addWidget(contentHost, 1);
    homeMainLayout->addWidget(navBarCard);

    // Rebuild once after nav bar is created, so profile top spacer can use real nav height.
    setupProfilePanelUi();

    QObject::connect(calendar, &QCalendarWidget::selectionChanged, this, [this]() {
        onCalendarDateChanged(calendar->selectedDate());
    });
    QObject::connect(planSettingButton, &QPushButton::clicked, this, &MainWindow::openPlanManagerDialog);

    QObject::connect(submitTodayButton, &QPushButton::clicked, this, &MainWindow::submitTodayRecord);

    QObject::connect(homeNavButton, &QToolButton::clicked, this, [this]() {
        if (homeContentStack && homeContentStack->currentWidget() == homeContentPanel) {
            homeNavButton->setChecked(true);
            themeNavButton->setChecked(false);
            profileNavButton->setChecked(false);
            return;
        }
        homeNavButton->setChecked(true);
        themeNavButton->setChecked(false);
        profileNavButton->setChecked(false);
        showHomeTab();
        rebuildDayView();
    });
    QObject::connect(themeNavButton, &QToolButton::clicked, this, [this]() {
        const bool onHome = homeContentStack && homeContentStack->currentWidget() == homeContentPanel;
        const bool onProfile = homeContentStack && homeContentStack->currentWidget() == profilePanel;
        homeNavButton->setChecked(onHome);
        themeNavButton->setChecked(true);
        profileNavButton->setChecked(onProfile);
        openThemeColorFromNav();
        themeNavButton->setChecked(false);
        homeNavButton->setChecked(homeContentStack && homeContentStack->currentWidget() == homeContentPanel);
        profileNavButton->setChecked(homeContentStack && homeContentStack->currentWidget() == profilePanel);
    });
    QObject::connect(profileNavButton, &QToolButton::clicked, this, [this]() {
        if (homeContentStack && homeContentStack->currentWidget() == profilePanel) {
            homeNavButton->setChecked(false);
            themeNavButton->setChecked(false);
            profileNavButton->setChecked(true);
            return;
        }
        homeNavButton->setChecked(false);
        themeNavButton->setChecked(false);
        profileNavButton->setChecked(true);
        showProfileTab();
    });
}

// 功能：切换到主页标签并更新导航状态。
void MainWindow::showHomeTab()
{
    if (homeContentStack && homeContentPanel) {
        homeContentStack->setCurrentWidget(homeContentPanel);
    }
    if (homeNavButton && themeNavButton && profileNavButton) {
        homeNavButton->setChecked(true);
        themeNavButton->setChecked(false);
        profileNavButton->setChecked(false);
    }
    if (navBarCard) {
        navBarCard->setVisible(true);
    }
    applyResponsiveLayout();
    QTimer::singleShot(0, this, [this]() {
        applyResponsiveLayout();
    });
}

// 功能：初始化我的页面控件。
void MainWindow::setupProfilePanelUi()
{
// 该文件由重构生成：保持原函数逻辑不变，仅做文件分层。
    if (!profilePanel) {
        return;
    }

    const bool wasProfileVisible = profilePanel->isVisible();
    const bool wasProfileCurrent = (homeContentStack && homeContentStack->currentWidget() == profilePanel);

    // Rebuild profile panel each time to avoid stale/overlapped widgets.
    if (QLayout *old = profilePanel->layout()) {
        while (old->count() > 0) {
            QLayoutItem *child = old->takeAt(0);
            if (child->widget()) {
                child->widget()->deleteLater();
            }
            delete child;
        }
        delete old;
    }

    const QString panelBg = QColor(themeSoft).lighter(107).name();
    const QString cardBg = QColor(themeSoft).lighter(112).name();
    const QString bannerBg = QColor(themePrimary).darker(120).name();
    const QString dividerColor = QColor(themeSelectedBg).darker(108).name();

    profilePanel->setStyleSheet(QString("background:%1;").arg(panelBg));
    QVBoxLayout *profileLayout = new QVBoxLayout(profilePanel);
    profileLayout->setContentsMargins(10, 10, 10, 10);
    profileLayout->setSpacing(12);

    QFrame *topBlank = new QFrame(profilePanel);
    topBlank->setObjectName(QStringLiteral("profileTopBlankSpacer"));
    const int navHeight = navBarCard ? navBarCard->height() : 0;
    const int navHint = navBarCard ? navBarCard->sizeHint().height() : 0;
    const int navMin = navBarCard ? navBarCard->minimumHeight() : 0;
    topBlank->setFixedHeight(qMax(120, qMax(navHeight, qMax(navHint, navMin))));
    topBlank->setStyleSheet(QString("background:%1;border:none;border-radius:16px;").arg(bannerBg));
    QVBoxLayout *topBlankLayout = new QVBoxLayout(topBlank);
    topBlankLayout->setContentsMargins(0, 0, 0, 0);
    topBlankLayout->setSpacing(0);
    profileBannerLabel = new QLabel(topBlank);
    profileBannerLabel->setAlignment(Qt::AlignCenter);
    profileBannerLabel->setStyleSheet(QString("background:%1;color:#f0f0f0;border:none;border-radius:16px;font-size:20px;").arg(bannerBg));
    profileBannerLabel->setCursor(Qt::PointingHandCursor);
    profileBannerLabel->setText(QStringLiteral("点击设置顶部图片"));
    profileBannerLabel->installEventFilter(this);
    topBlankLayout->addWidget(profileBannerLabel);

    const int avatarSize = 198;
    profileAvatarLabel = new QLabel(profilePanel);
    profileAvatarLabel->setFixedSize(avatarSize, avatarSize);
    profileAvatarLabel->setAlignment(Qt::AlignCenter);
    profileAvatarLabel->setStyleSheet("font-size:36px;font-weight:800;color:white;background:#3d7db8;border-radius:99px;");
    profileAvatarLabel->setCursor(Qt::PointingHandCursor);
    profileAvatarLabel->installEventFilter(this);
    profileNameLabel = new QLabel(profilePanel);
    profileNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    profileNameLabel->setStyleSheet("font-size:46px;font-weight:900;color:#232f2a;");
    profileHintLabel = new QLabel(profilePanel);
    profileHintLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    profileHintLabel->setWordWrap(true);
    profileHintLabel->setStyleSheet("font-size:30px;color:#58645f;");

    QFrame *headerCard = new QFrame(profilePanel);
    headerCard->setStyleSheet(QString("background:%1;border:none;border-radius:16px;").arg(cardBg));
    QHBoxLayout *headerCardLayout = new QHBoxLayout(headerCard);
    headerCardLayout->setContentsMargins(16, 14, 16, 14);
    headerCardLayout->setSpacing(14);

    QFrame *avatarCard = new QFrame(headerCard);
    avatarCard->setStyleSheet(QString("background:%1;border:none;border-radius:14px;").arg(cardBg));
    QVBoxLayout *avatarCardLayout = new QVBoxLayout(avatarCard);
    avatarCardLayout->setContentsMargins(8, 8, 8, 8);
    avatarCardLayout->addWidget(profileAvatarLabel, 0, Qt::AlignCenter);

    QFrame *textCard = new QFrame(headerCard);
    textCard->setStyleSheet(QString("background:%1;border:none;border-radius:14px;").arg(cardBg));
    QVBoxLayout *textCardLayout = new QVBoxLayout(textCard);
    textCardLayout->setContentsMargins(14, 10, 14, 10);
    textCardLayout->setSpacing(0);

    QFrame *nicknameCard = new QFrame(textCard);
    nicknameCard->setStyleSheet(QString("background:%1;border:none;border-radius:10px;").arg(cardBg));
    QHBoxLayout *nicknameLayout = new QHBoxLayout(nicknameCard);
    nicknameLayout->setContentsMargins(12, 8, 12, 8);
    nicknameLayout->addWidget(profileNameLabel);

    QFrame *usernameCard = new QFrame(textCard);
    usernameCard->setStyleSheet(QString("background:%1;border:none;border-radius:10px;").arg(cardBg));
    QHBoxLayout *usernameLayout = new QHBoxLayout(usernameCard);
    usernameLayout->setContentsMargins(12, 8, 12, 8);
    usernameLayout->addWidget(profileHintLabel);

    const int topGap = avatarSize * 6 / 100;
    const int middleGap = avatarSize * 38 / 100;
    textCardLayout->addSpacing(topGap);
    textCardLayout->addWidget(nicknameCard);
    textCardLayout->addSpacing(middleGap);
    textCardLayout->addWidget(usernameCard);

    headerCardLayout->addWidget(avatarCard, 0, Qt::AlignTop);
    headerCardLayout->addWidget(textCard, 1, Qt::AlignTop);

    QFrame *headerDivider = new QFrame(profilePanel);
    headerDivider->setFrameShape(QFrame::HLine);
    headerDivider->setStyleSheet(QString("color:%1;background:%1;min-height:1px;max-height:1px;").arg(dividerColor));
    const int actionCardHeight = 64;
    const int actionOptionHeight = qMax(44, static_cast<int>(actionCardHeight * 0.8));
    profileEditInfoButton = new QPushButton(QStringLiteral("修改个人信息"), profilePanel);
    profileEditInfoButton->setObjectName(QStringLiteral("profileEditInfoButton"));
    profileEditInfoButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#5f7f66;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileInfoButton = new QPushButton(QStringLiteral("软件信息"), profilePanel);
    profileInfoButton->setObjectName(QStringLiteral("profileInfoButton"));
    profileInfoButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#3d7b83;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileUpdateButton = new QPushButton(QStringLiteral("检查软件更新"), profilePanel);
    profileUpdateButton->setObjectName(QStringLiteral("profileUpdateButton"));
    profileUpdateButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#3d7db8;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileFeedbackButton = new QPushButton(QStringLiteral("反馈及建议"), profilePanel);
    profileFeedbackButton->setObjectName(QStringLiteral("profileFeedbackButton"));
    profileFeedbackButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#2f8f46;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileThemeSwitchButton = new QPushButton(QStringLiteral("切换应用主题"), profilePanel);
    profileThemeSwitchButton->setObjectName(QStringLiteral("profileThemeSwitchButton"));
    profileThemeSwitchButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#7a5ea9;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileServerConfigButton = new QPushButton(QStringLiteral("服务器设置"), profilePanel);
    profileServerConfigButton->setObjectName(QStringLiteral("profileServerConfigButton"));
    profileServerConfigButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#4f6e9a;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileResetDataButton = new QPushButton(QStringLiteral("格式化当前账户数据"), profilePanel);
    profileResetDataButton->setObjectName(QStringLiteral("profileResetDataButton"));
    profileResetDataButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#d38f2f;color:white;border-radius:16px;").arg(actionOptionHeight));
    profileLogoutButton = new QPushButton(QStringLiteral("退出登录"), profilePanel);
    profileLogoutButton->setObjectName(QStringLiteral("profileLogoutButton"));
    profileLogoutButton->setStyleSheet(QString("font-size:24px;min-height:%1px;background:#c95d5d;color:white;border-radius:16px;").arg(actionCardHeight));
    QFrame *logoutDivider = new QFrame(profilePanel);
    logoutDivider->setFrameShape(QFrame::HLine);
    logoutDivider->setStyleSheet(QString("color:%1;background:%1;min-height:1px;max-height:1px;").arg(dividerColor));

    profileLayout->addWidget(topBlank);
    profileLayout->addWidget(headerCard);
    profileLayout->addWidget(headerDivider);
    profileLayout->addWidget(profileEditInfoButton);
    profileLayout->addWidget(profileThemeSwitchButton);
    profileLayout->addWidget(profileServerConfigButton);
    profileLayout->addWidget(profileInfoButton);
    profileLayout->addWidget(profileUpdateButton);
    profileLayout->addWidget(profileFeedbackButton);
    profileLayout->addWidget(profileResetDataButton);
    profileLayout->addSpacing(24);
    profileLayout->addWidget(logoutDivider);
    profileLayout->addWidget(profileLogoutButton);
    profileLayout->addStretch();
    if (wasProfileVisible || wasProfileCurrent) {
        profilePanel->show();
        if (wasProfileCurrent && homeContentStack) {
            homeContentStack->setCurrentWidget(profilePanel);
        }
    } else {
        profilePanel->hide();
    }

    if (profileServerConfigButton) {
        profileServerConfigButton->setVisible(!loginLocalMode); //非在线模式不显示 “服务器设置”
    }

    QObject::connect(profileInfoButton, &QPushButton::clicked, this, [this]()
                     { QMessageBox::information(this, QStringLiteral("软件信息"), mouseplan::ui::profile::ProfileInteractionHelper::softwareInfoText()); });
    QObject::connect(profileThemeSwitchButton, &QPushButton::clicked, this, &MainWindow::openAppThemeSelectionFromProfile);
    QObject::connect(profileServerConfigButton, &QPushButton::clicked, this, &MainWindow::openServerConfigDialog);
    QObject::connect(profileUpdateButton, &QPushButton::clicked, this, &MainWindow::checkForUpdates);
    QObject::connect(profileFeedbackButton, &QPushButton::clicked, this, &MainWindow::submitFeedbackSuggestion);
    QObject::connect(profileEditInfoButton, &QPushButton::clicked, this, &MainWindow::editCurrentUserProfile);
    QObject::connect(profileResetDataButton, &QPushButton::clicked, this, &MainWindow::formatCurrentAccountData);
    QObject::connect(profileLogoutButton, &QPushButton::clicked, this, [this]() {
        User *u = currentUser();
        if (u) {
            lastLoginUsername = u->username;
            u->rememberLoginUntil = QDate();
            store.save();
        }
        currentUserId.clear();
        switchToLoginPage();
    });

    updateProfileBannerImage();
}

// 功能：切换到我的页面并刷新资料展示。
void MainWindow::showProfileTab()
{
    setupProfilePanelUi();
    applyResponsiveLayout();
    syncProfilePanelLayoutByNav();
    updateProfileHeader();
    if (homeContentStack && profilePanel) {
        profilePanel->show();
        homeContentStack->setCurrentWidget(profilePanel);
    }
    if (homeNavButton && themeNavButton && profileNavButton) {
        homeNavButton->setChecked(false);
        themeNavButton->setChecked(false);
        profileNavButton->setChecked(true);
    }
    if (navBarCard) {
        navBarCard->setVisible(true);
    }
    QTimer::singleShot(0, this, [this]() {
        applyResponsiveLayout();
        syncProfilePanelLayoutByNav();
        updateProfileHeader();
        updateProfileBannerImage();
    });
}

// 功能：切换到主题页面并保持导航区域可见。
void MainWindow::showThemeTab()
{
    if (homeContentStack && themePanel) {
        if (QWidget *topBlank = themePanel->findChild<QWidget *>(QStringLiteral("themeTopBlank"))) {
            topBlank->setFixedHeight(navBarCard ? navBarCard->height() : 120);
        }
        if (QWidget *bottomBlank = themePanel->findChild<QWidget *>(QStringLiteral("themeBottomBlank"))) {
            bottomBlank->setFixedHeight(navBarCard ? navBarCard->height() : 120);
        }
        homeContentStack->setCurrentWidget(themePanel);
    }
    if (navBarCard) {
        navBarCard->setVisible(true);
    }
}
