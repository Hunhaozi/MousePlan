#ifndef FITNESS_PLAN_FLOW_HELPER_H
#define FITNESS_PLAN_FLOW_HELPER_H

#include <QDate>
#include <QString>

namespace mouseplan {
namespace fitness {

// 健身计划流程辅助模块：抽离计划预览/提交场景下的纯判定逻辑。
class FitnessPlanFlowHelper {
public:
    // 生成日视图标题后缀（当日/历史/未来）。
    static QString buildDayTitleSuffix(const QDate &selectedDate,
                                       const QDate &today,
                                       bool isRestDay);

    // 判断提交按钮是否可用（仅当天且未提交）。
    static bool canSubmitTodayRecord(const QDate &selectedDate,
                                     const QDate &today,
                                     bool alreadySubmitted);

    // 历史日期是否应该显示补录入口。
    static bool shouldShowSupplementEntry(const QDate &selectedDate,
                                          const QDate &today,
                                          bool hasRecord);
};

} // namespace fitness
} // namespace mouseplan

#endif // FITNESS_PLAN_FLOW_HELPER_H
