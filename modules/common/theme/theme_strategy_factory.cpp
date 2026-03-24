#include "modules/common/theme/theme_strategy_factory.h"

#include "modules/common/theme/theme_feature_gate.h"

namespace mouseplan {
namespace common {
namespace theme {

QVector<ThemeStrategyDescriptor> ThemeStrategyFactory::buildStrategies()
{
    QVector<ThemeStrategyDescriptor> list;

    ThemeStrategyDescriptor fitness;
    fitness.code = QStringLiteral("fitness");
    fitness.displayName = QStringLiteral("健身主题");
    fitness.available = ThemeFeatureGate::isThemeAvailable(fitness.code);
    list.push_back(fitness);

    ThemeStrategyDescriptor study;
    study.code = QStringLiteral("study");
    study.displayName = QStringLiteral("学习主题");
    study.available = ThemeFeatureGate::isThemeAvailable(study.code);
    list.push_back(study);

    ThemeStrategyDescriptor normal;
    normal.code = QStringLiteral("normal");
    normal.displayName = QStringLiteral("普通计划主题");
    normal.available = ThemeFeatureGate::isThemeAvailable(normal.code);
    list.push_back(normal);

    return list;
}

QString ThemeStrategyFactory::resolveAvailableTheme(const QString &preferredCode)
{
    const QVector<ThemeStrategyDescriptor> list = buildStrategies();

    for (const ThemeStrategyDescriptor &item : list) {
        if (item.code == preferredCode && item.available) {
            return item.code;
        }
    }

    for (const ThemeStrategyDescriptor &item : list) {
        if (item.available) {
            return item.code;
        }
    }

    return QStringLiteral("fitness");
}

} // namespace theme
} // namespace common
} // namespace mouseplan
