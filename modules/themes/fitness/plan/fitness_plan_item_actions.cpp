#include "mainwindow.h"

#include "modules/common/config/network_config.h"
#include "modules/common/ui/runtime_dialog_helpers.h"

#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QDate>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

struct LocalUiTuning {
    int dialogControlHeight = 56;
    int planManagerListFont = 24;
    int planManagerActionFont = 24;
    int dialogTopRatio = 1;
    int dialogContentRatio = 5;
    int dialogBottomRatio = 1;
};

LocalUiTuning gLocalUi;

QString normalizeSetRemarkLocal(const QString &value)
{
    return value.trimmed().left(16);
}

QString formatWeightOneDecimalLocal(double value)
{
    return QString::number(qRound(value * 10.0) / 10.0, 'f', 1);
}

bool askChineseQuestionDialogLocal(QWidget *parent,
                                   const QString &title,
                                   const QString &text,
                                   const QString &confirmText = QStringLiteral("确认"),
                                   const QString &cancelText = QStringLiteral("取消"))
{
    QMessageBox msg(parent);
    msg.setIcon(QMessageBox::Question);
    msg.setWindowTitle(title);
    msg.setText(text);
    QPushButton *confirmBtn = msg.addButton(confirmText, QMessageBox::AcceptRole);
    QPushButton *cancelBtn = msg.addButton(cancelText, QMessageBox::RejectRole);
    msg.setDefaultButton(cancelBtn);
    msg.exec();
    return msg.clickedButton() == confirmBtn;
}

void enableMobileSingleFingerScrollLocal(QAbstractScrollArea *area)
{
    if (!area) {
        return;
    }
    QScroller *scroller = QScroller::scroller(area->viewport());
    scroller->grabGesture(area->viewport(), QScroller::TouchGesture);
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                          QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                          QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(props);
}

void applyDialogVerticalRatioMarginsLocal(QLayout *layout, const QDialog &dialog)
{
    if (!layout) {
        return;
    }
    const int t = qMax(1, gLocalUi.dialogTopRatio);
    const int c = qMax(1, gLocalUi.dialogContentRatio);
    const int b = qMax(1, gLocalUi.dialogBottomRatio);
    const int sum = qMax(1, t + c + b);
    const int h = qMax(1, dialog.height());
    const int top = h * t / sum;
    const int bottom = h * b / sum;
    const int side = qMax(12, dialog.width() / 22);
    layout->setContentsMargins(side, top, side, bottom);
}

QJsonObject postOnlineJsonLocal(const QString &path,
                                const QJsonObject &payload,
                                bool *ok = nullptr)
{
    if (ok) {
        *ok = false;
    }

    QString urlText = mouseplan::common::config::resolveServerBaseUrl().trimmed();
    if (urlText.endsWith('/')) {
        urlText.chop(1);
    }
    QString suffix = path.trimmed();
    if (!suffix.startsWith('/')) {
        suffix.prepend('/');
    }
    const QUrl url(urlText + suffix);
    if (!url.isValid()) {
        return QJsonObject();
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request,
                                        QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(mouseplan::common::config::resolveServerTimeoutMs());
    loop.exec();

    if (!timeout.isActive() && reply->isRunning()) {
        reply->abort();
        reply->deleteLater();
        return QJsonObject();
    }

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return QJsonObject();
    }

    const QByteArray raw = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject();
    }

    if (ok) {
        *ok = true;
    }
    return doc.object();
}

bool onlineBoolResultLocal(const QString &path,
                           const QJsonObject &payload,
                           const QString &resultField,
                           bool fallbackValue)
{
    bool ok = false;
    const QJsonObject response = postOnlineJsonLocal(path, payload, &ok);
    if (!ok) {
        return fallbackValue;
    }

    if (response.contains(resultField)) {
        return response.value(resultField).toBool(fallbackValue);
    }
    if (response.contains(QStringLiteral("success"))) {
        return response.value(QStringLiteral("success")).toBool(fallbackValue);
    }
    return fallbackValue;
}

