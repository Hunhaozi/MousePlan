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

// 功能：寻找当前总计划的索引
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
        qWarning() << "请求超时，已中止";
        reply->deleteLater();
        return QJsonObject();
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorText) {
            *errorText = reply->errorString();
        }
        qWarning() << "网络错误:" << reply->error() << reply->errorString();
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
        qWarning() << "JSON解析错误:" << parseErr.errorString();
        qWarning() << "在位置:" << parseErr.offset;
        qWarning() << "解析前的数据:" << raw.left(200);
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

// 功能：发送JSON串至服务器，并接收消息
QJsonObject postOnlineJson(const QString &path,
                           const QJsonObject &payload,
                           bool *ok = nullptr,
                           QString *errorText = nullptr)
{
    if (ok) {
        *ok = false;
    }

    qDebug() << "发送请求到路径:" << path;
    qDebug() << "请求负载:" << QJsonDocument(payload).toJson(QJsonDocument::Compact);

    const OnlineApiConfig cfg = loadOnlineApiConfig();  //加载原服务器配置
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
        qWarning() << "无效的URL:" << urlText + suffix;
        return QJsonObject();
    }

    qDebug() << "完整URL:" << url.toString();
    qDebug() << "超时设置:" << cfg.timeoutMs << "ms";

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
        qWarning() << "请求超时，已中止";
        reply->deleteLater();
        return QJsonObject();
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (errorText) {
            *errorText = reply->errorString();
        }
        qWarning() << "网络错误:" << reply->error() << reply->errorString();
        reply->deleteLater();
        return QJsonObject();
    }

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    // 打印接收到的原始数据
    qDebug() << "接收到的原始数据长度:" << raw.length() << "字节";
    if (raw.length() < 1024) {  // 如果数据不太大，打印内容
        qDebug() << "原始数据:" << raw;
    } else {
        qDebug() << "数据前200字节:" << raw.left(200);
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorText) {
            *errorText = QStringLiteral("服务器响应不是有效 JSON 对象");
        }
        qWarning() << "JSON解析错误:" << parseErr.errorString();
        qWarning() << "在位置:" << parseErr.offset;
        qWarning() << "解析前的数据:" << raw.left(200);
        return QJsonObject();
    }

    // 打印解析后的JSON
    QJsonObject result = doc.object();
    qDebug() << "成功解析JSON对象，包含字段:" << result.keys();
    if (result.contains("success")) {
        qDebug() << "success字段值:" << result["success"];
    }
    if (result.contains("userId")) {
        qDebug() << "userId字段值:" << result["userId"];
    }

    if (ok) {
        *ok = true;
    }
    qDebug() << "请求处理完成，结果:" << (ok ? *ok : false);

    return result;
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

bool MainWindow::tryAutoLoginOnStartup()
{
    const QDate today = QDate::currentDate();
    int bestIndex = -1;
    QDate bestExpiry;
    for (int i = 0; i < store.users.size(); ++i) {
        const User &u = store.users[i];
        if (!u.rememberLoginUntil.isValid() || u.rememberLoginUntil < today) {
            continue;
        }
        if (bestIndex < 0 || u.rememberLoginUntil > bestExpiry) {
            bestIndex = i;
            bestExpiry = u.rememberLoginUntil;
        }
    }

    if (bestIndex < 0) {
        return false;
    }

    const User &bestUser = store.users[bestIndex];
    loginLocalMode = bestUser.isLocalAccount;
    currentUserId = bestUser.id;
    lastLoginUsername = bestUser.username;
    loadBestPackage1ForCurrentUser();
    maybeRunThemeSelection();
    switchToHomePage();
    return true;
}

// 功能：处理相关逻辑。

