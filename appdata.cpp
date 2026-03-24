#include "appdata.h"

#include <QDir>
#include <QFile>
#include <QJsonValue>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFileInfo>
#include <QUuid>
#include <QCryptographicHash>

namespace {
QString dateToString(const QDate &date)
{
    return date.toString("yyyy-MM-dd");
}

QDate dateFromString(const QString &value)
{
    return QDate::fromString(value, "yyyy-MM-dd");
}

double roundWeightOneDecimal(double value)
{
    return qRound(value * 10.0) / 10.0;
}

QString sha256Hex(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool isSha256Hex(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() != 64) {
        return false;
    }
    for (const QChar ch : trimmed) {
        if (!((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
              || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
              || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F')))) {
            return false;
        }
    }
    return true;
}

QString normalizeSecretHash(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    return isSha256Hex(trimmed) ? trimmed.toLower() : sha256Hex(trimmed);
}

bool parseMousePresetPlan(const QString &ownerUserId, const QString &raw, MasterPlan &plan)
{
    QStringList lines = raw.split('\n');
    for (QString &line : lines) {
        line = line.trimmed();
    }

    plan = MasterPlan();
    plan.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    plan.ownerUserId = ownerUserId;
    plan.name = QStringLiteral("Mouse_五休一");
    plan.trainDays = 5;
    plan.restDays = 1;
    plan.startDate = QDate::currentDate();

    int defaultMinutes = 100;
    DayPlan *currentDay = nullptr;
    WorkoutItem *currentItem = nullptr;
    enum class SetMode { None, Warmup, Work };
    SetMode mode = SetMode::None;

    const QRegularExpression nameRx(QStringLiteral("^总计划名称[:：]\\s*(.+)$"));
    const QRegularExpression cycleRx(QStringLiteral("练\\s*(\\d+)\\s*天.*休息\\s*(\\d+)\\s*天"));
    const QRegularExpression minutesRx(QStringLiteral("每日预计训练时间[:：]\\s*(\\d+)"));
    const QRegularExpression dayRx(QStringLiteral("^第.+天[:：]\\s*(.+)$"));
    const QRegularExpression itemRx(QStringLiteral("^项目\\s*\\d+[:：]?\\s*(.+)$"));
    const QRegularExpression restRx(QStringLiteral("^间歇(?:时间)?[:：]?\\s*(\\d+)"));
    const QRegularExpression warmupRx(QStringLiteral("^热身组[:：]?\\s*(\\d*)"));
    const QRegularExpression workRx(QStringLiteral("^正式组[:：]?\\s*(\\d*)"));
    const QRegularExpression setRx(QStringLiteral("^组\\s*\\d+\\s+(\\d+)\\s+([0-9]+(?:\\.[0-9]+)?)(?:kg)?\\s*(.*)$"));

    for (const QString &line : lines) {
        if (line.isEmpty()) {
            continue;
        }

        QRegularExpressionMatch m = nameRx.match(line);
        if (m.hasMatch()) {
            plan.name = m.captured(1).trimmed();
            continue;
        }

        m = cycleRx.match(line);
        if (m.hasMatch()) {
            plan.trainDays = qMax(1, m.captured(1).toInt());
            plan.restDays = qMax(1, m.captured(2).toInt());
            continue;
        }

        m = minutesRx.match(line);
        if (m.hasMatch()) {
            defaultMinutes = qMax(1, m.captured(1).toInt());
            continue;
        }

        m = dayRx.match(line);
        if (m.hasMatch()) {
            DayPlan day;
            day.title = m.captured(1).trimmed();
            day.defaultMinutes = defaultMinutes;
            plan.dayPlans.push_back(day);
            currentDay = &plan.dayPlans.back();
            currentItem = nullptr;
            mode = SetMode::None;
            continue;
        }

        m = itemRx.match(line);
        if (m.hasMatch() && currentDay) {
            WorkoutItem item;
            item.name = m.captured(1).trimmed();
            item.restSeconds = 90;
            currentDay->items.push_back(item);
            currentItem = &currentDay->items.back();
            mode = SetMode::None;
            continue;
        }

        m = restRx.match(line);
        if (m.hasMatch() && currentItem) {
            currentItem->restSeconds = qMax(0, m.captured(1).toInt());
            continue;
        }

        if (warmupRx.match(line).hasMatch()) {
            mode = SetMode::Warmup;
            continue;
        }
        if (workRx.match(line).hasMatch()) {
            mode = SetMode::Work;
            continue;
        }

        m = setRx.match(line);
        if (m.hasMatch() && currentItem) {
            const int reps = qMax(0, m.captured(1).toInt());
            const double weight = roundWeightOneDecimal(m.captured(2).toDouble());
            QString remark = m.captured(3).trimmed();
            if (remark == QStringLiteral("无")) {
                remark.clear();
            }
            PlanSet set(weight, reps, remark);
            if (mode == SetMode::Work) {
                currentItem->workSets.push_back(set);
            } else {
                currentItem->warmupSets.push_back(set);
            }
        }
    }

    return !plan.dayPlans.isEmpty();
}

bool buildMousePresetPlan(const QString &ownerUserId, MasterPlan &plan)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("Mouse.txt")),
        QDir(appDir).filePath(QStringLiteral("../Mouse.txt")),
        QStringLiteral("Mouse.txt")
    };
    for (const QString &path : candidates) {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString raw = QString::fromUtf8(f.readAll());
        f.close();
        if (parseMousePresetPlan(ownerUserId, raw, plan)) {
            return true;
        }
    }
    return false;
}

    bool buildStructuredMousePresetPlan(const QString &ownerUserId, MasterPlan &plan)
    {
        struct SetSpec {
            int reps;
            double weight;
            QString remark;

            SetSpec(int r, double w, const QString &rm = QString())
                : reps(r)
                , weight(w)
                , remark(rm)
            {}
        };

        auto sets = [](std::initializer_list<SetSpec> list) {
        QVector<PlanSet> out;
        out.reserve(static_cast<int>(list.size()));
        for (const SetSpec &spec : list) {
            out.push_back(PlanSet(roundWeightOneDecimal(spec.weight), spec.reps, spec.remark));
        }
        return out;
        };

        auto addItem = [](DayPlan &day,
                  const QString &name,
                  int restSeconds,
                  const QVector<PlanSet> &warmup,
                  const QVector<PlanSet> &work) {
        WorkoutItem item;
        item.name = name;
        item.restSeconds = restSeconds;
        item.warmupSets = warmup;
        item.workSets = work;
        day.items.push_back(item);
        };

        plan = MasterPlan();
        plan.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        plan.ownerUserId = ownerUserId;
        plan.name = QStringLiteral("Mouse_五休一");
        plan.trainDays = 5;
        plan.restDays = 1;
        plan.startDate = QDate::currentDate();

        DayPlan d1;
        d1.title = QStringLiteral("胸部训练");
        d1.defaultMinutes = 90;
        addItem(d1, QStringLiteral("热身动作集合"), 20,
            sets({{1, 0, QStringLiteral("泡沫轴滚背")},
              {1, 0, QStringLiteral("弹力带前后环绕")},
              {1, 0, QStringLiteral("弹力带胸前拉伸")},
              {10, 0, QStringLiteral("俯卧撑")},
              {10, 0, QStringLiteral("俯卧撑")}}),
            {});
        addItem(d1, QStringLiteral("蝴蝶机夹胸（热身）"), 80,
            sets({{12, 20, QString()}, {12, 20, QString()}}),
            {});
        addItem(d1, QStringLiteral("平板杠铃卧推（中胸）"), 100,
            sets({{8, 20, QStringLiteral("空杆")}, {5, 30, QStringLiteral("递增")}, {5, 40, QStringLiteral("递增")}}),
            sets({{5, 45, QString()}, {5, 45, QString()}, {5, 45, QString()}, {5, 45, QString()}, {5, 45, QString()}}));
        addItem(d1, QStringLiteral("双杠臂屈伸（下胸）"), 100,
            {},
            sets({{10, 0, QString()}, {10, 0, QString()}, {10, 0, QString()}, {10, 0, QString()}}));
        addItem(d1, QStringLiteral("上斜杠铃卧推/上斜器械推胸（上胸）"), 100,
            sets({{8, 20, QStringLiteral("空杆/找对应重量")}}),
            sets({{8, 35, QString()}, {8, 35, QString()}, {8, 35, QString()}, {8, 35, QString()}}));
        addItem(d1, QStringLiteral("蝴蝶机夹胸（胸中缝）"), 100,
            {},
            sets({{12, 35, QString()}, {12, 35, QString()}, {12, 35, QString()}, {12, 35, QString()}}));
        addItem(d1, QStringLiteral("腹部（仰卧体坐 .. ）"), 120,
            {},
            sets({{12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}}));
        addItem(d1, QStringLiteral("放松"), 20, {}, {});
        plan.dayPlans.push_back(d1);

        DayPlan d2;
        d2.title = QStringLiteral("背部训练");
        d2.defaultMinutes = 90;
        addItem(d2, QStringLiteral("热身动作集合"), 20,
            sets({{1, 0, QStringLiteral("泡沫轴滚背")}, {2, 0, QStringLiteral("手肘滚泡沫轴")}, {1, 0, QStringLiteral("弹力带前后环绕")}, {5, 0, QStringLiteral("俯卧撑")}}),
            {});
        addItem(d2, QStringLiteral("引体向上（热身）"), 100,
            sets({{10, 40, QStringLiteral("窄距")}, {10, 40, QStringLiteral("窄距")}}),
            {});
        addItem(d2, QStringLiteral("硬拉（综合）"), 120,
            sets({{5, 30, QString()}, {5, 40, QString()}}),
            sets({{5, 60, QString()}, {5, 60, QString()}, {5, 60, QString()}, {5, 60, QString()}}));
        addItem(d2, QStringLiteral("高位下拉（宽度）"), 110,
            sets({{8, 33, QString()}}),
            sets({{12, 40, QString()}, {12, 40, QString()}, {12, 40, QString()}, {12, 40, QString()}}));
        addItem(d2, QStringLiteral("坐姿绳索划船（厚度）"), 100,
            sets({{8, 26, QString()}}),
            sets({{12, 40, QString()}, {12, 40, QString()}, {12, 40, QString()}, {12, 40, QString()}}));
        addItem(d2, QStringLiteral("引体向上（宽度）"), 100,
            {},
            sets({{12, 33, QStringLiteral("窄距")}, {12, 33, QStringLiteral("宽距")}, {12, 33, QStringLiteral("窄距")}, {12, 33, QStringLiteral("宽距")}}));
        addItem(d2, QStringLiteral("绳索直臂下压（下背）"), 100,
            sets({{8, 26, QString()}}),
            sets({{12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}}));
        addItem(d2, QStringLiteral("放松"), 20, {}, {});
        plan.dayPlans.push_back(d2);

        DayPlan d3;
        d3.title = QStringLiteral("肩部训练");
        d3.defaultMinutes = 90;
        addItem(d3, QStringLiteral("热身动作集合1"), 20,
            sets({{1, 0, QStringLiteral("泡沫轴滚背")}, {2, 0, QStringLiteral("手肘滚泡沫轴")}, {1, 0, QStringLiteral("弹力带前后环绕")}, {5, 0, QStringLiteral("俯卧撑")}}),
            {});
        addItem(d3, QStringLiteral("热身动作集合2"), 60,
            sets({{12, 5, QStringLiteral("哑铃推肩膀")}, {12, 5, QStringLiteral("招财猫")}}),
            {});
        addItem(d3, QStringLiteral("坐姿史密斯推肩（前束）"), 120,
            sets({{8, 20, QString()}}),
            sets({{8, 30, QString()}, {8, 30, QString()}, {8, 30, QString()}, {8, 30, QString()}}));
        addItem(d3, QStringLiteral("器械推肩（前束）"), 110,
            sets({{10, 33, QString()}}),
            sets({{12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}}));
        addItem(d3, QStringLiteral("绳索/器械侧平举（中束）"), 100,
            sets({{8, 10, QString()}}),
            sets({{12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}}));
        addItem(d3, QStringLiteral("坐姿俯身哑铃飞鸟（后束）"), 100,
            {},
            sets({{12, 5, QStringLiteral("递减")}, {12, 5, QStringLiteral("递减")}, {12, 5, QStringLiteral("递减")}, {12, 5, QStringLiteral("递减")}}));
        addItem(d3, QStringLiteral("蝴蝶机反向飞鸟（后束）"), 100,
            sets({{8, 26, QString()}}),
            sets({{12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}}));
        addItem(d3, QStringLiteral("腹部（仰卧体坐 .. ）"), 120,
            {},
            sets({{12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}}));
        addItem(d3, QStringLiteral("放松"), 20, {}, {});
        plan.dayPlans.push_back(d3);

        DayPlan d4;
        d4.title = QStringLiteral("手臂训练");
        d4.defaultMinutes = 90;
        addItem(d4, QStringLiteral("热身动作集合1"), 20,
            sets({{1, 0, QStringLiteral("泡沫轴滚背")}, {2, 0, QStringLiteral("手肘滚泡沫轴")}, {1, 0, QStringLiteral("弹力带前后环绕")}, {5, 0, QStringLiteral("俯卧撑")}}),
            {});
        addItem(d4, QStringLiteral("热身动作集合2"), 60,
            sets({{12, 5, QStringLiteral("哑铃弯举（二头）")}, {12, 5, QStringLiteral("哑铃弯举（三头）")}}),
            {});
        addItem(d4, QStringLiteral("窄距卧推（三头）"), 120,
            sets({{5, 20, QString()}}),
            sets({{8, 20, QString()}, {8, 20, QString()}, {8, 20, QString()}, {8, 20, QString()}}));
        addItem(d4, QStringLiteral("绳索下拉（三头）"), 110,
            sets({{10, 26, QString()}}),
            sets({{12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}}));
        addItem(d4, QStringLiteral("杠铃弯举（三头）"), 100,
            {},
            sets({{12, 15, QString()}, {12, 15, QString()}, {12, 15, QString()}, {12, 15, QString()}}));
        addItem(d4, QStringLiteral("杠铃弯举（二头）"), 100,
            {},
            sets({{12, 15, QString()}, {12, 15, QString()}, {12, 15, QString()}, {12, 15, QString()}}));
        addItem(d4, QStringLiteral("哑铃弯举/绳索弯举"), 100,
            {},
                sets({{12, 7.5, QString()}, {12, 7.5, QString()}, {12, 7.5, QString()}, {12, 7.5, QString()}}));
        addItem(d4, QStringLiteral("放松"), 20, {}, {});
        plan.dayPlans.push_back(d4);

        DayPlan d5;
        d5.title = QStringLiteral("腿部训练");
        d5.defaultMinutes = 90;
        addItem(d5, QStringLiteral("热身动作集合1"), 20,
            sets({{1, 0, QStringLiteral("泡沫轴滚大腿")}, {1, 0, QStringLiteral("泡沫轴滚小腿")}, {1, 0, QStringLiteral("弹力带环绕")}}),
            {});
        addItem(d5, QStringLiteral("热身动作集合2"), 60,
            sets({{1, 0, QStringLiteral("静态拉伸")}, {12, 0, QStringLiteral("弓步动态")}, {12, 0, QStringLiteral("开合跳深蹲")}}),
            {});
        addItem(d5, QStringLiteral("杠铃深蹲"), 120,
            sets({{5, 20, QString()}, {5, 40, QString()}}),
            sets({{8, 45, QString()}, {8, 50, QString()}, {8, 50, QString()}, {8, 50, QString()}}));
        addItem(d5, QStringLiteral("哈克深蹲"), 100,
            {},
            sets({{12, 50, QString()}, {12, 50, QString()}, {12, 50, QString()}}));
        addItem(d5, QStringLiteral("保加利亚蹲"), 110,
            {},
                sets({{12, 7.5, QString()}, {12, 7.5, QString()}, {12, 7.5, QString()}, {12, 7.5, QString()}}));
        addItem(d5, QStringLiteral("腿屈伸"), 100,
            {},
            sets({{12, 35, QString()}, {12, 35, QString()}, {12, 35, QString()}, {12, 35, QString()}}));
        addItem(d5, QStringLiteral("杠铃浅蹲起（小腿）"), 100,
            {},
            sets({{12, 30, QString()}, {12, 730, QString()}, {12, 30, QString()}, {12, 30, QString()}}));
        addItem(d5, QStringLiteral("内收肌/外扩肌"), 100,
            {},
            sets({{12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}, {12, 33, QString()}}));
        addItem(d5, QStringLiteral("腹部（仰卧体坐 .. ）"), 120,
            {},
            sets({{12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}, {12, 10, QString()}}));
        addItem(d5, QStringLiteral("放松"), 20, {}, {});
        plan.dayPlans.push_back(d5);

        return !plan.dayPlans.isEmpty();
    }
}

