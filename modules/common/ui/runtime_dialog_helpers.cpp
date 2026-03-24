#include "runtime_dialog_helpers.h"

#include "appdata.h"
#include "modules/common/theme/theme_feature_gate.h"
#include "ui_tuning.h"

#include <QtCore>
#include <QtGui>
#include <QtNetwork>
#include <QtWidgets>

const QString kDefaultAvatarResourcePath = QStringLiteral(":/img/dehead");

QString safeFileSuffixFromMime(const QString &mimeType)
{
    const QString mime = mimeType.trimmed().toLower();
    if (mime.contains(QStringLiteral("json")) || mime.contains(QStringLiteral("text"))) {
        return QStringLiteral("json");
    }
    if (mime.contains(QStringLiteral("octet-stream"))) {
        return QStringLiteral("bin");
    }
    if (mime.contains(QStringLiteral("png"))) {
        return QStringLiteral("png");
    }
    if (mime.contains(QStringLiteral("jpeg")) || mime.contains(QStringLiteral("jpg"))) {
        return QStringLiteral("jpg");
    }
    if (mime.contains(QStringLiteral("webp"))) {
        return QStringLiteral("webp");
    }
    return QStringLiteral("bin");
}

namespace {

QPixmap cropCenterByAspectRatio(const QPixmap &source, const QSize &targetSize)
{
    if (source.isNull() || !targetSize.isValid() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        return source;
    }

    const qreal sourceRatio = static_cast<qreal>(source.width()) / static_cast<qreal>(source.height());
    const qreal targetRatio = static_cast<qreal>(targetSize.width()) / static_cast<qreal>(targetSize.height());
    QRect cropRect(0, 0, source.width(), source.height());
    if (sourceRatio > targetRatio) {
        const int cropWidth = qMax(1, static_cast<int>(source.height() * targetRatio));
        cropRect.setWidth(cropWidth);
        cropRect.moveLeft((source.width() - cropWidth) / 2);
    } else {
        const int cropHeight = qMax(1, static_cast<int>(source.width() / targetRatio));
        cropRect.setHeight(cropHeight);
        cropRect.moveTop((source.height() - cropHeight) / 2);
    }
    return source.copy(cropRect);
}

int countSetsTextToInt(const QString &value)
{
    bool ok = false;
    const int result = value.trimmed().toInt(&ok);
    return ok ? result : 0;
}

double countSetsTextToWeightOneDecimal(const QString &value)
{
    bool ok = false;
    const double parsed = value.trimmed().toDouble(&ok);
    if (!ok) {
        return 0.0;
    }
    return qRound(parsed * 10.0) / 10.0;
}

QString formatWeightOneDecimal(double value)
{
    return QString::number(qRound(value * 10.0) / 10.0, 'f', 1);
}

QString normalizeSetRemark(const QString &value)
{
    return value.trimmed().left(16);
}

QString setsPreviewText(const QVector<PlanSet> &sets)
{
    if (sets.isEmpty()) {
        return QStringLiteral("无");
    }
    QStringList parts;
    for (const PlanSet &set : sets) {
        parts << QString("%1kgx%2").arg(formatWeightOneDecimal(set.weightKg)).arg(set.reps);
    }
    return parts.join(QStringLiteral("、"));
}

QDialogButtonBox *createChineseSaveCancelButtons(QWidget *parent)
{
    QDialogButtonBox *buttons = new QDialogButtonBox(Qt::Horizontal, parent);
    QPushButton *saveBtn = buttons->addButton(QStringLiteral("保存"), QDialogButtonBox::AcceptRole);
    QPushButton *cancelBtn = buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    saveBtn->setMinimumWidth(220);
    cancelBtn->setMinimumWidth(220);
    return buttons;
}

void enableMobileSingleFingerScroll(QAbstractScrollArea *area)
{
    if (!area || !area->viewport()) {
        return;
    }
    QWidget *viewport = area->viewport();
    QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture);

    QScroller *scroller = QScroller::scroller(viewport);
    QScrollerProperties props = scroller->scrollerProperties();
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.012);
    props.setScrollMetric(QScrollerProperties::MaximumVelocity, 0.22);
    props.setScrollMetric(QScrollerProperties::DecelerationFactor, 0.32);
    props.setScrollMetric(QScrollerProperties::AcceleratingFlickSpeedupFactor, 1.0);
    props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.12);
    scroller->setScrollerProperties(props);

    if (QAbstractItemView *itemView = qobject_cast<QAbstractItemView *>(area)) {
        itemView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        itemView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
}

void applyDialogVerticalRatioMargins(QLayout *layout, const QDialog &dialog)
{
    if (!layout) {
        return;
    }
    const int t = qMax(1, gUiTuning.dialogTopRatio);
    const int c = qMax(1, gUiTuning.dialogContentRatio);
    const int b = qMax(1, gUiTuning.dialogBottomRatio);
    const int total = t + c + b;
    const int h = qMax(600, dialog.height());

    const int top = h * t / total;
    const int bottom = h * b / total;
    const int side = qMax(22, dialog.width() / 18);
    layout->setContentsMargins(side, top, side, bottom);
}

QComboBox *createWheelCombo(QWidget *parent, int from, int to, int step, const QString &suffix = QString())
{
    QComboBox *combo = new QComboBox(parent);
    for (int v = from; v <= to; v += step) {
        combo->addItem(suffix.isEmpty() ? QString::number(v) : QString("%1%2").arg(v).arg(suffix), v);
    }
    combo->setEditable(false);
    combo->setMaxVisibleItems(10);
    combo->setStyleSheet("font-size:48px;min-height:96px;padding:10px 12px;");
    if (combo->view()) {
        QScroller::grabGesture(combo->view()->viewport(), QScroller::LeftMouseButtonGesture);
    }
    return combo;
}

int comboValue(const QComboBox *combo, int fallback)
{
    if (!combo) {
        return fallback;
    }
    const QVariant data = combo->currentData();
    if (data.isValid()) {
        return data.toInt();
    }
    bool ok = false;
    const int value = combo->currentText().toInt(&ok);
    return ok ? value : fallback;
}

void addPreviewCardItem(QListWidget *list,
                        const QStringList &lines,
                        const QString &cardBg,
                        int minHeight,
                        int titleFont,
                        int bodyFont)
{
    if (!list || lines.isEmpty()) {
        return;
    }

    const int computedHeight = qMax(minHeight, 46 + static_cast<int>(lines.size()) * (bodyFont + 34));
    QListWidgetItem *item = new QListWidgetItem(list);
    item->setSizeHint(QSize(0, computedHeight));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    list->addItem(item);

    QWidget *cardHost = new QWidget(list);
    QVBoxLayout *cardLayout = new QVBoxLayout(cardHost);
    cardLayout->setContentsMargins(20, 14, 20, 14);
    cardLayout->setSpacing(10);
    auto applyCardStyle = [cardHost, cardBg](bool selected) {
        cardHost->setStyleSheet(QString("background:%1;border:%2px solid %3;border-radius:16px;")
                                    .arg(cardBg)
                                    .arg(selected ? 3 : 1)
                                    .arg(selected ? QStringLiteral("#3d7db8") : QStringLiteral("#dbe8df")));
    };
    applyCardStyle(false);

    for (int i = 0; i < lines.size(); ++i) {
        QFrame *lineBox = new QFrame(cardHost);
        if (i == 0) {
            lineBox->setStyleSheet("background:#ffe9cc;border:1px solid #f1d4a9;border-radius:12px;");
        } else {
            lineBox->setStyleSheet("background:#ffffff;border:1px solid #e6efe9;border-radius:12px;");
        }
        QHBoxLayout *lineLayout = new QHBoxLayout(lineBox);
        lineLayout->setContentsMargins(16, 10, 16, 10);
        QLabel *lineLabel = new QLabel(lines[i], lineBox);
        lineLabel->setWordWrap(true);
        if (i == 0) {
            lineLabel->setStyleSheet(QString("font-size:%1px;font-weight:800;color:#1f392d;").arg(titleFont));
        } else {
            lineLabel->setStyleSheet(QString("font-size:%1px;color:#5f6f66;").arg(bodyFont));
        }
        lineLayout->addWidget(lineLabel);
        cardLayout->addWidget(lineBox);
    }

    list->setItemWidget(item, cardHost);
    QObject::connect(list, &QListWidget::currentItemChanged, cardHost, [=](QListWidgetItem *current, QListWidgetItem *) {
        applyCardStyle(current == item);
    });
    if (list->currentItem() == item) {
        applyCardStyle(true);
    }
}

} // namespace

bool askChineseQuestionDialog(QWidget *parent,
                              const QString &title,
                              const QString &text,
                              const QString &confirmText,
                              const QString &cancelText)
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