void MainWindow::formatCurrentAccountData()
{
    User *u = currentUser();
    if (!u) {
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this,
        QStringLiteral("危险操作确认"),
        QStringLiteral("将删除当前账户所有打卡记录与总计划，且无法恢复。是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    QString inputPwd;
    if (!runLargePasswordDialog(this,
                                QStringLiteral("身份验证"),
                                QStringLiteral("请输入当前账号密码："),
                                inputPwd)) {
        return;
    }
    if (!verifyPasswordHashLocal(*u, inputPwd)) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("密码错误，已取消格式化。"));
        return;
    }

    for (int i = store.records.size() - 1; i >= 0; --i) {
        if (store.records[i].ownerUserId == u->id) {
            store.records.removeAt(i);
        }
    }
    for (int i = store.plans.size() - 1; i >= 0; --i) {
        if (store.plans[i].ownerUserId == u->id) {
            store.plans.removeAt(i);
        }
    }

    u->activePlanId.clear();
    store.addMouseDefaultPresetPlan(u->id, nullptr, true);
    store.save();
    persistCurrentUserPackage1();

    rebuildCalendarFormats();
    rebuildDayView();
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当前账户数据已格式化，并创建了未启用的默认计划。"));
}

// 功能：处理相关逻辑。

void MainWindow::switchToLoginPage()
{
    if (rootStackLayout && loginPage) {
        rootStackLayout->setCurrentWidget(loginPage);
    } else {
        loginPage->show();
        homePage->hide();
    }

    QDate modeBestDate;
    bool modeResolvedFromHistory = false;
    for (const User &u : store.users) {
        if (!u.rememberLoginUntil.isValid()) {
            continue;
        }
        if (!modeResolvedFromHistory || u.rememberLoginUntil > modeBestDate) {
            modeBestDate = u.rememberLoginUntil;
            loginLocalMode = u.isLocalAccount;
            modeResolvedFromHistory = true;
        }
    }

    QString preferredUsername = lastLoginUsername;
    if (preferredUsername.trimmed().isEmpty()) {
        QDate bestDate;
        for (const User &u : store.users) {
            if (u.isLocalAccount != loginLocalMode) {
                continue;
            }
            if (!u.rememberLoginUntil.isValid()) {
                continue;
            }
            if (!bestDate.isValid() || u.rememberLoginUntil > bestDate) {
                bestDate = u.rememberLoginUntil;
                preferredUsername = u.username;
            }
        }
    }
    if (preferredUsername.trimmed().isEmpty()) {
        for (const User &u : store.users) {
            if (u.isLocalAccount == loginLocalMode) {
                preferredUsername = u.username;
                break;
            }
        }
    }
    if (usernameEdit) {
        usernameEdit->setText(preferredUsername);
        usernameEdit->setCursorPosition(usernameEdit->text().size());
    }
    passwordEdit->clear();
    if (agreementCheckBox) {
        agreementCheckBox->setChecked(false);
    }
    refreshLoginModeUi();
    applyResponsiveLayout();
}

// 功能：处理相关逻辑。

void MainWindow::refreshLoginModeUi()
{
    if (loginModeLabel) {
        loginModeLabel->setText(loginLocalMode ? QStringLiteral("当前模式：本地模式") : QStringLiteral("当前模式：在线模式"));
    }
    if (loginModeSwitchButton) {
        loginModeSwitchButton->setText(loginLocalMode ? QStringLiteral("切换为在线模式") : QStringLiteral("切换为本地模式"));
    }
    if (loginServerConfigButton) {
        loginServerConfigButton->setVisible(!loginLocalMode);
    }
}

// 功能：处理相关逻辑。

