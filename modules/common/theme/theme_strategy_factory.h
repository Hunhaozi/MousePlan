#ifndef THEME_STRATEGY_FACTORY_H
#define THEME_STRATEGY_FACTORY_H

#include <QString>
#include <QVector>

namespace mouseplan {
namespace common {
namespace theme {

struct ThemeStrategyDescriptor {
    QString code;
    QString displayName;
    bool available = false;
};

class ThemeStrategyFactory {
public:
    static QVector<ThemeStrategyDescriptor> buildStrategies();
    static QString resolveAvailableTheme(const QString &preferredCode);
};

} // namespace theme
} // namespace common
} // namespace mouseplan

#endif // THEME_STRATEGY_FACTORY_H
