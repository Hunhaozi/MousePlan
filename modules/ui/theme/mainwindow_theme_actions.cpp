#include "mainwindow.h"
#include "modules/common/agreement/agreement_text_loader.h"
#include "modules/common/config/network_config.h"
#include "modules/common/theme/theme_feature_gate.h"
#include "modules/themes/fitness/calendar/fitness_calendar_mark_builder.h"
#include "modules/themes/fitness/network/fitness_online_api.h"
#include "modules/ui/login/login_register_flow.h"
#include "modules/ui/profile/profile_interaction_helper.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QGroupBox>
#include <QGridLayout>
#include <QGraphicsBlurEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QIntValidator>
#include <QLayoutItem>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QPair>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScroller>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QStyle>
#include <QScreen>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScrollerProperties>
#include <QStringList>
#include <QTextEdit>
#include <QTextStream>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QToolButton>
#include <QTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QVBoxLayout>
#include <QUuid>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QWidget>
#include <QCryptographicHash>
#include "modules/common/ui/runtime_dialog_helpers.h"

#ifdef Q_OS_ANDROID
#include <QAndroidActivityResultReceiver>
#include <QtAndroid>
#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#include <QDebug>
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace {

using mouseplan::fitness::CalendarCellMark;

const QString kDefaultAvatarResourcePath = QStringLiteral(":/img/dehead");
const QString kDefaultBannerResourcePath = QStringLiteral(":/img/img_up");

// 功能：处理相关逻辑。
int findRecordIndex(const QVector<TrainingRecord> &records, const QString &userId, const QDate &date)
{
    for (int i = 0; i < records.size(); ++i) {
        if (records[i].ownerUserId == userId && records[i].date == date) {
            return i;
        }
    }
    return -1;
}

// 功能：处理相关逻辑。
int findPlanIndex(const QVector<MasterPlan> &plans, const QString &planId)
{
    for (int i = 0; i < plans.size(); ++i) {
        if (plans[i].id == planId) {
            return i;
        }
    }
    return -1;
}

// 功能：处理相关逻辑。
int countSetsTextToInt(const QString &value)
{
    bool ok = false;
    const int result = value.trimmed().toInt(&ok);
    return ok ? result : 0;
}

// 功能：处理相关逻辑。
double countSetsTextToWeightOneDecimal(const QString &value)
{
    bool ok = false;
    const double parsed = value.trimmed().toDouble(&ok);
    if (!ok) {
        return 0.0;
    }
    return qRound(parsed * 10.0) / 10.0;
}

// 功能：处理相关逻辑。
QString formatWeightOneDecimal(double value)
{
    return QString::number(qRound(value * 10.0) / 10.0, 'f', 1);
}

// 功能：处理相关逻辑。
QString normalizeSetRemark(const QString &value)
{
    return value.trimmed().left(16);
}

// 功能：处理相关逻辑。
QPixmap cropCenterByAspectRatio(const QPixmap &source, const QSize &targetSize)
{
    if (source.isNull() || !targetSize.isValid() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        return source;
    }

    const qreal sourceRatio = static_cast<qreal>(source.width()) / static_cast<qreal>(source.height());
    const qreal targetRatio = static_cast<qreal>(targetSize.width()) / static_cast<qreal>(targetSize.height());
    QRect cropRect(0, 0, source.width(), source.height());
    if (sourceRatio > targetRatio) {
        const int cropWidth = qMax(1, static_cast<int>(source.height() * targetRatio));
        cropRect.setWidth(cropWidth);
        cropRect.moveLeft((source.width() - cropWidth) / 2);
    } else {
        const int cropHeight = qMax(1, static_cast<int>(source.width() / targetRatio));
        cropRect.setHeight(cropHeight);
        cropRect.moveTop((source.height() - cropHeight) / 2);
    }
    return source.copy(cropRect);
}

[[maybe_unused]] MasterPlan createDefaultPlanForUser(const QString &ownerUserId)
{
    auto parsePresetPlan = [&](const QString &raw, MasterPlan &outPlan) -> bool {
        QStringList lines = raw.split('\n');
        for (QString &line : lines) {
            line = line.trimmed();
        }

        outPlan = MasterPlan();
        outPlan.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        outPlan.ownerUserId = ownerUserId;
        outPlan.name = QStringLiteral("默认计划（未启用）");
        outPlan.trainDays = 3;
        outPlan.restDays = 1;
        outPlan.startDate = QDate::currentDate();

        int defaultMinutes = 100;
        DayPlan *currentDay = nullptr;
        WorkoutItem *currentItem = nullptr;
        enum class SetMode { None, Warmup, Work };
        SetMode mode = SetMode::None;

        const QRegularExpression nameRx(QStringLiteral("^总计划名称[:：]\\s*(.+)$"));
        const QRegularExpression cycleRx(QStringLiteral("练\\s*(\\d+)\\s*天.*休息\\s*(\\d+)\\s*天"));
        const QRegularExpression minutesRx(QStringLiteral("每日预计训练时间[:：]\\s*(\\d+)"));
        const QRegularExpression dayRx(QStringLiteral("^第.+天[:：]\\s*(.+)$"));
        const QRegularExpression itemRx(QStringLiteral("^项目\\s*\\d+[:：]?\\s*(.+)$"));
        const QRegularExpression restRx(QStringLiteral("^间歇(?:时间)?[:：]?\\s*(\\d+)"));
        const QRegularExpression warmupRx(QStringLiteral("^热身组[:：]?\\s*(\\d*)"));
        const QRegularExpression workRx(QStringLiteral("^正式组[:：]?\\s*(\\d*)"));
        const QRegularExpression setRx(QStringLiteral("^组\\s*\\d+\\s+(\\d+)\\s+([0-9]+(?:\\.[0-9]+)?)(?:kg)?\\s*(.*)$"));

        for (const QString &line : lines) {
            if (line.isEmpty()) {
                continue;
            }

            QRegularExpressionMatch m = nameRx.match(line);
            if (m.hasMatch()) {
                outPlan.name = m.captured(1).trimmed();
                continue;
            }

            m = cycleRx.match(line);
            if (m.hasMatch()) {
                outPlan.trainDays = qMax(1, m.captured(1).toInt());
                outPlan.restDays = qMax(1, m.captured(2).toInt());
                continue;
            }

            m = minutesRx.match(line);
            if (m.hasMatch()) {
                defaultMinutes = qMax(1, m.captured(1).toInt());
                continue;
            }

            m = dayRx.match(line);
            if (m.hasMatch()) {
                DayPlan d;
                d.title = m.captured(1).trimmed();
                d.defaultMinutes = defaultMinutes;
                outPlan.dayPlans.push_back(d);
                currentDay = &outPlan.dayPlans.back();
                currentItem = nullptr;
                mode = SetMode::None;
                continue;
            }

            m = itemRx.match(line);
            if (m.hasMatch() && currentDay) {
                WorkoutItem item;
                item.name = m.captured(1).trimmed();
                item.restSeconds = 90;
                currentDay->items.push_back(item);
                currentItem = &currentDay->items.back();
                mode = SetMode::None;
                continue;
            }

            m = restRx.match(line);
            if (m.hasMatch() && currentItem) {
                currentItem->restSeconds = qMax(0, m.captured(1).toInt());
                continue;
            }

            if (warmupRx.match(line).hasMatch()) {
                mode = SetMode::Warmup;
                continue;
            }
            if (workRx.match(line).hasMatch()) {
                mode = SetMode::Work;
                continue;
            }

            m = setRx.match(line);
            if (m.hasMatch() && currentItem) {
                const int reps = qMax(0, m.captured(1).toInt());
                const double weight = countSetsTextToWeightOneDecimal(m.captured(2));
                QString remark = m.captured(3).trimmed();
                if (remark == QStringLiteral("无")) {
                    remark.clear();
                }
                PlanSet set(weight, reps, remark);
                if (mode == SetMode::Work) {
                    currentItem->workSets.push_back(set);
                } else {
                    currentItem->warmupSets.push_back(set);
                }
                continue;
            }
        }

        return !outPlan.dayPlans.isEmpty();
    };

    MasterPlan p;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("Mouse.txt")),
        QDir(appDir).filePath(QStringLiteral("../Mouse.txt")),
        QStringLiteral("Mouse.txt")
    };
    for (const QString &path : candidates) {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString raw = QString::fromUtf8(f.readAll());
        f.close();
        if (parsePresetPlan(raw, p)) {
            p.startDate = QDate::currentDate();
            return p;
        }
    }

    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.ownerUserId = ownerUserId;
    p.name = QStringLiteral("默认计划（未启用）");
    p.trainDays = 5;
    p.restDays = 1;
    p.startDate = QDate::currentDate();

    for (int i = 0; i < 5; ++i) {
        DayPlan day;
        day.title = QString("第%1天计划").arg(i + 1);
        day.defaultMinutes = 100;
        p.dayPlans.push_back(day);
    }
    return p;
}

