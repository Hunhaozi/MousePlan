#ifndef THEME_FEATURE_GATE_H
#define THEME_FEATURE_GATE_H

#include <QString>
#include <QWidget>

namespace mouseplan {
namespace common {

// 主题能力开关层：当前仅开放健身主题。
class ThemeFeatureGate {
public:
    static bool isThemeAvailable(const QString &themeCode);
    static void showThemeUnavailableHint(QWidget *parent, const QString &themeCode);
};

} // namespace common
} // namespace mouseplan

#endif // THEME_FEATURE_GATE_H
