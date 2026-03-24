#include "mainwindow.h"
#include <QCalendarWidget>
#include <QColor>
#include <QFont>
#include <QLayoutItem>
#include <QTextCharFormat>
#include <QVBoxLayout>

// 功能：统计指定月份已使用的补录次数。
int MainWindow::localSupplementCountForMonth(const QDate &date) const
{
    int count = 0;
    for (const TrainingRecord &record : store.records) {
        if (record.ownerUserId != currentUserId) {
            continue;
        }
        if (!record.isSupplement) {
            continue;
        }
        if (record.date.year() == date.year() && record.date.month() == date.month()) {
            ++count;
        }
    }
    return count;
}

// 功能：根据日期解析计划状态与当日索引信息，最后返回info
MainWindow::CalendarPlanInfo MainWindow::resolvePlanInfo(const QDate &date) const
{
    CalendarPlanInfo info;
    const MasterPlan *plan = activePlanForCurrentUser();    //获取当前活动的计划
    if (!plan) {
        info.message = QStringLiteral("当前无总计划，请添加计划");
        return info;
    }
    if (!plan->startDate.isValid() || plan->dayPlans.isEmpty()) {   //若开始时间和单日计划为空
        info.message = QStringLiteral("总计划不完整，请补充设置");
        return info;
    }
    info.hasPlan = true;


    const int diff = plan->startDate.daysTo(date);  //返回训练日期和当前的日期差
    if (diff < 0) {
        info.message = QStringLiteral("计划尚未开始");
        return info;
    }

    const int cycle = plan->trainDays + plan->restDays; //一整个训练周期：训练日+休息日
    if (cycle <= 0 || plan->trainDays <= 0) {
        info.message = QStringLiteral("总计划参数异常");
        return info;
    }

    const int pos = diff % cycle;   //得到当前日期的相对索引
    if (pos >= plan->trainDays) {   //大于训练日，则全是休息日
        info.isRestDay = true;
        info.message = QStringLiteral("休息日");
        return info;
    }

    info.dayPlanIndex = pos % plan->dayPlans.size();  //防止因为训练日3天，而计算只有两天的而出现越界错误emm
    info.message = plan->dayPlans[info.dayPlanIndex].title; //存储当日计划的标题
    return info;
}

// 功能：按计划日模板构建训练记录日对象。
RecordDay MainWindow::buildRecordDayFromPlan(const DayPlan &dayPlan) const
{
    RecordDay recordDay;
    recordDay.title = dayPlan.title;
    for (const WorkoutItem &item : dayPlan.items) {
        RecordItem ri;
        ri.item = item;
        ri.completed = false;
        ri.warmupChecked = QVector<bool>(item.warmupSets.size(), false);
        ri.workChecked = QVector<bool>(item.workSets.size(), false);
        recordDay.items.push_back(ri);
    }
    return recordDay;
}

// 功能：确保选中日期存在可编辑的训练记录副本。
void MainWindow::ensureRecordFromSelectedDayPlan()
{
    const CalendarPlanInfo info = resolvePlanInfo(selectedDate);
    if (!info.hasPlan || info.isRestDay || info.dayPlanIndex < 0) {
        return;
    }
    MasterPlan *plan = activePlanForCurrentUser();
    if (!plan || info.dayPlanIndex >= plan->dayPlans.size()) {
        return;
    }

    TrainingRecord *record = recordForDate(selectedDate);
    if (!record) {
        TrainingRecord newRecord;
        newRecord.ownerUserId = currentUserId;
        newRecord.date = selectedDate;
        newRecord.day = buildRecordDayFromPlan(plan->dayPlans[info.dayPlanIndex]);
        newRecord.submitted = false;
        store.records.push_back(newRecord);
    } else {
        RecordDay rebuilt = buildRecordDayFromPlan(plan->dayPlans[info.dayPlanIndex]);
        const int keepCount = qMin(record->day.items.size(), rebuilt.items.size());
        for (int i = 0; i < keepCount; ++i) {
            const RecordItem &oldItem = record->day.items[i];
            RecordItem &newItem = rebuilt.items[i];
            bool sameItemContent = (oldItem.item.name == newItem.item.name)
                                   && (oldItem.item.restSeconds == newItem.item.restSeconds)
                                   && (oldItem.item.warmupSets.size() == newItem.item.warmupSets.size())
                                   && (oldItem.item.workSets.size() == newItem.item.workSets.size());

            if (sameItemContent) {
                for (int s = 0; s < oldItem.item.warmupSets.size(); ++s) {
                    const PlanSet &a = oldItem.item.warmupSets[s];
                    const PlanSet &b = newItem.item.warmupSets[s];
                    if (qAbs(a.weightKg - b.weightKg) > 0.05 || a.reps != b.reps || a.remark != b.remark) {
                        sameItemContent = false;
                        break;
                    }
                }
            }
            if (sameItemContent) {
                for (int s = 0; s < oldItem.item.workSets.size(); ++s) {
                    const PlanSet &a = oldItem.item.workSets[s];
                    const PlanSet &b = newItem.item.workSets[s];
                    if (qAbs(a.weightKg - b.weightKg) > 0.05 || a.reps != b.reps || a.remark != b.remark) {
                        sameItemContent = false;
                        break;
                    }
                }
            }

            if (sameItemContent) {
                newItem.completed = oldItem.completed;
                newItem.ignored = oldItem.ignored;
                for (int w = 0; w < qMin(oldItem.warmupChecked.size(), newItem.warmupChecked.size()); ++w) {
                    newItem.warmupChecked[w] = oldItem.warmupChecked[w];
                }
                for (int w = 0; w < qMin(oldItem.workChecked.size(), newItem.workChecked.size()); ++w) {
                    newItem.workChecked[w] = oldItem.workChecked[w];
                }
            }
        }
        record->day = rebuilt;
    }
}

// 功能：重绘日历日期样式与打卡标记。
void MainWindow::rebuildCalendarFormats()
{
    if (!calendar) {
        return;
    }

    for (int offset = -365; offset <= 365; ++offset) {
        const QDate d = QDate::currentDate().addDays(offset);
        QTextCharFormat fmt;
        if (d < QDate::currentDate()) {
            fmt.setForeground(QColor("#9aa7a0"));
        } else {
            fmt.setForeground(QColor("#1f2f26"));
        }

        if (d == QDate::currentDate()) {
            fmt.setBackground(QColor("#8fbca0"));
            fmt.setFontWeight(QFont::Bold);
        }

        const TrainingRecord *record = recordForDate(d);
        if (record && record->submitted) {
            fmt.setForeground(QColor("#1f4a35"));
            fmt.setFontWeight(QFont::Black);
        }
        calendar->setDateTextFormat(d, fmt);
    }
    if (calendar) {
        calendar->update();
    }
}

// 功能：清空当日项目卡片容器中的所有控件。
void MainWindow::clearItemCards()
{
    while (itemsLayout->count() > 0) {
        QLayoutItem *child = itemsLayout->takeAt(0);
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
}

// 功能：生成训练组简要文本（组数x次数）。
QString MainWindow::setListSummary(const QVector<PlanSet> &sets) const
{
    if (sets.isEmpty()) {
        return QStringLiteral("0x0");
    }
    return QString("%1x%2").arg(sets.size()).arg(sets.front().reps);
}