#ifdef Q_OS_ANDROID
bool ensureAndroidStoragePermission(QWidget *parent, bool needWrite)
{
    QStringList requiredPermissions;
    requiredPermissions << QStringLiteral("android.permission.READ_MEDIA_IMAGES")
                        << QStringLiteral("android.permission.READ_EXTERNAL_STORAGE");
    if (needWrite) {
        requiredPermissions << QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE");
    }

    const auto permissions = QtAndroid::requestPermissionsSync(requiredPermissions);
    const bool readMediaGranted = permissions.value(QStringLiteral("android.permission.READ_MEDIA_IMAGES")) == QtAndroid::PermissionResult::Granted;
    const bool readStorageGranted = permissions.value(QStringLiteral("android.permission.READ_EXTERNAL_STORAGE")) == QtAndroid::PermissionResult::Granted;
    const bool writeStorageGranted = permissions.value(QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE")) == QtAndroid::PermissionResult::Granted;

    if (!readMediaGranted && !readStorageGranted) {
        QMessageBox::warning(parent, QStringLiteral("权限提示"), QStringLiteral("请先授予存储读取权限。"));
        return false;
    }
    if (needWrite && !writeStorageGranted && !readStorageGranted) {
        QMessageBox::warning(parent, QStringLiteral("权限提示"), QStringLiteral("请先授予存储写入权限。"));
        return false;
    }
    return true;
}

QString resolveRealPathByMediaStore(const QAndroidJniObject &uri)
{
    if (!uri.isValid()) {
        return QString();
    }

    QAndroidJniObject mediaDataColumn = QAndroidJniObject::getStaticObjectField(
        "android/provider/MediaStore$MediaColumns",
        "DATA",
        "Ljava/lang/String;");
    if (!mediaDataColumn.isValid()) {
        return QString();
    }

    QAndroidJniEnvironment env;
    jobjectArray projection = static_cast<jobjectArray>(
        env->NewObjectArray(1, env->FindClass("java/lang/String"), nullptr));
    if (!projection) {
        return QString();
    }
    env->SetObjectArrayElement(projection, 0, mediaDataColumn.object<jstring>());

    QAndroidJniObject contentResolver = QtAndroid::androidActivity().callObjectMethod(
        "getContentResolver",
        "()Landroid/content/ContentResolver;");
    if (!contentResolver.isValid()) {
        return QString();
    }

    QAndroidJniObject cursor = contentResolver.callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
        uri.object<jobject>(),
        projection,
        nullptr,
        nullptr,
        nullptr);
    if (!cursor.isValid()) {
        return QString();
    }

    const jint columnIndex = cursor.callMethod<jint>(
        "getColumnIndex",
        "(Ljava/lang/String;)I",
        mediaDataColumn.object<jstring>());
    if (columnIndex < 0) {
        return QString();
    }

    const jboolean moved = cursor.callMethod<jboolean>("moveToFirst", "()Z");
    if (!moved) {
        return QString();
    }

    QAndroidJniObject result = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", columnIndex);
    return result.toString();
}

QString copyContentUriToAppFile(const QAndroidJniObject &uri)
{
    if (!uri.isValid()) {
        return QString();
    }

    QAndroidJniObject activity = QtAndroid::androidActivity();
    if (!activity.isValid()) {
        return QString();
    }
    QAndroidJniObject resolver = activity.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) {
        return QString();
    }

    QAndroidJniObject inputStream = resolver.callObjectMethod(
        "openInputStream",
        "(Landroid/net/Uri;)Ljava/io/InputStream;",
        uri.object<jobject>());
    if (!inputStream.isValid()) {
        return QString();
    }

    QAndroidJniObject mimeType = resolver.callObjectMethod(
        "getType",
        "(Landroid/net/Uri;)Ljava/lang/String;",
        uri.object<jobject>());
    const QString ext = safeFileSuffixFromMime(mimeType.toString());

    QString picturesRoot = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesRoot.trimmed().isEmpty()) {
        picturesRoot = QDir::homePath();
    }
    QDir outDir(picturesRoot + QStringLiteral("/MousePlan"));
    if (!outDir.exists()) {
        outDir.mkpath(QStringLiteral("."));
    }
    const QString outPath = outDir.filePath(
        QStringLiteral("picked_%1.%2").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")).arg(ext));

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        return QString();
    }

    const jint bufferSize = 8192;
    QAndroidJniEnvironment env;
    jbyteArray byteArray = env->NewByteArray(bufferSize);
    if (!byteArray) {
        outFile.close();
        return QString();
    }

    while (true) {
        const jint count = inputStream.callMethod<jint>("read", "([B)I", byteArray);
        if (count <= 0) {
            break;
        }

        jbyte *raw = env->GetByteArrayElements(byteArray, nullptr);
        if (!raw) {
            break;
        }
        outFile.write(reinterpret_cast<const char *>(raw), count);
        env->ReleaseByteArrayElements(byteArray, raw, JNI_ABORT);
    }

    inputStream.callMethod<void>("close", "()V");
    outFile.close();
    return outPath;
}

// 功能：处理相关逻辑。
QString copyDocumentUriToTempFile(const QAndroidJniObject &uri,
                                  const QString &prefix,
                                  const QString &fallbackExt)
{
    if (!uri.isValid()) {
        return QString();
    }

    QAndroidJniObject activity = QtAndroid::androidActivity();
    if (!activity.isValid()) {
        return QString();
    }
    QAndroidJniObject resolver = activity.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) {
        return QString();
    }

    QAndroidJniObject inputStream = resolver.callObjectMethod(
        "openInputStream",
        "(Landroid/net/Uri;)Ljava/io/InputStream;",
        uri.object<jobject>());
    if (!inputStream.isValid()) {
        return QString();
    }

    QAndroidJniObject mimeType = resolver.callObjectMethod(
        "getType",
        "(Landroid/net/Uri;)Ljava/lang/String;",
        uri.object<jobject>());
    const QString mimeExt = safeFileSuffixFromMime(mimeType.toString());
    const QString ext = (mimeExt == QStringLiteral("bin") && !fallbackExt.trimmed().isEmpty()) ? fallbackExt : mimeExt;

    QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.trimmed().isEmpty()) {
        tempRoot = QDir::homePath();
    }
    QDir outDir(QDir(tempRoot).filePath(QStringLiteral("MousePlan")));
    if (!outDir.exists()) {
        outDir.mkpath(QStringLiteral("."));
    }
    const QString outPath = outDir.filePath(
        QStringLiteral("%1_%2.%3")
            .arg(prefix)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"))
            .arg(ext));

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        inputStream.callMethod<void>("close", "()V");
        return QString();
    }

    const jint bufferSize = 8192;
    QAndroidJniEnvironment env;
    jbyteArray byteArray = env->NewByteArray(bufferSize);
    if (!byteArray) {
        outFile.close();
        inputStream.callMethod<void>("close", "()V");
        return QString();
    }

    while (true) {
        const jint count = inputStream.callMethod<jint>("read", "([B)I", byteArray);
        if (count <= 0) {
            break;
        }

        jbyte *raw = env->GetByteArrayElements(byteArray, nullptr);
        if (!raw) {
            break;
        }
        outFile.write(reinterpret_cast<const char *>(raw), count);
        env->ReleaseByteArrayElements(byteArray, raw, JNI_ABORT);
    }

    inputStream.callMethod<void>("close", "()V");
    outFile.close();
    return outPath;
}

// 功能：处理相关逻辑。
QString resolveResultUriString(const QAndroidJniObject &data)
{
    if (!data.isValid()) {
        return QString();
    }

    QAndroidJniObject uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
    if (uri.isValid()) {
        QAndroidJniObject uriText = uri.callObjectMethod("toString", "()Ljava/lang/String;");
        const QString value = uriText.toString();
        if (!value.trimmed().isEmpty()) {
            return value;
        }
    }

    QAndroidJniObject dataStr = data.callObjectMethod("getDataString", "()Ljava/lang/String;");
    return dataStr.toString();
}

QAndroidJniObject parseUriString(const QString &uriText)
{
    if (uriText.trimmed().isEmpty()) {
        return QAndroidJniObject();
    }
    QAndroidJniObject text = QAndroidJniObject::fromString(uriText);
    return QAndroidJniObject::callStaticObjectMethod("android/net/Uri",
                                                     "parse",
                                                     "(Ljava/lang/String;)Landroid/net/Uri;",
                                                     text.object<jstring>());
}

// 功能：处理相关逻辑。
bool writeBytesToContentUri(const QString &uriText, const QByteArray &payload)
{
    if (uriText.trimmed().isEmpty() || payload.isEmpty()) {
        return false;
    }

    QAndroidJniObject uri = parseUriString(uriText);
    if (!uri.isValid()) {
        return false;
    }

    QAndroidJniObject activity = QtAndroid::androidActivity();
    if (!activity.isValid()) {
        return false;
    }
    QAndroidJniObject resolver = activity.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) {
        return false;
    }

    QAndroidJniObject outputStream = resolver.callObjectMethod(
        "openOutputStream",
        "(Landroid/net/Uri;)Ljava/io/OutputStream;",
        uri.object<jobject>());
    if (!outputStream.isValid()) {
        return false;
    }

    QAndroidJniEnvironment env;
    jbyteArray arr = env->NewByteArray(payload.size());
    if (!arr) {
        outputStream.callMethod<void>("close", "()V");
        return false;
    }
    env->SetByteArrayRegion(arr, 0, payload.size(), reinterpret_cast<const jbyte *>(payload.constData()));
    outputStream.callMethod<void>("write", "([B)V", arr);
    outputStream.callMethod<void>("flush", "()V");
    outputStream.callMethod<void>("close", "()V");

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return false;
    }
    return true;
}

// 功能：处理相关逻辑。
QString resolvePickedImagePath(const QAndroidJniObject &data)
{
    if (!data.isValid()) {
        return QString();
    }

    QAndroidJniObject uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
    if (!uri.isValid()) {
        return QString();
    }

    const QString mediaStorePath = resolveRealPathByMediaStore(uri);
    if (!mediaStorePath.trimmed().isEmpty()) {
        return mediaStorePath;
    }

    QAndroidJniObject dataStr = data.callObjectMethod("getDataString", "()Ljava/lang/String;");
    const QString uriText = dataStr.toString();
    if (uriText.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        const QString localPath = QUrl(uriText).toLocalFile();
        if (!localPath.trimmed().isEmpty()) {
            return localPath;
        }
    }

    return copyContentUriToAppFile(uri);
}

class ResultReceiver : public QObject, public QAndroidActivityResultReceiver {
public:
    ResultReceiver(int id,
                   const std::function<void(const QString &)> &onResult,
                   QObject *parent = nullptr)
        : QObject(parent)
        , requestId(id)
        , callback(onResult)
    {
    }

// 功能：处理相关逻辑。
    void handleActivityResult(int receiverRequestCode,
                              int resultCode,
                              const QAndroidJniObject &data) override
    {
        if (receiverRequestCode != requestId) {
            return;
        }

        QString path;
        if (resultCode == kAndroidResultOk) {
            path = resolvePickedImagePath(data);
        }

        auto cb = callback;
        QMetaObject::invokeMethod(qApp,
                                  [cb, path]() {
                                      if (cb) {
                                          cb(path);
                                      }
                                  },
                                  Qt::QueuedConnection);
        QMetaObject::invokeMethod(this, "deleteLater", Qt::QueuedConnection);
    }

private:
    int requestId = 0;
    std::function<void(const QString &)> callback;
};

