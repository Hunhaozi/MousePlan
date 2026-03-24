#include "mainwindow.h"
#include "modules/common/agreement/agreement_text_loader.h"
#include "modules/common/config/network_config.h"
#include "modules/common/theme/theme_feature_gate.h"
#include "modules/themes/fitness/calendar/fitness_calendar_mark_builder.h"
#include "modules/themes/fitness/network/fitness_online_api.h"
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

// 功能：刷新主页的日历视图
void MainWindow::rebuildDayView()
{
    clearItemCards();

    const int shortSide = qMin(width(), height());
    const int itemTitlePx = qMax(gUiTuning.planItemTitleMinFont + 2, shortSide / 38);
    const int itemMetaPx = qMax(gUiTuning.planItemMetaMinFont + 1, shortSide / 52);
    const int itemChipPx = qMax(gUiTuning.planItemMetaMinFont - 1, shortSide / 65);
    const int itemButtonPx = qMax(gUiTuning.planItemButtonMinFont + 1, shortSide / 60);
    const int itemCardMinH = (qMax(gUiTuning.planItemMinHeight, shortSide / 5) * 5) / 4;

    const QDate today = QDate::currentDate();
    const CalendarPlanInfo info = resolvePlanInfo(selectedDate);//解析当前选择的日期
    if (submitTodayButton) {
        const bool isToday = (selectedDate == today);
        submitTodayButton->setEnabled(isToday);
        submitTodayButton->setToolTip(isToday
                                          ? QStringLiteral("提交当天训练记录")
                                          : QStringLiteral("仅支持提交当天训练记录"));
    }
    if (selectedDate == today && info.hasPlan && !info.isRestDay) {
        ensureRecordFromSelectedDayPlan();
    }
    const TrainingRecord *record = recordForDate(selectedDate);
    if (submitTodayButton) {
        const bool isToday = (selectedDate == today);
        const bool submittedToday = (record && record->submitted);
        submitTodayButton->setEnabled(isToday && !submittedToday);
        if (!isToday) {
            submitTodayButton->setToolTip(QStringLiteral("仅支持提交当天训练记录"));
        } else if (submittedToday) {
            submitTodayButton->setToolTip(QStringLiteral("当日记录已提交"));
        } else {
            submitTodayButton->setToolTip(QStringLiteral("提交当天训练记录"));
        }
    }

    QString titleSuffix = QStringLiteral("当日计划");
    if (selectedDate > today) {
        if (info.isRestDay) {
            titleSuffix = QStringLiteral("预期为休息日");
        }
        else{
            titleSuffix = QStringLiteral("预期训练计划");
        }
    } else if (selectedDate < today) {
        titleSuffix = QStringLiteral("历史训练计划");
    }
    QString titleText = nullptr;
    if (selectedDate == today) {
        titleText = QString("%1").arg(titleSuffix);
    }
    else{
        titleText = QString("%1 | %2").arg(selectedDate.toString("M月d日")).arg(titleSuffix);
    }
    if (info.hasPlan && !info.isRestDay && !info.message.isEmpty()) {
        titleText += QStringLiteral(": <span style='color:#d97706;font-weight:700;'>%1</span>")
                         .arg(info.message.toHtmlEscaped());
    }
    dayTitleLabel->setText(titleText);

    if (selectedDate < today && !record) {
        dayHintLabel->setText(QStringLiteral("%1 未提交训练记录")
                                  .arg(selectedDate.toString("M月d日")));
        QPushButton *reserve = new QPushButton(QStringLiteral("当日未提交记录，点击补充训练记录"), itemsContainer);
        const User *u = currentUser();
        bool canSupplement = true;
        QString limitHint;
        if (u && u->isLocalAccount) {
            const int used = localSupplementCountForMonth(selectedDate);
            if (used >= 3) {
                canSupplement = false;
                limitHint = QStringLiteral("本地模式：同一月最多补录 3 次，当前月份已达上限。剩余补录机会：0 次。");
                reserve->setText(QStringLiteral("当月已经补充三次记录，无法继续补充训练记录"));
            } else {
                const int remain = 3 - used;
                limitHint = QStringLiteral("本地模式：本月已补录 %1/3 次，剩余补录机会 %2 次。\n点击进入单天计划编辑后生成补录记录。").arg(used).arg(remain);
                reserve->setText(QStringLiteral("当日未提交记录，点击补充训练记录（本月剩余 %1 次）").arg(remain));
            }
        } else {
            limitHint = QStringLiteral("在线模式：补录次数不限。\n点击进入单天计划编辑后生成补录记录。");
        }

        const int reserveHeight = qMax(160, itemCardMinH * 2 / 1);
        const int reserveFont = qMax(itemButtonPx + 8, itemTitlePx);
        reserve->setMinimumHeight(reserveHeight);
        reserve->setStyleSheet(QString("font-size:%1px;font-weight:800;background:#2f6fa6;color:white;border-radius:20px;padding:18px 20px;")
                                   .arg(reserveFont));
        reserve->setToolTip(limitHint);
        reserve->setEnabled(canSupplement);
        if (!canSupplement) {
            reserve->setStyleSheet(QString("font-size:%1px;font-weight:800;background:#c4cdd5;color:#eef2f5;border-radius:20px;padding:18px 20px;")
                                       .arg(reserveFont));
        }
        QObject::connect(reserve, &QPushButton::clicked, this, &MainWindow::supplementTrainingRecord);
        itemsLayout->addWidget(reserve);
        itemsLayout->addStretch();
        return;
    }

    if (!info.hasPlan && !record) {
        dayHintLabel->setText(info.message);
        itemsLayout->addStretch();
        return;
    }

    if (info.isRestDay) {
        if (record && record->submitted) {
            dayHintLabel->setText(QStringLiteral("休息日已打卡"));
        } else {
            dayHintLabel->setText(QStringLiteral("今日为休息日"));
        }
        itemsLayout->addStretch();
        return;
    }

    if (record && (selectedDate < today || record->submitted || record->isSupplement)) {
        if (record->submitted) {
            if (record->totalMinutes > 0) {
                dayHintLabel->setText(QString("已提交训练记录 | 当日总训练时长：%1 分钟").arg(record->totalMinutes));
            } else {
                dayHintLabel->setText(QStringLiteral("已提交训练记录"));
            }
        } else {
            dayHintLabel->setText(QStringLiteral("<span style='color:#203328;'>已生成训练记录，可继续修改</span> <span style='color:#c95d5d;'>当日计划未提交</span>"));
        }
        QVector<int> displayOrder;
        displayOrder.reserve(record->day.items.size());
        for (int i = 0; i < record->day.items.size(); ++i) {
            const RecordItem &ri = record->day.items[i];
            if (!ri.ignored && !ri.completed) {
                displayOrder.push_back(i);
            }
        }
        for (int i = 0; i < record->day.items.size(); ++i) {
            const RecordItem &ri = record->day.items[i];
            if (ri.ignored || ri.completed) {
                displayOrder.push_back(i);
            }
        }

        for (int ord = 0; ord < displayOrder.size(); ++ord) {
            const int itemIndex = displayOrder[ord];
            const RecordItem &ri = record->day.items[itemIndex];
            QFrame *card = new QFrame(itemsContainer);
            card->setObjectName(QStringLiteral("workoutItemCard"));
            QVBoxLayout *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(26, 16, 18, 16);
            cardLayout->setSpacing(10);
            QLabel *nameLabel = new QLabel(ri.item.name, card);
            nameLabel->setStyleSheet(QString("font-size:%1px;font-weight:800;color:#7a3d14;").arg(itemTitlePx));
            nameLabel->setAlignment(Qt::AlignCenter);
            nameLabel->setWordWrap(true);
            QFrame *titleCard = new QFrame(card);
            titleCard->setStyleSheet("background:#ffd9b3;border:none;border-radius:14px;");
            QVBoxLayout *titleCardLayout = new QVBoxLayout(titleCard);
            titleCardLayout->setContentsMargins(14, 8, 14, 8);
            titleCardLayout->addWidget(nameLabel);
            QLabel *metaLabel = new QLabel(
                QString("间歇 %1s    热身 %2    正式 %3")
                    .arg(ri.item.restSeconds)
                    .arg(setListSummary(ri.item.warmupSets))
                    .arg(setListSummary(ri.item.workSets)),
                card);
            metaLabel->setStyleSheet(QString("font-size:%1px;color:#6d7a72;").arg(itemMetaPx));
            metaLabel->setAlignment(Qt::AlignCenter);
            metaLabel->setWordWrap(true);
            QLabel *stateLabel = new QLabel(card);
            if (ri.ignored) {
                stateLabel->setText(QStringLiteral("已忽略"));
                stateLabel->setStyleSheet(QString("font-size:%1px;color:#7a7f84;background:#eef1f4;border-radius:12px;padding:6px 10px;").arg(itemChipPx));
            } else if (ri.completed) {
                stateLabel->setText(QStringLiteral("已打卡"));
                stateLabel->setStyleSheet(QString("font-size:%1px;color:#2f8f46;background:#edf8f0;border-radius:12px;padding:6px 10px;").arg(itemChipPx));
            } else {
                if (record->isSupplement) {
                    stateLabel->setText(QStringLiteral("补录"));
                    stateLabel->setStyleSheet(QString("font-size:%1px;color:#1f4f84;background:#eaf3fd;border-radius:12px;padding:6px 10px;").arg(itemChipPx));
                } else {
                    stateLabel->setText(QStringLiteral("未完成"));
                    stateLabel->setStyleSheet(QString("font-size:%1px;color:#b06a2b;background:#fff3e7;border-radius:12px;padding:6px 10px;").arg(itemChipPx));
                }
            }
            card->setMinimumHeight(itemCardMinH);
            cardLayout->addWidget(titleCard);
            cardLayout->addWidget(metaLabel);
            cardLayout->addWidget(stateLabel, 0, Qt::AlignLeft);

            QPushButton *previewBtn = new QPushButton(QStringLiteral("查看预览"), card);
            previewBtn->setStyleSheet(QString("font-size:%1px;background:#3d7db8;color:white;border-radius:12px;padding:10px 14px;").arg(itemButtonPx));
            cardLayout->addWidget(previewBtn);
            QObject::connect(previewBtn, &QPushButton::clicked, this, [this, itemIndex]() { openTodayItemPreview(itemIndex); });
            itemsLayout->addWidget(card);
        }
        itemsLayout->addStretch();
        return;
    }

    MasterPlan *plan = activePlanForCurrentUser();
    if (!plan || info.dayPlanIndex < 0 || info.dayPlanIndex >= plan->dayPlans.size()) {
        dayHintLabel->setText(QStringLiteral("计划索引异常"));
        itemsLayout->addStretch();
        return;
    }

    DayPlan &day = plan->dayPlans[info.dayPlanIndex];
    if (record && !record->submitted && selectedDate == today) {
        dayHintLabel->setText(QStringLiteral("<span style='color:#203328;'>当日计划已加载，加油大老鼠！  </span> <span style='color:#c95d5d;'>     当日计划未提交</span>"));
    } else {
        dayHintLabel->setText(QStringLiteral("%1 | 默认%2分钟")
                                  .arg(day.title)
                                  .arg(day.defaultMinutes));
    }

    QListWidget *dayItemList = new QListWidget(itemsContainer);
    enableMobileSingleFingerScroll(dayItemList);
    dayItemList->setStyleSheet("QListWidget{border:2px solid #8ea497;border-radius:14px;background:#f8fbf9;padding:6px;}"
                               "QListWidget::item{margin:8px 4px;border-bottom:1px solid #e3ece6;}"
                               "QListWidget::item:selected{background:rgba(61,125,184,80);}");

    const bool canDragSort = (selectedDate >= today) && (!record || !record->submitted);
    dayItemList->setDragDropMode(QAbstractItemView::NoDragDrop);
    dayItemList->setDefaultDropAction(Qt::MoveAction);
    dayItemList->setDragEnabled(false);
    dayItemList->setAcceptDrops(false);
    dayItemList->setDropIndicatorShown(false);

    QVector<int> displayOrder;
    displayOrder.reserve(day.items.size());
    for (int i = 0; i < day.items.size(); ++i) {
        const bool itemIgnored = (record && i < record->day.items.size() && record->day.items[i].ignored);
        const bool itemCompleted = (record && i < record->day.items.size() && record->day.items[i].completed);
        if (!itemIgnored && !itemCompleted) {
            displayOrder.push_back(i);
        }
    }
    for (int i = 0; i < day.items.size(); ++i) {
        const bool itemIgnored = (record && i < record->day.items.size() && record->day.items[i].ignored);
        const bool itemCompleted = (record && i < record->day.items.size() && record->day.items[i].completed);
        if (itemIgnored || itemCompleted) {
            displayOrder.push_back(i);
        }
    }

    for (int ord = 0; ord < displayOrder.size(); ++ord) {
        const int itemIndex = displayOrder[ord];
        const WorkoutItem &item = day.items[itemIndex];
        QListWidgetItem *rowItem = new QListWidgetItem(dayItemList);
        rowItem->setData(Qt::UserRole, itemIndex);
        rowItem->setSizeHint(QSize(0, itemCardMinH + 24));
        dayItemList->addItem(rowItem);

        QFrame *card = new QFrame(dayItemList);
        card->setObjectName(QStringLiteral("workoutItemCard"));
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(28, 16, 18, 16);
        cardLayout->setSpacing(10);

        const bool itemIgnored = (record && itemIndex < record->day.items.size() && record->day.items[itemIndex].ignored);
        const bool itemCompleted = (record && itemIndex < record->day.items.size() && record->day.items[itemIndex].completed);

        QLabel *nameLabel = new QLabel(item.name, card);
        nameLabel->setStyleSheet(QString("font-size:%5px;font-weight:800;color:#7a3d14;").arg(itemTitlePx));
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(true);
        QLabel *stateLabel = new QLabel(card);
        if (itemIgnored) {
            stateLabel->setText(QStringLiteral("已忽略"));
            stateLabel->setStyleSheet(QString("font-size:%1px;color:#7a7f84;background:#eef1f4;border-radius:12px;padding:6px 10px;").arg(itemChipPx));
        } else if (itemCompleted) {
            stateLabel->setText(QStringLiteral("已打卡"));
            stateLabel->setStyleSheet(QString("font-size:%1px;color:#2f8f46;background:#edf8f0;border-radius:12px;padding:6px 10px;").arg(itemChipPx));
        } else {
            stateLabel->hide();
        }
        QHBoxLayout *titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(10);
        titleRow->addWidget(nameLabel, 1);
        titleRow->addWidget(stateLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
        QFrame *titleCard = new QFrame(card);
        titleCard->setStyleSheet("background:#ffd9b3;border:none;border-radius:14px;");
        QVBoxLayout *titleCardLayout = new QVBoxLayout(titleCard);
        titleCardLayout->setContentsMargins(14, 8, 14, 8);
        titleCardLayout->addLayout(titleRow);
        QLabel *metaLabel = new QLabel(
            QString("间歇 %1s    热身 %2    正式 %3")
                .arg(item.restSeconds)
                .arg(setListSummary(item.warmupSets))
                .arg(setListSummary(item.workSets)),
            card);
        metaLabel->setStyleSheet(QString("font-size:%2px;color:#6d7a72;").arg(itemMetaPx));
        metaLabel->setAlignment(Qt::AlignCenter);
        metaLabel->setWordWrap(true);
        cardLayout->addWidget(titleCard);
        cardLayout->addWidget(metaLabel);

        QHBoxLayout *ops = new QHBoxLayout();
        QHBoxLayout *leftOps = new QHBoxLayout();
        leftOps->setSpacing(10);
        QPushButton *previewBtn = new QPushButton(QStringLiteral("预览"), card);
        QPushButton *editBtn = new QPushButton(QStringLiteral("修改"), card);
        QPushButton *delBtn = new QPushButton(QStringLiteral("删除"), card);
        QPushButton *ignoreBtn = new QPushButton(itemIgnored ? QStringLiteral("重新添加该项目") : QStringLiteral("忽略该项目"), card);
        previewBtn->setStyleSheet(QString("background:#3d7db8;color:white;border:3px solid #356ea2;border-radius:20px;padding:20px 90px;font-size:%4px;").arg(itemButtonPx));
        editBtn->setStyleSheet(QString("background:#eef7f0;color:#1e4d3c;border:3px solid #d4e8d8;border-radius:20px;padding:20px 45px;font-size:%3px;").arg(itemButtonPx));
        delBtn->setStyleSheet(QString("background:#fff1f1;color:#a14f4f;border:3px solid #f0d4d4;border-radius:20px;padding:20px 45px;font-size:%3px;").arg(itemButtonPx));
        ignoreBtn->setStyleSheet(QString("background:#f8f8f8;color:#666666;border:3px solid #d0d0d0;border-radius:20px;padding:20px 90px;font-size:%3px;").arg(itemButtonPx));

        const bool lockActionButtons = !canDragSort || itemIgnored || itemCompleted;
        if (lockActionButtons) {
            editBtn->setEnabled(false);
            delBtn->setEnabled(false);
            editBtn->setStyleSheet(QString("background:#dce4de;color:#9aa7a0;border:1px solid #d0d8d2;border-radius:12px;padding:8px 16px;font-size:%1px;").arg(itemButtonPx));
            delBtn->setStyleSheet(QString("background:#e5dfdf;color:#a7a1a1;border:1px solid #d9d2d2;border-radius:12px;padding:8px 16px;font-size:%1px;").arg(itemButtonPx));
        }
        if (!canDragSort) {
            ignoreBtn->setEnabled(false);
            ignoreBtn->setStyleSheet(QString("background:#f0f0f0;color:#b0b0b0;border:1px solid #d0d0d0;border-radius:12px;padding:8px 16px;font-size:%1px;").arg(itemButtonPx));
        }

        leftOps->addWidget(previewBtn);
        leftOps->addWidget(editBtn);
        leftOps->addWidget(delBtn);
        ops->addLayout(leftOps);
        ops->addStretch();
        ops->addWidget(ignoreBtn, 0, Qt::AlignRight);
        cardLayout->addLayout(ops);

        QObject::connect(previewBtn, &QPushButton::clicked, this, [this, rowItem]() {
            const int idx = rowItem ? rowItem->data(Qt::UserRole).toInt() : -1;
            if (idx >= 0) {
                openTodayItemPreview(idx);
            }
        });
        QObject::connect(editBtn, &QPushButton::clicked, this, [this, rowItem]() {
            const int idx = rowItem ? rowItem->data(Qt::UserRole).toInt() : -1;
            if (idx >= 0) {
                editTodayPlanItem(idx);
            }
        });
        QObject::connect(delBtn, &QPushButton::clicked, this, [this, rowItem]() {
            const int idx = rowItem ? rowItem->data(Qt::UserRole).toInt() : -1;
            if (idx >= 0) {
                deleteTodayPlanItem(idx);
            }
        });
        QObject::connect(ignoreBtn, &QPushButton::clicked, this, [this, rowItem]() {
            const int idx = rowItem ? rowItem->data(Qt::UserRole).toInt() : -1;
            if (idx >= 0) {
                ignoreTodayPlanItem(idx);
            }
        });
        dayItemList->setItemWidget(rowItem, card);
    }

    itemsLayout->addWidget(dayItemList);
}

// 功能：处理相关逻辑。

void MainWindow::openPlanManagerDialog()
{
// 该文件由重构生成：保持原函数逻辑不变，仅做文件分层。
    User *u = currentUser();
    if (!u) {
        return;
    }

    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("总计划设置"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMargins(layout, dialog);
    QListWidget *list = new QListWidget(&dialog);
    enableMobileSingleFingerScroll(list);
    list->setStyleSheet(
        QString("QListWidget{font-size:%1px;border:1px solid #d3e1d8;border-radius:16px;background:white;padding:10px;}"
                "QListWidget::item{min-height:%2px;padding:10px 8px;}"
                "QListWidget::item:selected{background:#e8f5ec;border-radius:12px;color:#173326;}")
            .arg(gUiTuning.planManagerListFont + 4)
            .arg(gUiTuning.dialogListItemHeight + 28));

    auto ownedPlanIndexes = [&]() {
        QVector<int> indexes;
        for (int i = 0; i < store.plans.size(); ++i) {
            if (store.plans[i].ownerUserId == u->id) {
                indexes.push_back(i);
            }
        }
        return indexes;
    };

    auto refresh = [&]() {
        list->clear();
        const QVector<int> indexes = ownedPlanIndexes();
        if (indexes.isEmpty()) {
            QListWidgetItem *item = new QListWidgetItem(list);
            item->setSizeHint(QSize(0, qMax(180, gUiTuning.dialogListItemHeight + 70)));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setData(Qt::UserRole, QStringLiteral("quickAddPreset"));
            list->addItem(item);

            QFrame *host = new QFrame(list);
            host->setStyleSheet(QString("background:%1;border:none;border-radius:18px;")
                                    .arg(themePrimary));
            QVBoxLayout *hostLayout = new QVBoxLayout(host);
            hostLayout->setContentsMargins(16, 14, 16, 14);
            hostLayout->setSpacing(8);

            QLabel *label = new QLabel(QStringLiteral("添加一个默认预设计划"), host);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet(QString("font-size:%1px;font-weight:900;color:white;background:transparent;")
                                     .arg(gUiTuning.planManagerActionFont + 8));
            hostLayout->addWidget(label);
            list->setItemWidget(item, host);
            return;
        }

        for (int i = 0; i < indexes.size(); ++i) {
            const int idx = indexes[i];
            const MasterPlan &p = store.plans[idx];
            const QString activeMark = (u->activePlanId == p.id) ? QStringLiteral("[当前]") : QString();

            QListWidgetItem *item = new QListWidgetItem(list);
            const int minHeight = gUiTuning.dialogListItemHeight + 420 + p.dayPlans.size() * 58;
            item->setSizeHint(QSize(0, minHeight));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            list->addItem(item);

            QWidget *host = new QWidget(list);
            QVBoxLayout *hostLayout = new QVBoxLayout(host);
            hostLayout->setContentsMargins(14, 12, 14, 12);
            hostLayout->setSpacing(10);
            host->setStyleSheet("background:#edf2ef;border:none;border-radius:16px;");

            QFrame *titleCard = new QFrame(host);
            titleCard->setStyleSheet("background:#ffffff;border:none;border-radius:12px;");
            QVBoxLayout *titleLayout = new QVBoxLayout(titleCard);
            titleLayout->setContentsMargins(14, 12, 14, 12);
            titleLayout->setSpacing(8);
            QLabel *titleLabel = new QLabel(QString("%1 %2").arg(activeMark).arg(p.name), titleCard);
            titleLabel->setWordWrap(true);
            titleLabel->setAlignment(Qt::AlignCenter);
            titleLabel->setStyleSheet(QString("font-size:%1px;font-weight:900;color:#1f392d;").arg(gUiTuning.planManagerListFont + 8));
            QLabel *metaLabel = new QLabel(QString("练%1休%2    开始日期：%3")
                                               .arg(p.trainDays)
                                               .arg(p.restDays)
                                               .arg(p.startDate.toString("yyyy年M月d日")),
                                           titleCard);
            metaLabel->setAlignment(Qt::AlignCenter);
            metaLabel->setStyleSheet(QString("font-size:%1px;color:#5f6f66;").arg(qMax(20, gUiTuning.planManagerListFont - 4)));
            titleLayout->addWidget(titleLabel);
            titleLayout->addWidget(metaLabel);
            hostLayout->addWidget(titleCard);

            const QStringList dayColors = {
                QStringLiteral("#ffdcca"),
                QStringLiteral("#d8e9ff"),
                QStringLiteral("#d6f5d8"),
                QStringLiteral("#efd9ff"),
                QStringLiteral("#fff0bf"),
                QStringLiteral("#d7f4f4")
            };

            if (p.dayPlans.isEmpty()) {
                QFrame *emptyCard = new QFrame(host);
                emptyCard->setStyleSheet("background:#ffffff;border:1px dashed #d8e1dc;border-radius:12px;");
                QHBoxLayout *emptyLayout = new QHBoxLayout(emptyCard);
                emptyLayout->setContentsMargins(14, 10, 14, 10);
                QLabel *emptyLabel = new QLabel(QStringLiteral("暂无单天计划"), emptyCard);
                emptyLabel->setStyleSheet(QString("font-size:%1px;color:#7b8780;").arg(qMax(18, gUiTuning.planManagerListFont - 8)));
                emptyLayout->addWidget(emptyLabel);
                hostLayout->addWidget(emptyCard);
            } else {
                for (int d = 0; d < p.dayPlans.size(); ++d) {
                    const DayPlan &day = p.dayPlans[d];
                    QStringList itemNames;
                    for (const WorkoutItem &wi : day.items) {
                        itemNames << wi.name;
                        if (itemNames.size() >= 2) {
                            break;
                        }
                    }
                    const QString preview = itemNames.isEmpty() ? QStringLiteral("无动作") : itemNames.join(QStringLiteral("、"));

                    QFrame *dayCard = new QFrame(host);
                    dayCard->setStyleSheet(QString("background:%1;border:none;border-radius:12px;")
                                               .arg(dayColors[d % dayColors.size()]));
                    QHBoxLayout *dayLayout = new QHBoxLayout(dayCard);
                    dayLayout->setContentsMargins(14, 10, 14, 10);
                    QLabel *dayLabel = new QLabel(QString("第%1天 %2：%3").arg(d + 1).arg(day.title).arg(preview), dayCard);
                    dayLabel->setWordWrap(true);
                    dayLabel->setStyleSheet(QString("font-size:%1px;font-weight:800;color:#33443a;").arg(qMax(28, gUiTuning.planManagerListFont + 4)));
                    dayLayout->addStretch(1);
                    dayLayout->addWidget(dayLabel, 9);
                    hostLayout->addWidget(dayCard);
                }
            }

            list->setItemWidget(item, host);
        }
    };

    auto refreshCurrentDayPlanImmediately = [&]() {
        if (calendar) {
            selectedDate = calendar->selectedDate();
        }
        ensureRecordFromSelectedDayPlan();
        rebuildCalendarFormats();
        rebuildDayView();
    };

    auto addDefaultPresetPlan = [&]() {
        QString createdPlanId;
        if (!store.addMouseDefaultPresetPlan(u->id, &createdPlanId, true)) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("默认预设计划添加失败。"));
            return;
        }
        store.save();
        persistCurrentUserPackage1();
        const int createdIdx = findPlanIndex(store.plans, createdPlanId);
        if (createdIdx >= 0) {
            syncPlanToRemote(store.plans[createdIdx]);
        }
        refresh();
        refreshCurrentDayPlanImmediately();
    };

    refresh();

    QHBoxLayout *ops = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QStringLiteral("添加总计划"), &dialog);
    QPushButton *importBtn = new QPushButton(QStringLiteral("导入"), &dialog);
    QPushButton *exportBtn = new QPushButton(QStringLiteral("导出"), &dialog);
    QPushButton *editBtn = new QPushButton(QStringLiteral("编辑总计划"), &dialog);
    QPushButton *delBtn = new QPushButton(QStringLiteral("删除总计划"), &dialog);
    QPushButton *setActiveBtn = new QPushButton(QStringLiteral("设为当前总计划"), &dialog);
    const QString largeActionBtnStyle = QString("font-size:%1px;min-height:%2px;font-weight:800;padding:12px 14px;border-radius:14px;")
                                            .arg(gUiTuning.planManagerActionFont + 4)
                                            .arg(gUiTuning.planManagerActionHeight + 12);
    addBtn->setStyleSheet(largeActionBtnStyle + "background:#2f8f46;color:white;");
    importBtn->setStyleSheet(largeActionBtnStyle + "background:#5f7f66;color:white;");
    exportBtn->setStyleSheet(largeActionBtnStyle + "background:#3d7db8;color:white;");
    editBtn->setStyleSheet(largeActionBtnStyle + "background:#3d7db8;color:white;");
    delBtn->setStyleSheet(largeActionBtnStyle + "background:#c95d5d;color:white;");
    setActiveBtn->setStyleSheet(largeActionBtnStyle + "background:#5f7f66;color:white;");
    ops->addWidget(addBtn);
    ops->addWidget(importBtn);
    ops->addWidget(exportBtn);
    ops->addWidget(editBtn);
    ops->addWidget(delBtn);
    ops->addWidget(setActiveBtn);
    ops->addStretch();

    QDialogButtonBox *closeBtns = new QDialogButtonBox(Qt::Horizontal, &dialog);
    QPushButton *closeBtn = closeBtns->addButton(QStringLiteral("关闭"), QDialogButtonBox::RejectRole);
    closeBtn->setMinimumWidth(260);
    closeBtns->setStyleSheet(QString("QPushButton{font-size:%1px;min-height:%2px;font-weight:800;padding:10px 14px;border-radius:14px;}")
                                 .arg(gUiTuning.planManagerActionFont + 4)
                                 .arg(gUiTuning.planManagerActionHeight + 12));
    QObject::connect(closeBtns, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QObject::connect(addBtn, &QPushButton::clicked, &dialog, [&]() {
        MasterPlan p;
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.ownerUserId = u->id;
        p.name = QStringLiteral("新总计划");
        p.startDate = QDate::currentDate();
        if (editMasterPlanDialog(&dialog, p)) {
            store.plans.push_back(p);
            if (u->activePlanId.isEmpty()) {
                u->activePlanId = p.id;
            }
            store.save();
            persistCurrentUserPackage1();
            syncPlanToRemote(p);
            refresh();
            refreshCurrentDayPlanImmediately();
        }
    });

    auto runLocalImport = [&]() {
#ifdef Q_OS_ANDROID
        const bool launched = launchAndroidDocumentPicker(&dialog,
                                                          kPlanImportRequestCode,
                                                          [&](const QString &uriText) {
            if (uriText.trimmed().isEmpty()) {
                return;
            }
            const QAndroidJniObject uri = parseUriString(uriText);
            const QString localTempFile = copyDocumentUriToTempFile(uri,
                                                                     QStringLiteral("plan_import"),
                                                                     QStringLiteral("mp2plan"));
            if (localTempFile.trimmed().isEmpty()) {
                QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("导入失败：无法读取所选文件。"));
                return;
            }

            QString createdPlanId;
            if (!store.importPlanPackage2(u->id, localTempFile, &createdPlanId)) {
                QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("导入失败：文件类型或内容校验未通过。"));
                return;
            }
            store.save();
            persistCurrentUserPackage1();
            refresh();
            refreshCurrentDayPlanImmediately();
            QMessageBox::information(&dialog, QStringLiteral("提示"), QStringLiteral("导入成功，已追加到当前总计划列表。"));
        },
                                                          QStringLiteral("*/*"));
        if (!launched) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("无法打开文件管理器，请检查权限设置。"));
        }
        return;
