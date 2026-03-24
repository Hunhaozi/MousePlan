#include "modules/ui/profile/profile_interaction_helper.h"

namespace mouseplan {
namespace ui {
namespace profile {

QString ProfileInteractionHelper::softwareInfoText()
{
    return QStringLiteral("Mouse Plan 移动训练计划应用\n当前版本:v1.00\n开发者: Mouse\n博客:https://blog.haozi-haozi.cn/\n欢迎反馈和建议！");
}

QJsonObject ProfileInteractionHelper::buildFeedbackPayload(const User &user,
                                                           const QString &content,
                                                           const QString &submittedAtIso)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), user.id);
    payload.insert(QStringLiteral("username"), user.username);
    payload.insert(QStringLiteral("content"), content);
    payload.insert(QStringLiteral("submittedAt"), submittedAtIso);
    return payload;
}

} // namespace profile
} // namespace ui
} // namespace mouseplan