class UriResultReceiver : public QObject, public QAndroidActivityResultReceiver {
public:
    UriResultReceiver(int id,
                      const std::function<void(const QString &)> &onResult,
                      QObject *parent = nullptr)
        : QObject(parent)
        , requestId(id)
        , callback(onResult)
    {
    }

// 功能：处理相关逻辑。
    void handleActivityResult(int receiverRequestCode,
                              int resultCode,
                              const QAndroidJniObject &data) override
    {
        if (receiverRequestCode != requestId) {
            return;
        }

        QString uriText;
        if (resultCode == kAndroidResultOk) {
            uriText = resolveResultUriString(data);
        }

        auto cb = callback;
        QMetaObject::invokeMethod(qApp,
                                  [cb, uriText]() {
                                      if (cb) {
                                          cb(uriText);
                                      }
                                  },
                                  Qt::QueuedConnection);
        QMetaObject::invokeMethod(this, "deleteLater", Qt::QueuedConnection);
    }

private:
    int requestId = 0;
    std::function<void(const QString &)> callback;
};

// 功能：处理相关逻辑。
bool launchAndroidImagePicker(QWidget *parent,
                              int requestCode,
                              const std::function<void(const QString &)> &onPicked)
{
    if (!ensureAndroidStoragePermission(parent, false)) {
        QMessageBox::warning(parent, QStringLiteral("权限提示"), QStringLiteral("请授予存储读取权限后再从相册选择图片。"));
        return false;
    }

    // Use ACTION_PICK with MediaStore to open system gallery directly instead of file manager.
    QAndroidJniObject action = QAndroidJniObject::fromString(QStringLiteral("android.intent.action.PICK"));
    QAndroidJniObject mediaUri = QAndroidJniObject::getStaticObjectField(
        "android/provider/MediaStore$Images$Media",
        "EXTERNAL_CONTENT_URI",
        "Landroid/net/Uri;");
    QAndroidJniObject intent("android/content/Intent",
                             "(Ljava/lang/String;Landroid/net/Uri;)V",
                             action.object<jstring>(),
                             mediaUri.object<jobject>());
    QAndroidJniObject image = QAndroidJniObject::fromString(QStringLiteral("image/*"));
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;", image.object<jstring>());

    ResultReceiver *receiver = new ResultReceiver(requestCode, onPicked, qApp);
    QtAndroid::startActivity(intent, requestCode, receiver);

    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        qDebug() << "exception occurred while opening image picker";
        env->ExceptionDescribe();
        env->ExceptionClear();
        return false;
    }
    return true;
}

// 功能：处理相关逻辑。
bool launchAndroidDocumentPicker(QWidget *parent,
                                 int requestCode,
                                 const std::function<void(const QString &)> &onPicked,
                                 const QString &mimeType = QStringLiteral("*/*"))
{
    if (!ensureAndroidStoragePermission(parent, false)) {
        return false;
    }

    QAndroidJniObject action = QAndroidJniObject::fromString(QStringLiteral("android.intent.action.OPEN_DOCUMENT"));
    QAndroidJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object<jstring>());
    QAndroidJniObject type = QAndroidJniObject::fromString(mimeType);
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;", type.object<jstring>());
    QAndroidJniObject openable = QAndroidJniObject::fromString(QStringLiteral("android.intent.category.OPENABLE"));
    intent.callObjectMethod("addCategory", "(Ljava/lang/String;)Landroid/content/Intent;", openable.object<jstring>());

    UriResultReceiver *receiver = new UriResultReceiver(requestCode, onPicked, qApp);
    QtAndroid::startActivity(intent, requestCode, receiver);

    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        qDebug() << "exception occurred while opening document picker";
        env->ExceptionDescribe();
        env->ExceptionClear();
        return false;
    }
    return true;
}

// 功能：处理相关逻辑。
bool launchAndroidDocumentCreator(QWidget *parent,
                                  int requestCode,
                                  const QString &suggestedName,
                                  const std::function<void(const QString &)> &onCreated,
                                  const QString &mimeType = QStringLiteral("application/json"))
{
    if (!ensureAndroidStoragePermission(parent, true)) {
        return false;
    }

    QAndroidJniObject action = QAndroidJniObject::fromString(QStringLiteral("android.intent.action.CREATE_DOCUMENT"));
    QAndroidJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object<jstring>());
    QAndroidJniObject type = QAndroidJniObject::fromString(mimeType);
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;", type.object<jstring>());
    QAndroidJniObject openable = QAndroidJniObject::fromString(QStringLiteral("android.intent.category.OPENABLE"));
    intent.callObjectMethod("addCategory", "(Ljava/lang/String;)Landroid/content/Intent;", openable.object<jstring>());

    QAndroidJniObject extraTitle = QAndroidJniObject::getStaticObjectField(
        "android/content/Intent",
        "EXTRA_TITLE",
        "Ljava/lang/String;");
    QAndroidJniObject title = QAndroidJniObject::fromString(suggestedName);
    intent.callObjectMethod("putExtra",
                            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                            extraTitle.object<jstring>(),
                            title.object<jstring>());

    UriResultReceiver *receiver = new UriResultReceiver(requestCode, onCreated, qApp);
    QtAndroid::startActivity(intent, requestCode, receiver);

    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        qDebug() << "exception occurred while opening create-document";
        env->ExceptionDescribe();
        env->ExceptionClear();
        return false;
    }
    return true;
}
#endif

class PlanCalendarWidget : public QCalendarWidget {
public:
    explicit PlanCalendarWidget(QWidget *parent = nullptr)
        : QCalendarWidget(parent)
    {}

// 功能：处理相关逻辑。
    void setMarkProvider(const std::function<CalendarCellMark(const QDate &)> &provider)
    {
        markProvider = provider;
        updateCells();
    }

// 功能：处理相关逻辑。
    void setMarkerColors(const QColor &trainColor, const QColor &restColor)
    {
        trainMarkerColor = trainColor;
        restMarkerColor = restColor;
        updateCells();
    }

protected:
// 功能：处理相关逻辑。
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
            painter->setPen(mark.marker == QStringLiteral("休") ? restMarkerColor : trainMarkerColor);
            QRect markerRect = rect.adjusted(0, rect.height() / 2, 0, -2);
            painter->drawText(markerRect, Qt::AlignHCenter | Qt::AlignBottom, mark.marker);
        }

        if (mark.submitted) {
            QFont checkFont = painter->font();
            checkFont.setPointSize(qMax(10, checkFont.pointSize()));
            checkFont.setBold(true);
            painter->setFont(checkFont);
            painter->setPen(QColor("#d36b2c"));
            painter->drawText(rect.adjusted(0, 1, -4, 0), Qt::AlignRight | Qt::AlignTop, QStringLiteral("?"));
        }

        painter->restore();
    }

private:
    std::function<CalendarCellMark(const QDate &)> markProvider;
    QColor trainMarkerColor = QColor("#2b6a4e");
    QColor restMarkerColor = QColor("#8fbca0");
};

// 功能：处理相关逻辑。
QString findConfigFilePath(const QString &fileName)
{
    return mouseplan::common::AgreementTextLoader::findConfigFilePath(fileName);
}

// 功能：处理相关逻辑。
QString parseAgreementTextFromRaw(const QByteArray &raw)
{
    return mouseplan::common::AgreementTextLoader::parseAgreementTextFromRaw(raw);
}

// 功能：处理相关逻辑。
QString loadAgreementTextByMode(bool isLocalMode)
{
    return mouseplan::common::AgreementTextLoader::loadAgreementTextByMode(isLocalMode);
}

// 功能：处理相关逻辑。
void enableMobileSingleFingerScroll(QAbstractScrollArea *area)
{
    if (!area || !area->viewport()) {
        return;
    }
    QWidget *viewport = area->viewport();
    QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture);

    // Lower scroll sensitivity: slower flick speed and stronger deceleration.
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

// 功能：处理相关逻辑。
void applyDialogVerticalRatioMargins(QLayout *layout, const QDialog &dialog)
{
    if (!layout) {
        return;
    }
    const int t = qMax(1, gUiTuning.dialogTopRatio);
    const int c = qMax(1, gUiTuning.dialogContentRatio);
    const int b = qMax(1, gUiTuning.dialogBottomRatio);
    const int total = t + c + b;
    const int h = qMax(600, dialog.height());

    const int top = h * t / total;
    const int bottom = h * b / total;
    const int side = qMax(22, dialog.width() / 18);
    layout->setContentsMargins(side, top, side, bottom);
}

QComboBox *createWheelCombo(QWidget *parent, int from, int to, int step, const QString &suffix = QString())
{
    QComboBox *combo = new QComboBox(parent);
    for (int v = from; v <= to; v += step) {
        combo->addItem(suffix.isEmpty() ? QString::number(v) : QString("%1%2").arg(v).arg(suffix), v);
    }
    combo->setEditable(false);
    combo->setMaxVisibleItems(10);
    combo->setStyleSheet("font-size:48px;min-height:96px;padding:10px 12px;");
    if (combo->view()) {
        QScroller::grabGesture(combo->view()->viewport(), QScroller::LeftMouseButtonGesture);
    }
    return combo;
}

// 功能：处理相关逻辑。
int comboValue(const QComboBox *combo, int fallback)
{
    if (!combo) {
        return fallback;
    }
    const QVariant data = combo->currentData();
    if (data.isValid()) {
        return data.toInt();
    }
    bool ok = false;
    const int value = combo->currentText().toInt(&ok);
    return ok ? value : fallback;
}