QJsonObject planSetToOnlineJsonLocal(const PlanSet &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("weightKg"), value.weightKg);
    obj.insert(QStringLiteral("reps"), value.reps);
    obj.insert(QStringLiteral("remark"), value.remark);
    return obj;
}

QJsonObject workoutItemToOnlineJsonLocal(const WorkoutItem &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), value.name);
    obj.insert(QStringLiteral("restSeconds"), value.restSeconds);

    QJsonArray warmup;
    for (const PlanSet &set : value.warmupSets) {
        warmup.append(planSetToOnlineJsonLocal(set));
    }
    obj.insert(QStringLiteral("warmupSets"), warmup);

    QJsonArray work;
    for (const PlanSet &set : value.workSets) {
        work.append(planSetToOnlineJsonLocal(set));
    }
    obj.insert(QStringLiteral("workSets"), work);
    return obj;
}

QJsonObject dayPlanToOnlineJsonLocal(const DayPlan &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("title"), value.title);
    obj.insert(QStringLiteral("defaultMinutes"), value.defaultMinutes);

    QJsonArray items;
    for (const WorkoutItem &item : value.items) {
        items.append(workoutItemToOnlineJsonLocal(item));
    }
    obj.insert(QStringLiteral("items"), items);
    return obj;
}

QJsonObject masterPlanToOnlineJsonLocal(const MasterPlan &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), value.id);
    obj.insert(QStringLiteral("ownerUserId"), value.ownerUserId);
    obj.insert(QStringLiteral("name"), value.name);
    obj.insert(QStringLiteral("trainDays"), value.trainDays);
    obj.insert(QStringLiteral("restDays"), value.restDays);
    obj.insert(QStringLiteral("startDate"), value.startDate.toString(QStringLiteral("yyyy-MM-dd")));

    QJsonArray days;
    for (const DayPlan &day : value.dayPlans) {
        days.append(dayPlanToOnlineJsonLocal(day));
    }
    obj.insert(QStringLiteral("dayPlans"), days);
    return obj;
}

QJsonObject recordItemToOnlineJsonLocal(const RecordItem &value)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("item"), workoutItemToOnlineJsonLocal(value.item));
    obj.insert(QStringLiteral("completed"), value.completed);
    obj.insert(QStringLiteral("ignored"), value.ignored);

    QJsonArray warmupChecked;
    for (bool v : value.warmupChecked) {
        warmupChecked.append(v);
    }
    obj.insert(QStringLiteral("warmupChecked"), warmupChecked);

    QJsonArray workChecked;
    for (bool v : value.workChecked) {
        workChecked.append(v);
    }
    obj.insert(QStringLiteral("workChecked"), workChecked);
    return obj;
}

QJsonObject trainingRecordToOnlineJsonLocal(const TrainingRecord &value)
{
    QJsonObject dayObj;
    dayObj.insert(QStringLiteral("title"), value.day.title);
    QJsonArray items;
    for (const RecordItem &item : value.day.items) {
        items.append(recordItemToOnlineJsonLocal(item));
    }
    dayObj.insert(QStringLiteral("items"), items);

    QJsonObject obj;
    obj.insert(QStringLiteral("ownerUserId"), value.ownerUserId);
    obj.insert(QStringLiteral("date"), value.date.toString(QStringLiteral("yyyy-MM-dd")));
    obj.insert(QStringLiteral("submitted"), value.submitted);
    obj.insert(QStringLiteral("totalMinutes"), value.totalMinutes);
    obj.insert(QStringLiteral("isSupplement"), value.isSupplement);
    obj.insert(QStringLiteral("day"), dayObj);
    return obj;
}

} // namespace

