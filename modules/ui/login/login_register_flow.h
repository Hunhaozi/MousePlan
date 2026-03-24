#ifndef LOGIN_REGISTER_FLOW_H
#define LOGIN_REGISTER_FLOW_H

#include "appdata.h"

#include <QDialog>
#include <QJsonObject>
#include <QString>
#include <functional>

namespace mouseplan {
namespace ui {
namespace login {

// 登录注册流程回调集合：用于把 UI 层与主窗口已有能力解耦。
struct LoginRegisterFlowCallbacks {
    std::function<QString(const QString &)> hashSecret;
    std::function<QString()> generateNickname;
    std::function<void(const User &)> syncUserToCloud;
    std::function<bool(const QString &, const QString &)> uploadPasswordHash;
    std::function<void(const QString &, const MasterPlan &)> syncPlanToCloud;
    std::function<void(const QString &, const QJsonObject &)> pushPackage1ToCloud;
    std::function<bool(const QString &)> verifyRegistrationCode;
    std::function<bool(const QString &, const QString &)> consumeRegistrationCode;
    std::function<void(QDialog &, QWidget *)> setupMobileDialog;
};

// 本地注册向导。
bool runLocalRegisterWizard(QWidget *parent,
                            AppDataStore &store,
                            const LoginRegisterFlowCallbacks &callbacks);

// 在线注册向导（含注册码校验流程）。
bool runOnlineRegisterWizard(QWidget *parent,
                             AppDataStore &store,
                             const LoginRegisterFlowCallbacks &callbacks);

} // namespace login
} // namespace ui
} // namespace mouseplan

#endif // LOGIN_REGISTER_FLOW_H
