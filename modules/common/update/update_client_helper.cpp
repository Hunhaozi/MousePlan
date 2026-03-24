#include "modules/common/update/update_client_helper.h"

#include <QRegularExpression>
#include <QUrl>
#include <QVector>

namespace {

QVector<int> parseVersionToSegments(const QString &text)
{
    QString t = text.trimmed();
    if (t.startsWith(QStringLiteral("v"), Qt::CaseInsensitive)) {
        t = t.mid(1);
    }

    const QStringList rawParts = t.split(QRegularExpression("[^0-9]+"), QString::SkipEmptyParts);
    QVector<int> result;
    result.reserve(rawParts.size());
    for (const QString &part : rawParts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        result.push_back(ok ? value : 0);
    }
    while (!result.isEmpty() && result.last() == 0) {
        result.removeLast();
    }
    if (result.isEmpty()) {
        result.push_back(0);
    }
    return result;
}

} // namespace

namespace mouseplan {
namespace common {

int UpdateClientHelper::compareVersionText(const QString &left, const QString &right)
{
    const QVector<int> a = parseVersionToSegments(left);
    const QVector<int> b = parseVersionToSegments(right);
    const int n = qMax(a.size(), b.size());
    for (int i = 0; i < n; ++i) {
        const int av = (i < a.size()) ? a[i] : 0;
        const int bv = (i < b.size()) ? b[i] : 0;
        if (av < bv) {
            return -1;
        }
        if (av > bv) {
            return 1;
        }
    }
    return 0;
}

QString UpdateClientHelper::currentAppVersionText()
{
    return QStringLiteral("1.00");
}

QString UpdateClientHelper::resolveUpdatePackageUrl(const QJsonObject &response)
{
    const QString absUrl = response.value(QStringLiteral("apkUrl")).toString().trimmed();
    if (!absUrl.isEmpty()) {
        return absUrl;
    }

    const QString path = response.value(QStringLiteral("apkPath")).toString().trimmed();
    if (path.isEmpty()) {
        return QString();
    }

    if (path.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || path.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return path;
    }

    return QString();
}

QString UpdateClientHelper::safeUpdateFileVersion(const QString &versionText)
{
    QString v = versionText.trimmed();
    if (v.startsWith(QStringLiteral("v"), Qt::CaseInsensitive)) {
        v = v.mid(1);
    }
    v.replace(QRegularExpression(QStringLiteral("[^0-9._-]")), QStringLiteral("_"));
    if (v.isEmpty()) {
        v = QStringLiteral("latest");
    }
    return v;
}

} // namespace common
} // namespace mouseplan
