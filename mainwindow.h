#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "appdata.h"
#include "modules/common/ui/ui_tuning.h"

#include <QMainWindow>


class QBoxLayout;
class QCalendarWidget;
class QCheckBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPixmap;
class QPushButton;
class QScrollArea;
class QStackedLayout;
class QTextEdit;
class QToolButton;
class QVBoxLayout;
class QResizeEvent;
class QEvent;
class QObject;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct CalendarPlanInfo {
        bool hasPlan = false;
        bool isRestDay = false;
        int dayPlanIndex = -1;
        QString message;
    };

    enum class ViewMode {
        CurrentPlan,
        SubmittedRecord,
        MissingHistory
    };

    Ui::MainWindow *ui;

    AppDataStore store;
    QString currentUserId;
    QString lastLoginUsername;
    QDate selectedDate;

    QWidget *rootWidget = nullptr;
    QVBoxLayout *rootOuterLayout = nullptr;
    QStackedLayout *rootStackLayout = nullptr;

    QWidget *loginPage = nullptr;
    QVBoxLayout *loginMainLayout = nullptr;
    QHBoxLayout *loginTailLayout = nullptr;
    QFrame *loginCard = nullptr;
    QLabel *loginCaptionLabel = nullptr;
    QLabel *loginModeLabel = nullptr;
    QLabel *logoLabel = nullptr;
    QCheckBox *agreementCheckBox = nullptr;
    QLineEdit *usernameEdit = nullptr;
    QLineEdit *passwordEdit = nullptr;
    QPushButton *loginButton = nullptr;
    QPushButton *registerButton = nullptr;
    QPushButton *loginServerConfigButton = nullptr;
    QPushButton *forgotButton = nullptr;
    QPushButton *loginModeSwitchButton = nullptr;
    bool loginLocalMode = true;

    QWidget *homePage = nullptr;
    QVBoxLayout *homeMainLayout = nullptr;
    QStackedLayout *homeContentStack = nullptr;
    QWidget *homeContentPanel = nullptr;
    QFrame *calendarCard = nullptr;
    QFrame *todayPlanCard = nullptr;
    QFrame *navBarCard = nullptr;
    QFrame *profilePanel = nullptr;
    QWidget *themePanel = nullptr;
    QLabel *profileBannerLabel = nullptr;
    QLabel *profileAvatarLabel = nullptr;
    QLabel *profileNameLabel = nullptr;
    QLabel *profileHintLabel = nullptr;
    QPushButton *profileLogoutButton = nullptr;
    QPushButton *profileInfoButton = nullptr;
    QPushButton *profileEditInfoButton = nullptr;
    QPushButton *profileUpdateButton = nullptr;
    QPushButton *profileFeedbackButton = nullptr;
    QPushButton *profileThemeSwitchButton = nullptr;
    QPushButton *profileServerConfigButton = nullptr;
    QPushButton *profileResetDataButton = nullptr;
    QWidget *middleContainer = nullptr;
    QBoxLayout *middleSplitLayout = nullptr;
    QWidget *leftPlanContainer = nullptr;
    QWidget *rightPlanContainer = nullptr;
    QHBoxLayout *navLayout = nullptr;
    QCalendarWidget *calendar = nullptr;
    QLabel *dayTitleLabel = nullptr;
    QLabel *dayHintLabel = nullptr;
    QScrollArea *itemsScrollArea = nullptr;
    QWidget *itemsContainer = nullptr;
    QVBoxLayout *itemsLayout = nullptr;
    QPushButton *planSettingButton = nullptr;
    QPushButton *submitTodayButton = nullptr;
    QToolButton *homeNavButton = nullptr;
    QToolButton *themeNavButton = nullptr;
    QToolButton *profileNavButton = nullptr;

    QString themePrimary = "#1e4d3c";
    QString themeAccent = "#f28f3b";
    QString themeSoft = "#edf7ef";
    QString themeTodayBg = "#c7e5d2";
    QString themeSelectedBg = "#8fd3a4";

    void initializeData();
    void loadThemeConfig();
    void setupRootUi();
    void setupLoginPage();
    void setupHomePage();
    void setupProfilePanelUi();
    void applyThemeStyle();
    void applyResponsiveLayout();
    void syncProfilePanelLayoutByNav();
    void showHomeTab();
    void showProfileTab();
    void showThemeTab();
    bool promptAgreementDialog();
    void updateProfileHeader();
    void onProfileImageAreaClicked(bool isBannerArea);
    void pickProfileImageFromAlbum(bool isBannerArea);
    void showImagePreviewDialog(const QPixmap &pixmap, const QString &suggestedBaseName);
    void updateProfileBannerImage();
    void editCurrentUserProfile();
    bool tryAutoLoginOnStartup();
    void formatCurrentAccountData();
    void switchToLoginPage();
    void switchToHomePage();
    void refreshLoginModeUi();
    void persistCurrentUserPackage1();
    void loadBestPackage1ForCurrentUser();
    bool pullOnlinePackage1ForCurrentUser(bool silent);
    bool runLocalRegisterFlow();
    bool runOnlineRegisterFlow();

    User *currentUser();
    const User *currentUser() const;
    MasterPlan *activePlanForCurrentUser();
    const MasterPlan *activePlanForCurrentUser() const;
    TrainingRecord *recordForDate(const QDate &date);
    const TrainingRecord *recordForDate(const QDate &date) const;

    CalendarPlanInfo resolvePlanInfo(const QDate &date) const;
    RecordDay buildRecordDayFromPlan(const DayPlan &dayPlan) const;
    void ensureRecordFromSelectedDayPlan();
    void rebuildCalendarFormats();
    void rebuildDayView();
    void clearItemCards();

    void tryLogin();
    void runRegisterFlow();
    void maybeRunThemeSelection();
    void openThemeSelectFromNav();
    void openThemeColorFromNav();
    void openAppThemeSelectionFromProfile();
    void openServerConfigDialog();
    void checkForUpdates();
    void submitFeedbackSuggestion();
    void openPlanManagerDialog();
    void submitTodayRecord();
    void supplementTrainingRecord();
    void onCalendarDateChanged(const QDate &date);
    void openTodayItemPreview(int itemIndex);
    void markTodayItemCompleted(int itemIndex);

    QString setListSummary(const QVector<PlanSet> &sets) const;
    void ignoreTodayPlanItem(int itemIndex);
    void editTodayPlanItem(int itemIndex);
    void deleteTodayPlanItem(int itemIndex);

    void syncPlanToRemote(const MasterPlan &plan);
    void syncRecordToRemote(const TrainingRecord &record);
    int localSupplementCountForMonth(const QDate &date) const;

    void showAgreementDialog(bool isLocalMode);
};
#endif // MAINWINDOW_H