#endif

        QString startDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (startDir.trimmed().isEmpty()) {
            startDir = QDir::homePath();
        }
        const QString filePath = QFileDialog::getOpenFileName(&dialog,
                                                              QStringLiteral("导入总计划数据包"),
                                                              startDir,
                                                              QStringLiteral("MousePlan 数据包2 (*.mp2plan *.json)"));
        if (filePath.trimmed().isEmpty()) {
            return;
        }
        QString createdPlanId;
        if (!store.importPlanPackage2(u->id, filePath, &createdPlanId)) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("导入失败：文件类型或内容校验未通过。"));
            return;
        }
        store.save();
        persistCurrentUserPackage1();
        refresh();
        refreshCurrentDayPlanImmediately();
        QMessageBox::information(&dialog, QStringLiteral("提示"), QStringLiteral("导入成功，已追加到当前总计划列表。"));
    };

    QObject::connect(importBtn, &QPushButton::clicked, &dialog, [&]() {
        if (u->isLocalAccount) {
            runLocalImport();
            return;
        }

        QGraphicsEffect *oldEffect = dialog.graphicsEffect();
        QGraphicsBlurEffect *blur = nullptr;
        if (!oldEffect) {
            blur = new QGraphicsBlurEffect(&dialog);
            blur->setBlurRadius(16.0);
            dialog.setGraphicsEffect(blur);
        }

        QDialog sheet(&dialog);
        sheet.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        sheet.setAttribute(Qt::WA_TranslucentBackground, true);

        const int navH = qMax(120, navBarCard ? navBarCard->height() : 120);
        const int popupW = qMax(380, dialog.width() * 9 / 10);
        const int popupH = qMax(260, (navH * 3) / 2);
        const int popupX = (dialog.width() - popupW) / 2;
        const int popupY = qMax(0, dialog.height() - popupH - 12);
        sheet.setGeometry(popupX, popupY, popupW, popupH);

        QVBoxLayout *root = new QVBoxLayout(&sheet);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        QFrame *panel = new QFrame(&sheet);
        panel->setStyleSheet("background:#f7f7f7;border-top-left-radius:20px;border-top-right-radius:20px;");
        QVBoxLayout *panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(16, 10, 16, 12);
        panelLayout->setSpacing(8);

        QPushButton *localImportBtn = new QPushButton(QStringLiteral("本地导入"), panel);
        QPushButton *onlineImportBtn = new QPushButton(QStringLiteral("线上导入（预留）"), panel);
        QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), panel);
        localImportBtn->setStyleSheet("font-size:23px;min-height:56px;background:#2f6fa6;color:white;border-radius:14px;border:none;");
        onlineImportBtn->setStyleSheet("font-size:23px;min-height:56px;background:#4f8a4f;color:white;border-radius:14px;border:none;");
        cancelBtn->setStyleSheet("font-size:23px;min-height:56px;background:#888;color:white;border-radius:14px;border:none;");

        panelLayout->addWidget(localImportBtn);
        panelLayout->addWidget(onlineImportBtn);
        panelLayout->addWidget(cancelBtn);
        root->addWidget(panel);

        QObject::connect(cancelBtn, &QPushButton::clicked, &sheet, &QDialog::reject);
        QObject::connect(localImportBtn, &QPushButton::clicked, &sheet, [&]() {
            sheet.accept();
            runLocalImport();
        });
        QObject::connect(onlineImportBtn, &QPushButton::clicked, &sheet, [&]() {
            sheet.accept();
            QMessageBox::information(&dialog, QStringLiteral("提示"), QStringLiteral("线上导入接口已预留，后续接入服务器。"));
        });

        sheet.exec();
        if (dialog.graphicsEffect() == blur) {
            dialog.setGraphicsEffect(oldEffect);
        }
    });

    QObject::connect(exportBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = list->currentRow();
        const QVector<int> idxes = ownedPlanIndexes();
        if (row < 0 || row >= idxes.size()) {
            QMessageBox::information(&dialog, QStringLiteral("提示"), QStringLiteral("请先选择要导出的总计划。"));
            return;
        }

#ifdef Q_OS_ANDROID
        const QString suggestedName = QString("%1.mp2plan").arg(store.plans[idxes[row]].name);
        const QString selectedPlanId = store.plans[idxes[row]].id;
        QPointer<QDialog> dialogGuard(&dialog);
        const bool launched = launchAndroidDocumentCreator(&dialog,
                                                           kPlanExportRequestCode,
                                                           suggestedName,
                                                           [this, dialogGuard, selectedPlanId](const QString &uriText) {
            if (uriText.trimmed().isEmpty()) {
                return;
            }

            QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            if (tempRoot.trimmed().isEmpty()) {
                tempRoot = QDir::homePath();
            }
            QDir tmpDir(QDir(tempRoot).filePath(QStringLiteral("MousePlan")));
            if (!tmpDir.exists()) {
                tmpDir.mkpath(QStringLiteral("."));
            }

            const QString tempFilePath = tmpDir.filePath(
                QStringLiteral("plan_export_%1.mp2plan").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")));
            if (!store.exportPlanPackage2(selectedPlanId, tempFilePath)) {
                QWidget *msgParent = dialogGuard ? static_cast<QWidget *>(dialogGuard.data()) : static_cast<QWidget *>(this);
                QMessageBox::warning(msgParent, QStringLiteral("提示"), QStringLiteral("导出失败，请检查数据包内容。"));
                return;
            }

            QFile tempFile(tempFilePath);
            if (!tempFile.open(QIODevice::ReadOnly)) {
                QWidget *msgParent = dialogGuard ? static_cast<QWidget *>(dialogGuard.data()) : static_cast<QWidget *>(this);
                QMessageBox::warning(msgParent, QStringLiteral("提示"), QStringLiteral("导出失败：无法读取临时导出文件。"));
                return;
            }
            const QByteArray payload = tempFile.readAll();
            tempFile.close();

            if (!writeBytesToContentUri(uriText, payload)) {
                QWidget *msgParent = dialogGuard ? static_cast<QWidget *>(dialogGuard.data()) : static_cast<QWidget *>(this);
                QMessageBox::warning(msgParent, QStringLiteral("提示"), QStringLiteral("导出失败，请检查文件管理器写入权限。"));
                return;
            }
            QWidget *msgParent = dialogGuard ? static_cast<QWidget *>(dialogGuard.data()) : static_cast<QWidget *>(this);
            QMessageBox::information(msgParent, QStringLiteral("提示"), QStringLiteral("导出成功。"));
        },
                                                           QStringLiteral("application/json"));
        if (!launched) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("无法打开文件管理器，请检查权限设置。"));
        }
        return;