void MainWindow::openServerConfigDialog()
{
    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("服务器设置"));

    const int navBlank = qMax(120, navBarCard ? navBarCard->height() : 120);
    QVBoxLayout *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *topBlank = new QFrame(&dialog);
    topBlank->setFixedHeight(navBlank);
    topBlank->setStyleSheet("background:#eef4ef;border:none;");

    QFrame *bottomBlank = new QFrame(&dialog);
    bottomBlank->setFixedHeight(navBlank);
    bottomBlank->setStyleSheet("background:#eef4ef;border:none;");

    QWidget *center = new QWidget(&dialog);
    center->setStyleSheet("background:#f8fbf8;");
    QVBoxLayout *layout = new QVBoxLayout(center);
    layout->setContentsMargins(22, 16, 22, 16);
    layout->setSpacing(14);

    QLabel *title = new QLabel(QStringLiteral("在线服务配置"), center);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:34px;font-weight:900;color:#2b3f35;");

    QLabel *urlLabel = new QLabel(QStringLiteral("服务器地址"), center);
    urlLabel->setStyleSheet("font-size:24px;font-weight:700;color:#34443a;");
    QLineEdit *urlEdit = new QLineEdit(center);
    urlEdit->setPlaceholderText(QString::fromLatin1(mouseplan::common::config::kDefaultServerBaseUrl));
    urlEdit->setText(mouseplan::common::config::resolveServerBaseUrl());
    urlEdit->setStyleSheet("font-size:22px;min-height:62px;padding:0 12px;background:white;border:1px solid #d7e5db;border-radius:12px;");

    QLabel *timeoutLabel = new QLabel(QStringLiteral("超时时长（毫秒）"), center);
    timeoutLabel->setStyleSheet("font-size:24px;font-weight:700;color:#34443a;");
    QLineEdit *timeoutEdit = new QLineEdit(center);
    timeoutEdit->setPlaceholderText(QString::number(mouseplan::common::config::kDefaultServerTimeoutMs));
    timeoutEdit->setText(QString::number(mouseplan::common::config::resolveServerTimeoutMs()));
    timeoutEdit->setValidator(new QIntValidator(1, 600000, timeoutEdit));
    timeoutEdit->setStyleSheet("font-size:22px;min-height:62px;padding:0 12px;background:white;border:1px solid #d7e5db;border-radius:12px;");

    QHBoxLayout *ops = new QHBoxLayout();
    ops->setSpacing(12);
    QPushButton *saveBtn = new QPushButton(QStringLiteral("保存"), center);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), center);
    saveBtn->setStyleSheet("font-size:24px;min-height:74px;background:#2f8f46;color:white;border-radius:14px;");
    cancelBtn->setStyleSheet("font-size:24px;min-height:74px;background:#8b8f99;color:white;border-radius:14px;");
    ops->addWidget(saveBtn);
    ops->addWidget(cancelBtn);

    layout->addWidget(title);
    layout->addSpacing(6);
    layout->addWidget(urlLabel);
    layout->addWidget(urlEdit);
    layout->addWidget(timeoutLabel);
    layout->addWidget(timeoutEdit);
    layout->addStretch();
    layout->addLayout(ops);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(saveBtn, &QPushButton::clicked, &dialog, [&]() {
        const QString inputUrl = urlEdit->text().trimmed();
        bool ok = false;
        const int inputTimeout = timeoutEdit->text().trimmed().toInt(&ok);
        mouseplan::common::config::saveServerConfig(inputUrl, ok ? inputTimeout : 0);
        QMessageBox::information(&dialog,
                                 QStringLiteral("提示"),
                                 QStringLiteral("服务器设置已保存。若填写为空，已自动回退为默认值。"));
        dialog.accept();
    });

    root->addWidget(topBlank);
    root->addWidget(center, 1);
    root->addWidget(bottomBlank);

    dialog.exec();
}

// 功能：处理相关逻辑。

void MainWindow::switchToHomePage()
{
    if (rootStackLayout && homePage) {
        rootStackLayout->setCurrentWidget(homePage);
    } else {
        loginPage->hide();
        homePage->show();
    }
    showHomeTab();
    selectedDate = QDate::currentDate();
    calendar->setSelectedDate(selectedDate);
    applyResponsiveLayout();
    setupProfilePanelUi();
    syncProfilePanelLayoutByNav();
    updateProfileHeader();
    QTimer::singleShot(0, this, [this]() {
        applyResponsiveLayout();
        syncProfilePanelLayoutByNav();
        updateProfileHeader();
    });
    rebuildCalendarFormats();
    rebuildDayView();
}

// 功能：处理相关逻辑。

