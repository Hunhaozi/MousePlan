#include "modules/themes/fitness/data/fitness_data_repository.h"

namespace mouseplan {
namespace fitness {

FitnessDataRepository::FitnessDataRepository(const AppDataStore &storeRef)
    : store(storeRef)
{}

FitnessCalendarDayView FitnessDataRepository::queryCalendarDayView(const QString &userId, const QDate &date) const
{
    FitnessCalendarDayView view;
    view.date = date;

    for (const TrainingRecord &record : store.records) {
        if (record.ownerUserId == userId && record.date == date) {
            view.hasRecord = true;
            view.submitted = record.submitted;
            view.supplement = record.isSupplement;
            if (record.isSupplement) {
                view.marker = QStringLiteral("补");
            }
            break;
        }
    }

    return view;
}

} // namespace fitness
} // namespace mouseplan
