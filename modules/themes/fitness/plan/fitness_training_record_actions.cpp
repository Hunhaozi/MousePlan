#include "mainwindow.h"

#include "modules/common/ui/runtime_dialog_helpers.h"

#include <QDate>
#include <QMessageBox>
#include <QPushButton>

namespace {

bool confirmSupplementSave(QWidget *parent, const QString &text)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("确认保存补录"));
    box.setText(text);
    QAbstractButton *confirmBtn = new QPushButton(QStringLiteral("确认保存"));
    box.addButton(confirmBtn, QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("返回修改"), QMessageBox::RejectRole);
    box.exec();
    return box.clickedButton() == confirmBtn;
}

} // namespace

// 功能：提交当日训练记录并同步数据。
void MainWindow::submitTodayRecord()
{
    if (selectedDate != QDate::currentDate()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("只能提交当天日期的训练记录。"));
        return;
    }

    const CalendarPlanInfo info = resolvePlanInfo(selectedDate);
    if (!info.hasPlan) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前日期无可提交训练计划。"));
        return;
    }

    if (info.isRestDay) {
        TrainingRecord *restRecord = recordForDate(selectedDate);
        if (!restRecord) {
            TrainingRecord newRecord;
            newRecord.ownerUserId = currentUserId;
            newRecord.date = selectedDate;
            newRecord.submitted = true;
            newRecord.totalMinutes = 0;
            newRecord.day.title = QStringLiteral("休息日");
            store.records.push_back(newRecord);
            restRecord = &store.records.back();
        } else {
            restRecord->submitted = true;
            restRecord->totalMinutes = 0;
            restRecord->day.title = QStringLiteral("休息日");
            restRecord->day.items.clear();
        }
        store.save();
        persistCurrentUserPackage1();
        syncRecordToRemote(*restRecord);
        rebuildCalendarFormats();
        rebuildDayView();
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("今日为休息日，已完成打卡。"));
        return;
    }

    if (info.dayPlanIndex < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("当前日期无可提交训练计划。"));
        return;
    }

    MasterPlan *plan = activePlanForCurrentUser();
    if (!plan || info.dayPlanIndex >= plan->dayPlans.size()) {
        return;
    }

    ensureRecordFromSelectedDayPlan();
    TrainingRecord *record = recordForDate(selectedDate);
    if (!record) {
        return;
    }

    for (int i = 0; i < record->day.items.size(); ++i) {
        if (record->day.items[i].ignored) {
            continue;
        }
        if (!record->day.items[i].completed) {
            QMessageBox::warning(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("当日仍有未完成项目，请先在项目详情中逐个标记已完成。"));
            return;
        }
    }



    const int defaultMinutes = plan->dayPlans[info.dayPlanIndex].defaultMinutes;
    int totalMinutes = qMax(1, defaultMinutes);
    if (!runCompactNumberInputDialog(this,
                                     QStringLiteral("提交当日记录"),
                                     QStringLiteral("请输入今日总训练时间（分钟）"),
                                     1,
                                     600,
                                     totalMinutes)) {
        return;
    }

    record->totalMinutes = totalMinutes;
    record->submitted = true;
    store.save();
    persistCurrentUserPackage1();
    syncRecordToRemote(*record);

    rebuildCalendarFormats();
    rebuildDayView();
    QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("当日训练记录已提交。"));
}

// 功能：补录历史日期训练记录。
void MainWindow::supplementTrainingRecord()
{
    const User *u = currentUser();
    if (!u) {
        return;
    }

    const QDate today = QDate::currentDate();
    if (selectedDate >= today) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("补录仅支持今天之前的日期。"));
        return;
    }

    if (recordForDate(selectedDate)) {
        QMessageBox::information(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("该日期已存在训练记录，无需重复补录。"));
        return;
    }

    if (u->isLocalAccount) {
        const int used = localSupplementCountForMonth(selectedDate);
        if (used >= 3) {
            QMessageBox::warning(this,
                                 QStringLiteral("提示"),
                                 QStringLiteral("本地模式同一月最多补录 3 次，当前月份已达上限。剩余补录机会：0 次。"));
            return;
        }
    }

    DayPlan supplementDay;
    supplementDay.title = QStringLiteral("补录训练计划");
    supplementDay.defaultMinutes = 60;
    if (!editDayPlanDialog(this, supplementDay)) {
        return;
    }

    if (supplementDay.title.trimmed().isEmpty() || supplementDay.items.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("提示"),
                             QStringLiteral("补录单天计划至少需要标题和一个项目。"));
        return;
    }

    const QString confirmText = QStringLiteral("确认保存本次补录记录吗？\n"
                                               "该日期本次补录创建后不可再次新增补录记录，"
                                               "如需调整请在已生成记录中继续编辑。\n"
                                               "日期：%1")
                                    .arg(selectedDate.toString(QStringLiteral("yyyy-MM-dd")));
    if (!confirmSupplementSave(this, confirmText)) {
        return;
    }

    TrainingRecord newRecord;
    newRecord.ownerUserId = currentUserId;
    newRecord.date = selectedDate;
    newRecord.day = buildRecordDayFromPlan(supplementDay);
    newRecord.submitted = false;
    newRecord.totalMinutes = qMax(1, supplementDay.defaultMinutes);
    newRecord.isSupplement = true;
    store.records.push_back(newRecord);

    store.save();
    persistCurrentUserPackage1();
    syncRecordToRemote(newRecord);
    rebuildCalendarFormats();
    rebuildDayView();
    QString successText = QStringLiteral("补录训练记录已创建，可继续编辑并提交。");
    if (u->isLocalAccount) {
        const int usedNow = localSupplementCountForMonth(selectedDate);
        const int remain = qMax(0, 3 - usedNow);
        successText += QStringLiteral("\n本月剩余补录机会：%1 次。").arg(remain);
    }
    QMessageBox::information(this,
                             QStringLiteral("提示"),
                             successText);
}

// 功能：日历日期切换后刷新当日训练视图。
void MainWindow::onCalendarDateChanged(const QDate &date)
{
    selectedDate = date;
    rebuildDayView();
}