void MainWindow::persistCurrentUserPackage1()
{
    User *u = currentUser();
    if (!u) {
        return;
    }
    if (!u->isLocalAccount) {
        const QJsonObject snapshot = store.buildUserPackage1Snapshot(u->id);
        if (!snapshot.isEmpty()) {
            pushPackage1ToCloudReserved(u->id, snapshot);
        }
        return;
    }

    store.saveUserPackage1Local(u->id);
}

// 功能：处理相关逻辑。

void MainWindow::loadBestPackage1ForCurrentUser()
{
    User *u = currentUser();
    if (!u) {
        return;
    }

    if (!u->isLocalAccount) {
        pullOnlinePackage1ForCurrentUser(true);
        return;
    }

    if (store.loadBestUserPackage1(u->id, !u->isLocalAccount)) {
        store.save();
    }
}

// 功能：处理相关逻辑。

bool MainWindow::pullOnlinePackage1ForCurrentUser(bool silent)
{
    User *u = currentUser();
    if (!u || u->isLocalAccount) {
        return false;
    }

    QString errorText;
    QJsonObject package1;
    if (!pullPackage1FromCloudReserved(u->id, &package1, &errorText)) {
        if (!silent && !errorText.trimmed().isEmpty()) {
            QMessageBox::warning(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("在线数据拉取失败：%1").arg(errorText));
        }
        return false;
    }

    if (!store.applyUserPackage1Snapshot(u->id, package1, true)) {
        if (!silent) {
            QMessageBox::warning(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("在线数据格式异常，无法应用到本地。"));
        }
        return false;
    }

    // Online mode keeps cloud as source of truth, avoid writing package cache to local files.
    return true;
}

// 功能：处理相关逻辑。

void MainWindow::tryLogin()
{
    if (!agreementCheckBox || !agreementCheckBox->isChecked()) {
        if (promptAgreementDialog()) {
            agreementCheckBox->blockSignals(true);
            agreementCheckBox->setChecked(true);
            agreementCheckBox->blockSignals(false);
        } else {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未同意协议，不能登录。"));
            return;
        }
    }

    if (!loginLocalMode && !ensureOnlineLoginPermission(this)) {
        return;
    }

    const QString username = usernameEdit->text().trimmed();
    const QString password = passwordEdit->text();
    const QString passwordHash = buildSecretHash(password);

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("账号或密码不能为空。"));
        return;
    }

    if (loginLocalMode) {
        const User *matchedLocalUser = nullptr;
        for (const User &u : store.users) {
            if (u.isLocalAccount && u.username == username) {
                matchedLocalUser = &u;
                break;
            }
        }

        if (!matchedLocalUser || matchedLocalUser->password != passwordHash) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("账号或密码错误。"));
            return;
        }

        currentUserId = matchedLocalUser->id;
        lastLoginUsername = matchedLocalUser->username;
    } else {
        QString localOnlineUserId;
        for (const User &u : store.users) {
            if (!u.isLocalAccount && u.username == username) {
                localOnlineUserId = u.id;
                break;
            }
        }

        QString serverUserId;
        const bool authOk = verifyPasswordHashWithServerReserved(localOnlineUserId,
                                                                  username,
                                                                  passwordHash,
                                                                  &serverUserId);
        if (!authOk) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("在线账号或密码错误，或服务器暂不可用。"));
            return;
        }

        if (serverUserId.trimmed().isEmpty()) {
            serverUserId = localOnlineUserId;
        }
        if (serverUserId.trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("在线登录成功但未返回账号标识，请联系服务端管理员。"));
            return;
        }

        User *onlineUser = nullptr;
        for (User &u : store.users) {
            if (!u.isLocalAccount && (u.id == serverUserId || u.username == username)) {
                onlineUser = &u;
                break;
            }
        }
        if (!onlineUser) {
            User newOnlineUser;
            newOnlineUser.id = serverUserId;
            newOnlineUser.username = username;
            newOnlineUser.password = passwordHash;
            newOnlineUser.isLocalAccount = false;
            newOnlineUser.nickname = generateRandomNicknameDigits();
            newOnlineUser.theme = QStringLiteral("fitness");
            newOnlineUser.themeChosen = false;
            store.users.push_back(newOnlineUser);
            onlineUser = &store.users.back();
        } else {
            onlineUser->id = serverUserId;
            onlineUser->username = username;
            onlineUser->password = passwordHash;
            onlineUser->isLocalAccount = false;
        }

        currentUserId = serverUserId;
        lastLoginUsername = username;
    }

    loadBestPackage1ForCurrentUser();
    if (User *current = currentUser()) {
        current->rememberLoginUntil = QDate::currentDate().addMonths(6);
    }
    store.save();
    persistCurrentUserPackage1();

    maybeRunThemeSelection();
    switchToHomePage();
    return;
}

