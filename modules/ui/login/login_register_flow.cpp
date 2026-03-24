#include "modules/ui/login/login_register_flow.h"

#include <QFrame>
#include <QGraphicsBlurEffect>
#include <QGraphicsEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QUuid>

namespace {

// 账号密码注册对话框：本地与在线共用。
bool runAccountPasswordRegisterDialog(QWidget *parent,
                                      AppDataStore &store,
                                      bool isLocalAccount,
                                      const QString &registerTitle,
                                      const mouseplan::ui::login::LoginRegisterFlowCallbacks &callbacks)
{
    QGraphicsEffect *oldEffect = parent ? parent->graphicsEffect() : nullptr;
    QGraphicsBlurEffect *blur = nullptr;
    if (parent && !oldEffect) {
        blur = new QGraphicsBlurEffect(parent);
        blur->setBlurRadius(18.0);
        parent->setGraphicsEffect(blur);
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(registerTitle);
    dialog.setModal(true);

    QRect targetRect;
    if (parent) {
        targetRect = QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size());
    }
    if (!targetRect.isValid() || targetRect.isEmpty()) {
        QScreen *screen = QGuiApplication::primaryScreen();
        targetRect = screen ? screen->availableGeometry() : QRect(0, 0, 1080, 1920);
    }

    const int w = static_cast<int>(targetRect.width() * 0.95);
    const int h = static_cast<int>(targetRect.height() * 0.78);
    const int x = targetRect.x() + (targetRect.width() - w) / 2;
    const int y = targetRect.y() + (targetRect.height() - h) / 2;
    dialog.setGeometry(x, y, w, h);
    dialog.setMinimumSize(w, h);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(34, 30, 34, 30);
    layout->setSpacing(22);

    QLabel *titleLabel = new QLabel(isLocalAccount ? QStringLiteral("本地模式账号注册") : QStringLiteral("在线模式账号注册"), &dialog);
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size:52px;font-weight:900;color:#23382d;");

    QLineEdit *usernameEdit = new QLineEdit(&dialog);
    usernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    usernameEdit->setClearButtonEnabled(true);
    usernameEdit->setStyleSheet("font-size:42px;min-height:220px;padding:10px 18px;border-radius:18px;border:2px solid #cfded5;background:#ffffff;");

    QLineEdit *passwordEdit = new QLineEdit(&dialog);
    passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet("font-size:42px;min-height:220px;padding:10px 18px;border-radius:18px;border:2px solid #cfded5;background:#ffffff;");

    QLineEdit *confirmEdit = new QLineEdit(&dialog);
    confirmEdit->setPlaceholderText(QStringLiteral("请确认密码"));
    confirmEdit->setEchoMode(QLineEdit::Password);
    confirmEdit->setStyleSheet("font-size:42px;min-height:220px;padding:10px 18px;border-radius:18px;border:2px solid #cfded5;background:#ffffff;");

    QHBoxLayout *ops = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(QStringLiteral("确认注册"), &dialog);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    okBtn->setStyleSheet("font-size:44px;min-height:210px;background:#2f8f46;color:white;border-radius:18px;");
    cancelBtn->setStyleSheet("font-size:44px;min-height:210px;background:#9ca5a1;color:white;border-radius:18px;");
    ops->addWidget(okBtn);
    ops->addWidget(cancelBtn);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    layout->addStretch(1);
    layout->addWidget(titleLabel);
    layout->addWidget(usernameEdit);
    layout->addWidget(passwordEdit);
    layout->addWidget(confirmEdit);
    layout->addLayout(ops);
    layout->addStretch(1);

    const int result = dialog.exec();
    if (parent && parent->graphicsEffect() == blur) {
        parent->setGraphicsEffect(oldEffect);
    }

    if (result != QDialog::Accepted) {
        return false;
    }

    const QString username = usernameEdit->text().trimmed();
    const QString password = passwordEdit->text();
    const QString confirm = confirmEdit->text();

    if (username.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("用户名不能为空。"));
        return false;
    }