// 功能：处理相关逻辑。
void addPreviewCardItem(QListWidget *list,
                        const QStringList &lines,
                        const QString &cardBg,
                        int minHeight,
                        int titleFont,
                        int bodyFont)
{
    if (!list || lines.isEmpty()) {
        return;
    }

    const int computedHeight = qMax(minHeight, 46 + static_cast<int>(lines.size()) * (bodyFont + 34));
    QListWidgetItem *item = new QListWidgetItem(list);
    item->setSizeHint(QSize(0, computedHeight));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    list->addItem(item);

    QWidget *cardHost = new QWidget(list);
    QVBoxLayout *cardLayout = new QVBoxLayout(cardHost);
    cardLayout->setContentsMargins(20, 14, 20, 14);
    cardLayout->setSpacing(10);
    auto applyCardStyle = [cardHost, cardBg](bool selected) {
        cardHost->setStyleSheet(QString("background:%1;border:%2px solid %3;border-radius:16px;")
                                    .arg(cardBg)
                                    .arg(selected ? 3 : 1)
                                    .arg(selected ? QStringLiteral("#3d7db8") : QStringLiteral("#dbe8df")));
    };
    applyCardStyle(false);

    for (int i = 0; i < lines.size(); ++i) {
        QFrame *lineBox = new QFrame(cardHost);
        if (i == 0) {
            lineBox->setStyleSheet("background:#ffe9cc;border:1px solid #f1d4a9;border-radius:12px;");
        } else {
            lineBox->setStyleSheet("background:#ffffff;border:1px solid #e6efe9;border-radius:12px;");
        }
        QHBoxLayout *lineLayout = new QHBoxLayout(lineBox);
        lineLayout->setContentsMargins(16, 10, 16, 10);
        QLabel *lineLabel = new QLabel(lines[i], lineBox);
        lineLabel->setWordWrap(true);
        if (i == 0) {
            lineLabel->setStyleSheet(QString("font-size:%1px;font-weight:800;color:#1f392d;").arg(titleFont));
        } else {
            lineLabel->setStyleSheet(QString("font-size:%1px;color:#5f6f66;").arg(bodyFont));
        }
        lineLayout->addWidget(lineLabel);
        cardLayout->addWidget(lineBox);
    }

    list->setItemWidget(item, cardHost);
    QObject::connect(list, &QListWidget::currentItemChanged, cardHost, [=](QListWidgetItem *current, QListWidgetItem *) {
        applyCardStyle(current == item);
    });
    if (list->currentItem() == item) {
        applyCardStyle(true);
    }
}



// 功能：处理相关逻辑。
void showThemeReloadPlaceholderDialog(QWidget *parent, const QString &themeName)
{
    QDialog dialog(parent);
    setupMobileDialog(dialog, parent);
    dialog.setWindowTitle(QStringLiteral("应用重载"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);

    QLabel *logo = new QLabel(QStringLiteral("MousePlan"), &dialog);
    logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet("font-size:64px;font-weight:900;color:#d97706;");

    QLabel *title = new QLabel(QStringLiteral("正在切换应用主题"), &dialog);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:34px;font-weight:800;color:#2b6a4e;");

    QLabel *tip = new QLabel(QStringLiteral("即将应用：%1\n正在准备相关主题资源...").arg(themeName), &dialog);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("font-size:26px;color:#4e5f56;");

    layout->addStretch();
    layout->addWidget(logo);
    layout->addWidget(title);
    layout->addWidget(tip);
    layout->addStretch();

    QTimer::singleShot(900, &dialog, &QDialog::accept);
    dialog.exec();
}

// 功能：处理相关逻辑。
QString generateRandomNicknameDigits()
{
    const int value = QRandomGenerator::global()->bounded(100000, 100000000);
    return QString::number(value);
}

// 功能：处理相关逻辑。
QString sha256Hex(const QString &value)
{
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}

// 功能：处理相关逻辑。
QString buildSecretHash(const QString &value)
{
    return sha256Hex(value.trimmed());
}

// 功能：处理相关逻辑。
bool verifyPasswordHashLocal(const User &user, const QString &inputPassword)
{
    return user.password == buildSecretHash(inputPassword);
}

struct OnlineApiConfig {
    QString baseUrl;
    int timeoutMs = 8000;
};

OnlineApiConfig loadOnlineApiConfig();

// 功能：处理相关逻辑。
QString normalizedVersionToken(const QString &raw)
{
    QString token = raw.trimmed();
    token.remove(QRegularExpression(QStringLiteral("^[vV]")));
    return token;
}

// 功能：处理相关逻辑。
int compareVersionText(const QString &left, const QString &right)
{
    const QStringList leftParts = normalizedVersionToken(left).split('.', QString::SkipEmptyParts);
    const QStringList rightParts = normalizedVersionToken(right).split('.', QString::SkipEmptyParts);
    const int count = qMax(leftParts.size(), rightParts.size());
    for (int i = 0; i < count; ++i) {
        bool leftOk = false;
        bool rightOk = false;
        const int lv = (i < leftParts.size()) ? leftParts[i].toInt(&leftOk) : 0;
        const int rv = (i < rightParts.size()) ? rightParts[i].toInt(&rightOk) : 0;
        const int safeLv = leftOk ? lv : 0;
        const int safeRv = rightOk ? rv : 0;
        if (safeLv != safeRv) {
            return safeLv > safeRv ? 1 : -1;
        }
    }
    return 0;
}

// 功能：处理相关逻辑。
QUrl buildOnlineUrl(const QString &baseUrlText, const QString &path)
{
    QString urlText = baseUrlText.trimmed();
    if (urlText.endsWith('/')) {
        urlText.chop(1);
    }
    QString suffix = path.trimmed();
    if (!suffix.startsWith('/')) {
        suffix.prepend('/');
    }
    return QUrl(urlText + suffix);
}

// 功能：处理相关逻辑。
QJsonObject getOnlineJson(const QString &path,
                          bool *ok = nullptr,
                          QString *errorText = nullptr)
{
    if (ok) {
        *ok = false;
    }

    const OnlineApiConfig cfg = loadOnlineApiConfig();
    const QUrl url = buildOnlineUrl(cfg.baseUrl, path);
    if (!url.isValid()) {
        if (errorText) {
            *errorText = QStringLiteral("服务器地址无效");
        }
        return QJsonObject();
    }

    QNetworkRequest request(url);
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(cfg.timeoutMs);
    loop.exec();

    if (timeout.isActive() == false && reply->isRunning()) {
        reply->abort();
        if (errorText) {
            *errorText = QStringLiteral("请求超时");
        }
        reply->deleteLater();
        return QJsonObject();
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorText) {
            *errorText = reply->errorString();
        }
        reply->deleteLater();
        return QJsonObject();
    }

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorText) {
            *errorText = QStringLiteral("服务器响应不是有效 JSON 对象");
        }
        return QJsonObject();
    }

    if (ok) {
        *ok = true;
    }
    return doc.object();
}

// 功能：处理相关逻辑。
QString currentAppVersionText()
{
    const QString version = QCoreApplication::applicationVersion().trimmed();
    if (!version.isEmpty()) {
        return version;
    }
    return QStringLiteral("1.00");
}

// 功能：处理相关逻辑。
QString resolveUpdatePackageUrl(const QJsonObject &response)
{
    const QString directUrl = response.value(QStringLiteral("apkUrl")).toString().trimmed();
    if (!directUrl.isEmpty()) {
        return directUrl;
    }

    const QString relativePath = response.value(QStringLiteral("apkPath")).toString().trimmed();
    if (relativePath.isEmpty()) {
        return QString();
    }

    const OnlineApiConfig cfg = loadOnlineApiConfig();
    return buildOnlineUrl(cfg.baseUrl, relativePath).toString();
}

// 功能：处理相关逻辑。
QString safeUpdateFileVersion(const QString &versionText)
{
    QString cleaned = versionText.trimmed();
    cleaned.remove(QRegularExpression(QStringLiteral("[^0-9A-Za-z._-]")));
    if (cleaned.isEmpty()) {
        return QStringLiteral("unknown");
    }
    return cleaned;
}

#ifdef Q_OS_ANDROID
// 功能：处理相关逻辑。
bool launchAndroidInstallerForApk(const QString &apkPath, QString *errorText = nullptr)
{
    if (apkPath.trimmed().isEmpty()) {
        if (errorText) {
            *errorText = QStringLiteral("安装包路径为空");
        }
        return false;
    }

    if (!QFileInfo::exists(apkPath)) {
        if (errorText) {
            *errorText = QStringLiteral("安装包不存在：%1").arg(apkPath);
        }
        return false;
    }

    QAndroidJniObject action = QAndroidJniObject::fromString(QStringLiteral("android.intent.action.VIEW"));
    QAndroidJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object<jstring>());

    QAndroidJniObject activity = QtAndroid::androidActivity();
    if (!activity.isValid()) {
        if (errorText) {
            *errorText = QStringLiteral("无法获取当前 Android Activity");
        }
        return false;
    }

    QAndroidJniObject filePath = QAndroidJniObject::fromString(apkPath);
    QAndroidJniObject apkFile("java/io/File", "(Ljava/lang/String;)V", filePath.object<jstring>());

    QAndroidJniObject packageName = activity.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QAndroidJniObject authority;
    if (packageName.isValid()) {
        QAndroidJniObject suffix = QAndroidJniObject::fromString(QStringLiteral(".fileprovider"));
        authority = packageName.callObjectMethod("concat", "(Ljava/lang/String;)Ljava/lang/String;", suffix.object<jstring>());
    }

    QAndroidJniObject uri;
    if (authority.isValid()) {
        uri = QAndroidJniObject::callStaticObjectMethod(
            "androidx/core/content/FileProvider",
            "getUriForFile",
            "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
            activity.object<jobject>(),
            authority.object<jstring>(),
            apkFile.object<jobject>());
        QAndroidJniEnvironment env;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            uri = QAndroidJniObject();
        }

        if (!uri.isValid()) {
            uri = QAndroidJniObject::callStaticObjectMethod(
                "android/support/v4/content/FileProvider",
                "getUriForFile",
                "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
                activity.object<jobject>(),
                authority.object<jstring>(),
                apkFile.object<jobject>());
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                uri = QAndroidJniObject();
            }
        }
    }

    if (!uri.isValid()) {
        QAndroidJniObject uriString = QAndroidJniObject::fromString(QUrl::fromLocalFile(apkPath).toString());
        uri = QAndroidJniObject::callStaticObjectMethod(
            "android/net/Uri",
            "parse",
            "(Ljava/lang/String;)Landroid/net/Uri;",
            uriString.object<jstring>());
    }

    if (!uri.isValid()) {
        if (errorText) {
            *errorText = QStringLiteral("无法构造安装 URI，请手动安装：%1").arg(apkPath);
        }
        return false;
    }

    QAndroidJniObject mimeType = QAndroidJniObject::fromString(QStringLiteral("application/vnd.android.package-archive"));

    intent.callObjectMethod("setDataAndType",
                            "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;",
                            uri.object<jobject>(),
                            mimeType.object<jstring>());

    constexpr jint kFlagActivityNewTask = 0x10000000;
    constexpr jint kFlagGrantReadUriPermission = 0x00000001;
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", kFlagActivityNewTask | kFlagGrantReadUriPermission);
    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());

    QAndroidJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        if (errorText) {
            *errorText = QStringLiteral("系统安装器调用失败，请手动安装：%1\n如为 Android 8+，请在系统设置中允许本应用安装未知来源应用。").arg(apkPath);
        }
        return false;
    }
    return true;
}
#endif