// 主题/账号与训练记录动作逻辑依赖本文件中的内部辅助函数
// 通过同一翻译单元聚合编译以保持原行为并避免可见性问题
#include "mainwindow.h"

#include "modules/common/theme/theme_strategy_factory.h"
#include "modules/ui/login/login_register_flow.h"

// 功能：根据当前模式分发注册入口。
void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyResponsiveLayout();
    updateProfileBannerImage();
}

// 功能：处理相关逻辑。

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        if (watched == profileAvatarLabel) {
            onProfileImageAreaClicked(false);
            return true;
        }
        if (watched == profileBannerLabel) {
            onProfileImageAreaClicked(true);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// 功能：处理相关逻辑。

void MainWindow::applyResponsiveLayout()
{
// 该文件由重构生成：保持原函数逻辑不变，仅做文件分层。
    if (!logoLabel || !usernameEdit || !calendar || !planSettingButton || !submitTodayButton) {
        return;
    }

    const QSize s = size();
    if (s.width() <= 0 || s.height() <= 0) {
        return;
    }

    const int shortSide = qMin(s.width(), s.height());
    qreal scale = shortSide / 420.0;
    if (scale < 0.85) {
        scale = 0.85;
    }
    if (scale > 1.8) {
        scale = 1.8;
    }

    // Conservative per-resolution tuning for high-end Android screens.
    // Keep 1200-short-side devices unchanged; only slightly adjust nearby classes.
    QScreen *deviceScreen = QGuiApplication::primaryScreen();
    if (deviceScreen) {
        const QSize deviceSize = deviceScreen->availableGeometry().size();
        const int deviceShort = qMin(deviceSize.width(), deviceSize.height());
        if (deviceShort >= 1080) {
            qreal ratioTo1200 = static_cast<qreal>(deviceShort) / 1200.0;
            ratioTo1200 = qBound(0.95, ratioTo1200, 1.10);
            scale *= ratioTo1200;
            if (scale < 0.85) {
                scale = 0.85;
            }
            if (scale > 2.0) {
                scale = 2.0;
            }
        }
    }

    const bool portraitLike = s.height() > s.width();
    const int edge = qMax(10, static_cast<int>(12 * scale));
    const int spacing = qMax(6, static_cast<int>(8 * scale));
    const int inputHeight = qMax(58, static_cast<int>(66 * scale));
    const int btnHeight = qMax(62, static_cast<int>(70 * scale));
    const int navHeight = qMax(78, static_cast<int>(88 * scale));
    const int bodyFont = qMax(gUiTuning.mainBodyMinFont, static_cast<int>(17 * scale));

    if (rootOuterLayout) {
        rootOuterLayout->setContentsMargins(edge, edge, edge, edge);
        rootOuterLayout->setSpacing(spacing);
    }
    if (loginMainLayout) {
        const int hMargin = qMax(18, static_cast<int>(24 * scale));
        const int vMargin = qMax(22, static_cast<int>(28 * scale));
        loginMainLayout->setContentsMargins(hMargin, vMargin, hMargin, vMargin);
        loginMainLayout->setSpacing(spacing + 6);
    }
    if (homeMainLayout) {
        homeMainLayout->setContentsMargins(0, 0, 0, 0);
        homeMainLayout->setSpacing(spacing);
        homeMainLayout->setStretch(0, 1);
        homeMainLayout->setStretch(1, 0);
    }
    if (middleSplitLayout) {
        middleSplitLayout->setSpacing(spacing);
        middleSplitLayout->setDirection(portraitLike ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        if (portraitLike) {
            middleSplitLayout->setStretch(0, 8);
            middleSplitLayout->setStretch(1, 0);
        } else {
            middleSplitLayout->setStretch(0, 5);
            middleSplitLayout->setStretch(1, 1);
        }
    }

    const int logoSize = qMax(gUiTuning.loginLogoMinFont, static_cast<int>(44 * scale));
    logoLabel->setStyleSheet(QString("font-size: %1px; font-weight: 800;").arg(logoSize));
    logoLabel->setMinimumHeight(qMax(116, static_cast<int>(126 * scale)));
    if (loginCaptionLabel) {
        loginCaptionLabel->setStyleSheet(QString("font-size:%1px;").arg(qMax(gUiTuning.loginCaptionMinFont, bodyFont)));
    }
    if (loginModeLabel) {
        loginModeLabel->setStyleSheet(QString("font-size:%1px;font-weight:800;color:%2;")
                                          .arg(qMax(gUiTuning.loginCaptionMinFont, bodyFont + 2))
                                          .arg(loginLocalMode ? QStringLiteral("#2f8f46") : QStringLiteral("#2f6fa6")));
    }
    if (loginCard) {
        loginCard->setMaximumWidth(portraitLike ? qMin(width() - edge * 2, 1080) : 700);
        loginCard->setMinimumWidth(portraitLike ? qMin(width() - edge * 2, gUiTuning.loginCardMinWidthPortrait + 80) : 560);
        loginCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        if (loginCard->layout()) {
            loginCard->layout()->setSpacing(qMax(18, static_cast<int>(20 * scale)));
        }
    }

    usernameEdit->setMinimumHeight(inputHeight);
    passwordEdit->setMinimumHeight(inputHeight);
    loginButton->setMinimumHeight(btnHeight);
    if (loginModeSwitchButton) {
        loginModeSwitchButton->setMinimumHeight(btnHeight - 2);
    }
    registerButton->setMinimumHeight(btnHeight - 2);
    forgotButton->setMinimumHeight(btnHeight - 2);
    usernameEdit->setStyleSheet(QString("font-size:%1px;").arg(qMax(gUiTuning.loginInputMinFont, bodyFont)));
    passwordEdit->setStyleSheet(QString("font-size:%1px;").arg(qMax(gUiTuning.loginInputMinFont, bodyFont)));
    if (agreementCheckBox) {
        agreementCheckBox->setStyleSheet(QString("font-size:%1px;color:#4e5f56;spacing:10px;").arg(qMax(gUiTuning.loginCaptionMinFont, bodyFont - 1)));
    }
    if (loginModeSwitchButton) {
        loginModeSwitchButton->setStyleSheet(QString("font-size:%1px;font-weight:700;padding:10px 14px;")
                                                 .arg(qMax(gUiTuning.loginLinkMinFont, bodyFont + 3)));
    }
    registerButton->setStyleSheet(QString("font-size:%1px;font-weight:700;padding:10px 14px;").arg(qMax(gUiTuning.loginLinkMinFont, bodyFont + 3)));
    forgotButton->setStyleSheet(QString("font-size:%1px;font-weight:700;padding:10px 14px;").arg(qMax(gUiTuning.loginLinkMinFont, bodyFont + 3)));
    loginButton->setStyleSheet(QString("font-size:%1px;font-weight:800;").arg(qMax(gUiTuning.loginButtonMinFont, bodyFont + 5)));

    const int titleSize = qMax(22, static_cast<int>(24 * scale));
    dayTitleLabel->setStyleSheet(QString("font-size: %1px; font-weight: 700;").arg(titleSize));
    dayHintLabel->setMinimumHeight(qMax(44, static_cast<int>(48 * scale)));
    dayHintLabel->setStyleSheet(QString("font-size:%1px;").arg(qMax(18, bodyFont + 2)));

    const int calendarHeight = portraitLike
                                   ? qMax(360, static_cast<int>(s.height() * 0.35))
                                   : qMax(220, static_cast<int>(s.height() * 0.40));
    calendar->setMinimumHeight(calendarHeight);
    QFont calFont = calendar->font();
    calFont.setPointSize(qMax(gUiTuning.calendarDayMinFont, static_cast<int>(21 * scale)));
    calendar->setFont(calFont);

    submitTodayButton->setMinimumHeight(qMax(gUiTuning.mainActionButtonMinHeight, static_cast<int>(66 * scale)));
    submitTodayButton->setStyleSheet(QString("font-size:%1px;font-weight:800;").arg(qMax(gUiTuning.mainActionButtonMinFont, bodyFont + 6)));
    homeNavButton->setMinimumHeight(navHeight);
    themeNavButton->setMinimumHeight(navHeight);
    profileNavButton->setMinimumHeight(navHeight);
    if (navBarCard) {
        const int navCardHeight = qMax(qMax(navHeight + 18, static_cast<int>(navHeight * 1.15)),
                                       qMax(132, static_cast<int>(146 * scale)));
        navBarCard->setMinimumHeight(navCardHeight);
        navBarCard->setMaximumHeight(navCardHeight);
        navBarCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    const int navIconEdge = (qMax(gUiTuning.navIconMinSize, static_cast<int>(42 * scale)) * 5) / 4;
    const QSize navIconSize(navIconEdge, navIconEdge);
    homeNavButton->setIconSize(navIconSize);
    themeNavButton->setIconSize(navIconSize);
    profileNavButton->setIconSize(navIconSize);
    const int navTextPx = (qMax(gUiTuning.navTextMinFont, bodyFont + 3) * 5) / 4;
    homeNavButton->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(navTextPx));
    themeNavButton->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(navTextPx));
    profileNavButton->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(navTextPx));

    if (portraitLike) {
        planSettingButton->setText(QStringLiteral("打开总计划设置"));
        planSettingButton->setMinimumWidth(0);
        planSettingButton->setMinimumHeight(qMax(gUiTuning.mainActionButtonMinHeight, static_cast<int>(52 * scale)));
        planSettingButton->setStyleSheet(QString("font-size:%1px;font-weight:800;padding:12px 18px;").arg(qMax(gUiTuning.mainActionButtonMinFont, bodyFont + 5)));
        if (rightPlanContainer) {
            rightPlanContainer->setMaximumWidth(QWIDGETSIZE_MAX);
            rightPlanContainer->setMaximumHeight(qMax(120, static_cast<int>(130 * scale)));
        }
    } else {
        planSettingButton->setText(QStringLiteral("总\n计\n划\n设\n置"));
        planSettingButton->setMinimumWidth(qMax(68, static_cast<int>(72 * scale)));
        planSettingButton->setMinimumHeight(0);
        planSettingButton->setStyleSheet(QString("font-size:%1px;font-weight:800;padding:12px 18px;").arg(qMax(gUiTuning.mainActionButtonMinFont, bodyFont + 4)));
        if (rightPlanContainer) {
            rightPlanContainer->setMaximumWidth(qMax(120, static_cast<int>(130 * scale)));
            rightPlanContainer->setMaximumHeight(QWIDGETSIZE_MAX);
        }
    }

    if (itemsLayout) {
        itemsLayout->setSpacing(qMax(12, static_cast<int>(14 * scale)));
    }

    if (profilePanel) {
        QVBoxLayout *profileLayout = qobject_cast<QVBoxLayout *>(profilePanel->layout());
        if (profileLayout) {
            const int profileSidePadding = qMax(16, static_cast<int>(s.width() * 0.09));
            profileLayout->setContentsMargins(profileSidePadding, qMax(6, static_cast<int>(8 * scale)), profileSidePadding, qMax(10, static_cast<int>(12 * scale)));
        }
        syncProfilePanelLayoutByNav();
    }
    const int profileBtnHeight = qMax(80, static_cast<int>(86 * scale));
    const int profileOptionBtnHeight = qMax(64, static_cast<int>(profileBtnHeight * 0.8));
    if (profileEditInfoButton) {
        profileEditInfoButton->setMinimumHeight(profileOptionBtnHeight);
    }
    if (profileInfoButton) {
        profileInfoButton->setMinimumHeight(profileOptionBtnHeight);
    }
    if (profileUpdateButton) {
        profileUpdateButton->setMinimumHeight(profileOptionBtnHeight);
    }
    if (profileResetDataButton) {
        profileResetDataButton->setMinimumHeight(profileOptionBtnHeight);
    }
    if (profileThemeSwitchButton) {
        profileThemeSwitchButton->setMinimumHeight(profileOptionBtnHeight);
    }
    if (profileFeedbackButton) {
        profileFeedbackButton->setMinimumHeight(profileOptionBtnHeight);
    }
    if (profileLogoutButton) {
        profileLogoutButton->setMinimumHeight(profileBtnHeight);
    }

    if (profileEditInfoButton) {
        profileEditInfoButton->setMaximumHeight(profileOptionBtnHeight);
    }
    if (profileInfoButton) {
        profileInfoButton->setMaximumHeight(profileOptionBtnHeight);
    }
    if (profileUpdateButton) {
        profileUpdateButton->setMaximumHeight(profileOptionBtnHeight);
    }
    if (profileResetDataButton) {
        profileResetDataButton->setMaximumHeight(profileOptionBtnHeight);
    }
    if (profileThemeSwitchButton) {
        profileThemeSwitchButton->setMaximumHeight(profileOptionBtnHeight);
    }
    if (profileFeedbackButton) {
        profileFeedbackButton->setMaximumHeight(profileOptionBtnHeight);
    }
    if (profileLogoutButton) {
        profileLogoutButton->setMaximumHeight(profileBtnHeight);
    }
}

