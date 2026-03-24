#include "modules/common/agreement/agreement_text_loader.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mouseplan {
namespace common {

QString AgreementTextLoader::findConfigFilePath(const QString &fileName)
{
    QStringList candidates;
    const QString appDir = QApplication::applicationDirPath();

    candidates << appDir + "/config/" + fileName;
    candidates << QDir::currentPath() + "/config/" + fileName;

    QDir walker(appDir);
    for (int i = 0; i < 8; ++i) {
        candidates << walker.filePath("config/" + fileName);
        if (!walker.cdUp()) {
            break;
        }
    }

#ifdef Q_OS_ANDROID
    candidates << ":/config/" + fileName;
    candidates << QStringLiteral("assets:/config/") + fileName;
#endif

    QString bestPath;
    QDateTime bestMtime;
    for (const QString &path : candidates) {
        if (path.isEmpty()) {
            continue;
        }
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) {
            continue;
        }
        if (bestPath.isEmpty() || fi.lastModified() > bestMtime) {
            bestPath = path;
            bestMtime = fi.lastModified();
        }
    }
    return bestPath;
}

QString AgreementTextLoader::parseAgreementTextFromRaw(const QByteArray &raw)
{
    const QString plain = QString::fromUtf8(raw).trimmed();
    if (plain.isEmpty()) {
        return QString();
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonArray cells = doc.object().value("cells").toArray();
        QStringList lines;
        for (const QJsonValue &cellVal : cells) {
            const QJsonObject cellObj = cellVal.toObject();
            if (cellObj.value("cell_type").toString() != QStringLiteral("markdown")) {
                continue;
            }
            const QJsonArray src = cellObj.value("source").toArray();
            for (const QJsonValue &v : src) {
                lines << v.toString();
            }
        }
        const QString parsed = lines.join(QString());
        if (!parsed.trimmed().isEmpty()) {
            return parsed;
        }
    }

    return plain;
}

QString AgreementTextLoader::loadAgreementTextByMode(bool isLocalMode)
{
    const QString modeFile = isLocalMode ? QStringLiteral(":/config/agreement_local.md")
                                         : QStringLiteral(":/config/agreement_online.md");
    const QString fallbackFile = QStringLiteral(":/config/agreement.md");

    QStringList candidates;
    candidates << modeFile;
    candidates << fallbackFile;

    for (const QString &path : candidates) {
        QFile file(path);
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray raw = file.readAll();
            file.close();
            return parseAgreementTextFromRaw(raw);
        }
    }

    return QString();
}

} // namespace common
} // namespace mouseplan