// 功能：配置远程服务器地址和超时，优先级：环境变量 > 编译宏 > 默认值。
OnlineApiConfig loadOnlineApiConfig()
{
    OnlineApiConfig cfg;
    cfg.baseUrl = mouseplan::common::config::resolveServerBaseUrl();
    cfg.timeoutMs = mouseplan::common::config::resolveServerTimeoutMs();
    return cfg;
}

// 功能：处理相关逻辑。
QJsonObject postOnlineJson(const QString &path,
                           const QJsonObject &payload,
                           bool *ok = nullptr,
                           QString *errorText = nullptr)
{
    if (ok) {
        *ok = false;
    }

    const OnlineApiConfig cfg = loadOnlineApiConfig();
    QString urlText = cfg.baseUrl.trimmed();
    if (urlText.endsWith('/')) {
        urlText.chop(1);
    }
    QString suffix = path.trimmed();
    if (!suffix.startsWith('/')) {
        suffix.prepend('/');
    }
    const QUrl url(urlText + suffix);
    if (!url.isValid()) {
        if (errorText) {
            *errorText = QStringLiteral("服务器地址无效");
        }
        return QJsonObject();
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request,
                                        QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(cfg.timeoutMs);
    loop.exec();

    if (timeout.isActive() == false && reply->isRunning()) {
        reply->abort();
        if (errorText) {
            *errorText = QStringLiteral("请求超时");
        }
        reply->deleteLater();
        return QJsonObject();
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorText) {
            *errorText = reply->errorString();
        }
        reply->deleteLater();
        return QJsonObject();
    }

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorText) {
            *errorText = QStringLiteral("服务器响应不是有效 JSON 对象");
        }
        return QJsonObject();
    }

    if (ok) {
        *ok = true;
    }
    return doc.object();
}

// 功能：处理相关逻辑。
bool onlineBoolResult(const QString &path,
                      const QJsonObject &payload,
                      const QString &resultField,
                      bool fallbackValue,
                      bool *requestOk = nullptr)
{
    bool ok = false;
    const QJsonObject response = postOnlineJson(path, payload, &ok, nullptr);
    if (requestOk) {
        *requestOk = ok;
    }
    if (!ok) {
        return fallbackValue;
    }

    if (response.contains(resultField)) {
        return response.value(resultField).toBool(fallbackValue);
    }
    if (response.contains(QStringLiteral("success"))) {
        return response.value(QStringLiteral("success")).toBool(fallbackValue);
    }
    return fallbackValue;
}

// 功能：处理相关逻辑。
QJsonObject planSetToOnlineJson(const PlanSet &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("weightKg"), value.weightKg);
    obj.insert(QStringLiteral("reps"), value.reps);
    obj.insert(QStringLiteral("remark"), value.remark);
    return obj;
}

// 功能：处理相关逻辑。
QJsonObject workoutItemToOnlineJson(const WorkoutItem &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), value.name);
    obj.insert(QStringLiteral("restSeconds"), value.restSeconds);

    QJsonArray warmup;
    for (const PlanSet &set : value.warmupSets) {
        warmup.append(planSetToOnlineJson(set));
    }
    obj.insert(QStringLiteral("warmupSets"), warmup);

    QJsonArray work;
    for (const PlanSet &set : value.workSets) {
        work.append(planSetToOnlineJson(set));
    }
    obj.insert(QStringLiteral("workSets"), work);
    return obj;
}

// 功能：处理相关逻辑。
QJsonObject dayPlanToOnlineJson(const DayPlan &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("title"), value.title);
    obj.insert(QStringLiteral("defaultMinutes"), value.defaultMinutes);

    QJsonArray items;
    for (const WorkoutItem &item : value.items) {
        items.append(workoutItemToOnlineJson(item));
    }
    obj.insert(QStringLiteral("items"), items);
    return obj;
}

// 功能：处理相关逻辑。
QJsonObject masterPlanToOnlineJson(const MasterPlan &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), value.id);
    obj.insert(QStringLiteral("ownerUserId"), value.ownerUserId);
    obj.insert(QStringLiteral("name"), value.name);
    obj.insert(QStringLiteral("trainDays"), value.trainDays);
    obj.insert(QStringLiteral("restDays"), value.restDays);
    obj.insert(QStringLiteral("startDate"), value.startDate.toString(QStringLiteral("yyyy-MM-dd")));

    QJsonArray days;
    for (const DayPlan &day : value.dayPlans) {
        days.append(dayPlanToOnlineJson(day));
    }
    obj.insert(QStringLiteral("dayPlans"), days);
    return obj;
}

// 功能：处理相关逻辑。
QJsonObject recordItemToOnlineJson(const RecordItem &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("item"), workoutItemToOnlineJson(value.item));
    obj.insert(QStringLiteral("completed"), value.completed);
    obj.insert(QStringLiteral("ignored"), value.ignored);

    QJsonArray warmupChecked;
    for (bool v : value.warmupChecked) {
        warmupChecked.append(v);
    }
    obj.insert(QStringLiteral("warmupChecked"), warmupChecked);

    QJsonArray workChecked;
    for (bool v : value.workChecked) {
        workChecked.append(v);
    }
    obj.insert(QStringLiteral("workChecked"), workChecked);
    return obj;
}

// 功能：处理相关逻辑。
QJsonObject trainingRecordToOnlineJson(const TrainingRecord &value)
{
    QJsonObject dayObj;
    dayObj.insert(QStringLiteral("title"), value.day.title);
    QJsonArray items;
    for (const RecordItem &item : value.day.items) {
        items.append(recordItemToOnlineJson(item));
    }
    dayObj.insert(QStringLiteral("items"), items);

    QJsonObject obj;
    obj.insert(QStringLiteral("ownerUserId"), value.ownerUserId);
    obj.insert(QStringLiteral("date"), value.date.toString(QStringLiteral("yyyy-MM-dd")));
    obj.insert(QStringLiteral("submitted"), value.submitted);
    obj.insert(QStringLiteral("totalMinutes"), value.totalMinutes);
    obj.insert(QStringLiteral("isSupplement"), value.isSupplement);
    obj.insert(QStringLiteral("day"), dayObj);
    return obj;
}

// 功能：处理相关逻辑。
QJsonObject userToOnlineJson(const User &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), value.id);
    obj.insert(QStringLiteral("username"), value.username);
    obj.insert(QStringLiteral("nickname"), value.nickname);
    obj.insert(QStringLiteral("isLocalAccount"), value.isLocalAccount);
    obj.insert(QStringLiteral("theme"), value.theme);
    obj.insert(QStringLiteral("themeColorPreset"), value.themeColorPreset);
    obj.insert(QStringLiteral("themeChosen"), value.themeChosen);
    obj.insert(QStringLiteral("activePlanId"), value.activePlanId);
    obj.insert(QStringLiteral("avatarImagePath"), value.avatarImagePath);
    obj.insert(QStringLiteral("profileCoverImagePath"), value.profileCoverImagePath);
    obj.insert(QStringLiteral("gender"), value.gender);
    obj.insert(QStringLiteral("age"), value.age);
    obj.insert(QStringLiteral("messageToMouse"), value.messageToMouse);
    return obj;
}

// 功能：处理相关逻辑。
bool ensureStoragePermissionForUpdate(QWidget *parent)
{
#ifdef Q_OS_ANDROID
    const QString kWriteStorage = QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE");
    const QString kReadStorage = QStringLiteral("android.permission.READ_EXTERNAL_STORAGE");
    QStringList requiredPermissions;
    requiredPermissions << kWriteStorage << kReadStorage;
    const auto permissions = QtAndroid::requestPermissionsSync(requiredPermissions);
    auto granted = [&](const QString &name) {
        if (!permissions.contains(name)) {
            return true;
        }
        return permissions.value(name) == QtAndroid::PermissionResult::Granted;
    };

    if (!granted(kWriteStorage) && !granted(kReadStorage)) {
        QMessageBox::warning(parent,
                             QStringLiteral("权限提示"),
                             QStringLiteral("应用更新需要存储权限，请授权后重试。"));
        return false;
    }
    return true;
#else
    Q_UNUSED(parent);
    return true;
#endif
}