#endif

        QString startDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (startDir.trimmed().isEmpty()) {
            startDir = QDir::homePath();
        }
        const QString suggest = QString("%1.mp2plan").arg(store.plans[idxes[row]].name);
        const QString outputPath = QFileDialog::getSaveFileName(&dialog,
                                                                QStringLiteral("导出总计划数据包"),
                                                                QDir(startDir).filePath(suggest),
                                                                QStringLiteral("MousePlan 数据包2 (*.mp2plan)"));
        if (outputPath.trimmed().isEmpty()) {
            return;
        }
        if (!store.exportPlanPackage2(store.plans[idxes[row]].id, outputPath)) {
            QMessageBox::warning(&dialog, QStringLiteral("提示"), QStringLiteral("导出失败，请检查存储权限。"));
            return;
        }
        QMessageBox::information(&dialog, QStringLiteral("提示"), QStringLiteral("导出成功：\n%1").arg(outputPath));
    });

    QObject::connect(list, &QListWidget::itemClicked, &dialog, [&](QListWidgetItem *item) {
        if (!item || !item->data(Qt::UserRole).toString().startsWith(QStringLiteral("quickAddPreset"))) {
            return;
        }
        addDefaultPresetPlan();
    });

    QObject::connect(editBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = list->currentRow();
        const QVector<int> idxes = ownedPlanIndexes();
        if (row < 0 || row >= idxes.size()) {
            return;
        }
        MasterPlan copy = store.plans[idxes[row]];
        if (editMasterPlanDialog(&dialog, copy)) {
            store.plans[idxes[row]] = copy;
            store.save();
            persistCurrentUserPackage1();
            syncPlanToRemote(copy);
            refresh();
            refreshCurrentDayPlanImmediately();
        }
    });

    QObject::connect(delBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = list->currentRow();
        const QVector<int> idxes = ownedPlanIndexes();
        if (row < 0 || row >= idxes.size()) {
            return;
        }
        if (!askChineseQuestionDialog(&dialog,
                                      QStringLiteral("确认删除"),
                                      QStringLiteral("确认删除该总计划吗？"),
                                      QStringLiteral("删除"),
                                      QStringLiteral("取消"))) {
            return;
        }
        const MasterPlan deleting = store.plans[idxes[row]];
        store.plans.removeAt(idxes[row]);
        if (u->activePlanId == deleting.id) {
            u->activePlanId.clear();
        }
        store.save();
        persistCurrentUserPackage1();
        refresh();
        refreshCurrentDayPlanImmediately();
    });

    QObject::connect(setActiveBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = list->currentRow();
        const QVector<int> idxes = ownedPlanIndexes();
        if (row < 0 || row >= idxes.size()) {
            return;
        }
        u->activePlanId = store.plans[idxes[row]].id;
        store.save();
        persistCurrentUserPackage1();
        refresh();
        refreshCurrentDayPlanImmediately();
    });

    layout->addWidget(list);
    layout->addLayout(ops);
    layout->addWidget(closeBtns);

    dialog.exec();
    refreshCurrentDayPlanImmediately();
}
/***************************APP主界面的入口 EBGIN***************************** */

// 功能：构造主窗口并初始化核心页面与数据。

