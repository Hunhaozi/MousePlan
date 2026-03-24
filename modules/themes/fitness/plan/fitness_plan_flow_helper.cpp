#include "modules/themes/fitness/plan/fitness_plan_flow_helper.h"

namespace mouseplan {
namespace fitness {

QString FitnessPlanFlowHelper::buildDayTitleSuffix(const QDate &selectedDate,
                                                   const QDate &today,
                                                   bool isRestDay)
{
    if (selectedDate > today) {
        return isRestDay ? QStringLiteral("预期为休息日") : QStringLiteral("预期训练计划");
    }
    if (selectedDate < today) {
        return QStringLiteral("历史训练计划");
    }
    return QStringLiteral("当日计划");
}

bool FitnessPlanFlowHelper::canSubmitTodayRecord(const QDate &selectedDate,
                                                 const QDate &today,
                                                 bool alreadySubmitted)
{
    return selectedDate == today && !alreadySubmitted;
}

bool FitnessPlanFlowHelper::shouldShowSupplementEntry(const QDate &selectedDate,
                                                      const QDate &today,
                                                      bool hasRecord)
{
    return selectedDate < today && !hasRecord;
}

} // namespace fitness
} // namespace mouseplan