// 功能：处理相关逻辑。
bool ensureOnlineLoginPermission(QWidget *parent)
{
#ifdef Q_OS_ANDROID
    const QString kInternet = QStringLiteral("android.permission.INTERNET");
    const QString kNetworkState = QStringLiteral("android.permission.ACCESS_NETWORK_STATE");
    QStringList requiredPermissions;
    requiredPermissions << kInternet << kNetworkState;
    const auto permissions = QtAndroid::requestPermissionsSync(requiredPermissions);
    auto granted = [&](const QString &name) {
        // Normal permissions may not always appear in runtime map, treat missing as granted.
        if (!permissions.contains(name)) {
            return true;
        }
        return permissions.value(name) == QtAndroid::PermissionResult::Granted;
    };

    if (!granted(kInternet) || !granted(kNetworkState)) {
        QMessageBox::warning(parent,
                             QStringLiteral("权限提示"),
                             QStringLiteral("在线模式登录需要网络权限，请授权后重试。"));
        return false;
    }
#else
    Q_UNUSED(parent);
#endif
    return true;
}

// 功能：处理相关逻辑。
bool verifyRegistrationCodeOnlineReserved(const QString &codeHash)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("codeHash"), codeHash);
    return onlineBoolResult(QStringLiteral("/auth/verify-registration-code"),
                            payload,
                            QStringLiteral("valid"),
                            false,
                            nullptr);
}

// 功能：处理相关逻辑。
bool consumeRegistrationCodeOnlineReserved(const QString &codeHash, const QString &userId)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("codeHash"), codeHash);
    payload.insert(QStringLiteral("userId"), userId);
    return onlineBoolResult(QStringLiteral("/auth/consume-registration-code"),
                            payload,
                            QStringLiteral("success"),
                            false,
                            nullptr);
}

// 功能：处理相关逻辑。
bool uploadPasswordHashToServerReserved(const QString &userId, const QString &passwordHash)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), userId);
    payload.insert(QStringLiteral("passwordHash"), passwordHash);
    return onlineBoolResult(QStringLiteral("/auth/upload-password-hash"),
                            payload,
                            QStringLiteral("success"),
                            false,
                            nullptr);
}

// 功能：处理相关逻辑。
bool verifyPasswordHashWithServerReserved(const QString &userId,
                                          const QString &username,
                                          const QString &passwordHash,
                                          QString *resolvedUserIdOut = nullptr)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), userId);
    payload.insert(QStringLiteral("username"), username);
    payload.insert(QStringLiteral("passwordHash"), passwordHash);

    bool requestOk = false;
    // 健身主题接口路径从主题网络模块读取，避免在主窗口散落字符串常量。
    const QJsonObject response = postOnlineJson(mouseplan::fitness::FitnessOnlineApi::authLoginPath(), payload, &requestOk, nullptr);
    if (!requestOk) {
        return false;
    }

    const bool success = response.value(QStringLiteral("success")).toBool(false);
    if (success && resolvedUserIdOut) {
        QString serverUserId = response.value(QStringLiteral("userId")).toString().trimmed();
        if (serverUserId.isEmpty()) {
            serverUserId = userId.trimmed();
        }
        *resolvedUserIdOut = serverUserId;
    }
    return success;
}

// 功能：处理相关逻辑。
void syncUserToCloudReserved(const User &user)
{
    if (user.isLocalAccount) {
        return;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("user"), userToOnlineJson(user));
    // 主题化网络分层：用户同步路径由 fitness_online_api 统一维护。
    onlineBoolResult(mouseplan::fitness::FitnessOnlineApi::syncUserPath(),
                     payload,
                     QStringLiteral("success"),
                     true,
                     nullptr);
}

// 功能：处理相关逻辑。
bool syncPlanToCloudReserved(const QString &userId, const MasterPlan &plan)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), userId);
    payload.insert(QStringLiteral("plan"), masterPlanToOnlineJson(plan));
    return onlineBoolResult(QStringLiteral("/sync/plan"),
                            payload,
                            QStringLiteral("success"),
                            false,
                            nullptr);
}

// 功能：处理相关逻辑。
bool pushPackage1ToCloudReserved(const QString &userId, const QJsonObject &package1)
{
    if (userId.trimmed().isEmpty() || package1.isEmpty()) {
        return false;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), userId);
    payload.insert(QStringLiteral("package1"), package1);
    return onlineBoolResult(QStringLiteral("/sync/package1/push"),
                            payload,
                            QStringLiteral("success"),
                            false,
                            nullptr);
}

// 功能：处理相关逻辑。
bool pullPackage1FromCloudReserved(const QString &userId,
                                   QJsonObject *package1Out,
                                   QString *errorText = nullptr)
{
    if (package1Out) {
        *package1Out = QJsonObject();
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), userId);

    bool ok = false;
    const QJsonObject response = postOnlineJson(QStringLiteral("/sync/package1/pull"), payload, &ok, errorText);
    if (!ok) {
        return false;
    }
    if (!response.value(QStringLiteral("success")).toBool(true)) {
        if (errorText) {
            *errorText = response.value(QStringLiteral("message")).toString();
        }
        return false;
    }
    const QJsonObject package1 = response.value(QStringLiteral("package1")).toObject();
    if (package1Out) {
        *package1Out = package1;
    }
    return !package1.isEmpty();
}

} // namespace

// 功能：数据加载与准备

void MainWindow::applyThemeStyle()
{
    if (const User *u = currentUser()) {
        const ThemeColorPreset preset = resolveThemeColorPreset(u->theme, u->themeColorPreset);
        themePrimary = preset.primary;
        themeAccent = preset.accent;
        themeSoft = preset.soft;
        themeTodayBg = preset.todayBg;
        themeSelectedBg = preset.selectedBg;
    }

    const QString css = QString(
                            "QWidget{background:%1;color:#1d2a20;font-size:14px;}"
                            "QWidget#loginPage{background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #f6f7f3,stop:1 #eef3ef);}"
                            "QLabel#logoLabel{color:%2;letter-spacing:2px;}"
                            "QLabel#loginCaptionLabel{color:#6a766f;font-size:13px;}"
                            "QFrame#loginCard,QFrame#navBarCard{background:white;border:1px solid #dde7e0;border-radius:20px;}"
                            "QFrame#calendarCard,QFrame#todayPlanCard,QFrame#planQuickCard{background:white;border:2px dashed #cfe0d5;border-radius:20px;}"
                            "QFrame#workoutItemCard{background:#fbfcfb;border:1px solid #e6efea;border-radius:18px;}"
                            "QFrame#navBarCard{border-radius:24px;}"
                            "QLineEdit{padding:10px 12px;border:1px solid #d6dfda;border-radius:16px;background:#fbfcfb;color:#24352a;}"
                            "QLineEdit:focus{border:1px solid %4;background:white;}"
                            "QPushButton{background:%2;color:white;border:none;border-radius:16px;padding:10px 12px;font-weight:600;}"
                            "QPushButton:hover{background:%3;}"
                            "QPushButton#primaryLoginButton,QPushButton#submitTodayButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %2,stop:1 #3ca45d);font-size:20px;font-weight:800;}"
                            "QPushButton#submitTodayButton:disabled{background:#cfd6d2;color:#7a8680;}"
                            "QPushButton#profileEditInfoButton{background:#5f7f66;color:white;border-radius:16px;font-size:24px;min-height:64px;}"
                            "QPushButton#profileInfoButton{background:#3d7b83;color:white;border-radius:16px;font-size:24px;min-height:64px;}"
                            "QPushButton#profileUpdateButton{background:#3d7db8;color:white;border-radius:16px;font-size:24px;min-height:64px;}"
                            "QPushButton#profileThemeSwitchButton{background:#7a5ea9;color:white;border-radius:16px;font-size:24px;min-height:64px;}"
                            "QPushButton#profileResetDataButton{background:#d38f2f;color:white;border-radius:16px;font-size:24px;min-height:64px;}"
                            "QPushButton#profileLogoutButton{background:#c95d5d;color:white;border-radius:16px;font-size:24px;min-height:64px;}"
                            "QPushButton#planSettingButton{background:#f5faf7;color:%2;border:1px solid #d7e9dc;border-radius:18px;padding:12px;font-weight:700;}"
                            "QPushButton:flat{background:transparent;color:#6a766f;border:none;padding:4px 8px;font-weight:500;}"
                            "QPushButton:flat:hover{color:%2;background:transparent;}"
                            "QLabel#dayTitleLabel{color:#203328;font-weight:700;}"
                            "QLabel#dayHintLabel{color:#738078;font-size:16px;}"
                            "QLabel#quickTitleLabel{color:#5d6b63;font-size:24px;font-weight:800;}"
                            "QScrollArea{border:none;background:transparent;}"
                            "QCalendarWidget{background:transparent;border:none;}"
                            "QCalendarWidget QWidget#qt_calendar_navigationbar{background:transparent;color:%2;}"
                            "QCalendarWidget QToolButton{color:%2;background:#eef7f0;border:none;border-radius:12px;padding:12px 16px;font-weight:700;font-size:%5px;}"
                            "QCalendarWidget QSpinBox{font-size:%5px;min-height:52px;min-width:150px;padding:4px 8px;}"
                            "QCalendarWidget QSpinBox::up-button,QCalendarWidget QSpinBox::down-button{width:34px;}"
                            "QCalendarWidget QMenu{background:white;border:1px solid #dbe6de;}"
                            "QCalendarWidget QAbstractItemView:enabled{selection-background-color:%4;selection-color:#163022;outline:0;border:none;background:white;font-size:%6px;}"
                            "QToolButton{background:transparent;border:none;color:#738078;font-size:%7px;font-weight:600;padding:10px 12px;}"
                            "QToolButton::menu-indicator{image:none;}"
                            "QToolButton#navHomeButton,QToolButton#navThemeButton,QToolButton#navProfileButton{min-width:110px;border-radius:16px;}"
                            "QToolButton#navHomeButton:checked,QToolButton#navThemeButton:checked,QToolButton#navProfileButton:checked{color:%2;background:#dcefff;}"
                            "QToolButton:hover{color:%2;}"
                            "QListWidget{border:1px solid #dde7e0;border-radius:14px;background:#fafcfb;padding:6px;}"
                            "QMessageBox{background:%1;}"
                            "QMessageBox QLabel{color:#1d2a20;font-size:20px;}"
                            "QMessageBox QPushButton{background:%2;color:white;border:none;border-radius:12px;min-width:160px;min-height:56px;padding:8px 14px;font-size:22px;font-weight:800;}"
                            "QMessageBox QPushButton:hover{background:%3;}"
                            "QDialog{background:#f5f7f4;}")
                            .arg(themeSoft)
                            .arg(themePrimary)
                            .arg(themeAccent)
                            .arg(themeSelectedBg)
                            .arg(gUiTuning.calendarNavMinFont)
                            .arg(gUiTuning.calendarDayMinFont)
                            .arg(gUiTuning.navTextMinFont);
    qApp->setStyleSheet(css);

    if (PlanCalendarWidget *planCal = dynamic_cast<PlanCalendarWidget *>(calendar)) {
        planCal->setMarkerColors(QColor(themePrimary).darker(145), QColor(themeSoft).darker(106));
    }

    if (homeNavButton) {
        homeNavButton->setCheckable(true);
        homeNavButton->setChecked(true);
    }
    if (themeNavButton) {
        themeNavButton->setCheckable(true);
    }
    if (profileNavButton) {
        profileNavButton->setCheckable(true);
    }
}

