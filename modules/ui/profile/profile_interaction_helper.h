#ifndef PROFILE_INTERACTION_HELPER_H
#define PROFILE_INTERACTION_HELPER_H

#include "appdata.h"

#include <QJsonObject>
#include <QString>

namespace mouseplan {
namespace ui {
namespace profile {

// 我的界面辅助模块：抽离文案与反馈数据组装。
class ProfileInteractionHelper {
public:
    // 软件信息弹窗内容。
    static QString softwareInfoText();

    // 构建反馈提交 JSON 载荷。
    static QJsonObject buildFeedbackPayload(const User &user, const QString &content, const QString &submittedAtIso);
};

} // namespace profile
} // namespace ui
} // namespace mouseplan

#endif // PROFILE_INTERACTION_HELPER_H
