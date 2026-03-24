#ifndef FITNESS_CALENDAR_MARK_BUILDER_H
#define FITNESS_CALENDAR_MARK_BUILDER_H

#include "appdata.h"

#include <QDate>
#include <QString>

namespace mouseplan {
namespace fitness {

// 日历单元格展示数据：marker 显示在日期下方，submitted 控制右上角是否打勾。
struct CalendarCellMark {
    QString marker;
    bool submitted = false;
};

// 健身主题日历标记构建器。
// 目标：在不改动主业务逻辑的前提下，把日期标记判定从主窗口中抽离为独立模块。
class FitnessCalendarMarkBuilder {
public:
    // 根据日期上下文和记录状态生成最终标记结果。
    static CalendarCellMark buildMark(const QDate &date,
                                      const QDate &today,
                                      bool hasPlan,
                                      bool isRestDay,
                                      const TrainingRecord *record);
};

} // namespace fitness
} // namespace mouseplan

#endif // FITNESS_CALENDAR_MARK_BUILDER_H
