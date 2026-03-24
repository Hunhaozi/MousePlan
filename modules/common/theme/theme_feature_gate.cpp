#include "modules/common/theme/theme_feature_gate.h"

#include <QMessageBox>

namespace mouseplan {
namespace common {

bool ThemeFeatureGate::isThemeAvailable(const QString &themeCode)
{
    return themeCode == QStringLiteral("fitness");
}

void ThemeFeatureGate::showThemeUnavailableHint(QWidget *parent, const QString &themeCode)
{
    if (themeCode == QStringLiteral("study")) {
        QMessageBox::information(parent,
                                 QStringLiteral("主题切换"),
                                 QStringLiteral("学习主题还未更新，敬请期待。"));
        return;
    }

    if (themeCode == QStringLiteral("normal")) {
        QMessageBox::information(parent,
                                 QStringLiteral("主题切换"),
                                 QStringLiteral("普通计划主题还未更新，敬请期待。"));
        return;
    }

    QMessageBox::information(parent,
                             QStringLiteral("主题切换"),
                             QStringLiteral("该主题还未更新，敬请期待。"));
}

} // namespace common
} // namespace mouseplan