QVector<ThemeColorPreset> themeColorPresets()
{
    return {
        {QStringLiteral("fitness_classic"), QStringLiteral("活力经典(原健身)"), QStringLiteral("#d56b00"), QStringLiteral("#b85a00"), QStringLiteral("#ffe9d2"), QStringLiteral("#ffd7b0"), QStringLiteral("#ffd3aa")},
        {QStringLiteral("study_classic"), QStringLiteral("沉静经典(原学习)"), QStringLiteral("#2d74c6"), QStringLiteral("#1f5fa8"), QStringLiteral("#e2efff"), QStringLiteral("#cde4ff"), QStringLiteral("#cfe2ff")},
        {QStringLiteral("normal_classic"), QStringLiteral("日常经典(原普通)"), QStringLiteral("#3e9a55"), QStringLiteral("#2f7b42"), QStringLiteral("#ddf3de"), QStringLiteral("#cdeccf"), QStringLiteral("#cff0d2")},
        {QStringLiteral("teal_lagoon"), QStringLiteral("湖泊青"), QStringLiteral("#0f6a66"), QStringLiteral("#18a8a1"), QStringLiteral("#e8f7f6"), QStringLiteral("#cdeeed"), QStringLiteral("#a9dfdc")},
        {QStringLiteral("amber_gold"), QStringLiteral("琥珀金"), QStringLiteral("#8a5a10"), QStringLiteral("#d99a1f"), QStringLiteral("#fff7e8"), QStringLiteral("#fdebc8"), QStringLiteral("#f6d69e")},
        {QStringLiteral("crimson_fire"), QStringLiteral("绯红"), QStringLiteral("#8f1f35"), QStringLiteral("#d44b66"), QStringLiteral("#fff0f3"), QStringLiteral("#ffdce3"), QStringLiteral("#f5bcc8")},
        {QStringLiteral("indigo_night"), QStringLiteral("靛蓝夜"), QStringLiteral("#2e3f8f"), QStringLiteral("#5f79da"), QStringLiteral("#eef1ff"), QStringLiteral("#dce3ff"), QStringLiteral("#bcc9f7")},
        {QStringLiteral("lime_spring"), QStringLiteral("青柠春"), QStringLiteral("#4c7f1a"), QStringLiteral("#79be32"), QStringLiteral("#f3faea"), QStringLiteral("#e2f2ce"), QStringLiteral("#c8e59f")},
        {QStringLiteral("steel_slate"), QStringLiteral("钢岩灰"), QStringLiteral("#3d4a5c"), QStringLiteral("#7288a3"), QStringLiteral("#eff3f7"), QStringLiteral("#dfe6ee"), QStringLiteral("#c6d2de")}
    };
}

ThemeColorPreset resolveThemeColorPreset(const QString &theme, const QString &presetKey)
{
    const QVector<ThemeColorPreset> presets = themeColorPresets();
    const QString wanted = presetKey.trimmed();
    for (const ThemeColorPreset &preset : presets) {
        if (preset.key == wanted) {
            return preset;
        }
    }
    if (theme == QStringLiteral("study")) {
        return presets[1];
    }
    if (theme == QStringLiteral("normal")) {
        return presets[2];
    }
    return presets[0];
}

QPixmap createCircularAvatarPixmap(const QPixmap &source, const QSize &targetSize)
{
    if (source.isNull() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        return QPixmap();
    }

    const int d = qMin(targetSize.width(), targetSize.height());
    QPixmap square = source.scaled(d,
                                   d,
                                   Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);

    QPixmap masked(d, d);
    masked.fill(Qt::transparent);

    QPainter painter(&masked);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clipPath;
    clipPath.addEllipse(0, 0, d, d);
    painter.setClipPath(clipPath);
    painter.drawPixmap(0, 0, square);
    return masked;
}

QPixmap createSoftEdgeBannerPixmap(const QPixmap &source,
                                   const QSize &targetSize,
                                   const QColor &bg,
                                   int radius)
{
    if (source.isNull() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        return QPixmap();
    }

    const QPixmap cropped = cropCenterByAspectRatio(source, targetSize);
    QPixmap canvas(targetSize);
    canvas.fill(bg);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, targetSize.width(), targetSize.height()), radius, radius);
    painter.setClipPath(clip);
    painter.drawPixmap(0, 0,
                       cropped.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    painter.setClipping(false);

    const int fade = qMax(10, qMin(targetSize.width(), targetSize.height()) / 8);
    auto drawEdge = [&](int x1, int y1, int x2, int y2) {
        QLinearGradient g(x1, y1, x2, y2);
        QColor edge = bg;
        edge.setAlpha(210);
        QColor center = bg;
        center.setAlpha(0);
        g.setColorAt(0.0, edge);
        g.setColorAt(1.0, center);
        painter.fillRect(QRect(qMin(x1, x2), qMin(y1, y2), qAbs(x2 - x1) + (x1 == x2 ? fade : 1), qAbs(y2 - y1) + (y1 == y2 ? fade : 1)), g);
    };

    drawEdge(0, 0, fade, 0);
    drawEdge(targetSize.width() - 1, 0, targetSize.width() - fade - 1, 0);
    drawEdge(0, 0, 0, fade);
    drawEdge(0, targetSize.height() - 1, 0, targetSize.height() - fade - 1);

    return canvas;
}

void setupMobileDialog(QDialog &dialog, QWidget *parent)
{
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setModal(true);
    dialog.setSizeGripEnabled(false);
    dialog.setWindowFlag(Qt::FramelessWindowHint, true);
    dialog.setWindowFlag(Qt::Dialog, true);

    dialog.setStyleSheet(QString(
                             "QDialog{background:%1;font-size:%2px;}"
                             "QLabel{font-size:%2px;}"
                             "QLineEdit,QSpinBox,QDateEdit,QListWidget,QTableWidget{font-size:%2px;min-height:%3px;border-radius:%4px;}"
                             "QPushButton{font-size:%2px;min-height:%5px;border-radius:%4px;padding:10px 18px;}"
                             "QDialogButtonBox QPushButton{min-height:%5px;font-size:%2px;font-weight:700;}")
                             .arg(gUiTuning.dialogBg)
                             .arg(gUiTuning.dialogBaseFont)
                             .arg(gUiTuning.dialogControlHeight)
                             .arg(gUiTuning.dialogCornerRadius)
                             .arg(gUiTuning.dialogButtonHeight));

    QRect targetRect;
    if (parent) {
        targetRect = QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size());
    }
    if (!targetRect.isValid() || targetRect.isEmpty()) {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            targetRect = screen->availableGeometry();
        } else {
            targetRect = QRect(0, 0, 1080, 1920);
        }
    }

    dialog.setGeometry(targetRect);
    dialog.setMinimumSize(targetRect.size());
    dialog.setMaximumSize(targetRect.size());
}

bool runCompactInputDialog(QWidget *parent,
                           const QString &title,
                           const QString &label,
                           QString &value,
                           bool numberOnly,
                           int minValue,
                           int maxValue)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);

    QRect targetRect;
    if (parent) {
        targetRect = QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size());
    }
    if (!targetRect.isValid() || targetRect.isEmpty()) {
        QScreen *screen = QGuiApplication::primaryScreen();
        targetRect = screen ? screen->availableGeometry() : QRect(0, 0, 1080, 1920);
    }

    const int w = static_cast<int>(targetRect.width() * 0.82);
    const int h = static_cast<int>(targetRect.height() * 0.33);
    const int x = targetRect.x() + (targetRect.width() - w) / 2;
    const int confirmHeight = 76;
    const int moveUp = confirmHeight * 6 / 5;
    const int y = qMax(targetRect.y() + 8,
                       targetRect.y() + (targetRect.height() - h) / 2 - moveUp);
    dialog.setGeometry(x, y, w, h);
    dialog.setMinimumSize(w, h);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);

    QLabel *titleLabel = new QLabel(label, &dialog);
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size:36px;font-weight:900;color:#23382d;");

    QLineEdit *input = new QLineEdit(value, &dialog);
    if (numberOnly) {
        input->setValidator(new QIntValidator(minValue, maxValue, input));
    }
    input->setAlignment(Qt::AlignCenter);
    input->setStyleSheet("font-size:32px;min-height:88px;padding:10px 14px;border-radius:14px;");

    QHBoxLayout *ops = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(QStringLiteral("确认"), &dialog);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    okBtn->setStyleSheet("font-size:26px;min-height:76px;background:#2f8f46;color:white;border-radius:14px;");
    cancelBtn->setStyleSheet("font-size:26px;min-height:76px;background:#d9dee1;color:#455058;border-radius:14px;");
    ops->addWidget(okBtn);
    ops->addWidget(cancelBtn);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, &dialog, [&]() {
        const QString text = input->text().trimmed();
        if (numberOnly) {
            bool ok = false;
            const int parsed = text.toInt(&ok);
            if (!ok || parsed < minValue || parsed > maxValue) {
                QMessageBox::warning(&dialog,
                                     QStringLiteral("输入错误"),
                                     QString("请输入 %1 到 %2 之间的整数。")
                                         .arg(minValue)
                                         .arg(maxValue));
                return;
            }
        } else if (text.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("输入错误"), QStringLiteral("输入内容不能为空。"));
            return;
        }
        value = text;
        dialog.accept();
    });

    layout->addWidget(titleLabel);
    layout->addWidget(input);
    layout->addLayout(ops);
    return dialog.exec() == QDialog::Accepted;
}

bool runCompactNumberInputDialog(QWidget *parent,
                                 const QString &title,
                                 const QString &label,
                                 int minValue,
                                 int maxValue,
                                 int &value)
{
    QString text = QString::number(value);
    if (!runCompactInputDialog(parent, title, label, text, true, minValue, maxValue)) {
        return false;
    }
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok) {
        return false;
    }
    value = parsed;
    return true;
}

bool runCompactTextInputDialog(QWidget *parent,
                               const QString &title,
                               const QString &label,
                               QString &value)
{
    return runCompactInputDialog(parent, title, label, value, false, 0, 0);
}

