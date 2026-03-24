#ifndef UPDATE_CLIENT_HELPER_H
#define UPDATE_CLIENT_HELPER_H

#include <QJsonObject>
#include <QString>

namespace mouseplan {
namespace common {

// 更新客户端辅助工具：版本比较、版本文本、安装包URL解析与安全文件版本字符串。
class UpdateClientHelper {
public:
    static int compareVersionText(const QString &left, const QString &right);
    static QString currentAppVersionText();
    static QString resolveUpdatePackageUrl(const QJsonObject &response);
    static QString safeUpdateFileVersion(const QString &versionText);
};

} // namespace common
} // namespace mouseplan

#endif // UPDATE_CLIENT_HELPER_H
