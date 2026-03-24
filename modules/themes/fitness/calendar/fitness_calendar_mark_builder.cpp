#include "modules/themes/fitness/calendar/fitness_calendar_mark_builder.h"

namespace mouseplan {
namespace fitness {

CalendarCellMark FitnessCalendarMarkBuilder::buildMark(const QDate &date,
                                                       const QDate &today,
                                                       bool hasPlan,
                                                       bool isRestDay,
                                                       const TrainingRecord *record)
{
    CalendarCellMark mark;

    if (date > today) {
        if (hasPlan) {
            mark.marker = isRestDay ? QStringLiteral("休") : QStringLiteral("训");
        }
    } else if (date < today) {
        if (record) {
            if (record->isSupplement) {
                mark.marker = QStringLiteral("补");
            } else if (record->submitted) {
                if (hasPlan) {
                    mark.marker = isRestDay ? QStringLiteral("休") : QStringLiteral("训");
                } else {
                    mark.marker = record->day.items.isEmpty()
                                      ? QStringLiteral("休")
                                      : QStringLiteral("训");
                }
            }
        }
    } else {
        if (hasPlan) {
            mark.marker = isRestDay ? QStringLiteral("休") : QStringLiteral("训");
        }
    }

    mark.submitted = (record && (record->submitted || (record->isSupplement && date != today)));
    return mark;
}

} // namespace fitness
} // namespace mouseplan
