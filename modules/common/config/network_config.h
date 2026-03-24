#ifndef MOUSEPLAN_NETWORK_CONFIG_H
#define MOUSEPLAN_NETWORK_CONFIG_H

#include <QString>
#include <QSettings>
#include <QtGlobal>



namespace mouseplan {
namespace common {
namespace config {

// 在线服务基础地址环境变量名。
static constexpr const char *kEnvServerBaseUrl = "MOUSEPLAN_SERVER_BASE_URL";
// 在线服务超时环境变量名（单位：毫秒）。
static constexpr const char *kEnvServerTimeoutMs = "MOUSEPLAN_SERVER_TIMEOUT_MS";
static constexpr const char *kSettingsServerBaseUrl = "network/serverBaseUrl";
static constexpr const char *kSettingsServerTimeoutMs = "network/serverTimeoutMs";

// 默认值：无效地址，仅用于提示未配置。
static constexpr const char *kDefaultServerBaseUrl = "http://mouseplan.haozi-haozi.cn:55555/";
static constexpr int kDefaultServerTimeoutMs = 8000;

inline void saveServerConfig(const QString &baseUrl, int timeoutMs)
{
    QSettings settings;
    const QString normalizedUrl = baseUrl.trimmed();
    if (normalizedUrl.isEmpty()) {
        settings.remove(QString::fromLatin1(kSettingsServerBaseUrl));
    } else {
        settings.setValue(QString::fromLatin1(kSettingsServerBaseUrl), normalizedUrl);
    }

    if (timeoutMs <= 0) {
        settings.remove(QString::fromLatin1(kSettingsServerTimeoutMs));
    } else {
        settings.setValue(QString::fromLatin1(kSettingsServerTimeoutMs), timeoutMs);
    }
}

inline QString resolveServerBaseUrl()
{
    QSettings settings;
    const QString savedUrl = settings.value(QString::fromLatin1(kSettingsServerBaseUrl)).toString().trimmed();
    if (!savedUrl.isEmpty()) {
        return savedUrl;
    }

    const QString envBaseUrl = qEnvironmentVariable(kEnvServerBaseUrl).trimmed();
    if (!envBaseUrl.isEmpty()) {
        return envBaseUrl;
    }

    // 返回配置文件中的默认地址
    return QString::fromLatin1(kDefaultServerBaseUrl);
}

inline int resolveServerTimeoutMs()
{
    QSettings settings;
    bool ok = false;
    const int savedTimeout = settings.value(QString::fromLatin1(kSettingsServerTimeoutMs)).toString().trimmed().toInt(&ok);
    if (ok && savedTimeout > 0) {
        return savedTimeout;
    }

    const int envTimeout = qEnvironmentVariableIntValue(kEnvServerTimeoutMs);
    if (envTimeout > 0) {
        return envTimeout;
    }
    return kDefaultServerTimeoutMs;
}

} // namespace config
} // namespace common
} // namespace mouseplan

#endif // MOUSEPLAN_NETWORK_CONFIG_H