bool runLargePasswordDialog(QWidget *parent,
                            const QString &title,
                            const QString &label,
                            QString &value)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);

    QRect targetRect;
    if (parent) {
        targetRect = QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size());
    }
    if (!targetRect.isValid() || targetRect.isEmpty()) {
        QScreen *screen = QGuiApplication::primaryScreen();
        targetRect = screen ? screen->availableGeometry() : QRect(0, 0, 1080, 1920);
    }

    const int w = static_cast<int>(targetRect.width() * 0.95);
    const int h = static_cast<int>(targetRect.height() * 0.72);
    const int x = targetRect.x() + (targetRect.width() - w) / 2;
    const int y = targetRect.y() + (targetRect.height() - h) / 2;
    dialog.setGeometry(x, y, w, h);
    dialog.setMinimumSize(w, h);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(34, 30, 34, 30);
    layout->setSpacing(22);

    QLabel *titleLabel = new QLabel(label, &dialog);
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size:52px;font-weight:900;color:#23382d;");

    QLineEdit *input = new QLineEdit(value, &dialog);
    input->setEchoMode(QLineEdit::Password);
    input->setAlignment(Qt::AlignCenter);
    input->setStyleSheet("font-size:48px;min-height:248px;padding:10px 18px;border-radius:18px;border:2px solid #cfded5;background:#ffffff;");

    QHBoxLayout *ops = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(QStringLiteral("确认"), &dialog);
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), &dialog);
    okBtn->setStyleSheet("font-size:44px;min-height:220px;background:#2f8f46;color:white;border-radius:18px;");
    cancelBtn->setStyleSheet("font-size:44px;min-height:220px;background:#9ca5a1;color:white;border-radius:18px;");
    ops->addWidget(okBtn);
    ops->addWidget(cancelBtn);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, &dialog, [&]() {
        value = input->text();
        dialog.accept();
    });

    layout->addStretch(1);
    layout->addWidget(titleLabel);
    layout->addWidget(input);
    layout->addLayout(ops);
    layout->addStretch(1);
    return dialog.exec() == QDialog::Accepted;
}

