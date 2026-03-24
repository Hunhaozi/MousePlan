#ifndef FITNESS_DATA_MODELS_H
#define FITNESS_DATA_MODELS_H

#include <QDate>
#include <QString>

namespace mouseplan {
namespace fitness {

// 健身主题日历维度的轻量视图模型。
struct FitnessCalendarDayView {
    QDate date;
    bool hasRecord = false;
    bool submitted = false;
    bool supplement = false;
    QString marker;
};

} // namespace fitness
} // namespace mouseplan

#endif // FITNESS_DATA_MODELS_H