User *MainWindow::currentUser()
{
    for (User &u : store.users) {
        if (u.id == currentUserId) {
            return &u;
        }
    }
    return nullptr;
}

//获取当前用户
const User *MainWindow::currentUser() const
{
    for (const User &u : store.users) {
        if (u.id == currentUserId) {
            return &u;
        }
    }
    return nullptr;
}

MasterPlan *MainWindow::activePlanForCurrentUser()
{
    User *u = currentUser();
    if (!u || u->activePlanId.isEmpty()) {
        return nullptr;
    }
    const int idx = findPlanIndex(store.plans, u->activePlanId);
    return idx >= 0 ? &store.plans[idx] : nullptr;
}

//获取当前用户的活动计划
const MasterPlan *MainWindow::activePlanForCurrentUser() const
{
    const User *u = currentUser();
    if (!u || u->activePlanId.isEmpty()) {
        return nullptr;
    }
    const int idx = findPlanIndex(store.plans, u->activePlanId);
    return idx >= 0 ? &store.plans[idx] : nullptr;
}

TrainingRecord *MainWindow::recordForDate(const QDate &date)
{
    const int idx = findRecordIndex(store.records, currentUserId, date);
    return idx >= 0 ? &store.records[idx] : nullptr;
}

const TrainingRecord *MainWindow::recordForDate(const QDate &date) const
{
    const int idx = findRecordIndex(store.records, currentUserId, date);
    return idx >= 0 ? &store.records[idx] : nullptr;
}



// 功能：处理相关逻辑.

void MainWindow::showAgreementDialog(bool isLocalMode)
{
    QString agreementText = mouseplan::common::AgreementTextLoader::loadAgreementTextByMode(isLocalMode);
    if (agreementText.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load the agreement text."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("User Agreement"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QTextEdit *textEdit = new QTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setText(agreementText);
    layout->addWidget(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.exec();
}