// 功能：处理相关逻辑。

void MainWindow::runRegisterFlow()
{
    if (loginLocalMode) {
        runLocalRegisterFlow();
    } else {
        runOnlineRegisterFlow();
    }
}

// 功能：执行本地模式注册向导。

bool MainWindow::runLocalRegisterFlow()
{
    mouseplan::ui::login::LoginRegisterFlowCallbacks callbacks;
    callbacks.hashSecret = [](const QString &value) { return buildSecretHash(value); };
    callbacks.generateNickname = []() { return generateRandomNicknameDigits(); };
    callbacks.syncUserToCloud = [](const User &user) { syncUserToCloudReserved(user); };
    callbacks.uploadPasswordHash = [](const QString &userId, const QString &passwordHash) {
        return uploadPasswordHashToServerReserved(userId, passwordHash);
    };
    callbacks.syncPlanToCloud = [](const QString &userId, const MasterPlan &plan) {
        syncPlanToCloudReserved(userId, plan);
    };
    callbacks.pushPackage1ToCloud = [](const QString &userId, const QJsonObject &snapshot) {
        pushPackage1ToCloudReserved(userId, snapshot);
    };
    callbacks.verifyRegistrationCode = [](const QString &codeHash) {
        return verifyRegistrationCodeOnlineReserved(codeHash);
    };
    callbacks.consumeRegistrationCode = [](const QString &codeHash, const QString &userId) {
        return consumeRegistrationCodeOnlineReserved(codeHash, userId);
    };
    callbacks.setupMobileDialog = [](QDialog &dialog, QWidget *parent) {
        setupMobileDialog(dialog, parent);
    };

    return mouseplan::ui::login::runLocalRegisterWizard(this, store, callbacks);
}

// 功能：执行在线模式注册向导。

bool MainWindow::runOnlineRegisterFlow()
{
    mouseplan::ui::login::LoginRegisterFlowCallbacks callbacks;
    callbacks.hashSecret = [](const QString &value) { return buildSecretHash(value); };
    callbacks.generateNickname = []() { return generateRandomNicknameDigits(); };
    callbacks.syncUserToCloud = [](const User &user) { syncUserToCloudReserved(user); };
    callbacks.uploadPasswordHash = [](const QString &userId, const QString &passwordHash) {
        return uploadPasswordHashToServerReserved(userId, passwordHash);
    };
    callbacks.syncPlanToCloud = [](const QString &userId, const MasterPlan &plan) {
        syncPlanToCloudReserved(userId, plan);
    };
    callbacks.pushPackage1ToCloud = [](const QString &userId, const QJsonObject &snapshot) {
        pushPackage1ToCloudReserved(userId, snapshot);
    };
    callbacks.verifyRegistrationCode = [](const QString &codeHash) {
        return verifyRegistrationCodeOnlineReserved(codeHash);
    };
    callbacks.consumeRegistrationCode = [](const QString &codeHash, const QString &userId) {
        return consumeRegistrationCodeOnlineReserved(codeHash, userId);
    };
    callbacks.setupMobileDialog = [](QDialog &dialog, QWidget *parent) {
        setupMobileDialog(dialog, parent);
    };

    return mouseplan::ui::login::runOnlineRegisterWizard(this, store, callbacks);
}

// 功能：首次登录时弹出主题选择并持久化结果。

void MainWindow::maybeRunThemeSelection()
{
    User *u = currentUser();
    if (!u || u->themeChosen) {
        return;
    }
    QString theme = u->theme.trimmed().isEmpty() ? QStringLiteral("fitness") : u->theme;
    if (runThemeSelectionDialog(this,
                                theme,
                                true,
                                QStringLiteral("选择应用主题"),
                                QStringLiteral("欢迎使用 MousePlan，请先选择应用主题后进入主界面。"),
                                QStringLiteral("确认并进入"))) {
        u->theme = theme;
        u->themeChosen = true;
        store.save();
        persistCurrentUserPackage1();
        applyThemeStyle();
    }
}

// 功能：从底部导航进入主题页。

void MainWindow::openThemeSelectFromNav()
{
    showThemeTab();
}

// 功能：打开主题配色选择面板。