    for (const User &u : store.users) {
        if (u.isLocalAccount == isLocalAccount && u.username == username) {
            QMessageBox::warning(parent,
                                 QStringLiteral("提示"),
                                 isLocalAccount
                                     ? QStringLiteral("本地模式下用户名已存在。")
                                     : QStringLiteral("在线模式下用户名已存在。"));
            return false;
        }
    }

    if (password.isEmpty() || password != confirm) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("两次密码不一致或为空。"));
        return false;
    }

    User user;
    user.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    user.username = username;
    user.nickname = callbacks.generateNickname ? callbacks.generateNickname() : QStringLiteral("用户");
    user.password = callbacks.hashSecret ? callbacks.hashSecret(password) : password;
    user.isLocalAccount = isLocalAccount;
    user.theme = QStringLiteral("fitness");
    user.themeChosen = false;

    store.users.push_back(user);
    QString defaultPlanId;
    store.addMouseDefaultPresetPlan(user.id, &defaultPlanId, true);
    if (!isLocalAccount) {
        if (callbacks.syncUserToCloud) {
            callbacks.syncUserToCloud(user);
        }
        if (callbacks.uploadPasswordHash && !callbacks.uploadPasswordHash(user.id, user.password)) {
            QMessageBox::warning(parent,
                                 QStringLiteral("提示"),
                                 QStringLiteral("在线模式：密码哈希上送失败，请检查网络后重试。"));
        }
        if (!defaultPlanId.trimmed().isEmpty() && callbacks.syncPlanToCloud) {
            for (const MasterPlan &p : store.plans) {
                if (p.id == defaultPlanId) {
                    callbacks.syncPlanToCloud(user.id, p);
                    break;
                }
            }
        }
    }

    if (isLocalAccount) {
        store.save();
        store.saveUserPackage1Local(user.id);
    } else if (callbacks.pushPackage1ToCloud) {
        const QJsonObject snapshot = store.buildUserPackage1Snapshot(user.id);
        if (!snapshot.isEmpty()) {
            callbacks.pushPackage1ToCloud(user.id, snapshot);
        }
    }

    QMessageBox::information(parent,
                             QStringLiteral("提示"),
                             isLocalAccount
                                 ? QStringLiteral("本地账号注册成功，请登录。")
                                 : QStringLiteral("在线账号注册成功，已上传基础用户信息。请登录。"));
    return true;
}

} // namespace