AppDataStore::AppDataStore(const QString &baseDirPath)
{
    QDir dir(baseDirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    dataFilePath = dir.filePath("mouse_plan_data.json");
}

bool AppDataStore::load()
{
    //打开总数据文件
    QFile file(dataFilePath);
    if (!file.exists()) {
        return false;
    }


    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return false;
    }

    //清空现有数据
    users.clear();
    registrationCodes.clear();
    plans.clear();
    records.clear();

    const QJsonObject root = doc.object();

    for (const QJsonValue &v : root.value("users").toArray()) {
        users.push_back(userFromJson(v.toObject()));
    }
    for (const QJsonValue &v : root.value("registrationCodes").toArray()) {
        registrationCodes.push_back(registrationCodeFromJson(v.toObject()));
    }
    for (const QJsonValue &v : root.value("plans").toArray()) {
        plans.push_back(masterPlanFromJson(v.toObject()));
    }
    for (const QJsonValue &v : root.value("records").toArray()) {
        records.push_back(trainingRecordFromJson(v.toObject()));
    }

    // Backward compatibility: migrate old plaintext fields to sha256 in memory.
    for (User &u : users) {
        u.password = normalizeSecretHash(u.password);
    }
    for (RegistrationCode &c : registrationCodes) {
        c.code = normalizeSecretHash(c.code);
    }
    return true;
}