void MainWindow::openThemeColorFromNav()
{
// 该文件由重构生成：保持原函数逻辑不变，仅做文件分层。
    User *u = currentUser();
    if (!u) {
        return;
    }

    const QVector<ThemeColorPreset> presets = themeColorPresets();
    QString selectedKey = resolveThemeColorPreset(u->theme, u->themeColorPreset).key;

    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("选择主题色"));

    const ThemeColorPreset initialPreset = resolveThemeColorPreset(u->theme, selectedKey);
    const int navBlank = qMax(120, navBarCard ? navBarCard->height() : 120);
    QRect targetRect = geometry();
    if (!targetRect.isValid() || targetRect.isEmpty()) {
        targetRect = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->geometry() : QRect(0, 0, 1080, 1920);
    }
    const int sideMargin = qMax(24, targetRect.width() / 15);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QFrame *topBlank = new QFrame(&dialog);
    topBlank->setFixedHeight(navBlank);

    QFrame *bottomBlank = new QFrame(&dialog);
    bottomBlank->setFixedHeight(navBlank);

    QWidget *center = new QWidget(&dialog);
    center->setStyleSheet(QString("background:%1;").arg(initialPreset.soft));
    QHBoxLayout *centerWrap = new QHBoxLayout(center);
    centerWrap->setContentsMargins(sideMargin, 14, sideMargin, 14);
    centerWrap->setSpacing(0);

    QFrame *card = new QFrame(center);
    card->setStyleSheet("background:#ffffff;border:none;border-radius:24px;");
    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 18, 22, 18);
    cardLayout->setSpacing(14);

    QLabel *title = new QLabel(card);
    title->setAlignment(Qt::AlignCenter);
    title->setFixedHeight(96);
    title->setStyleSheet("background:transparent;");

    QLabel *tip = new QLabel(QStringLiteral("仅切换颜色，不改变应用主题类型"), card);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet("font-size:30px;font-weight:700;color:#4f5f57;");

    QWidget *gridHost = new QWidget(card);
    QGridLayout *grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);

    QVector<QPushButton *> presetButtons;
    for (int i = 0; i < presets.size(); ++i) {
        const ThemeColorPreset &preset = presets[i];
        QPushButton *btn = new QPushButton(gridHost);
        btn->setCheckable(true);
        btn->setProperty("presetKey", preset.key);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setText(QString("%1\n").arg(preset.name));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        btn->setMinimumHeight(210);
        presetButtons.push_back(btn);
        grid->addWidget(btn, i / 3, i % 3);
    }
    for (int r = 0; r < 3; ++r) {
        grid->setRowStretch(r, 1);
    }
    for (int c = 0; c < 3; ++c) {
        grid->setColumnStretch(c, 1);
    }

    QHBoxLayout *ops = new QHBoxLayout();
    ops->setSpacing(18);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), card);
    QPushButton *saveBtn = new QPushButton(QStringLiteral("应用"), card);
    cancelBtn->setStyleSheet("font-size:34px;min-height:96px;background:#8a8a8a;color:white;border:none;border-radius:16px;");
    saveBtn->setStyleSheet("font-size:34px;min-height:96px;background:#2f8f46;color:white;border:none;border-radius:16px;");
    ops->addWidget(cancelBtn);
    ops->addWidget(saveBtn);

    auto refreshTitle = [&](const ThemeColorPreset &preset) {
        title->setPixmap(QPixmap());
        title->setText(QStringLiteral("主题色"));
        title->setStyleSheet(QString("background:transparent;font-size:62px;font-weight:900;color:%1;")
                             .arg(QColor(preset.primary).darker(130).name()));
    };

    auto refreshSurface = [&]() {
        const ThemeColorPreset currentPreset = resolveThemeColorPreset(u->theme, selectedKey);
        topBlank->setStyleSheet(QString("background:%1;").arg(QColor(currentPreset.soft).lighter(108).name()));
        bottomBlank->setStyleSheet(QString("background:%1;").arg(QColor(currentPreset.soft).lighter(108).name()));
        center->setStyleSheet(QString("background:%1;").arg(QColor(currentPreset.soft).lighter(104).name()));
        saveBtn->setStyleSheet(QString("font-size:34px;min-height:96px;background:%1;color:white;border:none;border-radius:16px;")
                               .arg(currentPreset.primary));
        refreshTitle(currentPreset);

        for (QPushButton *btn : presetButtons) {
            const QString key = btn->property("presetKey").toString();
            const ThemeColorPreset preset = resolveThemeColorPreset(u->theme, key);
            const bool chosen = (key == selectedKey);
            btn->setChecked(chosen);
            btn->setStyleSheet(QString(
                                   "QPushButton{font-size:31px;font-weight:%1;padding:10px 10px;line-height:1.35;"
                                   "text-align:center;border-radius:18px;border:3px solid %2;background:%3;color:%4;}"
                                   "QPushButton:pressed{background:%5;}")
                                   .arg(chosen ? 900 : 700)
                                   .arg(chosen ? preset.accent : QStringLiteral("#d7e2db"))
                                   .arg(chosen ? preset.selectedBg : preset.soft)
                                   .arg(QColor(preset.primary).darker(120).name())
                                   .arg(QColor(preset.soft).darker(106).name()));
        }
    };

    for (QPushButton *btn : presetButtons) {
        QObject::connect(btn, &QPushButton::clicked, &dialog, [&, btn]() {
            selectedKey = btn->property("presetKey").toString();
            refreshSurface();
        });
    }

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(saveBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    cardLayout->addWidget(title);
    cardLayout->addWidget(tip);
    cardLayout->addWidget(gridHost, 1);
    cardLayout->addLayout(ops);
    centerWrap->addWidget(card);

    layout->addWidget(topBlank);
    layout->addWidget(center, 1);
    layout->addWidget(bottomBlank);

    refreshSurface();
    QTimer::singleShot(0, &dialog, [&]() {
        refreshSurface();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    u->themeColorPreset = selectedKey;
    store.save();
    persistCurrentUserPackage1();
    const bool wasOnProfile = (homeContentStack && homeContentStack->currentWidget() == profilePanel);
    applyThemeStyle();
    if (wasOnProfile) {
        showProfileTab();
    } else {
        showHomeTab();
    }
    rebuildCalendarFormats();
    rebuildDayView();
}

// 功能：在个人资料页切换应用主题并刷新界面。

void MainWindow::openAppThemeSelectionFromProfile()
{
    User *u = currentUser();
    if (!u) {
        return;
    }

    QString theme = u->theme.trimmed().isEmpty() ? QStringLiteral("fitness") : u->theme;
    if (!runThemeSelectionDialog(this,
                                 theme,
                                 false,
                                 QStringLiteral("切换应用主题"),
                                 QStringLiteral("选择后将重新加载应用界面---主题颜色可以选择主题后自定义"),
                                 QStringLiteral("确认切换"))) {
        return;
    }

    u->theme = theme;
    u->themeChosen = true;
    store.save();
    persistCurrentUserPackage1();

    const QString themeName = (theme == QStringLiteral("study"))
                                  ? QStringLiteral("学习主题")
                                  : (theme == QStringLiteral("normal")
                                         ? QStringLiteral("普通计划主题")
                                         : QStringLiteral("健身主题"));
    showThemeReloadPlaceholderDialog(this, themeName);

    const bool wasOnProfile = (homeContentStack && homeContentStack->currentWidget() == profilePanel);
    applyThemeStyle();
    if (wasOnProfile) {
        showProfileTab();
    } else {
        showHomeTab();
    }
    rebuildCalendarFormats();
    rebuildDayView();
}

// 功能：检查新版本并引导下载与安装。

void MainWindow::checkForUpdates()
{
    if (!ensureOnlineLoginPermission(this)) {
        return;
    }

    bool requestOk = false;
    QString errorText;
    const QJsonObject response = getOnlineJson(mouseplan::fitness::FitnessOnlineApi::updateLatestPath(), &requestOk, &errorText);
    if (!requestOk) {
        QMessageBox::warning(this,
                             QStringLiteral("更新检查失败"),
                             QStringLiteral("无法连接更新服务器：%1").arg(errorText.isEmpty() ? QStringLiteral("未知错误") : errorText));
        return;
    }

    const bool success = response.value(QStringLiteral("success")).toBool(true);
    if (!success) {
        const QString message = response.value(QStringLiteral("message")).toString().trimmed();
        QMessageBox::warning(this,
                             QStringLiteral("更新检查失败"),
                             message.isEmpty() ? QStringLiteral("服务器返回失败结果。") : message);
        return;
    }

    const QString latestVersion = response.value(QStringLiteral("latestVersion")).toString().trimmed();
    if (latestVersion.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("更新检查失败"),
                             QStringLiteral("服务器未返回最新版本号。"));
        return;
    }

    const QString currentVersion = currentAppVersionText();
    if (compareVersionText(currentVersion, latestVersion) >= 0) {
        QMessageBox::information(this,
                                 QStringLiteral("检查更新"),
                                 QStringLiteral("当前已经为最新版本"));
        return;
    }

    QString promptText = QStringLiteral("检测到新版本 %1（当前版本 %2），是否立即下载并安装？")
                             .arg(latestVersion)
                             .arg(currentVersion);
    const QString changelog = response.value(QStringLiteral("changelog")).toString().trimmed();
    if (!changelog.isEmpty()) {
        promptText += QStringLiteral("\n\n更新内容：\n%1").arg(changelog);
    }

    if (QMessageBox::question(this,
                              QStringLiteral("发现新版本"),
                              promptText,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes)
        != QMessageBox::Yes) {
        return;
    }

    const QString packageUrlText = resolveUpdatePackageUrl(response);
    const QUrl packageUrl(packageUrlText);
    if (!packageUrl.isValid() || packageUrlText.trimmed().isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("更新失败"),
                             QStringLiteral("服务器未返回有效的安装包地址。"));
        return;
    }

    if (!ensureStoragePermissionForUpdate(this)) {
        return;
    }

    const QString targetFileName = QStringLiteral("MousePlan_%1.apk")
                                       .arg(safeUpdateFileVersion(latestVersion));
    const QString downloadedPath = downloadFileWithProgress(this,
                                                            packageUrl,
                                                            targetFileName,
                                                            &errorText);
    if (downloadedPath.trimmed().isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("更新失败"),
                             errorText.isEmpty() ? QStringLiteral("安装包下载失败。") : errorText);
        return;
    }

#ifdef Q_OS_ANDROID
    if (!launchAndroidInstallerForApk(downloadedPath, &errorText)) {
        QMessageBox::warning(this,
                             QStringLiteral("安装启动失败"),
                             errorText.isEmpty()
                                 ? QStringLiteral("安装器调用失败，请手动安装。")
                                 : errorText);
        return;
    }
    QMessageBox::information(this,
                             QStringLiteral("更新下载完成"),
                             QStringLiteral("安装包已下载，正在调用系统安装器。"));
#else
    QMessageBox::information(this,
                             QStringLiteral("更新下载完成"),
                             QStringLiteral("安装包已下载到：\n%1")
                                 .arg(downloadedPath));
#endif
}

// 功能：提交用户反馈与建议到服务器。

void MainWindow::submitFeedbackSuggestion()
{
    User *u = currentUser();
    if (!u) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先登录后再提交反馈。"));
        return;
    }

    if (u->isLocalAccount) {
        QMessageBox::information(this,
                                 QStringLiteral("反馈及建议"),
                                 QStringLiteral("本地模式暂不支持提交建议，请切换到在线模式。"));
        return;
    }

    if (!ensureOnlineLoginPermission(this)) {
        return;
    }

    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("反馈及建议"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMargins(layout, dialog);
    layout->addSpacing(qMax(20, dialog.height() / 6));

    QTextEdit *input = new QTextEdit(&dialog);
    input->setPlaceholderText(QStringLiteral("请输入你的反馈或建议..."));
    const int oldInputHeight = qMax(160, dialog.height() / 4);
    const int newInputHeight = qMax(80, oldInputHeight / 2);
    input->setMinimumHeight(newInputHeight);
    input->setMaximumHeight(newInputHeight);
    const int oldInputWidth = qMax(220, dialog.width() / 4);
    const int targetInputWidth = qMin(dialog.width() - 20, oldInputWidth * 3);
    input->setMinimumWidth(targetInputWidth);
    input->setStyleSheet("font-size:22px;color:#33443a;background:white;border:1px solid #dbe6de;border-radius:14px;padding:8px;");
    layout->addWidget(input, 0, Qt::AlignHCenter);

    QHBoxLayout *ops = new QHBoxLayout();
    QPushButton *submitBtn = new QPushButton(QStringLiteral("提交"), &dialog);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    submitBtn->setStyleSheet("font-size:24px;min-height:74px;background:#2f8f46;color:white;border-radius:16px;");
    cancelBtn->setStyleSheet("font-size:24px;min-height:74px;background:#8b8f99;color:white;border-radius:16px;");
    ops->addWidget(submitBtn);
    ops->addWidget(cancelBtn);
    const int optionHeight = qMax(74, submitBtn->sizeHint().height());
    const int originalGap = qMax(20, dialog.height() / 6);
    layout->addSpacing(qMax(0, originalGap - optionHeight * 2));
    layout->addLayout(ops);
    layout->addStretch(1);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(submitBtn, &QPushButton::clicked, &dialog, [&]() {
        const QString content = input->toPlainText().trimmed();
        if (content.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("反馈内容不能为空。"));
            return;
        }

        const QJsonObject payload = mouseplan::ui::profile::ProfileInteractionHelper::buildFeedbackPayload(
            *u,
            content,
            QDateTime::currentDateTime().toString(Qt::ISODate));

        bool requestOk = false;
        QString errorText;
        const QJsonObject response = postOnlineJson(mouseplan::fitness::FitnessOnlineApi::feedbackSubmitPath(),
                                                    payload,
                                                    &requestOk,
                                                    &errorText);
        if (!requestOk) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("提交失败"),
                                 QStringLiteral("无法连接服务器：%1").arg(errorText.isEmpty() ? QStringLiteral("未知错误") : errorText));
            return;
        }

        if (!response.value(QStringLiteral("success")).toBool(false)) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("提交失败"),
                                 response.value(QStringLiteral("message")).toString().trimmed().isEmpty()
                                     ? QStringLiteral("服务器拒绝了此次提交。")
                                     : response.value(QStringLiteral("message")).toString().trimmed());
            return;
        }

        QMessageBox::information(this,
                                 QStringLiteral("反馈及建议"),
                                 QStringLiteral("感谢你的反馈与建议，我们已成功收到。"));
        dialog.accept();
    });

    dialog.exec();
}

// 功能：打开总计划管理对话框。