namespace mouseplan {
namespace ui {
namespace login {

// 在线注册：先过注册码，再进入账号注册。
bool runOnlineRegisterWizard(QWidget *parent,
                             AppDataStore &store,
                             const LoginRegisterFlowCallbacks &callbacks)
{
    QDialog codeDialog(parent);
    if (callbacks.setupMobileDialog) {
        callbacks.setupMobileDialog(codeDialog, parent);
    }
    codeDialog.setWindowTitle(QStringLiteral("注册码验证"));

    QVBoxLayout *codeLayout = new QVBoxLayout(&codeDialog);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(0);

    int navBlank = qMax(120, parent ? parent->height() / 13 : 120);
    if (parent) {
        if (QFrame *navCard = parent->findChild<QFrame *>(QStringLiteral("navBarCard"))) {
            navBlank = qMax(120, navCard->height());
        }
    }
    QFrame *topBlank = new QFrame(&codeDialog);
    topBlank->setFixedHeight(navBlank);
    topBlank->setStyleSheet("background:#e7f3ea;");
    QFrame *bottomBlank = new QFrame(&codeDialog);
    bottomBlank->setFixedHeight(navBlank);
    bottomBlank->setStyleSheet("background:#e7f3ea;");

    QWidget *center = new QWidget(&codeDialog);
    center->setStyleSheet("background:#e7f3ea;");
    QVBoxLayout *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(26, 22, 26, 22);
    centerLayout->setSpacing(0);

    QFrame *centerCard = new QFrame(center);
    centerCard->setStyleSheet("background:#ffffff;border:2px solid #cde4d2;border-radius:20px;");
    QVBoxLayout *cardLayout = new QVBoxLayout(centerCard);
    cardLayout->setContentsMargins(22, 20, 22, 20);
    cardLayout->setSpacing(16);

    QLabel *codeLabel = new QLabel(QStringLiteral("请输入16位注册码"), centerCard);
    codeLabel->setAlignment(Qt::AlignCenter);
    codeLabel->setStyleSheet("font-size:26px;font-weight:800;color:#2b3e33;");

    // 链接提示
    QLabel *linkLabel = new QLabel(QStringLiteral("如果需要注册码，可以<a href=\"https://blog.haozi-haozi.cn/2026/03/20/Qt-MousePlan-Android/#%E7%95%99%E8%A8%80\">MouseBlog</a>，然后在文章底部发送邮箱或留言申请"), centerCard);
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setStyleSheet("font-size:20px;color:#2b8bf2;");
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setTextFormat(Qt::RichText);

    QLineEdit *codeEdit = new QLineEdit(centerCard);
    codeEdit->setPlaceholderText(QStringLiteral("16位注册码"));
    codeEdit->setMaxLength(16);
    codeEdit->setAlignment(Qt::AlignCenter);
    codeEdit->setStyleSheet("font-size:26px;min-height:84px;padding:14px 16px;border:2px solid #cfe1d4;border-radius:14px;background:#fbfdfb;");

    QHBoxLayout *codeButtons = new QHBoxLayout();
    codeButtons->setSpacing(14);
    QPushButton *confirmBtn = new QPushButton(QStringLiteral("确认"), centerCard);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), centerCard);
    confirmBtn->setStyleSheet("font-size:30px;min-height:86px;font-weight:900;padding:12px 14px;border-radius:14px;background:#2f8f46;color:white;border:none;");
    cancelBtn->setStyleSheet("font-size:30px;min-height:86px;font-weight:900;padding:12px 14px;border-radius:14px;background:#9ca5a1;color:white;border:none;");
    codeButtons->addWidget(confirmBtn);
    codeButtons->addWidget(cancelBtn);
    QObject::connect(confirmBtn, &QPushButton::clicked, &codeDialog, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, &codeDialog, &QDialog::reject);

    cardLayout->addWidget(codeLabel);
    cardLayout->addWidget(linkLabel);
    cardLayout->addWidget(codeEdit);
    cardLayout->addLayout(codeButtons);
    centerLayout->addStretch(1);
    centerLayout->addWidget(centerCard);
    centerLayout->addStretch(1);

    codeLayout->addWidget(topBlank);
    codeLayout->addWidget(center, 1);
    codeLayout->addWidget(bottomBlank);

    if (codeDialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString code = codeEdit->text().trimmed();
    if (code.size() != 16) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("注册码必须为16位。"));
        return false;
    }

    const QString codeHash = callbacks.hashSecret ? callbacks.hashSecret(code) : code;

    if (!callbacks.verifyRegistrationCode || !callbacks.verifyRegistrationCode(codeHash)) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("注册码在线校验失败，请稍后重试。"));
        return false;
    }

    if (!runAccountPasswordRegisterDialog(parent, store, false, QStringLiteral("在线模式注册"), callbacks)) {
        return false;
    }

    const User &newUser = store.users.last();
    if (!callbacks.consumeRegistrationCode || !callbacks.consumeRegistrationCode(codeHash, newUser.id)) {
        QMessageBox::warning(parent,
                             QStringLiteral("提示"),
                             QStringLiteral("注册码状态更新失败，请稍后重试。为避免重复占用，本次注册已取消。"));
        for (int i = store.users.size() - 1; i >= 0; --i) {
            if (store.users[i].id == newUser.id) {
                store.users.removeAt(i);
                break;
            }
        }
        return false;
    }

    return true;
}

// 本地注册：直接进入账号密码注册。
bool runLocalRegisterWizard(QWidget *parent,
                            AppDataStore &store,
                            const LoginRegisterFlowCallbacks &callbacks)
{
    return runAccountPasswordRegisterDialog(parent,
                                            store,
                                            true,
                                            QStringLiteral("本地模式注册"),
                                            callbacks);
}

} // namespace login
} // namespace ui
} // namespace mouseplan
