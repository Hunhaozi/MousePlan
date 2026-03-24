#ifndef MOUSEPLAN_RUNTIME_DIALOG_HELPERS_H
#define MOUSEPLAN_RUNTIME_DIALOG_HELPERS_H

#include <QString>
#include <QVector>
#include <QUrl>

class QColor;
class QPixmap;
class QSize;

class QDialog;
class QWidget;

struct WorkoutItem;
struct DayPlan;
struct MasterPlan;

struct ThemeColorPreset {
    QString key;
    QString name;
    QString primary;
    QString accent;
    QString soft;
    QString todayBg;
    QString selectedBg;
};

constexpr int kImagePickRequestCode = 1001;
constexpr int kEditAvatarPickRequestCode = 1002;
constexpr int kPlanImportRequestCode = 1003;
constexpr int kPlanExportRequestCode = 1004;
constexpr int kAndroidResultOk = -1;

void setupMobileDialog(QDialog &dialog, QWidget *parent);

bool runCompactInputDialog(QWidget *parent,
                           const QString &title,
                           const QString &label,
                           QString &value,
                           bool numberOnly,
                           int minValue,
                           int maxValue);

bool runCompactNumberInputDialog(QWidget *parent,
                                 const QString &title,
                                 const QString &label,
                                 int minValue,
                                 int maxValue,
                                 int &value);

bool runCompactTextInputDialog(QWidget *parent,
                               const QString &title,
                               const QString &label,
                               QString &value);

bool runLargePasswordDialog(QWidget *parent,
                            const QString &title,
                            const QString &label,
                            QString &value);

bool askChineseQuestionDialog(QWidget *parent,
                              const QString &title,
                              const QString &text,
                              const QString &confirmText = QStringLiteral("确认"),
                              const QString &cancelText = QStringLiteral("取消"));

QString safeFileSuffixFromMime(const QString &mimeType);

QVector<ThemeColorPreset> themeColorPresets();
ThemeColorPreset resolveThemeColorPreset(const QString &theme, const QString &presetKey);

QPixmap createCircularAvatarPixmap(const QPixmap &source, const QSize &targetSize);
QPixmap createSoftEdgeBannerPixmap(const QPixmap &source,
                                   const QSize &targetSize,
                                   const QColor &bg,
                                   int radius);

bool editWorkoutItemDialog(QWidget *parent, WorkoutItem &item);
bool editDayPlanDialog(QWidget *parent, DayPlan &dayPlan);
bool editMasterPlanDialog(QWidget *parent, MasterPlan &plan);

bool runThemeSelectionDialog(QWidget *parent,
                             QString &themeOut,
                             bool newUserEntry,
                             const QString &windowTitle,
                             const QString &tipText,
                             const QString &confirmText);

void showLoadingOverlayDialog(QWidget *parent,
                              int blankHeight,
                              const QString &blankColor,
                              const QString &centerColor,
                              const QString &loadingText,
                              bool withLogo,
                              bool moveUpStrong);

QString downloadFileWithProgress(QWidget *parent,
                                 const QUrl &url,
                                 const QString &suggestedFileName,
                                 QString *errorText = nullptr);

#endif