bool editWorkoutItemDialog(QWidget *parent, WorkoutItem &item)
{
    QDialog dialog(parent);
    setupMobileDialog(dialog, parent);
    dialog.setWindowTitle(QStringLiteral("编辑单项目块"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMargins(layout, dialog);
    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setHorizontalSpacing(22);
    form->setVerticalSpacing(20);

    QString itemNameInput = item.name;
    int restSecondsInput = item.restSeconds;
    if (restSecondsInput < 0) {
        restSecondsInput = 0;
    }

    QPushButton *nameBtn = new QPushButton(&dialog);
    QPushButton *restBtn = new QPushButton(&dialog);
    const QString inputBtnStyle = "font-size:34px;min-height:104px;background:#f4f8f5;color:#203328;border:2px solid #cfe0d5;border-radius:14px;padding:12px 18px;text-align:left;";
    nameBtn->setStyleSheet(inputBtnStyle);
    restBtn->setStyleSheet(inputBtnStyle);
    auto refreshHeaderInputs = [&]() {
        nameBtn->setText(itemNameInput.isEmpty() ? QStringLiteral("点击输入项目名称") : itemNameInput);
        restBtn->setText(QString("%1 秒").arg(restSecondsInput));
    };
    refreshHeaderInputs();

    QObject::connect(nameBtn, &QPushButton::clicked, &dialog, [&]() {
        QString value = itemNameInput;
        if (runCompactTextInputDialog(&dialog,
                                      QStringLiteral("设置项目名称"),
                                      QStringLiteral("请输入项目名称"),
                                      value)) {
            itemNameInput = value.trimmed();
            refreshHeaderInputs();
        }
    });
    QObject::connect(restBtn, &QPushButton::clicked, &dialog, [&]() {
        int value = restSecondsInput;
        if (runCompactNumberInputDialog(&dialog,
                                        QStringLiteral("设置间歇秒数"),
                                        QStringLiteral("请输入间歇秒数"),
                                        0,
                                        600,
                                        value)) {
            restSecondsInput = value;
            refreshHeaderInputs();
        }
    });

    form->addRow(QStringLiteral("项目名称"), nameBtn);
    form->addRow(QStringLiteral("间歇秒数"), restBtn);
    if (QWidget *l = form->labelForField(nameBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }
    if (QWidget *l = form->labelForField(restBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }

    auto createSetTable = [&dialog](const QVector<PlanSet> &sets, const QString &header) {
        QGroupBox *box = new QGroupBox(header, &dialog);
        box->setStyleSheet("QGroupBox{font-size:47px;font-weight:900;color:#203328;background:#eef4ff;} QGroupBox::title{subcontrol-origin: margin; left: 12px; top: -2px;}");
        QVBoxLayout *boxLayout = new QVBoxLayout(box);
        QTableWidget *table = new QTableWidget(box);
        enableMobileSingleFingerScroll(table);
        table->setColumnCount(3);
        table->setHorizontalHeaderLabels({QStringLiteral("重量(kg)"), QStringLiteral("次数"), QStringLiteral("备注")});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setMinimumHeight(420);
        table->setStyleSheet("QTableWidget{font-size:26px;} QTableWidget::item{padding:4px;}");
        table->horizontalHeader()->setStyleSheet("QHeaderView::section{font-size:32px;font-weight:800;padding:8px;}");
        table->verticalHeader()->setStyleSheet("QHeaderView::section{font-size:20px;font-weight:700;padding:2px;}");
        table->verticalHeader()->setDefaultSectionSize(56);
        table->verticalHeader()->setMinimumSectionSize(42);
        table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectItems);

        auto addEditorRow = [table](double weight, int reps, const QString &remark) {
            const int row = table->rowCount();
            table->insertRow(row);

            QTableWidgetItem *weightItem = new QTableWidgetItem(formatWeightOneDecimal(weight));
            QTableWidgetItem *repsItem = new QTableWidgetItem(QString::number(reps));
            QTableWidgetItem *remarkItem = new QTableWidgetItem(normalizeSetRemark(remark));
            weightItem->setTextAlignment(Qt::AlignCenter);
            repsItem->setTextAlignment(Qt::AlignCenter);
            remarkItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            table->setItem(row, 0, weightItem);
            table->setItem(row, 1, repsItem);
            table->setItem(row, 2, remarkItem);
        };

        for (const PlanSet &set : sets) {
            addEditorRow(set.weightKg, set.reps, set.remark);
        }

        QHBoxLayout *ops = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton(QStringLiteral("添加组"), box);
        QPushButton *delBtn = new QPushButton(QStringLiteral("删除组"), box);
        const QString opStyle = QString("font-size:%1px;min-height:%2px;font-weight:800;padding:10px 14px;border-radius:12px;")
                       .arg(gUiTuning.planManagerActionFont + 4)
                       .arg(gUiTuning.planManagerActionHeight);
        addBtn->setStyleSheet(opStyle + "background:#2f8f46;color:white;");
        delBtn->setStyleSheet(opStyle + "background:#c95d5d;color:white;");
        ops->addWidget(addBtn);
        ops->addWidget(delBtn);
        ops->addStretch();

        QObject::connect(addBtn, &QPushButton::clicked, box, [table]() {
            const int row = table->rowCount();
            table->insertRow(row);
            QTableWidgetItem *weightItem = new QTableWidgetItem(QStringLiteral("20.0"));
            QTableWidgetItem *repsItem = new QTableWidgetItem(QStringLiteral("6"));
            QTableWidgetItem *remarkItem = new QTableWidgetItem(QString());
            weightItem->setTextAlignment(Qt::AlignCenter);
            repsItem->setTextAlignment(Qt::AlignCenter);
            remarkItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            table->setItem(row, 0, weightItem);
            table->setItem(row, 1, repsItem);
            table->setItem(row, 2, remarkItem);
        });
        QObject::connect(delBtn, &QPushButton::clicked, box, [table]() {
            const int row = table->currentRow();
            if (row >= 0) {
                table->removeRow(row);
            }
        });

        QObject::connect(table, &QTableWidget::cellClicked, box, [table, &dialog](int row, int column) {
            if (row < 0 || column < 0) {
                return;
            }
            QTableWidgetItem *cell = table->item(row, column);
            if (!cell) {
                return;
            }
            if (column == 2) {
                QString remarkValue = cell->text();
                if (runCompactTextInputDialog(&dialog,
                                              QStringLiteral("设置备注"),
                                              QStringLiteral("请输入备注（最多16字）"),
                                              remarkValue)) {
                    const QString trimmed = remarkValue.trimmed();
                    if (trimmed.size() > 16) {
                        QMessageBox::warning(&dialog,
                                             QStringLiteral("输入错误"),
                                             QStringLiteral("备注最多16个字。"));
                        return;
                    }
                    const QString normalized = normalizeSetRemark(trimmed);
                    cell->setText(normalized);
                    cell->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                }
                return;
            }
            if (column == 0) {
                QString weightValue = cell->text().trimmed();
                if (weightValue.isEmpty()) {
                    weightValue = QStringLiteral("20.0");
                }
                if (runCompactTextInputDialog(&dialog,
                                              QStringLiteral("设置重量"),
                                              QStringLiteral("请输入重量(kg)，保留1位小数"),
                                              weightValue)) {
                    QRegularExpression rx(QStringLiteral("^\\d{1,4}(?:\\.\\d)?$"));
                    if (!rx.match(weightValue.trimmed()).hasMatch()) {
                        QMessageBox::warning(&dialog,
                                             QStringLiteral("输入错误"),
                                             QStringLiteral("重量格式不正确，请输入如 20 或 20.5，最多1位小数。"));
                        return;
                    }
                    const double parsed = countSetsTextToWeightOneDecimal(weightValue);
                    if (parsed < 0.0 || parsed > 2000.0) {
                        QMessageBox::warning(&dialog,
                                             QStringLiteral("输入错误"),
                                             QStringLiteral("重量必须在 0 到 2000 之间。"));
                        return;
                    }
                    cell->setText(formatWeightOneDecimal(parsed));
                    cell->setTextAlignment(Qt::AlignCenter);
                }
                return;
            }

            bool ok = false;
            int current = cell->text().toInt(&ok);
            if (!ok) {
                current = 6;
            }
            if (runCompactNumberInputDialog(&dialog,
                                            QStringLiteral("设置次数"),
                                            QStringLiteral("请输入次数"),
                                            0,
                                            300,
                                            current)) {
                cell->setText(QString::number(current));
                cell->setTextAlignment(Qt::AlignCenter);
            }
        });

        boxLayout->addWidget(table);
        boxLayout->addLayout(ops);
        return qMakePair(box, table);
    };

    QPair<QGroupBox *, QTableWidget *> warmupView = createSetTable(item.warmupSets, QStringLiteral("热身组"));
    QPair<QGroupBox *, QTableWidget *> workView = createSetTable(item.workSets, QStringLiteral("正式组"));

    QDialogButtonBox *buttons = createChineseSaveCancelButtons(&dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QWidget *itemInfoSection = new QWidget(&dialog);
    itemInfoSection->setStyleSheet("background:#f6faf7;border-radius:14px;");
    QVBoxLayout *itemInfoLayout = new QVBoxLayout(itemInfoSection);
    itemInfoLayout->setContentsMargins(8, 8, 8, 8);
    itemInfoLayout->addLayout(form);
    layout->addWidget(itemInfoSection);
    QFrame *itemSectionDivider = new QFrame(&dialog);
    itemSectionDivider->setFrameShape(QFrame::HLine);
    itemSectionDivider->setStyleSheet("color:#c9d7d0;background:#c9d7d0;min-height:2px;max-height:2px;");
    layout->addWidget(itemSectionDivider);
    layout->addWidget(warmupView.first);
    layout->addWidget(workView.first);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    item.name = itemNameInput.trimmed();
    item.restSeconds = restSecondsInput;

    auto parseSets = [](QTableWidget *table) {
        QVector<PlanSet> parsed;
        for (int row = 0; row < table->rowCount(); ++row) {
            PlanSet set;
            QTableWidgetItem *weightItem = table->item(row, 0);
            QTableWidgetItem *repsItem = table->item(row, 1);
            QTableWidgetItem *remarkItem = table->item(row, 2);
            set.weightKg = countSetsTextToWeightOneDecimal(weightItem ? weightItem->text() : QString());
            set.reps = countSetsTextToInt(repsItem ? repsItem->text() : QString());
            set.remark = normalizeSetRemark(remarkItem ? remarkItem->text() : QString());
            parsed.push_back(set);
        }
        return parsed;
    };
    item.warmupSets = parseSets(warmupView.second);
    item.workSets = parseSets(workView.second);

    if (item.name.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("项目名称不能为空。"));
        return false;
    }
    return true;
}

bool editDayPlanDialog(QWidget *parent, DayPlan &dayPlan)
{
    QDialog dialog(parent);
    setupMobileDialog(dialog, parent);
    dialog.setWindowTitle(QStringLiteral("编辑单天计划"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMargins(layout, dialog);
    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setHorizontalSpacing(22);
    form->setVerticalSpacing(20);
    QString dayTitleInput = dayPlan.title;
    int defaultMinutesInput = qMax(1, dayPlan.defaultMinutes);
    QPushButton *titleBtn = new QPushButton(&dialog);
    QPushButton *minutesBtn = new QPushButton(&dialog);
    const QString inputBtnStyle = "font-size:32px;min-height:96px;background:#f4f8f5;color:#203328;border:2px solid #cfe0d5;border-radius:14px;padding:10px 14px;text-align:left;";
    titleBtn->setStyleSheet(inputBtnStyle);
    minutesBtn->setStyleSheet(inputBtnStyle);
    auto refreshDayInputs = [&]() {
        titleBtn->setText(dayTitleInput.isEmpty() ? QStringLiteral("点击输入单天标题") : dayTitleInput);
        minutesBtn->setText(QString("%1 分钟").arg(defaultMinutesInput));
    };
    refreshDayInputs();

    QObject::connect(titleBtn, &QPushButton::clicked, &dialog, [&]() {
        QString value = dayTitleInput;
        if (runCompactTextInputDialog(&dialog,
                                      QStringLiteral("设置单天标题"),
                                      QStringLiteral("请输入单天标题"),
                                      value)) {
            dayTitleInput = value.trimmed();
            refreshDayInputs();
        }
    });
    QObject::connect(minutesBtn, &QPushButton::clicked, &dialog, [&]() {
        int value = defaultMinutesInput;
        if (runCompactNumberInputDialog(&dialog,
                                        QStringLiteral("设置默认训练分钟"),
                                        QStringLiteral("请输入默认训练分钟"),
                                        1,
                                        600,
                                        value)) {
            defaultMinutesInput = value;
            refreshDayInputs();
        }
    });

    form->addRow(QStringLiteral("单天标题"), titleBtn);
    form->addRow(QStringLiteral("默认训练分钟"), minutesBtn);
    if (QWidget *l = form->labelForField(titleBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }
    if (QWidget *l = form->labelForField(minutesBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }

    QListWidget *itemList = new QListWidget(&dialog);
    enableMobileSingleFingerScroll(itemList);
    itemList->setStyleSheet(
        QString("QListWidget{font-size:%1px;border:1px solid #d3e1d8;border-radius:16px;background:#eef4ff;padding:10px;}"
            "QListWidget::item{min-height:%2px;padding:10px 8px;margin:8px 4px;border-bottom:1px solid #e3ece6;}"
            "QListWidget::item:selected{background:rgba(61,125,184,80);border-radius:12px;color:#173326;}")
            .arg(gUiTuning.planManagerListFont + 16)
            .arg(gUiTuning.dialogListItemHeight + 48));
        itemList->setDragDropMode(QAbstractItemView::InternalMove);
        itemList->setDefaultDropAction(Qt::MoveAction);
        itemList->setDragEnabled(true);
        itemList->setAcceptDrops(true);
        itemList->setDropIndicatorShown(true);

        QLabel *movingHint = new QLabel(QStringLiteral("正在移动"), &dialog);
        movingHint->setAlignment(Qt::AlignCenter);
        movingHint->setStyleSheet("font-size:24px;font-weight:800;color:#234037;background:rgba(47,143,70,120);border-radius:12px;padding:6px 12px;");
        movingHint->hide();
    auto refreshList = [&]() {
        itemList->clear();
        for (int idx = 0; idx < dayPlan.items.size(); ++idx) {
            const WorkoutItem &item = dayPlan.items[idx];
            QStringList lines;
            lines << item.name;
            lines << QString("间歇：%1 秒").arg(item.restSeconds);
            lines << QString("热身组：%1").arg(setsPreviewText(item.warmupSets));
            lines << QString("正式组：%1").arg(setsPreviewText(item.workSets));
            addPreviewCardItem(itemList,
                               lines,
                               QStringLiteral("#f5fbf7"),
                               gUiTuning.dialogListItemHeight + 420,
                               gUiTuning.planManagerListFont + 20,
                               qMax(28, gUiTuning.planManagerListFont + 10));
            QListWidgetItem *listItem = itemList->item(itemList->count() - 1);
            if (listItem) {
                listItem->setData(Qt::UserRole, idx);
            }
        }
    };
    refreshList();

    QObject::connect(itemList->model(), &QAbstractItemModel::rowsAboutToBeMoved, &dialog, [=](const QModelIndex &, int sourceStart, int, const QModelIndex &, int) {
        movingHint->show();
        if (sourceStart >= 0 && sourceStart < itemList->count()) {
            QListWidgetItem *it = itemList->item(sourceStart);
            QWidget *w = it ? itemList->itemWidget(it) : nullptr;
            if (w) {
                auto *eff = new QGraphicsOpacityEffect(w);
                eff->setOpacity(0.55);
                w->setGraphicsEffect(eff);
            }
        }
    });

    QObject::connect(itemList->model(), &QAbstractItemModel::rowsMoved, &dialog, [=, &dayPlan](const QModelIndex &, int, int, const QModelIndex &, int) {
        movingHint->hide();
        QVector<WorkoutItem> oldItems = dayPlan.items;
        QVector<WorkoutItem> reordered;
        reordered.reserve(itemList->count());
        for (int row = 0; row < itemList->count(); ++row) {
            QListWidgetItem *it = itemList->item(row);
            if (!it) {
                continue;
            }
            QWidget *w = itemList->itemWidget(it);
            if (w && w->graphicsEffect()) {
                w->graphicsEffect()->deleteLater();
                w->setGraphicsEffect(nullptr);
            }
            const int oldIdx = it->data(Qt::UserRole).toInt();
            if (oldIdx >= 0 && oldIdx < oldItems.size()) {
                reordered.push_back(oldItems[oldIdx]);
            }
        }
        if (reordered.size() == oldItems.size()) {
            dayPlan.items = reordered;
            refreshList();
        }
    });

    QHBoxLayout *itemButtons = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(QStringLiteral("添加项目"), &dialog);
    QPushButton *editBtn = new QPushButton(QStringLiteral("编辑项目"), &dialog);
    QPushButton *delBtn = new QPushButton(QStringLiteral("删除项目"), &dialog);
    QPushButton *upBtn = new QPushButton(QStringLiteral("上移"), &dialog);
    QPushButton *downBtn = new QPushButton(QStringLiteral("下移"), &dialog);
    const QString dayEditBtnStyle = QString("font-size:%1px;min-height:%2px;font-weight:800;padding:12px 14px;border-radius:14px;")
                                        .arg(gUiTuning.planManagerActionFont + 4)
                                        .arg(gUiTuning.planManagerActionHeight + 12);
    addBtn->setStyleSheet(dayEditBtnStyle + "background:#2f8f46;color:white;");
    editBtn->setStyleSheet(dayEditBtnStyle + "background:#3d7db8;color:white;");
    delBtn->setStyleSheet(dayEditBtnStyle + "background:#c95d5d;color:white;");
    upBtn->setStyleSheet(dayEditBtnStyle + "background:#5f7f66;color:white;");
    downBtn->setStyleSheet(dayEditBtnStyle + "background:#5f7f66;color:white;");
    itemButtons->addWidget(addBtn);
    itemButtons->addWidget(editBtn);
    itemButtons->addWidget(delBtn);
    itemButtons->addWidget(upBtn);
    itemButtons->addWidget(downBtn);
    itemButtons->addStretch();

    QObject::connect(addBtn, &QPushButton::clicked, &dialog, [&]() {
        WorkoutItem item;
        item.name = QStringLiteral("新项目");
        if (editWorkoutItemDialog(&dialog, item)) {
            dayPlan.items.push_back(item);
            refreshList();
        }
    });

    QObject::connect(editBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = itemList->currentRow();
        if (row < 0 || row >= dayPlan.items.size()) {
            return;
        }
        WorkoutItem item = dayPlan.items[row];
        if (editWorkoutItemDialog(&dialog, item)) {
            dayPlan.items[row] = item;
            refreshList();
        }
    });

    QObject::connect(delBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = itemList->currentRow();
        if (row < 0 || row >= dayPlan.items.size()) {
            return;
        }
        if (!askChineseQuestionDialog(&dialog,
                                      QStringLiteral("确认删除"),
                                      QStringLiteral("确认删除该单项目吗？"),
                                      QStringLiteral("删除"),
                                      QStringLiteral("取消"))) {
            return;
        }
        dayPlan.items.removeAt(row);
        refreshList();
    });

    QObject::connect(upBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = itemList->currentRow();
        if (row <= 0 || row >= dayPlan.items.size()) {
            return;
        }
        WorkoutItem tmp = dayPlan.items[row];
        dayPlan.items[row] = dayPlan.items[row - 1];
        dayPlan.items[row - 1] = tmp;
        refreshList();
        itemList->setCurrentRow(row - 1);
    });

    QObject::connect(downBtn, &QPushButton::clicked, &dialog, [&]() {
        const int row = itemList->currentRow();
        if (row < 0 || row >= dayPlan.items.size() - 1) {
            return;
        }
        WorkoutItem tmp = dayPlan.items[row];
        dayPlan.items[row] = dayPlan.items[row + 1];
        dayPlan.items[row + 1] = tmp;
        refreshList();
        itemList->setCurrentRow(row + 1);
    });

    QDialogButtonBox *buttons = createChineseSaveCancelButtons(&dialog);
    buttons->setStyleSheet(QString("QPushButton{font-size:%1px;min-height:%2px;font-weight:800;padding:10px 14px;border-radius:14px;}")
                               .arg(gUiTuning.planManagerActionFont + 4)
                               .arg(gUiTuning.planManagerActionHeight + 12));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QWidget *dayInfoSection = new QWidget(&dialog);
    dayInfoSection->setStyleSheet("background:#f6faf7;border-radius:14px;");
    QVBoxLayout *dayInfoLayout = new QVBoxLayout(dayInfoSection);
    dayInfoLayout->setContentsMargins(8, 8, 8, 8);
    dayInfoLayout->addLayout(form);
    layout->addWidget(dayInfoSection);
    QFrame *daySectionDivider = new QFrame(&dialog);
    daySectionDivider->setFrameShape(QFrame::HLine);
    daySectionDivider->setStyleSheet("color:#c9d7d0;background:#c9d7d0;min-height:2px;max-height:2px;");
    layout->addWidget(daySectionDivider);
    QLabel *itemListTitle = new QLabel(QStringLiteral("单项目列表"), &dialog);
    itemListTitle->setStyleSheet("font-size:47px;font-weight:900;color:#203328;");
    layout->addWidget(itemListTitle);
    layout->addWidget(movingHint);
    layout->addWidget(itemList);
    layout->addLayout(itemButtons);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    dayPlan.title = dayTitleInput.trimmed();
    dayPlan.defaultMinutes = defaultMinutesInput;
    if (dayPlan.title.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("单天标题不能为空。"));
        return false;
    }
    return true;
}

bool editMasterPlanDialog(QWidget *parent, MasterPlan &plan)
{
    QDialog dialog(parent);
    setupMobileDialog(dialog, parent);
    dialog.setWindowTitle(QStringLiteral("编辑总计划"));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    applyDialogVerticalRatioMargins(layout, dialog);
    QFormLayout *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    form->setHorizontalSpacing(22);
    form->setVerticalSpacing(20);

    QString planNameInput = plan.name;
    int trainDaysInput = qMax(1, plan.trainDays);
    int restDaysInput = qMax(1, plan.restDays);

    QPushButton *nameBtn = new QPushButton(&dialog);
    QPushButton *trainBtn = new QPushButton(&dialog);
    QPushButton *restBtn = new QPushButton(&dialog);
    const QString numberBtnStyle = "font-size:32px;min-height:96px;background:#f4f8f5;color:#203328;border:2px solid #cfe0d5;border-radius:14px;padding:10px 14px;text-align:left;";
    nameBtn->setStyleSheet(numberBtnStyle);
    trainBtn->setStyleSheet(numberBtnStyle);
    restBtn->setStyleSheet(numberBtnStyle);
    auto refreshDayButtons = [&]() {
        nameBtn->setText(planNameInput.isEmpty() ? QStringLiteral("点击输入总计划名称") : planNameInput);
        trainBtn->setText(QString("%1 天").arg(trainDaysInput));
        restBtn->setText(QString("%1 天").arg(restDaysInput));
    };
    refreshDayButtons();

    QObject::connect(nameBtn, &QPushButton::clicked, &dialog, [&]() {
        QString value = planNameInput;
        if (runCompactTextInputDialog(&dialog,
                                      QStringLiteral("设置总计划名称"),
                                      QStringLiteral("请输入总计划名称"),
                                      value)) {
            planNameInput = value.trimmed();
            refreshDayButtons();
        }
    });

    const QDate initialDate = plan.startDate.isValid() ? plan.startDate : QDate::currentDate();
    QComboBox *yearCombo = createWheelCombo(&dialog, 2020, 2045, 1, QStringLiteral("年"));
    QComboBox *monthCombo = createWheelCombo(&dialog, 1, 12, 1, QStringLiteral("月"));
    QComboBox *dayCombo = createWheelCombo(&dialog, 1, 31, 1, QStringLiteral("日"));
    yearCombo->setCurrentIndex(qMax(0, yearCombo->findData(initialDate.year())));
    monthCombo->setCurrentIndex(qMax(0, monthCombo->findData(initialDate.month())));

    auto refillDayCombo = [&]() {
        const int y = comboValue(yearCombo, initialDate.year());
        const int m = comboValue(monthCombo, initialDate.month());
        const int current = comboValue(dayCombo, initialDate.day());
        dayCombo->clear();
        const int maxDay = QDate(y, m, 1).daysInMonth();
        for (int d = 1; d <= maxDay; ++d) {
            dayCombo->addItem(QString("%1日").arg(d), d);
        }
        const int newDay = qMin(current, maxDay);
        const int idx = dayCombo->findData(newDay);
        dayCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    };
    QObject::connect(yearCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int) { refillDayCombo(); });
    QObject::connect(monthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int) { refillDayCombo(); });
    refillDayCombo();

    QWidget *dateRow = new QWidget(&dialog);
    QHBoxLayout *dateLayout = new QHBoxLayout(dateRow);
    dateLayout->setContentsMargins(0, 0, 0, 0);
    dateLayout->setSpacing(10);
    dateLayout->addWidget(yearCombo, 2);
    dateLayout->addWidget(monthCombo, 1);
    dateLayout->addWidget(dayCombo, 1);

    form->addRow(QStringLiteral("总计划名称"), nameBtn);
    form->addRow(QStringLiteral("练几天"), trainBtn);
    form->addRow(QStringLiteral("休几天"), restBtn);
    form->addRow(QStringLiteral("开始日期"), dateRow);
    if (QWidget *l = form->labelForField(nameBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }
    if (QWidget *l = form->labelForField(trainBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }
    if (QWidget *l = form->labelForField(restBtn)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }
    if (QWidget *l = form->labelForField(dateRow)) {
        l->setStyleSheet("font-size:32px;font-weight:800;");
    }

    auto appendEmptyDayPlan = [&](int idx) {
        DayPlan day;
        day.title = QString("第%1天计划").arg(idx + 1);
        day.defaultMinutes = 60;
        plan.dayPlans.push_back(day);
    };
    auto trainDaysValue = [&]() {
        return trainDaysInput;
    };

    if (plan.dayPlans.isEmpty()) {
        for (int i = 0; i < trainDaysValue(); ++i) {
            appendEmptyDayPlan(i);
        }
    }

    QListWidget *dayList = new QListWidget(&dialog);
    enableMobileSingleFingerScroll(dayList);
    dayList->setStyleSheet(
        QString("QListWidget{font-size:%1px;border:1px solid #d3e1d8;border-radius:16px;background:#eef4ff;padding:10px;}"
                "QListWidget::item{min-height:%2px;padding:10px 8px;}"
                "QListWidget::item:selected{background:#e8f5ec;border-radius:12px;color:#173326;}")
            .arg(gUiTuning.planManagerListFont + 16)
            .arg(gUiTuning.dialogListItemHeight + 48));
    dayList->setDragDropMode(QAbstractItemView::InternalMove);
    dayList->setDefaultDropAction(Qt::MoveAction);
    dayList->setDragEnabled(true);
    dayList->setAcceptDrops(true);
    dayList->setDropIndicatorShown(true);

    QLabel *movingDayHint = new QLabel(QStringLiteral("正在移动"), &dialog);
    movingDayHint->setAlignment(Qt::AlignCenter);
    movingDayHint->setStyleSheet("font-size:24px;font-weight:800;color:#234037;background:rgba(47,143,70,120);border-radius:12px;padding:6px 12px;");
    movingDayHint->hide();
    auto refreshDayList = [&]() {
        dayList->clear();
        for (int i = 0; i < plan.dayPlans.size(); ++i) {
            const DayPlan &day = plan.dayPlans[i];
            QStringList names;
            for (const WorkoutItem &w : day.items) {
                names << w.name;
            }
            const QString preview = names.isEmpty() ? QStringLiteral("无动作") : names.join(QStringLiteral("、"));
            QStringList lines;
            lines << QString("%1. %2").arg(i + 1).arg(day.title);
            lines << QString("默认训练：%1 分钟  |  项目数：%2").arg(day.defaultMinutes).arg(day.items.size());
            lines << QString("动作预览：%1").arg(preview);
            addPreviewCardItem(dayList,
                               lines,
                               (i % 2 == 0) ? QStringLiteral("#f1faf4") : QStringLiteral("#eef5ff"),
                               gUiTuning.dialogListItemHeight + 420,
                               gUiTuning.planManagerListFont + 20,
                               qMax(28, gUiTuning.planManagerListFont + 10));
            QListWidgetItem *listItem = dayList->item(dayList->count() - 1);
            if (listItem) {
                listItem->setData(Qt::UserRole, i);
            }
        }
    };
    refreshDayList();

    QObject::connect(dayList->model(), &QAbstractItemModel::rowsAboutToBeMoved, &dialog, [=](const QModelIndex &, int sourceStart, int, const QModelIndex &, int) {
        movingDayHint->show();
        if (sourceStart >= 0 && sourceStart < dayList->count()) {
            QListWidgetItem *it = dayList->item(sourceStart);
            QWidget *w = it ? dayList->itemWidget(it) : nullptr;
            if (w) {
                auto *eff = new QGraphicsOpacityEffect(w);
                eff->setOpacity(0.55);
                w->setGraphicsEffect(eff);
            }
        }
    });

    QObject::connect(dayList->model(), &QAbstractItemModel::rowsMoved, &dialog, [=, &plan](const QModelIndex &, int, int, const QModelIndex &, int) {
        movingDayHint->hide();
        QVector<DayPlan> oldDays = plan.dayPlans;
        QVector<DayPlan> reordered;
        reordered.reserve(dayList->count());
        for (int row = 0; row < dayList->count(); ++row) {
            QListWidgetItem *it = dayList->item(row);
            if (!it) {
                continue;
            }
            QWidget *w = dayList->itemWidget(it);
            if (w && w->graphicsEffect()) {
                w->graphicsEffect()->deleteLater();
                w->setGraphicsEffect(nullptr);
            }
            const int oldIdx = it->data(Qt::UserRole).toInt();
            if (oldIdx >= 0 && oldIdx < oldDays.size()) {
                reordered.push_back(oldDays[oldIdx]);
            }
        }
        if (reordered.size() == oldDays.size()) {
            plan.dayPlans = reordered;
            refreshDayList();
        }
    });

    QObject::connect(trainBtn, &QPushButton::clicked, &dialog, [&]() {
        int value = trainDaysInput;
        if (runCompactNumberInputDialog(&dialog,
                                        QStringLiteral("设置训练日天数"),
                                        QStringLiteral("请输入练几天"),
                                        1,
                                        14,
                                        value)) {
            trainDaysInput = value;
            refreshDayButtons();
            if (trainDaysInput > plan.dayPlans.size()) {
                const int oldSize = plan.dayPlans.size();
                for (int i = oldSize; i < trainDaysInput; ++i) {
                    appendEmptyDayPlan(i);
                }
            } else if (trainDaysInput < plan.dayPlans.size()) {
                while (plan.dayPlans.size() > trainDaysInput) {
                    plan.dayPlans.removeLast();
                }
            }
            refreshDayList();
        }
    });

    QObject::connect(restBtn, &QPushButton::clicked, &dialog, [&]() {
        int value = restDaysInput;
        if (runCompactNumberInputDialog(&dialog,
                                        QStringLiteral("设置休息日天数"),
                                        QStringLiteral("请输入休几天"),
                                        1,
                                        14,
                                        value)) {
            restDaysInput = value;
            refreshDayButtons();
        }
    });

    QHBoxLayout *dayOps = new QHBoxLayout();
    QPushButton *addDay = new QPushButton(QStringLiteral("添加单天计划"), &dialog);
    QPushButton *editDay = new QPushButton(QStringLiteral("编辑单天计划"), &dialog);
    QPushButton *deleteDay = new QPushButton(QStringLiteral("删除单天计划"), &dialog);
    QPushButton *upDay = new QPushButton(QStringLiteral("上移"), &dialog);
    QPushButton *downDay = new QPushButton(QStringLiteral("下移"), &dialog);
    const QString masterDayBtnStyle = QString("font-size:%1px;min-height:%2px;font-weight:800;padding:12px 14px;border-radius:14px;")
                                          .arg(gUiTuning.planManagerActionFont + 4)
                                          .arg(gUiTuning.planManagerActionHeight + 12);
    addDay->setStyleSheet(masterDayBtnStyle + "background:#2f8f46;color:white;");
    editDay->setStyleSheet(masterDayBtnStyle + "background:#3d7db8;color:white;");
    deleteDay->setStyleSheet(masterDayBtnStyle + "background:#c95d5d;color:white;");
    upDay->setStyleSheet(masterDayBtnStyle + "background:#5f7f66;color:white;");
    downDay->setStyleSheet(masterDayBtnStyle + "background:#5f7f66;color:white;");
    dayOps->addWidget(addDay);
    dayOps->addWidget(editDay);
    dayOps->addWidget(deleteDay);
    dayOps->addWidget(upDay);
    dayOps->addWidget(downDay);
    dayOps->addStretch();

    QObject::connect(addDay, &QPushButton::clicked, &dialog, [&]() {
        if (plan.dayPlans.size() >= trainDaysValue()) {
            QMessageBox::information(&dialog,
                                     QStringLiteral("提示"),
                                     QString("当前训练日为%1天。若要新增第%2个单天计划，请先把训练日改为%2天。")
                                         .arg(trainDaysValue())
                                         .arg(plan.dayPlans.size() + 1));
            return;
        }
        DayPlan day;
        day.title = QStringLiteral("新单天计划");
        if (editDayPlanDialog(&dialog, day)) {
            plan.dayPlans.push_back(day);
            refreshDayList();
        }
    });

    QObject::connect(editDay, &QPushButton::clicked, &dialog, [&]() {
        const int row = dayList->currentRow();
        if (row < 0 || row >= plan.dayPlans.size()) {
            return;
        }
        DayPlan day = plan.dayPlans[row];
        if (editDayPlanDialog(&dialog, day)) {
            plan.dayPlans[row] = day;
            refreshDayList();
        }
    });

    QObject::connect(deleteDay, &QPushButton::clicked, &dialog, [&]() {
        const int row = dayList->currentRow();
        if (row < 0 || row >= plan.dayPlans.size()) {
            return;
        }
        if (!askChineseQuestionDialog(&dialog,
                                      QStringLiteral("确认删除"),
                                      QStringLiteral("确认删除该单天计划吗？"),
                                      QStringLiteral("删除"),
                                      QStringLiteral("取消"))) {
            return;
        }
        plan.dayPlans.removeAt(row);
        refreshDayList();
    });

    QObject::connect(upDay, &QPushButton::clicked, &dialog, [&]() {
        const int row = dayList->currentRow();
        if (row <= 0 || row >= plan.dayPlans.size()) {
            return;
        }
        DayPlan tmp = plan.dayPlans[row];
        plan.dayPlans[row] = plan.dayPlans[row - 1];
        plan.dayPlans[row - 1] = tmp;
        refreshDayList();
        dayList->setCurrentRow(row - 1);
    });
    QObject::connect(downDay, &QPushButton::clicked, &dialog, [&]() {
        const int row = dayList->currentRow();
        if (row < 0 || row >= plan.dayPlans.size() - 1) {
            return;
        }
        DayPlan tmp = plan.dayPlans[row];
        plan.dayPlans[row] = plan.dayPlans[row + 1];
        plan.dayPlans[row + 1] = tmp;
        refreshDayList();
        dayList->setCurrentRow(row + 1);
    });

    QDialogButtonBox *buttons = createChineseSaveCancelButtons(&dialog);
    buttons->setStyleSheet(QString("QPushButton{font-size:%1px;min-height:%2px;font-weight:800;padding:10px 14px;border-radius:14px;}")
                               .arg(gUiTuning.planManagerActionFont + 4)
                               .arg(gUiTuning.planManagerActionHeight + 12));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QWidget *masterInfoSection = new QWidget(&dialog);
    masterInfoSection->setStyleSheet("background:#f6faf7;border-radius:14px;");
    QVBoxLayout *masterInfoLayout = new QVBoxLayout(masterInfoSection);
    masterInfoLayout->setContentsMargins(8, 8, 8, 8);
    masterInfoLayout->addLayout(form);
    layout->addWidget(masterInfoSection);
    QFrame *masterSectionDivider = new QFrame(&dialog);
    masterSectionDivider->setFrameShape(QFrame::HLine);
    masterSectionDivider->setStyleSheet("color:#c9d7d0;background:#c9d7d0;min-height:2px;max-height:2px;");
    layout->addWidget(masterSectionDivider);
    layout->addWidget(movingDayHint);

    QLabel *dayPlanLabel = new QLabel(QStringLiteral("单天计划列表"), &dialog);
    dayPlanLabel->setStyleSheet("font-size:49px;font-weight:900;color:#203328;");
    layout->addWidget(dayPlanLabel);

    layout->addWidget(dayList);
    layout->addLayout(dayOps);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    plan.name = planNameInput.trimmed();
    plan.trainDays = trainDaysValue();
    plan.restDays = restDaysInput;
    plan.startDate = QDate(comboValue(yearCombo, initialDate.year()),
                           comboValue(monthCombo, initialDate.month()),
                           comboValue(dayCombo, initialDate.day()));

    if (plan.name.isEmpty() || plan.dayPlans.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("提示"), QStringLiteral("总计划名称不能为空且至少需要一个单天计划。"));
        return false;
    }
    return true;
}

bool runThemeSelectionDialog(QWidget *parent,
                             QString &themeOut,
                             bool newUserEntry,
                             const QString &windowTitle,
                             const QString &tipText,
                             const QString &confirmText)
{
    QDialog dialog(parent);
    setupMobileDialog(dialog, parent);
    dialog.setWindowTitle(windowTitle);

    if (newUserEntry) {
        dialog.setWindowState(dialog.windowState() | Qt::WindowFullScreen);
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen ? screen->geometry() : QRect(0, 0, 1080, 1920);
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();

    int horizontalMargin = screenWidth / 15;
    int verticalMargin = screenHeight / 9;

    struct ThemeStyle {
        QString name;
        QString code;
        QString pageColor;
        QString panelColor;
        QString titleColor;
        QString accentColor;
        QString optionColor;
        QString selectedOptionColor;
        QString mainDesc;
        QString detailDesc;
    };

    QVector<ThemeStyle> themes = {
        {
            QStringLiteral("健身主题"),
            QStringLiteral("fitness"),
            QStringLiteral("#ffd7b0"),
            QStringLiteral("#ffe9d2"),
            QStringLiteral("#b85a00"),
            QStringLiteral("#d56b00"),
            QStringLiteral("#ffe8cf"),
            QStringLiteral("#ffd3aa"),
            QStringLiteral("活力·训练·进度"),
            QStringLiteral("专为运动健身设计的主题，强调活力和动力，\n帮助您保持运动热情，追踪训练进度。")
        },
        {
            QStringLiteral("学习主题"),
            QStringLiteral("study"),
            QStringLiteral("#cde4ff"),
            QStringLiteral("#e2efff"),
            QStringLiteral("#1f5fa8"),
            QStringLiteral("#2d74c6"),
            QStringLiteral("#e8f1ff"),
            QStringLiteral("#cfe2ff"),
            QStringLiteral("专注·清晰·冷静"),
            QStringLiteral("专为学习工作设计的主题，帮助集中注意力，\n提供清晰冷静的界面，提升学习效率。")
        },
        {
            QStringLiteral("普通计划主题"),
            QStringLiteral("normal"),
            QStringLiteral("#cdeccf"),
            QStringLiteral("#ddf3de"),
            QStringLiteral("#2f7b42"),
            QStringLiteral("#3e9a55"),
            QStringLiteral("#e7f8e8"),
            QStringLiteral("#cff0d2"),
            QStringLiteral("中性·轻量·日常"),
            QStringLiteral("适合日常使用的主题，简洁轻量，\n不打扰您的日常生活，保持界面干净清爽。")
        }
    };

    int currentThemeIndex = 0;

    QWidget *mainContainer = new QWidget(&dialog);
    mainContainer->setStyleSheet(QString("background:%1;").arg(themes[currentThemeIndex].pageColor));

    QHBoxLayout *containerLayout = new QHBoxLayout(mainContainer);
    containerLayout->setContentsMargins(horizontalMargin, verticalMargin, horizontalMargin, verticalMargin);

    QWidget *contentWidget = new QWidget(mainContainer);
    contentWidget->setStyleSheet(QString("background:%1;border:none;border-radius:20px;").arg(themes[currentThemeIndex].panelColor));

    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    contentLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel(windowTitle, contentWidget);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QString("font-size:60px;font-weight:900;padding:20px 0 10px 0;color:%1;")
                                  .arg(themes[currentThemeIndex].titleColor));

    QLabel *tipLabel = new QLabel(tipText, contentWidget);
    tipLabel->setAlignment(Qt::AlignCenter);
    tipLabel->setWordWrap(true);
    tipLabel->setStyleSheet("font-size:30px;font-weight:700;color:#3d4d46;padding:8px 0 16px 0;");

    QWidget *themeOptionsWidget = new QWidget(contentWidget);
    themeOptionsWidget->setStyleSheet("background:transparent;border:none;");

    QHBoxLayout *themesLayout = new QHBoxLayout(themeOptionsWidget);
    themesLayout->setContentsMargins(0, 0, 0, 0);
    themesLayout->setSpacing(20);

    QVector<QPushButton *> themeCards;

    for (int i = 0; i < themes.size(); ++i) {
        const ThemeStyle &theme = themes[i];
        QPushButton *card = new QPushButton(themeOptionsWidget);
        card->setCursor(Qt::PointingHandCursor);
        card->setCheckable(true);
        card->setMinimumHeight(260);
        card->setText(QString("%1\n%2").arg(theme.name).arg(theme.mainDesc));
        card->setStyleSheet(QString("font-size:34px;font-weight:800;line-height:1.4;border:none;border-radius:20px;padding:18px;"
                                    "background:%1;color:%2;")
                                .arg(theme.optionColor)
                                .arg(theme.accentColor));
        if (i == 0) {
            card->setChecked(true);
        }
        themeCards.push_back(card);
        themesLayout->addWidget(card);
    }

    QLabel *detailCard = new QLabel(contentWidget);
    detailCard->setAlignment(Qt::AlignCenter);
    detailCard->setWordWrap(true);
    detailCard->setMinimumHeight(160);
    detailCard->setStyleSheet("background:#f8f9fa;border:none;border-radius:20px;padding:25px;margin:30px 0;font-size:30px;color:#333333;");
    detailCard->setText(themes[currentThemeIndex].detailDesc);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(20);

    QPushButton *cancelBtn = new QPushButton(QStringLiteral("取消"), contentWidget);
    QPushButton *confirmBtn = new QPushButton(confirmText, contentWidget);

    QString btnCommonStyle = 
        "font-size:36px;"
        "font-weight:800;"
        "min-height:110px;"
        "border-radius:20px;"
        "padding:0 40px;"
        "border:none;";

    cancelBtn->setStyleSheet(btnCommonStyle + "background:#e0e0e0;color:#666;");
    confirmBtn->setStyleSheet(QString(btnCommonStyle + "background:%1;color:white;").arg(themes[currentThemeIndex].accentColor));

    confirmBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cancelBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(confirmBtn);

    contentLayout->addWidget(titleLabel);
    contentLayout->addWidget(tipLabel);
    contentLayout->addSpacing(12);
    contentLayout->addWidget(themeOptionsWidget, 1);
    contentLayout->addWidget(detailCard);
    contentLayout->addLayout(buttonLayout);

    containerLayout->addWidget(contentWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(mainContainer);

    auto updateThemeColors = [&](int themeIndex) {
        if (themeIndex < 0 || themeIndex >= themes.size()) {
            return;
        }

        const ThemeStyle &theme = themes[themeIndex];
        currentThemeIndex = themeIndex;

        mainContainer->setStyleSheet(QString("background:%1;").arg(theme.pageColor));
        contentWidget->setStyleSheet(QString("background:%1;border:none;border-radius:20px;").arg(theme.panelColor));
        titleLabel->setStyleSheet(QString("font-size:60px;font-weight:900;padding:20px 0 10px 0;color:%1;").arg(theme.titleColor));

        for (int i = 0; i < themeCards.size(); ++i) {
            const ThemeStyle &cardTheme = themes[i];
            bool isSelected = (i == themeIndex);
            themeCards[i]->setChecked(isSelected);
            themeCards[i]->setStyleSheet(QString("font-size:34px;font-weight:800;line-height:1.4;border:none;border-radius:20px;padding:18px;"
                                                "background:%1;color:%2;")
                                             .arg(isSelected ? cardTheme.selectedOptionColor : cardTheme.optionColor)
                                             .arg(cardTheme.accentColor));
            themeCards[i]->setText(isSelected
                                       ? QString("%1 ?\n%2").arg(cardTheme.name).arg(cardTheme.mainDesc)
                                       : QString("%1\n%2").arg(cardTheme.name).arg(cardTheme.mainDesc));
        }

        detailCard->setText(theme.detailDesc);
        detailCard->setStyleSheet(QString("background:%1;border:none;border-radius:20px;padding:25px;margin:30px 0;font-size:30px;color:#2e2e2e;")
                                      .arg(theme.selectedOptionColor));
        confirmBtn->setStyleSheet(btnCommonStyle + QString("background:%1;color:white;").arg(theme.accentColor));
    };
 
    for (int i = 0; i < themeCards.size(); ++i) {
        QObject::connect(themeCards[i], &QPushButton::clicked, &dialog, [=, &updateThemeColors]() {
            updateThemeColors(i);
        });
    }

    QObject::connect(confirmBtn, &QPushButton::clicked, &dialog, [&]() {
        const QString picked = themes[currentThemeIndex].code;
        if (!mouseplan::common::ThemeFeatureGate::isThemeAvailable(picked)) {
            mouseplan::common::ThemeFeatureGate::showThemeUnavailableHint(&dialog, picked);
            return;
        }
        themeOut = picked;
        dialog.accept();
    });

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    themeOut = QStringLiteral("fitness");
    updateThemeColors(0);

    contentWidget->setMinimumHeight(screenHeight - 2 * verticalMargin);

    return dialog.exec() == QDialog::Accepted;
}

void showLoadingOverlayDialog(QWidget *parent,
                              int blankHeight,
                              const QString &blankColor,
                              const QString &centerColor,
                              const QString &loadingText,
                              bool withLogo,
                              bool moveUpStrong)
{
    QDialog dialog(parent);
    setupMobileDialog(dialog, parent);
    dialog.setWindowTitle(QStringLiteral("加载中"));

    QVBoxLayout *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *topBlank = new QFrame(&dialog);
    topBlank->setFixedHeight(qMax(80, blankHeight));
    topBlank->setStyleSheet(QString("background:%1;border:none;").arg(blankColor));

    QFrame *bottomBlank = new QFrame(&dialog);
    bottomBlank->setFixedHeight(qMax(80, blankHeight));
    bottomBlank->setStyleSheet(QString("background:%1;border:none;").arg(blankColor));

    QWidget *center = new QWidget(&dialog);
    center->setStyleSheet(QString("background:%1;").arg(centerColor));
    QVBoxLayout *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);

    QWidget *group = new QWidget(center);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(12);

    if (withLogo) {
        QLabel *logo = new QLabel(group);
        logo->setAlignment(Qt::AlignCenter);
        QPixmap icon(kDefaultAvatarResourcePath);
        if (!icon.isNull()) {
            logo->setPixmap(icon.scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            logo->setText(QStringLiteral("MousePlan"));
            logo->setStyleSheet("font-size:48px;font-weight:900;color:#ffffff;background:transparent;");
        }
        groupLayout->addWidget(logo);
    }

    QLabel *loadingLabel = new QLabel(loadingText, group);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLabel->setStyleSheet("font-size:36px;font-weight:900;color:#ffffff;background:transparent;border:none;");
    groupLayout->addWidget(loadingLabel);

    centerLayout->addStretch(moveUpStrong ? 1 : 2);
    centerLayout->addWidget(group, 0, Qt::AlignHCenter);
    centerLayout->addStretch(moveUpStrong ? 7 : 3);

    root->addWidget(topBlank);
    root->addWidget(center, 1);
    root->addWidget(bottomBlank);

    int frame = 0;
    QTimer anim;
    anim.setInterval(120);
    QObject::connect(&anim, &QTimer::timeout, &dialog, [&]() {
        const QString dots = QString(frame % 4, QLatin1Char('.'));
        loadingLabel->setText(loadingText + dots);
        frame++;
    });
    anim.start();

    QTimer::singleShot(360, &dialog, &QDialog::accept);
    dialog.exec();
}

QString downloadFileWithProgress(QWidget *parent,
                                 const QUrl &url,
                                 const QString &suggestedFileName,
                                 QString *errorText)
{
    if (!url.isValid()) {
        if (errorText) {
            *errorText = QStringLiteral("下载地址无效");
        }
        return QString();
    }

    // 优先级顺序：CacheLocation（最安全，无需权限） -> AppDataLocation -> DownloadLocation
    QString downloadRoot;
    QStringList candidates;
    
    // 1. 应用缓存目录（最优，无需 WRITE_EXTERNAL_STORAGE 权限）
    QString cacheScope = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!cacheScope.trimmed().isEmpty()) {
        candidates.append(cacheScope);
    }
    
    // 2. 应用数据目录（次优，无需 WRITE_EXTERNAL_STORAGE 权限）
    QString appDataScope = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataScope.trimmed().isEmpty()) {
        candidates.append(appDataScope);
    }
    
    // 3. 公共下载目录（需 WRITE_EXTERNAL_STORAGE 权限）
    QString downloadScope = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!downloadScope.trimmed().isEmpty()) {
        candidates.append(downloadScope);
    }
    
    // 4. 回退到主目录
    candidates.append(QDir::homePath());

    // 尝试创建目录并打开文件，直到成功
    QFile output;
    for (const QString &candidate : candidates) {
        QDir dir(candidate);
        if (!dir.exists()) {
            if (!dir.mkpath(QStringLiteral("."))) {
                continue;
            }
        }
        
        const QString fileName = suggestedFileName.trimmed().isEmpty()
                                     ? QStringLiteral("MousePlan_update.apk")
                                     : suggestedFileName;
        const QString outputPath = dir.filePath(fileName);
        
        output.setFileName(outputPath);
        if (output.open(QIODevice::WriteOnly)) {
            downloadRoot = outputPath;
            break;
        }
    }

    if (downloadRoot.isEmpty() || !output.isOpen()) {
        if (errorText) {
            *errorText = QStringLiteral("无法创建更新包文件：无可用的存储位置或权限不足。请检查存储权限设置。");
        }
        return QString();
    }

    QProgressDialog progress(parent);
    progress.setWindowTitle(QStringLiteral("软件更新"));
    progress.setLabelText(QStringLiteral("正在下载更新包..."));
    progress.setCancelButtonText(QStringLiteral("取消"));
    progress.setRange(0, 0);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    QFont progressFont = progress.font();
    if (progressFont.pointSize() > 0) {
        progressFont.setPointSize(qMax(progressFont.pointSize() * 2, progressFont.pointSize() + 10));
    }
    progress.setFont(progressFont);
    progress.setStyleSheet(QStringLiteral("QLabel{font-size:32px;}QPushButton{font-size:30px;min-height:88px;min-width:180px;}QProgressBar{min-height:42px;font-size:24px;}"));
    const QSize progressBaseSize = progress.sizeHint();
    const int scaledW = qMax(progressBaseSize.width(), static_cast<int>(progressBaseSize.width() * 2.0));
    const int scaledH = qMax(progressBaseSize.height(), static_cast<int>(progressBaseSize.height() * 2.0));
    progress.setMinimumSize(scaledW, scaledH);
    progress.resize(scaledW, scaledH);

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        output.write(reply->readAll());
    });

    QObject::connect(reply, &QNetworkReply::downloadProgress, [&](qint64 received, qint64 total) {
        if (total > 0) {
            progress.setRange(0, 100);
            const int percent = static_cast<int>((received * 100) / total);
            progress.setValue(qBound(0, percent, 100));
            const double recMb = static_cast<double>(received) / (1024.0 * 1024.0);
            const double totalMb = static_cast<double>(total) / (1024.0 * 1024.0);
            progress.setLabelText(QStringLiteral("正在下载更新包... %1% (%2MB/%3MB)")
                                      .arg(progress.value())
                                      .arg(QString::number(recMb, 'f', 1))
                                      .arg(QString::number(totalMb, 'f', 1)));
        } else {
            progress.setRange(0, 0);
            progress.setLabelText(QStringLiteral("正在下载更新包..."));
        }
    });

    QObject::connect(&progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    output.flush();
    output.close();
    progress.close();

    if (reply->error() != QNetworkReply::NoError) {
        const bool canceled = reply->error() == QNetworkReply::OperationCanceledError;
        if (errorText) {
            *errorText = canceled ? QStringLiteral("已取消下载")
                                  : QStringLiteral("下载失败：%1").arg(reply->errorString());
        }
        reply->deleteLater();
        output.remove();
        return QString();
    }

    reply->deleteLater();
    return downloadRoot;
}