bool AppDataStore::save() const
{
    QJsonObject root;

    QJsonArray userArray;
    for (const User &u : users) {
        userArray.append(toJson(u));
    }
    root.insert("users", userArray);

    QJsonArray codeArray;
    for (const RegistrationCode &c : registrationCodes) {
        codeArray.append(toJson(c));
    }
    root.insert("registrationCodes", codeArray);

    QJsonArray planArray;
    for (const MasterPlan &p : plans) {
        planArray.append(toJson(p));
    }
    root.insert("plans", planArray);

    QJsonArray recordArray;
    for (const TrainingRecord &r : records) {
        recordArray.append(toJson(r));
    }
    root.insert("records", recordArray);

    QFile file(dataFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void AppDataStore::ensureDefaultData()
{
    bool hasMouse = false;
    bool createdMouseUser = false;
    QString mouseUserId;
    for (const User &u : users) {
        if (u.username == "mouse") {
            hasMouse = true;
            mouseUserId = u.id;
            break;
        }
    }

    if (!hasMouse) {
        User defaultUser;
        defaultUser.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        defaultUser.username = "mouse";
        defaultUser.password = normalizeSecretHash(QStringLiteral("hhaozi"));
        defaultUser.isLocalAccount = true;
        defaultUser.theme = "fitness";
        defaultUser.themeChosen = false;
        mouseUserId = defaultUser.id;
        users.push_back(defaultUser);
        createdMouseUser = true;
    }

    //默认注册码
    const QStringList requiredCodePlaintexts = {
        QStringLiteral("7Q9K2M8P4T1X6R3N"),
        QStringLiteral("A5D8L2C9V7B1N4Q6")
    };

    for (const QString &codeText : requiredCodePlaintexts) {
        const QString hashedCode = normalizeSecretHash(codeText);
        bool found = false;
        for (RegistrationCode &code : registrationCodes) {
            code.code = normalizeSecretHash(code.code);
            if (code.code == hashedCode) {
                found = true;
                break;
            }
        }
        if (!found) {
            RegistrationCode code;
            code.code = hashedCode;
            code.used = false;
            registrationCodes.push_back(code);
        }
    }

    if (createdMouseUser && !mouseUserId.isEmpty()) {
        addMouseDefaultPresetPlan(mouseUserId, nullptr, true);
    }
}

MasterPlan AppDataStore::createMouseDefaultPresetPlan(const QString &ownerUserId) const
{
    MasterPlan preset;
    if (buildStructuredMousePresetPlan(ownerUserId, preset)) {
        return preset;
    }
    if (buildMousePresetPlan(ownerUserId, preset)) {
        return preset;
    }

    preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    preset.ownerUserId = ownerUserId;
    preset.name = QStringLiteral("Mouse_五休一");
    preset.trainDays = 5;
    preset.restDays = 1;
    preset.startDate = QDate::currentDate();
    for (int i = 0; i < 5; ++i) {
        DayPlan day;
        day.title = QStringLiteral("第%1天").arg(i + 1);
        day.defaultMinutes = 90;
        preset.dayPlans.push_back(day);
    }
    return preset;
}

bool AppDataStore::addMouseDefaultPresetPlan(const QString &ownerUserId,
                                             QString *createdPlanId,
                                             bool setActiveIfEmpty)
{
    if (ownerUserId.trimmed().isEmpty()) {
        return false;
    }

    MasterPlan preset = createMouseDefaultPresetPlan(ownerUserId);
    plans.push_back(preset);

    if (createdPlanId) {
        *createdPlanId = preset.id;
    }

    if (setActiveIfEmpty) {
        for (User &u : users) {
            if (u.id == ownerUserId && u.activePlanId.trimmed().isEmpty()) {
                u.activePlanId = preset.id;
                break;
            }
        }
    }
    return true;
}

QString AppDataStore::package1LocalFilePath(const QString &userId) const
{
    const QString rootDir = QFileInfo(dataFilePath).absolutePath();
    QDir dir(rootDir);
    dir.mkpath(QStringLiteral("packages"));
    return dir.filePath(QStringLiteral("packages/package1_local_%1.mp1.json").arg(userId));
}

QString AppDataStore::package1RemoteMockFilePath(const QString &userId) const
{
    const QString rootDir = QFileInfo(dataFilePath).absolutePath();
    QDir dir(rootDir);
    dir.mkpath(QStringLiteral("packages"));
    return dir.filePath(QStringLiteral("packages/package1_remote_%1.mp1.json").arg(userId));
}

QJsonObject AppDataStore::buildUserPackage1Json(const QString &userId) const
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("mouseplan.package1.v1"));
    root.insert(QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("userId"), userId);

    int userIndex = -1;
    for (int i = 0; i < users.size(); ++i) {
        if (users[i].id == userId) {
            userIndex = i;
            break;
        }
    }
    if (userIndex < 0) {
        return QJsonObject();
    }

    root.insert(QStringLiteral("user"), toJson(users[userIndex]));

    QJsonArray planArray;
    for (const MasterPlan &p : plans) {
        if (p.ownerUserId == userId) {
            planArray.append(toJson(p));
        }
    }
    root.insert(QStringLiteral("plans"), planArray);

    QJsonArray recordArray;
    for (const TrainingRecord &r : records) {
        if (r.ownerUserId == userId) {
            recordArray.append(toJson(r));
        }
    }
    root.insert(QStringLiteral("records"), recordArray);
    return root;
}

