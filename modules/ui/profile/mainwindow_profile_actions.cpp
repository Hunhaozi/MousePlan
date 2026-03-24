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

// 功能：数据加载与准备

void MainWindow::syncProfilePanelLayoutByNav()
{
    if (!profilePanel) {
        return;
    }

    const int navHeight = navBarCard ? navBarCard->height() : 0;
    const int navHint = navBarCard ? navBarCard->sizeHint().height() : 0;
    const int navMin = navBarCard ? navBarCard->minimumHeight() : 0;
    const int safeHeight = qMax(120, qMax(navHeight, qMax(navHint, navMin)));
    if (QFrame *topBlank = profilePanel->findChild<QFrame *>(QStringLiteral("profileTopBlankSpacer"))) {
        topBlank->setFixedHeight(safeHeight);
    }
    profilePanel->updateGeometry();
}



// 功能：处理相关逻辑。

bool MainWindow::promptAgreementDialog()
{
    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("用户服务协议与隐私说明"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMargins(layout, dialog);
    QString agreementText = loadAgreementTextByMode(loginLocalMode);
    if (agreementText.trimmed().isEmpty()) {
        agreementText = QStringLiteral("欢迎使用 MousePlan。\n\n点击“同意”即表示你已阅读并同意协议。\n后续可在 config/agreement_local.md 与 config/agreement_online.md 自定义协议内容。");
    }

    QLabel *titleLabel = new QLabel(loginLocalMode
                                         ? QStringLiteral("本地模式用户服务协议")
                                         : QStringLiteral("在线模式用户服务协议与隐私说明"),
                                     &dialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size:30px;font-weight:900;color:#24362d;");

    QTextEdit *content = new QTextEdit(&dialog);
    content->setReadOnly(true);
    content->setText(agreementText);
    const int agreementOldHeight = qMax(220, dialog.height() - 260);
    const int agreementNewHeight = qMax(180, agreementOldHeight / 2);
    content->setMinimumHeight(agreementNewHeight);
    content->setMaximumHeight(agreementNewHeight);
    content->setStyleSheet("font-size:30px;color:#33443a;background:white;border:1px solid #dbe6de;border-radius:14px;padding:12px;");

    QHBoxLayout *ops = new QHBoxLayout();
    QPushButton *agreeBtn = new QPushButton(QStringLiteral("同意"), &dialog);
    QPushButton *rejectBtn = new QPushButton(QStringLiteral("不同意"), &dialog);
    agreeBtn->setStyleSheet("font-size:24px;min-height:74px;background:#2f8f46;color:white;border-radius:16px;");
    rejectBtn->setStyleSheet("font-size:24px;min-height:74px;background:#c95d5d;color:white;border-radius:16px;");
    ops->addWidget(agreeBtn);
    ops->addWidget(rejectBtn);

    QObject::connect(agreeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(rejectBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    const int agreementButtonHeight = qMax(74, agreeBtn->sizeHint().height());
    layout->addWidget(titleLabel, 0, Qt::AlignHCenter);
    layout->addWidget(content, 0);
    layout->addSpacing(qMax(0, 20 - agreementButtonHeight * 2));
    layout->addLayout(ops);
    layout->addStretch(1);
    return dialog.exec() == QDialog::Accepted;
}

// 功能：处理相关逻辑。

void MainWindow::updateProfileHeader()
{
    User *u = currentUser();
    if (!u) {
        return;
    }
    if (u->nickname.trimmed().isEmpty()) {
        u->nickname = generateRandomNicknameDigits();
        store.save();
    }
    const QString displayName = u->nickname.trimmed();
    if (profileNameLabel) {
        profileNameLabel->setText(displayName);
    }
    if (profileHintLabel) {
        QStringList lines;
        lines << QString("用户名:%1").arg(u->username);
        lines << QString("性别:%1").arg(u->gender.trimmed().isEmpty() ? QStringLiteral("未填写") : u->gender.trimmed());
        lines << QString("年龄:%1").arg(u->age > 0 ? QString::number(u->age) : QStringLiteral("未填写"));
        lines << QString("送给大老鼠的话:%1").arg(u->messageToMouse.trimmed().isEmpty() ? QStringLiteral("还没有留下寄语") : u->messageToMouse.trimmed());
        profileHintLabel->setText(lines.join("\n"));
    }
    updateProfileBannerImage();
    if (profileAvatarLabel) {
        if (!u->avatarImagePath.trimmed().isEmpty() && QFile::exists(u->avatarImagePath)) {
            QPixmap avatarPixmap(u->avatarImagePath);
            if (!avatarPixmap.isNull()) {
                const QPixmap clipped = createCircularAvatarPixmap(avatarPixmap, profileAvatarLabel->size());
                profileAvatarLabel->setText(QString());
                profileAvatarLabel->setPixmap(clipped.isNull() ? avatarPixmap : clipped);
                return;
            }
        }

        // Default avatar from QRC resource.
        const QPixmap defaultAvatarPixmap(kDefaultAvatarResourcePath);
        if (!defaultAvatarPixmap.isNull()) {
            const QPixmap clipped = createCircularAvatarPixmap(defaultAvatarPixmap, profileAvatarLabel->size());
            profileAvatarLabel->setText(QString());
            profileAvatarLabel->setPixmap(clipped.isNull() ? defaultAvatarPixmap : clipped);
            return;
        }

        profileAvatarLabel->setPixmap(QPixmap());
        QString avatar = u->avatarText.trimmed();
        if (avatar.isEmpty()) {
            avatar = displayName.left(1);
        }
        if (avatar.isEmpty()) {
            avatar = QStringLiteral("U");
        }
        profileAvatarLabel->setText(avatar.left(2));
    }
}

// 功能：处理相关逻辑。

void MainWindow::onProfileImageAreaClicked(bool isBannerArea)
{
    QGraphicsEffect *oldEffect = profilePanel ? profilePanel->graphicsEffect() : nullptr;
    QGraphicsBlurEffect *blur = nullptr;
    if (profilePanel && !oldEffect) {
        blur = new QGraphicsBlurEffect(profilePanel);
        blur->setBlurRadius(16.0);
        profilePanel->setGraphicsEffect(blur);
    }

    QDialog chooser(this);
    chooser.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    chooser.setAttribute(Qt::WA_TranslucentBackground, true);

    const int popupW = qMax(360, width() * 9 / 10);
    const int popupH = qMax(220, height() / 5);
    const int popupX = (width() - popupW) / 2;
    const int popupY = qMax(0, height() - popupH - 10);
    chooser.setGeometry(popupX, popupY, popupW, popupH);

    QVBoxLayout *root = new QVBoxLayout(&chooser);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *sheet = new QFrame(&chooser);
    sheet->setStyleSheet("background:#f7f7f7;border-top-left-radius:22px;border-top-right-radius:22px;");
    sheet->setFixedHeight(popupH);
    QVBoxLayout *layout = new QVBoxLayout(sheet);
    layout->setContentsMargins(22, 12, 22, 12);
    layout->setSpacing(10);

    QLabel *title = new QLabel(sheet);
    title->setAlignment(Qt::AlignCenter);
    title->setFixedHeight(70);
    title->setStyleSheet("background:transparent;font-size:36px;font-weight:900;");
    const QString titleText = isBannerArea ? QStringLiteral("顶部照片") : QStringLiteral("头像");
    title->setText(titleText);
    title->setStyleSheet(QString("background:transparent;font-size:36px;font-weight:900;color:%1;").arg(QColor(themePrimary).darker(120).name()));

    QPushButton *viewBtn = new QPushButton(QStringLiteral("查看图片"), sheet);
    QPushButton *pickBtn = new QPushButton(QStringLiteral("从相册中选择"), sheet);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), sheet);
    viewBtn->setStyleSheet("font-size:23px;min-height:56px;background:#2f6fa6;color:white;border-radius:14px;border:none;");
    pickBtn->setStyleSheet("font-size:23px;min-height:56px;background:#4f8a4f;color:white;border-radius:14px;border:none;");
    cancelBtn->setStyleSheet("font-size:23px;min-height:56px;background:#888;color:white;border-radius:14px;border:none;");

    layout->addWidget(title);
    layout->addWidget(viewBtn);
    layout->addWidget(pickBtn);
    layout->addWidget(cancelBtn);
    root->addWidget(sheet);

    QObject::connect(cancelBtn, &QPushButton::clicked, &chooser, &QDialog::reject);
    QObject::connect(pickBtn, &QPushButton::clicked, &chooser, [&]() {
        chooser.accept();
        pickProfileImageFromAlbum(isBannerArea);
    });
    QObject::connect(viewBtn, &QPushButton::clicked, &chooser, [&]() {
        chooser.accept();
        const User *u = currentUser();
        if (!u) {
            return;
        }
        QString path = isBannerArea ? u->profileCoverImagePath.trimmed() : u->avatarImagePath.trimmed();
        QPixmap pix;
        if (!path.isEmpty() && QFile::exists(path)) {
            pix.load(path);
        }
        if (pix.isNull()) {
            pix.load(isBannerArea ? kDefaultBannerResourcePath : kDefaultAvatarResourcePath);
        }
        if (pix.isNull()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前没有可查看的图片。"));
            return;
        }
        showImagePreviewDialog(pix, isBannerArea ? QStringLiteral("profile_cover") : QStringLiteral("avatar"));
    });

    chooser.exec();

    if (profilePanel && profilePanel->graphicsEffect() == blur) {
        profilePanel->setGraphicsEffect(oldEffect);
    }
}

// 功能：处理相关逻辑。

void MainWindow::pickProfileImageFromAlbum(bool isBannerArea)
{
    User *u = currentUser();
    if (!u) {
        return;
    }

#ifdef Q_OS_ANDROID
    if (launchAndroidImagePicker(this,
                                 kImagePickRequestCode,
                                 [this, isBannerArea](const QString &pickedPath) {
                                     if (pickedPath.trimmed().isEmpty()) {
                                         return;
                                     }
                                     User *pickedUser = currentUser();
                                     if (!pickedUser) {
                                         return;
                                     }
                                     if (isBannerArea) {
                                         pickedUser->profileCoverImagePath = pickedPath;
                                     } else {
                                         pickedUser->avatarImagePath = pickedPath;
                                     }
                                     store.save();
                                     updateProfileHeader();
                                 })) {
        return;
    }
#endif

    QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (startDir.trimmed().isEmpty()) {
        startDir = QDir::homePath();
    }
    QFileDialog picker(this,
                       QStringLiteral("从相册中选择图片"),
                       startDir,
                       QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
    picker.setFileMode(QFileDialog::ExistingFile);
    picker.setOption(QFileDialog::DontUseNativeDialog, false);
    QString file;
    if (picker.exec() == QDialog::Accepted) {
        const QStringList files = picker.selectedFiles();
        if (!files.isEmpty()) {
            file = files.first();
        }
    }
    if (file.isEmpty()) {
        return;
    }

    if (isBannerArea) {
        u->profileCoverImagePath = file;
    } else {
        u->avatarImagePath = file;
    }
    store.save();
    updateProfileHeader();
}

// 功能：处理相关逻辑。

void MainWindow::showImagePreviewDialog(const QPixmap &pixmap, const QString &suggestedBaseName)
{
    if (pixmap.isNull()) {
        return;
    }

    QDialog viewer(this);
    viewer.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    viewer.setModal(true);
    viewer.setWindowState(viewer.windowState() | Qt::WindowFullScreen);
    viewer.setStyleSheet("background:black;");

    QVBoxLayout *layout = new QVBoxLayout(&viewer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *imageLabel = new QLabel(&viewer);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setStyleSheet("background:black;");
    layout->addWidget(imageLabel, 1);

    auto refreshImage = [&]() {
        if (viewer.size().isEmpty()) {
            return;
        }
        imageLabel->setPixmap(pixmap.scaled(viewer.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    };
    refreshImage();

    class PreviewEventFilter final : public QObject {
    public:
        PreviewEventFilter(QDialog *targetDialog,
                           QLabel *targetLabel,
                           const QPixmap &src,
                           const QString &baseName,
                           MainWindow *owner)
            : QObject(targetDialog)
            , dialog(targetDialog)
            , label(targetLabel)
            , original(src)
            , fileBase(baseName)
            , window(owner)
        {
            timer.setSingleShot(true);
            timer.setInterval(520);
            QObject::connect(&timer, &QTimer::timeout, dialog, [this]() {
                if (!pressed || longPressTriggered || !window) {
                    return;
                }
                longPressTriggered = true;

                QGraphicsEffect *oldEffect = dialog->graphicsEffect();
                QGraphicsBlurEffect *blur = nullptr;
                if (!oldEffect) {
                    blur = new QGraphicsBlurEffect(dialog);
                    blur->setBlurRadius(16.0);
                    dialog->setGraphicsEffect(blur);
                }

                QDialog sheet(dialog);
                sheet.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
                sheet.setAttribute(Qt::WA_TranslucentBackground, true);
                sheet.setModal(true);

                QVBoxLayout *sheetRoot = new QVBoxLayout(&sheet);
                sheetRoot->setContentsMargins(0, 0, 0, 0);
                sheetRoot->setSpacing(0);

                QFrame *panel = new QFrame(&sheet);
                panel->setStyleSheet("background:#f7f7f7;border-top-left-radius:20px;border-top-right-radius:20px;");
                QVBoxLayout *panelLayout = new QVBoxLayout(panel);
                panelLayout->setContentsMargins(18, 18, 18, 24);
                panelLayout->setSpacing(12);

                QPushButton *saveBtn = new QPushButton(QStringLiteral("保存图像置本地"), panel);
                QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), panel);
                saveBtn->setStyleSheet("font-size:24px;min-height:72px;background:#2f8f46;color:white;border-radius:14px;");
                cancelBtn->setStyleSheet("font-size:24px;min-height:72px;background:#888;color:white;border-radius:14px;");

                panelLayout->addWidget(saveBtn);
                panelLayout->addWidget(cancelBtn);
                sheetRoot->addWidget(panel);

                const int popupW = qMax(360, dialog->width() * 9 / 10);
                const int popupH = qMax(220, dialog->height() / 5);
                const int popupX = (dialog->width() - popupW) / 2;
                const int popupY = qMax(0, dialog->height() - popupH - 10);
                sheet.setGeometry(popupX, popupY, popupW, popupH);

                QObject::connect(cancelBtn, &QPushButton::clicked, &sheet, &QDialog::reject);
                QObject::connect(saveBtn, &QPushButton::clicked, &sheet, [this, &sheet]() {
                    QString picturesRoot = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
                    if (picturesRoot.trimmed().isEmpty()) {
                        picturesRoot = QDir::homePath();
                    }
                    QDir outDir(picturesRoot + "/MousePlan");
                    if (!outDir.exists()) {
                        outDir.mkpath(".");
                    }
                    const QString fileName = QString("%1_%2.jpg")
                                                 .arg(fileBase)
                                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
                    const QString outputPath = outDir.filePath(fileName);
                    if (original.save(outputPath, "JPG", 95)) {
                        QMessageBox::information(dialog, QStringLiteral("保存成功"), QStringLiteral("已保存到：\n%1").arg(outputPath));
                    } else {
                        QMessageBox::warning(dialog, QStringLiteral("保存失败"), QStringLiteral("无法保存图片，请检查存储权限。"));
                    }
                    sheet.accept();
                });
                sheet.exec();

                if (dialog->graphicsEffect() == blur) {
                    dialog->setGraphicsEffect(oldEffect);
                }
            });
        }

// 功能：处理eventFilter相关业务逻辑。
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (watched == dialog && event->type() == QEvent::Resize) {
                if (label) {
                    label->setPixmap(original.scaled(dialog->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                return false;
            }

            if ((watched == dialog || watched == label) && event->type() == QEvent::MouseButtonPress) {
                pressed = true;
                longPressTriggered = false;
                timer.start();
                return true;
            }

            if ((watched == dialog || watched == label) && event->type() == QEvent::MouseButtonRelease) {
                pressed = false;
                timer.stop();
                if (!longPressTriggered && dialog) {
                    dialog->accept();
                }
                longPressTriggered = false;
                return true;
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        QDialog *dialog = nullptr;
        QLabel *label = nullptr;
        QPixmap original;
        QString fileBase;
        MainWindow *window = nullptr;
        QTimer timer;
        bool pressed = false;
        bool longPressTriggered = false;
    };

    PreviewEventFilter *filter = new PreviewEventFilter(&viewer, imageLabel, pixmap, suggestedBaseName, this);
    viewer.installEventFilter(filter);
    imageLabel->installEventFilter(filter);

    viewer.exec();
}

// 功能：处理相关逻辑。

void MainWindow::updateProfileBannerImage()
{
    if (!profileBannerLabel) {
        return;
    }
    const User *u = currentUser();
    QPixmap bannerPixmap;
    if (u && !u->profileCoverImagePath.trimmed().isEmpty() && QFile::exists(u->profileCoverImagePath)) {
        bannerPixmap.load(u->profileCoverImagePath);
    }
    if (bannerPixmap.isNull()) {
        bannerPixmap.load(kDefaultBannerResourcePath);
    }

    if (bannerPixmap.isNull()) {
        profileBannerLabel->setText(QStringLiteral("点击设置顶部图片"));
        profileBannerLabel->setPixmap(QPixmap());
        profileBannerLabel->setStyleSheet(QString("background:%1;color:#f0f0f0;border:none;border-radius:16px;font-size:20px;")
                                          .arg(QColor(themeSoft).lighter(108).name()));
        return;
    }

    profileBannerLabel->setText(QString());
    const QColor bannerBg = QColor(themeSoft).lighter(108);
    profileBannerLabel->setStyleSheet(QString("background:%1;color:#f0f0f0;border:none;border-radius:16px;font-size:20px;")
                                      .arg(bannerBg.name()));
    if (!profileBannerLabel->size().isValid() || profileBannerLabel->width() <= 0 || profileBannerLabel->height() <= 0) {
        profileBannerLabel->setPixmap(bannerPixmap);
        return;
    }
    const QPixmap softened = createSoftEdgeBannerPixmap(bannerPixmap,
                                                        profileBannerLabel->size(),
                                                        bannerBg,
                                                        16);
    profileBannerLabel->setPixmap(softened.isNull() ? bannerPixmap : softened);
}

// 功能：处理相关逻辑。

void MainWindow::editCurrentUserProfile()
{
// 该文件由重构生成：保持原函数逻辑不变，仅做文件分层。
    User *u = currentUser();
    if (!u) {
        return;
    }

    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("修改个人信息"));

    const QString lightTheme = QColor(themeSoft).lighter(106).name();
    const int navHeight = navBarCard ? navBarCard->height() : 120;

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QFrame *topBlank = new QFrame(&dialog);
    topBlank->setFixedHeight(navHeight);
    topBlank->setStyleSheet(QString("background:%1;border:none;").arg(lightTheme));

    QFrame *bottomBlank = new QFrame(&dialog);
    bottomBlank->setFixedHeight(navHeight);
    bottomBlank->setStyleSheet(QString("background:%1;border:none;").arg(lightTheme));

    QWidget *content = new QWidget(&dialog);
    content->setStyleSheet("background:#fffef8;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(28, 20, 28, 20);
    contentLayout->setSpacing(18);

    QLabel *title = new QLabel(QStringLiteral("修改个人信息"), content);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:42px;font-weight:900;color:#2b3f35;");
    contentLayout->addWidget(title);

    QString avatarPath = u->avatarImagePath.trimmed();
    QString nickName = u->nickname.trimmed();
    QString gender = u->gender.trimmed();
    int age = u->age;
    QString message = u->messageToMouse.trimmed();

    auto valueOrDefault = [](const QString &value, const QString &fallback) {
        return value.trimmed().isEmpty() ? fallback : value.trimmed();
    };

    QFrame *avatarRow = new QFrame(content);
    avatarRow->setStyleSheet("background:#f7fbf8;border:none;border-radius:18px;");
    QHBoxLayout *avatarRowLayout = new QHBoxLayout(avatarRow);
    avatarRowLayout->setContentsMargins(18, 14, 18, 14);
    avatarRowLayout->setSpacing(16);

    QLabel *avatarLabel = new QLabel(QStringLiteral("头像图片"), avatarRow);
    avatarLabel->setStyleSheet("font-size:28px;font-weight:800;color:#2a3f35;");
    QPushButton *pickAvatarBtn = new QPushButton(QStringLiteral("从相册中选择"), avatarRow);
    pickAvatarBtn->setStyleSheet("font-size:24px;min-height:72px;padding:0 20px;background:#3d7db8;color:white;border:none;border-radius:14px;");
    const int previewSize = (profileAvatarLabel && profileAvatarLabel->width() > 0) ? profileAvatarLabel->width() : 198;
    QLabel *avatarPreview = new QLabel(avatarRow);
    avatarPreview->setFixedSize(previewSize, previewSize);
    avatarPreview->setStyleSheet("background:#dae7e0;border:2px solid #cad9d1;border-radius:99px;");
    avatarPreview->setAlignment(Qt::AlignCenter);

    auto refreshAvatarPreview = [&]() {
        QPixmap pix;
        if (!avatarPath.isEmpty() && QFile::exists(avatarPath)) {
            pix.load(avatarPath);
        }
        if (pix.isNull()) {
            pix.load(kDefaultAvatarResourcePath);
        }
        if (!pix.isNull()) {
            const QPixmap clipped = createCircularAvatarPixmap(pix, avatarPreview->size());
            avatarPreview->setText(QString());
            avatarPreview->setPixmap(clipped.isNull() ? pix : clipped);
            return;
        }
        avatarPreview->setPixmap(QPixmap());
        avatarPreview->setText(QStringLiteral("无"));
    };

    avatarRowLayout->addWidget(avatarLabel);
    avatarRowLayout->addStretch();
    avatarRowLayout->addWidget(pickAvatarBtn);
    avatarRowLayout->addWidget(avatarPreview);

    QPushButton *nicknameRow = new QPushButton(content);
    QPushButton *genderRow = new QPushButton(content);
    QPushButton *ageRow = new QPushButton(content);
    QPushButton *messageRow = new QPushButton(content);

    auto styleOptionRow = [](QPushButton *btn) {
        btn->setStyleSheet("QPushButton{font-size:28px;font-weight:700;min-height:92px;padding:0 20px;text-align:left;"
                           "background:#f7fbf8;color:#2a3f35;border:2px solid #dce8e0;border-radius:18px;}"
                           "QPushButton:hover{background:#eef6f1;}");
    };

    auto refreshOptionRows = [&]() {
        nicknameRow->setText(QString("昵称：%1  >").arg(valueOrDefault(nickName, QStringLiteral("点击填写"))));
        genderRow->setText(QString("性别：%1  >").arg(valueOrDefault(gender, QStringLiteral("点击选择"))));
        ageRow->setText(QString("年龄：%1  >").arg(age > 0 ? QString::number(age) : QStringLiteral("点击填写")));
        messageRow->setText(QString("送给大老鼠的话：%1  >").arg(valueOrDefault(message.left(20), QStringLiteral("点击填写"))));
    };

    QWidget *editorOverlay = new QWidget(&dialog);
    editorOverlay->setStyleSheet("background:rgba(0,0,0,110);");
    editorOverlay->hide();
    editorOverlay->raise();

    QFrame *editorPanel = new QFrame(editorOverlay);
    editorPanel->setStyleSheet("background:#ffffff;border:none;border-radius:20px;");
    QVBoxLayout *editorPanelLayout = new QVBoxLayout(editorPanel);
    editorPanelLayout->setContentsMargins(20, 18, 20, 18);
    editorPanelLayout->setSpacing(12);

    QLabel *editorTitle = new QLabel(editorPanel);
    editorTitle->setAlignment(Qt::AlignCenter);
    editorTitle->setStyleSheet("font-size:30px;font-weight:900;color:#2e4439;");

    QLineEdit *singleEdit = new QLineEdit(editorPanel);
    singleEdit->setStyleSheet("font-size:28px;min-height:86px;padding:0 14px;border:2px solid #d9e5dd;border-radius:14px;background:#ffffff;");

    QTextEdit *messageEdit = new QTextEdit(editorPanel);
    messageEdit->setStyleSheet("font-size:26px;min-height:180px;padding:10px;border:2px solid #d9e5dd;border-radius:14px;background:#ffffff;");

    QWidget *genderPane = new QWidget(editorPanel);
    QVBoxLayout *genderLayout = new QVBoxLayout(genderPane);
    genderLayout->setContentsMargins(0, 0, 0, 0);
    genderLayout->setSpacing(10);
    QPushButton *maleBtn = new QPushButton(QStringLiteral("男"), genderPane);
    QPushButton *femaleBtn = new QPushButton(QStringLiteral("女"), genderPane);
    QPushButton *superMouseBtn = new QPushButton(QStringLiteral("超级老鼠"), genderPane);
    const QString genderBtnStyle = "font-size:26px;min-height:74px;background:#f4f8f5;color:#24372e;border:2px solid #d8e4db;border-radius:12px;";
    maleBtn->setStyleSheet(genderBtnStyle);
    femaleBtn->setStyleSheet(genderBtnStyle);
    superMouseBtn->setStyleSheet(genderBtnStyle);
    genderLayout->addWidget(maleBtn);
    genderLayout->addWidget(femaleBtn);
    genderLayout->addWidget(superMouseBtn);

    QHBoxLayout *editorButtons = new QHBoxLayout();
    editorButtons->setSpacing(16);
    QPushButton *editorCancelBtn = new QPushButton(QStringLiteral("取消"), editorPanel);
    QPushButton *editorOkBtn = new QPushButton(QStringLiteral("确认"), editorPanel);
    editorCancelBtn->setStyleSheet("font-size:28px;min-height:78px;background:#8a8a8a;color:white;border:none;border-radius:14px;");
    editorOkBtn->setStyleSheet("font-size:28px;min-height:78px;background:#2f8f46;color:white;border:none;border-radius:14px;");
    editorButtons->addWidget(editorCancelBtn);
    editorButtons->addWidget(editorOkBtn);

    editorPanelLayout->addWidget(editorTitle);
    editorPanelLayout->addWidget(singleEdit);
    editorPanelLayout->addWidget(messageEdit);
    editorPanelLayout->addWidget(genderPane);
    editorPanelLayout->addLayout(editorButtons);

    enum class EditMode { Nickname, Gender, Age, Message };
    EditMode currentMode = EditMode::Nickname;

    auto layoutEditorOverlay = [&]() {
        editorOverlay->setGeometry(dialog.rect());
        const int panelW = dialog.width() * 2 / 3;
        const int panelH = dialog.height() / 3;
        const int x = (dialog.width() - panelW) / 2;
        const int confirmHeight = 78;
        const int moveDown = confirmHeight * 2;
        const int y = dialog.height() / 15 + moveDown;
        editorPanel->setGeometry(x, qMin(dialog.height() - panelH - 8, qMax(8, y)), panelW, panelH);
    };

    auto showEditorOverlay = [&](EditMode mode) {
        currentMode = mode;
        singleEdit->hide();
        messageEdit->hide();
        genderPane->hide();

        if (mode == EditMode::Nickname) {
            editorTitle->setText(QStringLiteral("设置昵称"));
            singleEdit->setPlaceholderText(QStringLiteral("请输入昵称"));
            singleEdit->setMaxLength(24);
            singleEdit->setValidator(nullptr);
            singleEdit->setText(nickName);
            singleEdit->show();
        } else if (mode == EditMode::Age) {
            editorTitle->setText(QStringLiteral("设置年龄"));
            singleEdit->setPlaceholderText(QStringLiteral("请输入年龄"));
            singleEdit->setMaxLength(3);
            singleEdit->setValidator(new QIntValidator(0, 120, singleEdit));
            singleEdit->setText(age > 0 ? QString::number(age) : QString());
            singleEdit->show();
        } else if (mode == EditMode::Gender) {
            editorTitle->setText(QStringLiteral("选择性别"));
            genderPane->show();
        } else {
            editorTitle->setText(QStringLiteral("送给大老鼠的话"));
            messageEdit->setPlainText(message);
            messageEdit->show();
        }

        layoutEditorOverlay();
        editorOverlay->show();
        editorOverlay->raise();
    };

    QObject::connect(editorCancelBtn, &QPushButton::clicked, &dialog, [&]() {
        editorOverlay->hide();
    });
    QObject::connect(editorOkBtn, &QPushButton::clicked, &dialog, [&]() {
        if (currentMode == EditMode::Nickname) {
            nickName = singleEdit->text().trimmed();
        } else if (currentMode == EditMode::Age) {
            const QString text = singleEdit->text().trimmed();
            age = text.isEmpty() ? -1 : text.toInt();
        } else if (currentMode == EditMode::Message) {
            message = messageEdit->toPlainText().trimmed().left(120);
        }
        refreshOptionRows();
        editorOverlay->hide();
    });

    auto selectGender = [&](const QString &g) {
        gender = g;
        refreshOptionRows();
        editorOverlay->hide();
    };
    QObject::connect(maleBtn, &QPushButton::clicked, &dialog, [=, &selectGender]() { selectGender(QStringLiteral("男")); });
    QObject::connect(femaleBtn, &QPushButton::clicked, &dialog, [=, &selectGender]() { selectGender(QStringLiteral("女")); });
    QObject::connect(superMouseBtn, &QPushButton::clicked, &dialog, [=, &selectGender]() { selectGender(QStringLiteral("超级老鼠")); });

    styleOptionRow(nicknameRow);
    styleOptionRow(genderRow);
    styleOptionRow(ageRow);
    styleOptionRow(messageRow);
    refreshOptionRows();
    refreshAvatarPreview();

    QObject::connect(pickAvatarBtn, &QPushButton::clicked, &dialog, [&]() {
#ifdef Q_OS_ANDROID
        if (launchAndroidImagePicker(&dialog,
                                     kEditAvatarPickRequestCode,
                                     [&](const QString &pickedPath) {
                                         if (pickedPath.trimmed().isEmpty()) {
                                             return;
                                         }
                                         avatarPath = pickedPath;
                                         refreshAvatarPreview();
                                     })) {
            return;
        }
#endif
        QString startDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (startDir.trimmed().isEmpty()) {
            startDir = QDir::homePath();
        }
        QFileDialog picker(&dialog,
                           QStringLiteral("从相册中选择头像"),
                           startDir,
                           QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        picker.setFileMode(QFileDialog::ExistingFile);
        picker.setOption(QFileDialog::DontUseNativeDialog, false);
        QString file;
        if (picker.exec() == QDialog::Accepted) {
            const QStringList files = picker.selectedFiles();
            if (!files.isEmpty()) {
                file = files.first();
            }
        }
        if (file.isEmpty()) {
            return;
        }
        avatarPath = file;
        refreshAvatarPreview();
    });

    QObject::connect(nicknameRow, &QPushButton::clicked, &dialog, [&]() { showEditorOverlay(EditMode::Nickname); });
    QObject::connect(genderRow, &QPushButton::clicked, &dialog, [&]() { showEditorOverlay(EditMode::Gender); });
    QObject::connect(ageRow, &QPushButton::clicked, &dialog, [&]() { showEditorOverlay(EditMode::Age); });
    QObject::connect(messageRow, &QPushButton::clicked, &dialog, [&]() { showEditorOverlay(EditMode::Message); });

    contentLayout->addWidget(avatarRow);
    contentLayout->addWidget(nicknameRow);
    contentLayout->addWidget(genderRow);
    contentLayout->addWidget(ageRow);
    contentLayout->addWidget(messageRow);
    contentLayout->addStretch();

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 8, 0, 0);
    const int actionButtonWidth = 280;
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), content);
    QPushButton *saveBtn = new QPushButton(QStringLiteral("保存"), content);
    cancelBtn->setFixedSize(actionButtonWidth, 96);
    saveBtn->setFixedSize(actionButtonWidth, 96);
    cancelBtn->setStyleSheet("font-size:34px;font-weight:900;background:#8f8f8f;color:white;border:none;border-radius:18px;");
    saveBtn->setStyleSheet("font-size:34px;font-weight:900;background:#2f8f46;color:white;border:none;border-radius:18px;");
    buttons->addStretch();
    buttons->addWidget(cancelBtn);
    buttons->addSpacing(actionButtonWidth);
    buttons->addWidget(saveBtn);
    buttons->addStretch();
    contentLayout->addLayout(buttons);

    layout->addWidget(topBlank);
    layout->addWidget(content, 1);
    layout->addWidget(bottomBlank);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(saveBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    u->avatarImagePath = avatarPath;
    u->nickname = nickName;
    u->gender = gender;
    u->age = age;
    u->messageToMouse = message;
    store.save();
    persistCurrentUserPackage1();
    updateProfileHeader();
}

// 功能：处理相关逻辑。