// 功能：打开当日训练项目详情预览弹窗。
void MainWindow::openTodayItemPreview(int itemIndex)
{
    TrainingRecord *record = recordForDate(selectedDate);

    RecordItem previewItem;
    bool hasSource = false;
    if (record && itemIndex >= 0 && itemIndex < record->day.items.size()) {
        previewItem = record->day.items[itemIndex];
        hasSource = true;
    } else {
        CalendarPlanInfo info = resolvePlanInfo(selectedDate);
        if (!info.hasPlan || info.isRestDay) {
            return;
        }

        MasterPlan *plan = activePlanForCurrentUser();
        if (!plan || info.dayPlanIndex < 0 || info.dayPlanIndex >= plan->dayPlans.size()) {
            return;
        }
        DayPlan &day = plan->dayPlans[info.dayPlanIndex];
        if (itemIndex >= 0 && itemIndex < day.items.size()) {
            previewItem.item = day.items[itemIndex];
            previewItem.completed = false;
            hasSource = true;
        }
    }
    if (!hasSource) {
        return;
    }

    if (record && itemIndex >= 0 && itemIndex < record->day.items.size()) {
        RecordItem &ri = record->day.items[itemIndex];
        if (ri.warmupChecked.size() != ri.item.warmupSets.size()) {
            ri.warmupChecked = QVector<bool>(ri.item.warmupSets.size(), ri.completed);
        }
        if (ri.workChecked.size() != ri.item.workSets.size()) {
            ri.workChecked = QVector<bool>(ri.item.workSets.size(), ri.completed);
        }
        previewItem = ri;
    }

    const QDate today = QDate::currentDate();
    const bool itemIgnored = previewItem.ignored;
    const bool canModify = (selectedDate >= today) && !(record && record->submitted) && !itemIgnored;
    const bool canPunch = (selectedDate == today) && !(record && record->submitted) && !itemIgnored;

    QDialog dialog(this);
    setupMobileDialog(dialog, this);
    dialog.setWindowTitle(QStringLiteral("项目预览"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMarginsLocal(layout, dialog);

    QFrame *mainCard = new QFrame(&dialog);
    mainCard->setStyleSheet("background:#f3f8f5;border:1px solid #dce9e1;border-radius:22px;");
    QVBoxLayout *mainCardLayout = new QVBoxLayout(mainCard);
    mainCardLayout->setContentsMargins(18, 10, 18, 16);
    mainCardLayout->setSpacing(12);

    auto addBigSubCard = [&](const QString &title,
                             const QString &bg,
                             const QString &fg,
                             int titlePx,
                             int minHeight,
                             Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter,
                             int widthPercent = 100) {
        QFrame *card = new QFrame(mainCard);
        card->setStyleSheet(QString("background:%1;border:none;border-radius:16px;").arg(bg));
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(26, 12, 16, 12);
        cardLayout->setSpacing(8);
        QLabel *titleLabel = new QLabel(title, card);
        titleLabel->setStyleSheet(QString("font-size:%1px;font-weight:800;color:%2;").arg(titlePx).arg(fg));
        titleLabel->setWordWrap(true);
        titleLabel->setAlignment(align);
        cardLayout->addWidget(titleLabel);
        card->setMinimumHeight(minHeight);

        if (widthPercent >= 100) {
            mainCardLayout->addWidget(card);
        } else {
            const int side = qMax(0, 100 - widthPercent);
            QHBoxLayout *row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(0);
            row->addStretch(side / 2);
            row->addWidget(card, widthPercent);
            row->addStretch(side - side / 2);
            mainCardLayout->addLayout(row);
        }
        return qMakePair(card, cardLayout);
    };

    QPair<QFrame *, QVBoxLayout *> nameCard = addBigSubCard(previewItem.item.name,
                                                             QStringLiteral("#edf8f0"),
                                                             QStringLiteral("#ff8c00"),
                                                             gLocalUi.planManagerActionFont + 26,
                                                             qMax(86, gLocalUi.dialogControlHeight + 20),
                                                             Qt::AlignHCenter | Qt::AlignVCenter,
                                                             65);
    if (nameCard.first) {
        nameCard.first->setStyleSheet("background:#edf8f0;border:none;border-radius:16px;");
    }
    if (nameCard.second && nameCard.second->count() > 0) {
        QWidget *w = nameCard.second->itemAt(0)->widget();
        QLabel *title = qobject_cast<QLabel *>(w);
        if (title) {
            title->setStyleSheet("font-size:70px;font-weight:900;color:#ff8c00;");
        }
    }

    if (itemIgnored) {
        QLabel *ignoredHint = new QLabel(QStringLiteral("当前项目已忽略，今日无需打卡"), mainCard);
        ignoredHint->setAlignment(Qt::AlignCenter);
        ignoredHint->setStyleSheet("font-size:30px;font-weight:800;color:#b06a2b;background:#fff3e7;border-radius:14px;padding:8px 12px;");
        mainCardLayout->addWidget(ignoredHint);
    }

    QPair<QFrame *, QVBoxLayout *> intervalCard = addBigSubCard(QString("间歇时间：%1 秒").arg(previewItem.item.restSeconds),
                                                                 QStringLiteral("#eef5ff"),
                                                                 QStringLiteral("#244d7a"),
                                                                 gLocalUi.planManagerActionFont + 6,
                                                                 qMax(58, gLocalUi.dialogControlHeight - 4),
                                                                 Qt::AlignCenter,
                                                                 65);
    if (intervalCard.first) {
        intervalCard.first->setStyleSheet("background:#eef5ff;border:none;border-radius:16px;");
    }

    struct SetRowView {
        QLabel *valueLabel = nullptr;
        QCheckBox *check = nullptr;
        QString normalHtml;
        QString checkedHtml;
        bool warmup = false;
        int setIndex = -1;
    };
    QVector<SetRowView> allRows;

    auto addSetGroupCard = [&](const QString &groupTitle,
                               const QVector<PlanSet> &sets,
                               const QString &bg,
                               const QString &fg,
                               qreal groupScale) {
        const int baseViewportHeight = qMax(340, gLocalUi.dialogControlHeight * 5);
        const int rowFixedHeight = qMax(96, gLocalUi.dialogControlHeight + 36);
        const bool warmupGroup = (groupTitle == QStringLiteral("热身组"));
        const int minVisibleRows = 5;
        const int groupViewportHeight = qMax(rowFixedHeight * minVisibleRows + 18,
                                             qMax(220, static_cast<int>(baseViewportHeight * groupScale)));

        QFrame *groupCard = new QFrame(mainCard);
        groupCard->setStyleSheet(QString("background:%1;border:none;border-radius:16px;").arg(bg));
        QVBoxLayout *groupCardLayout = new QVBoxLayout(groupCard);
        groupCardLayout->setContentsMargins(16, 10, 16, 10);
        groupCardLayout->setSpacing(10);

        QHBoxLayout *groupRow = new QHBoxLayout();
        groupRow->setContentsMargins(0, 0, 0, 0);
        groupRow->setSpacing(10);

        QFrame *titleCard = new QFrame(groupCard);
        titleCard->setStyleSheet("background:#ffffff;border:1px solid #e3ece6;border-radius:14px;");
        titleCard->setFixedWidth(160);
        QVBoxLayout *titleCardLayout = new QVBoxLayout(titleCard);
        titleCardLayout->setContentsMargins(8, 10, 8, 10);

        const bool warmupTitle = warmupGroup;
        QLabel *titleLabel = new QLabel(warmupTitle ? QStringLiteral("热\n身\n组") : groupTitle, titleCard);
        titleLabel->setStyleSheet(QString("font-size:%1px;font-weight:900;color:%2;")
                                  .arg(warmupTitle ? qMax(52, gLocalUi.planManagerActionFont + 16)
                                                   : qMax(48, gLocalUi.planManagerActionFont + 12))
                                  .arg(fg));
        titleLabel->setWordWrap(true);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleCardLayout->addStretch();
        titleCardLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
        titleCardLayout->addStretch();

        QFrame *contentCard = new QFrame(groupCard);
        contentCard->setStyleSheet("background:rgba(255,255,255,0.62);border:1px solid #e3ece6;border-radius:14px;");
        QVBoxLayout *contentCardLayout = new QVBoxLayout(contentCard);
        contentCardLayout->setContentsMargins(10, 8, 10, 8);

        QScrollArea *setScrollArea = new QScrollArea(contentCard);
        setScrollArea->setWidgetResizable(true);
        setScrollArea->setFrameShape(QFrame::NoFrame);
        setScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setScrollArea->setFixedHeight(groupViewportHeight + 16);
        setScrollArea->setStyleSheet("border:none;margin:0;padding:0;");
        enableMobileSingleFingerScrollLocal(setScrollArea);

        QWidget *setHost = new QWidget(setScrollArea);
        QVBoxLayout *setHostLayout = new QVBoxLayout(setHost);
        setHostLayout->setContentsMargins(0, 0, 0, 0);
        setHostLayout->setSpacing(10);
        setHostLayout->setAlignment(Qt::AlignTop);
        setScrollArea->setWidget(setHost);

        contentCardLayout->addWidget(setScrollArea);
        groupRow->addWidget(titleCard, 0);
        groupRow->addWidget(contentCard, 1);
        groupCardLayout->addLayout(groupRow);
        mainCardLayout->addWidget(groupCard);

        if (sets.isEmpty()) {
            QLabel *empty = new QLabel(QStringLiteral("无"), setHost);
            empty->setStyleSheet(QString("font-size:%1px;color:%2;padding:8px 0;margin:0;")
                                 .arg((gLocalUi.planManagerListFont - 4) * 2)
                                 .arg(fg));
            setHostLayout->addWidget(empty);
            return;
        }

        for (int i = 0; i < sets.size(); ++i) {
            const QString remarkText = normalizeSetRemarkLocal(sets[i].remark);
            const QString remarkValue = remarkText.isEmpty() ? QStringLiteral("无") : remarkText;
            const QString groupText = QString("第%1组").arg(i + 1);
            const QString repsText = QString("次数 %1").arg(sets[i].reps);
            const QString weightText = QString("重量 %1kg").arg(formatWeightOneDecimalLocal(sets[i].weightKg));

            QFrame *rowCard = new QFrame(setHost);
            rowCard->setStyleSheet("background:#ffffff;border:1px solid #e3ece6;border-radius:12px;");
            rowCard->setFixedHeight(rowFixedHeight);
            QHBoxLayout *rowLayout = new QHBoxLayout(rowCard);
            rowLayout->setContentsMargins(18, 12, 14, 12);
            rowLayout->setSpacing(12);
            rowLayout->setAlignment(Qt::AlignTop);

            const int baseFontPx = (gLocalUi.planManagerListFont - 8) * 2;
            const int remarkFontPx = qMax(18, baseFontPx - 6);
            const QString normalHtml = QString(
                                           "<table cellspacing='0' cellpadding='0' style='border:none;'>"
                                           "<tr>"
                                           "<td width='120'><span style='font-size:%1px;color:%2;'>%3</span></td>"
                                           "<td width='150'><span style='font-size:%1px;color:%2;'>%4</span></td>"
                                           "<td width='190'><span style='font-size:%1px;color:%2;'>%5</span></td>"
                                           "<td><span style='font-size:%6px;color:%2;'>%7</span></td>"
                                           "</tr>"
                                           "</table>")
                                           .arg(baseFontPx)
                                           .arg(fg)
                                           .arg(groupText.toHtmlEscaped())
                                           .arg(repsText.toHtmlEscaped())
                                           .arg(weightText.toHtmlEscaped())
                                           .arg(remarkFontPx)
                                           .arg(remarkValue.toHtmlEscaped());
            const QString checkedHtml = QString(
                                            "<table cellspacing='0' cellpadding='0' style='border:none;'>"
                                            "<tr>"
                                            "<td width='120'><span style='font-size:%1px;color:#9aa7a0;text-decoration:line-through;'>%2</span></td>"
                                            "<td width='150'><span style='font-size:%1px;color:#9aa7a0;text-decoration:line-through;'>%3</span></td>"
                                            "<td width='190'><span style='font-size:%1px;color:#9aa7a0;text-decoration:line-through;'>%4</span></td>"
                                            "<td><span style='font-size:%5px;color:#9aa7a0;text-decoration:line-through;'>%6</span></td>"
                                            "</tr>"
                                            "</table>")
                                            .arg(baseFontPx)
                                            .arg(groupText.toHtmlEscaped())
                                            .arg(repsText.toHtmlEscaped())
                                            .arg(weightText.toHtmlEscaped())
                                            .arg(remarkFontPx)
                                            .arg(remarkValue.toHtmlEscaped());

            QLabel *valueLabel = new QLabel(rowCard);
            valueLabel->setTextFormat(Qt::RichText);
            valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            valueLabel->setWordWrap(false);
            valueLabel->setText(normalHtml);
            valueLabel->setStyleSheet("margin:0;padding:0;");

            QCheckBox *check = new QCheckBox(rowCard);
            check->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            check->setMinimumHeight(rowFixedHeight - 16);
            check->setStyleSheet("QCheckBox{margin:0;padding:0;} QCheckBox::indicator{width:72px;height:72px;}");

            const bool isWarmup = (groupTitle == QStringLiteral("热身组"));
            bool checked = previewItem.completed;
            if (isWarmup && i < previewItem.warmupChecked.size()) {
                checked = previewItem.warmupChecked[i];
            }
            if (!isWarmup && i < previewItem.workChecked.size()) {
                checked = previewItem.workChecked[i];
            }
            check->setChecked(checked);
            check->setEnabled(canPunch);

            rowLayout->addWidget(valueLabel, 1, Qt::AlignLeft | Qt::AlignVCenter);
            rowLayout->addStretch(1);
            rowLayout->addWidget(check, 0, Qt::AlignRight | Qt::AlignVCenter);
            setHostLayout->addWidget(rowCard);

            SetRowView rowView;
            rowView.valueLabel = valueLabel;
            rowView.check = check;
            rowView.normalHtml = normalHtml;
            rowView.checkedHtml = checkedHtml;
            rowView.warmup = isWarmup;
            rowView.setIndex = i;
            allRows.push_back(rowView);
        }
    };

    addSetGroupCard(QStringLiteral("热身组"), previewItem.item.warmupSets, QStringLiteral("#fff2df"), QStringLiteral("#7a5224"), 0.6);
    addSetGroupCard(QStringLiteral("正式组"), previewItem.item.workSets, QStringLiteral("#f8eef8"), QStringLiteral("#6d4a74"), 1.0);

    layout->addWidget(mainCard);

    QHBoxLayout *ops = new QHBoxLayout();
    ops->setSpacing(16);
    QPushButton *editBtn = new QPushButton(QStringLiteral("修改项目"), &dialog);
    QPushButton *doneBtn = new QPushButton(QStringLiteral("打卡完成"), &dialog);
    QPushButton *backBtn = new QPushButton(QStringLiteral("返回主界面"), &dialog);

    const int buttonFontSize = 28;
    const int buttonHeight = 88;
    const QString normalBtnStyle = QString("font-size:%1px;min-height:%2px;padding:12px 16px;border-radius:16px;")
                                       .arg(buttonFontSize)
                                       .arg(buttonHeight);

    editBtn->setStyleSheet(canModify
                               ? normalBtnStyle + "background:#3d7db8;color:white;"
                               : normalBtnStyle + "background:#b7c7d9;color:#eef2f7;");
    doneBtn->setStyleSheet(normalBtnStyle + "background:#2f8f46;color:white;");
    backBtn->setStyleSheet(normalBtnStyle + "background:#d9dee1;color:#44515a;");

    editBtn->setEnabled(canModify);
    ops->addWidget(editBtn);
    ops->addWidget(doneBtn);
    ops->addWidget(backBtn);
    layout->addLayout(ops);

    auto refreshDoneState = [&]() {
        bool allChecked = true;
        for (int i = 0; i < allRows.size(); ++i) {
            const bool checked = allRows[i].check && allRows[i].check->isChecked();
            if (allRows[i].valueLabel) {
                allRows[i].valueLabel->setText(checked ? allRows[i].checkedHtml : allRows[i].normalHtml);
            }
            allChecked = allChecked && checked;

            if (record && itemIndex >= 0 && itemIndex < record->day.items.size()) {
                RecordItem &ri = record->day.items[itemIndex];
                if (allRows[i].warmup && allRows[i].setIndex >= 0 && allRows[i].setIndex < ri.warmupChecked.size()) {
                    ri.warmupChecked[allRows[i].setIndex] = checked;
                }
                if (!allRows[i].warmup && allRows[i].setIndex >= 0 && allRows[i].setIndex < ri.workChecked.size()) {
                    ri.workChecked[allRows[i].setIndex] = checked;
                }
            }
        }
        if (allRows.isEmpty()) {
            allChecked = true;
        }

        if (record && itemIndex >= 0 && itemIndex < record->day.items.size()) {
            record->day.items[itemIndex].completed = allChecked;
            store.save();
        }

        const bool allowDone = allChecked && canPunch;
        doneBtn->setEnabled(allowDone);
        doneBtn->setStyleSheet(allowDone
                                   ? normalBtnStyle + "background:#2f8f46;color:white;"
                                   : normalBtnStyle + "background:#b8c5bb;color:#eef3ef;");
    };

    for (int i = 0; i < allRows.size(); ++i) {
        if (allRows[i].check) {
            QObject::connect(allRows[i].check, &QCheckBox::toggled, &dialog, [=](bool) { refreshDoneState(); });
        }
    }
    refreshDoneState();

    QObject::connect(editBtn, &QPushButton::clicked, &dialog, [&]() { dialog.done(1001); });
    QObject::connect(doneBtn, &QPushButton::clicked, &dialog, [&]() {
        if (!askChineseQuestionDialogLocal(&dialog,
                                           QStringLiteral("确认打卡"),
                                           QStringLiteral("确认该项目所有组都已完成吗？\n提交打卡后，今日该项目将视为已完成，不可再修改或删除。"),
                                           QStringLiteral("确认打卡"),
                                           QStringLiteral("再检查一下"))) {
            return;
        }
        dialog.done(1002);
    });
    QObject::connect(backBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    const int result = dialog.exec();
    if (result == 1001) {
        editTodayPlanItem(itemIndex);
    } else if (result == 1002) {
        markTodayItemCompleted(itemIndex);
    }
}

// 功能：将当日训练项目标记为完成。
void MainWindow::markTodayItemCompleted(int itemIndex)
{
    if (selectedDate != QDate::currentDate()) {
        return;
    }
    ensureRecordFromSelectedDayPlan();
    TrainingRecord *record = recordForDate(selectedDate);
    if (!record || itemIndex < 0 || itemIndex >= record->day.items.size()) {
        return;
    }
    if (record->day.items[itemIndex].ignored) {
        return;
    }
    RecordItem &ri = record->day.items[itemIndex];
    ri.completed = true;
    ri.warmupChecked = QVector<bool>(ri.item.warmupSets.size(), true);
    ri.workChecked = QVector<bool>(ri.item.workSets.size(), true);
    store.save();
    persistCurrentUserPackage1();
    syncRecordToRemote(*record);
    rebuildDayView();
}

// 功能：切换当日项目忽略状态。
void MainWindow::ignoreTodayPlanItem(int itemIndex)
{
    const CalendarPlanInfo info = resolvePlanInfo(selectedDate);
    if (!info.hasPlan || info.isRestDay) {
        return;
    }
    if (selectedDate < QDate::currentDate()) {
        return;
    }

    ensureRecordFromSelectedDayPlan();
    TrainingRecord *record = recordForDate(selectedDate);
    if (!record || itemIndex < 0 || itemIndex >= record->day.items.size()) {
        return;
    }

    RecordItem &target = record->day.items[itemIndex];
    const bool nextIgnoredState = !target.ignored;
    if (nextIgnoredState) {
        const QString confirmText = target.completed
                                        ? QStringLiteral("该项目已完成打卡。确认仍要忽略吗？\n忽略后将取消该项目今日打卡状态，并在下一个循环重新出现。")
                                        : QStringLiteral("忽略该项目后今日将无需打卡该项目，会在下一个循环重新添加该项目，是否确认？");
        if (!askChineseQuestionDialogLocal(this,
                                           QStringLiteral("确认忽略项目"),
                                           confirmText,
                                           QStringLiteral("确认忽略"),
                                           QStringLiteral("取消"))) {
            return;
        }
    }

    target.ignored = nextIgnoredState;
    if (target.ignored) {
        target.completed = false;
    }
    store.save();
    rebuildDayView();
}

// 功能：编辑当日训练项目并回写记录。
void MainWindow::editTodayPlanItem(int itemIndex)
{
    CalendarPlanInfo info = resolvePlanInfo(selectedDate);
    MasterPlan *plan = activePlanForCurrentUser();
    if (!plan || info.dayPlanIndex < 0 || info.dayPlanIndex >= plan->dayPlans.size()) {
        return;
    }
    DayPlan &day = plan->dayPlans[info.dayPlanIndex];
    if (itemIndex < 0 || itemIndex >= day.items.size()) {
        return;
    }

    WorkoutItem edited = day.items[itemIndex];
    if (!editWorkoutItemDialog(this, edited)) {
        return;
    }

    day.items[itemIndex] = edited;
    ensureRecordFromSelectedDayPlan();
    if (TrainingRecord *record = recordForDate(selectedDate)) {
        if (itemIndex >= 0 && itemIndex < record->day.items.size()) {
            RecordItem &ri = record->day.items[itemIndex];
            ri.item = edited;

            auto resizeChecked = [](QVector<bool> &checked, int targetSize, bool fillValue) {
                const int oldSize = checked.size();
                checked.resize(targetSize);
                if (targetSize > oldSize) {
                    for (int i = oldSize; i < targetSize; ++i) {
                        checked[i] = fillValue;
                    }
                }
            };

            resizeChecked(ri.warmupChecked, edited.warmupSets.size(), ri.completed);
            resizeChecked(ri.workChecked, edited.workSets.size(), ri.completed);
        }
    }
    store.save();
    rebuildCalendarFormats();
    rebuildDayView();
}

// 功能：删除当日训练项目并刷新视图。
void MainWindow::deleteTodayPlanItem(int itemIndex)
{
    CalendarPlanInfo info = resolvePlanInfo(selectedDate);
    MasterPlan *plan = activePlanForCurrentUser();
    if (!plan || info.dayPlanIndex < 0 || info.dayPlanIndex >= plan->dayPlans.size()) {
        return;
    }
    DayPlan &day = plan->dayPlans[info.dayPlanIndex];
    if (itemIndex < 0 || itemIndex >= day.items.size()) {
        return;
    }

    if (!askChineseQuestionDialogLocal(this,
                                       QStringLiteral("确认删除"),
                                       QStringLiteral("确认删除该项目吗？\n删除后会同步影响后续循环日的该项目。\n如果仅是今天不训练，请返回使用“忽略该项目”。"),
                                       QStringLiteral("确认删除"),
                                       QStringLiteral("取消"))) {
        return;
    }

    day.items.removeAt(itemIndex);
    ensureRecordFromSelectedDayPlan();
    store.save();
    rebuildDayView();
}

// 功能：将总计划同步到云端。
void MainWindow::syncPlanToRemote(const MasterPlan &plan)
{
    const User *u = currentUser();
    if (!u || u->isLocalAccount) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), u->id);
    payload.insert(QStringLiteral("plan"), masterPlanToOnlineJsonLocal(plan));
    onlineBoolResultLocal(QStringLiteral("/sync/plan"),
                          payload,
                          QStringLiteral("success"),
                          true);
}

// 功能：将训练记录同步到云端。
void MainWindow::syncRecordToRemote(const TrainingRecord &record)
{
    const User *u = currentUser();
    if (!u || u->isLocalAccount) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("userId"), u->id);
    payload.insert(QStringLiteral("record"), trainingRecordToOnlineJsonLocal(record));
    onlineBoolResultLocal(QStringLiteral("/sync/record"),
                          payload,
                          QStringLiteral("success"),
                          true);
}