QDateTime AppDataStore::packageTimestamp(const QJsonObject &root)
{
    return QDateTime::fromString(root.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
}

bool AppDataStore::applyUserPackage1Json(const QString &userId, const QJsonObject &root)
{
    if (root.value(QStringLiteral("schema")).toString() != QStringLiteral("mouseplan.package1.v1")) {
        return false;
    }

    int userIndex = -1;
    for (int i = 0; i < users.size(); ++i) {
        if (users[i].id == userId) {
            userIndex = i;
            break;
        }
    }
    if (userIndex < 0) {
        return false;
    }

    const User oldUser = users[userIndex];
    User loadedUser = userFromJson(root.value(QStringLiteral("user")).toObject());
    loadedUser.id = oldUser.id;
    loadedUser.username = oldUser.username;
    loadedUser.password = oldUser.password;
    loadedUser.isLocalAccount = oldUser.isLocalAccount;
    users[userIndex] = loadedUser;

    for (int i = plans.size() - 1; i >= 0; --i) {
        if (plans[i].ownerUserId == userId) {
            plans.removeAt(i);
        }
    }
    for (const QJsonValue &v : root.value(QStringLiteral("plans")).toArray()) {
        MasterPlan p = masterPlanFromJson(v.toObject());
        p.ownerUserId = userId;
        if (p.id.trimmed().isEmpty()) {
            p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        plans.push_back(p);
    }

    for (int i = records.size() - 1; i >= 0; --i) {
        if (records[i].ownerUserId == userId) {
            records.removeAt(i);
        }
    }
    for (const QJsonValue &v : root.value(QStringLiteral("records")).toArray()) {
        TrainingRecord r = trainingRecordFromJson(v.toObject());
        r.ownerUserId = userId;
        records.push_back(r);
    }
    return true;
}

bool AppDataStore::saveUserPackage1Local(const QString &userId) const
{
    const QJsonObject root = buildUserPackage1Json(userId);
    if (root.isEmpty()) {
        return false;
    }
    QFile file(package1LocalFilePath(userId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool AppDataStore::saveUserPackage1RemoteMock(const QString &userId) const
{
    const QJsonObject root = buildUserPackage1Json(userId);
    if (root.isEmpty()) {
        return false;
    }
    QFile file(package1RemoteMockFilePath(userId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool AppDataStore::loadBestUserPackage1(const QString &userId, bool isOnlineMode)
{
    QJsonObject localRoot;
    QJsonObject remoteRoot;

    QFile localFile(package1LocalFilePath(userId));
    if (localFile.exists() && localFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument d = QJsonDocument::fromJson(localFile.readAll());
        localFile.close();
        if (d.isObject()) {
            localRoot = d.object();
        }
    }

    if (isOnlineMode) {
        QFile remoteFile(package1RemoteMockFilePath(userId));
        if (remoteFile.exists() && remoteFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument d = QJsonDocument::fromJson(remoteFile.readAll());
            remoteFile.close();
            if (d.isObject()) {
                remoteRoot = d.object();
            }
        }
    }

    QJsonObject picked;
    if (!localRoot.isEmpty() && remoteRoot.isEmpty()) {
        picked = localRoot;
    } else if (localRoot.isEmpty() && !remoteRoot.isEmpty()) {
        picked = remoteRoot;
    } else if (!localRoot.isEmpty() && !remoteRoot.isEmpty()) {
        const QDateTime localTs = packageTimestamp(localRoot);
        const QDateTime remoteTs = packageTimestamp(remoteRoot);
        picked = (remoteTs.isValid() && remoteTs > localTs) ? remoteRoot : localRoot;
    }

    if (picked.isEmpty()) {
        return false;
    }
    return applyUserPackage1Json(userId, picked);
}

QJsonObject AppDataStore::buildUserPackage1Snapshot(const QString &userId) const
{
    return buildUserPackage1Json(userId);
}

bool AppDataStore::applyUserPackage1Snapshot(const QString &userId,
                                             const QJsonObject &root,
                                             bool saveRemoteSnapshot)
{
    if (!applyUserPackage1Json(userId, root)) {
        return false;
    }

    if (saveRemoteSnapshot) {
        QFile file(package1RemoteMockFilePath(userId));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            file.close();
        }
    }
    return true;
}

bool AppDataStore::exportPlanPackage2(const QString &planId, const QString &outputFilePath) const
{
    int idx = -1;
    for (int i = 0; i < plans.size(); ++i) {
        if (plans[i].id == planId) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("mouseplan.package2.v1"));
    root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("plan"), toJson(plans[idx]));

    QFile file(outputFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool AppDataStore::importPlanPackage2(const QString &ownerUserId,
                                      const QString &filePath,
                                      QString *createdPlanId)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("schema")).toString() != QStringLiteral("mouseplan.package2.v1")) {
        return false;
    }

    MasterPlan p = masterPlanFromJson(root.value(QStringLiteral("plan")).toObject());
    if (p.name.trimmed().isEmpty() || p.dayPlans.isEmpty()) {
        return false;
    }
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.ownerUserId = ownerUserId;
    plans.push_back(p);
    if (createdPlanId) {
        *createdPlanId = p.id;
    }
    return true;
}

QJsonObject AppDataStore::toJson(const PlanSet &value)
{
    QJsonObject obj;
    obj.insert("weightKg", roundWeightOneDecimal(value.weightKg));
    obj.insert("reps", value.reps);
    obj.insert("remark", value.remark.left(64));
    return obj;
}

QJsonObject AppDataStore::toJson(const WorkoutItem &value)
{
    QJsonObject obj;
    obj.insert("name", value.name);
    obj.insert("restSeconds", value.restSeconds);

    QJsonArray warmup;
    for (const PlanSet &set : value.warmupSets) {
        warmup.append(toJson(set));
    }
    obj.insert("warmupSets", warmup);

    QJsonArray work;
    for (const PlanSet &set : value.workSets) {
        work.append(toJson(set));
    }
    obj.insert("workSets", work);

    return obj;
}

QJsonObject AppDataStore::toJson(const DayPlan &value)
{
    QJsonObject obj;
    obj.insert("title", value.title);
    obj.insert("defaultMinutes", value.defaultMinutes);

    QJsonArray items;
    for (const WorkoutItem &item : value.items) {
        items.append(toJson(item));
    }
    obj.insert("items", items);
    return obj;
}

QJsonObject AppDataStore::toJson(const MasterPlan &value)
{
    QJsonObject obj;
    obj.insert("id", value.id);
    obj.insert("ownerUserId", value.ownerUserId);
    obj.insert("name", value.name);
    obj.insert("trainDays", value.trainDays);
    obj.insert("restDays", value.restDays);
    obj.insert("startDate", dateToString(value.startDate));

    QJsonArray dayPlans;
    for (const DayPlan &d : value.dayPlans) {
        dayPlans.append(toJson(d));
    }
    obj.insert("dayPlans", dayPlans);
    return obj;
}

QJsonObject AppDataStore::toJson(const RecordItem &value)
{
    QJsonObject obj;
    obj.insert("item", toJson(value.item));
    obj.insert("completed", value.completed);
    obj.insert("ignored", value.ignored);

    QJsonArray warmupChecked;
    for (bool v : value.warmupChecked) {
        warmupChecked.append(v);
    }
    obj.insert("warmupChecked", warmupChecked);

    QJsonArray workChecked;
    for (bool v : value.workChecked) {
        workChecked.append(v);
    }
    obj.insert("workChecked", workChecked);
    return obj;
}

QJsonObject AppDataStore::toJson(const RecordDay &value)
{
    QJsonObject obj;
    obj.insert("title", value.title);

    QJsonArray items;
    for (const RecordItem &i : value.items) {
        items.append(toJson(i));
    }
    obj.insert("items", items);
    return obj;
}

QJsonObject AppDataStore::toJson(const TrainingRecord &value)
{
    QJsonObject obj;
    obj.insert("ownerUserId", value.ownerUserId);
    obj.insert("date", dateToString(value.date));
    obj.insert("submitted", value.submitted);
    obj.insert("totalMinutes", value.totalMinutes);
    obj.insert("isSupplement", value.isSupplement);
    obj.insert("day", toJson(value.day));
    return obj;
}

QJsonObject AppDataStore::toJson(const RegistrationCode &value)
{
    QJsonObject obj;
    obj.insert("code", value.code);
    obj.insert("used", value.used);
    obj.insert("usedByUserId", value.usedByUserId);
    return obj;
}

QJsonObject AppDataStore::toJson(const User &value)
{
    QJsonObject obj;
    obj.insert("id", value.id);
    obj.insert("username", value.username);
    obj.insert("password", value.password);
    obj.insert("isLocalAccount", value.isLocalAccount);
    obj.insert("theme", value.theme);
    obj.insert("themeColorPreset", value.themeColorPreset);
    obj.insert("themeChosen", value.themeChosen);
    obj.insert("activePlanId", value.activePlanId);
    obj.insert("rememberLoginUntil", value.rememberLoginUntil.isValid() ? dateToString(value.rememberLoginUntil) : QString());
    obj.insert("avatarText", value.avatarText);
    obj.insert("avatarImagePath", value.avatarImagePath);
    obj.insert("profileCoverImagePath", value.profileCoverImagePath);
    obj.insert("nickname", value.nickname);
    obj.insert("gender", value.gender);
    obj.insert("age", value.age);
    obj.insert("messageToMouse", value.messageToMouse);
    return obj;
}

PlanSet AppDataStore::planSetFromJson(const QJsonObject &obj)
{
    PlanSet value;
    value.weightKg = roundWeightOneDecimal(obj.value("weightKg").toDouble());
    value.reps = obj.value("reps").toInt();
    value.remark = obj.value("remark").toString().left(64);
    return value;
}

WorkoutItem AppDataStore::workoutItemFromJson(const QJsonObject &obj)
{
    WorkoutItem value;
    value.name = obj.value("name").toString();
    value.restSeconds = obj.value("restSeconds").toInt(90);

    for (const QJsonValue &v : obj.value("warmupSets").toArray()) {
        value.warmupSets.push_back(planSetFromJson(v.toObject()));
    }
    for (const QJsonValue &v : obj.value("workSets").toArray()) {
        value.workSets.push_back(planSetFromJson(v.toObject()));
    }
    return value;
}

DayPlan AppDataStore::dayPlanFromJson(const QJsonObject &obj)
{
    DayPlan value;
    value.title = obj.value("title").toString();
    value.defaultMinutes = obj.value("defaultMinutes").toInt(60);

    for (const QJsonValue &v : obj.value("items").toArray()) {
        value.items.push_back(workoutItemFromJson(v.toObject()));
    }
    return value;
}

MasterPlan AppDataStore::masterPlanFromJson(const QJsonObject &obj)
{
    MasterPlan value;
    value.id = obj.value("id").toString();
    value.ownerUserId = obj.value("ownerUserId").toString();
    value.name = obj.value("name").toString();
    value.trainDays = obj.value("trainDays").toInt(3);
    value.restDays = obj.value("restDays").toInt(1);
    value.startDate = dateFromString(obj.value("startDate").toString());

    for (const QJsonValue &v : obj.value("dayPlans").toArray()) {
        value.dayPlans.push_back(dayPlanFromJson(v.toObject()));
    }
    return value;
}

RecordItem AppDataStore::recordItemFromJson(const QJsonObject &obj)
{
    RecordItem value;
    value.item = workoutItemFromJson(obj.value("item").toObject());
    value.completed = obj.value("completed").toBool(false);
    value.ignored = obj.value("ignored").toBool(false);
    for (const QJsonValue &v : obj.value("warmupChecked").toArray()) {
        value.warmupChecked.push_back(v.toBool(false));
    }
    for (const QJsonValue &v : obj.value("workChecked").toArray()) {
        value.workChecked.push_back(v.toBool(false));
    }
    return value;
}

RecordDay AppDataStore::recordDayFromJson(const QJsonObject &obj)
{
    RecordDay value;
    value.title = obj.value("title").toString();

    for (const QJsonValue &v : obj.value("items").toArray()) {
        value.items.push_back(recordItemFromJson(v.toObject()));
    }
    return value;
}

TrainingRecord AppDataStore::trainingRecordFromJson(const QJsonObject &obj)
{
    TrainingRecord value;
    value.ownerUserId = obj.value("ownerUserId").toString();
    value.date = dateFromString(obj.value("date").toString());
    value.submitted = obj.value("submitted").toBool(false);
    value.totalMinutes = obj.value("totalMinutes").toInt(0);
    value.isSupplement = obj.value("isSupplement").toBool(false);
    value.day = recordDayFromJson(obj.value("day").toObject());
    return value;
}

RegistrationCode AppDataStore::registrationCodeFromJson(const QJsonObject &obj)
{
    RegistrationCode value;
    value.code = normalizeSecretHash(obj.value("code").toString());
    value.used = obj.value("used").toBool(false);
    value.usedByUserId = obj.value("usedByUserId").toString();
    return value;
}

User AppDataStore::userFromJson(const QJsonObject &obj)
{
    User value;
    value.id = obj.value("id").toString();
    value.username = obj.value("username").toString();
    value.password = normalizeSecretHash(obj.value("password").toString());
    value.isLocalAccount = obj.value("isLocalAccount").toBool(true);
    value.theme = obj.value("theme").toString("fitness");
    value.themeColorPreset = obj.value("themeColorPreset").toString();
    value.themeChosen = obj.value("themeChosen").toBool(false);
    value.activePlanId = obj.value("activePlanId").toString();
    value.rememberLoginUntil = dateFromString(obj.value("rememberLoginUntil").toString());
    value.avatarText = obj.value("avatarText").toString();
    value.avatarImagePath = obj.value("avatarImagePath").toString();
    value.profileCoverImagePath = obj.value("profileCoverImagePath").toString();
    value.nickname = obj.value("nickname").toString();
    value.gender = obj.value("gender").toString();
    value.age = obj.value("age").toInt(-1);
    value.messageToMouse = obj.value("messageToMouse").toString();
    return value;
}
