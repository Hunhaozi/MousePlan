#ifndef APPDATA_H
#define APPDATA_H

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QDateTime>

//热身组或正式组容器
struct PlanSet {
    double weightKg = 0.0;      //重量
    int reps = 0;               //次数
    QString remark;             //备注

    //方法: 设置该组对象的重量、次数、备注
    PlanSet() {}
    PlanSet(double weight, int repeat, const QString &setRemark = QString())
        : weightKg(weight)
        , reps(repeat)
        , remark(setRemark.left(64))
    {}
};

struct WorkoutItem {
    QString name;                   //单项目名称
    int restSeconds = 90;           //单项目间歇时间
    QVector<PlanSet> warmupSets;    //热身组容器
    QVector<PlanSet> workSets;      //正式组容器
};

// 单日训练计划：包含标题、默认时长和项目列表
struct DayPlan {
    QString title;                  //单日计划标题
    int defaultMinutes = 60;        //单日默认训练时间
    QVector<WorkoutItem> items;     //单项目容器
};

//总计划设置
struct MasterPlan {
    QString id;                 //总计划ID
    QString ownerUserId;        //所属用户ID
    QString name;               //总计划名称
    int trainDays = 3;          //训练日
    int restDays = 1;           //休息日
    QDate startDate;            //开始时间
    QVector<DayPlan> dayPlans;  //单日计划容器
};

// 单个项目在某日训练记录中的状态快照
struct RecordItem {
    WorkoutItem item;               //项目基础信息
    bool completed = false;         //该项目是否已完成
    bool ignored = false;           //该项目是否被忽略
    QVector<bool> warmupChecked;    //热身组逐组勾选状态
    QVector<bool> workChecked;      //正式组逐组勾选状态
};

// 某一天完整训练记录
struct RecordDay {
    QString title;                  //当日记录标题
    QVector<RecordItem> items;      //当日项目记录列表
};

// 训练打卡记录：用于日历显示与提交状态追踪
struct TrainingRecord {
    QString ownerUserId;            //所属用户ID
    QDate date;                     //记录日期
    RecordDay day;                  //当日记录详情
    bool submitted = false;         //是否已正式提交打卡
    int totalMinutes = 0;           //总训练分钟数
    bool isSupplement = false;      //是否为补录记录
};

// 注册码实体：用于在线注册授权控制
struct RegistrationCode {
    QString code;                   //注册码明文或标识
    bool used = false;              //是否已使用
    QString usedByUserId;           //被哪个用户使用
};

//用户相关具体信息
struct User {
    QString id;                     //ID
    QString username;               //用户名
    QString password;               //密码
    bool isLocalAccount = true;     //是否为本地登录模式
    QString theme = "fitness";      //默认主题为"健身主题"
    QString themeColorPreset;       //主题色变量
    bool themeChosen = false;       //是否已经选择了主题
    QString activePlanId;           //活动的总计划ID(即当前选择的总计划)
    QDate rememberLoginUntil;       //是否登陆过
    QString avatarText;             //头像文字
    QString avatarImagePath;        //头像图片路径
    QString profileCoverImagePath;  //底部图片路径
    QString nickname;               //昵称
    QString gender;                 //性别
    int age = -1;                   //年龄
    QString messageToMouse;         //对大耗子说的话
};

//全局总数据结构
class AppDataStore {
public:
    // 构造数据存储对象，baseDirPath 为数据根目录
    explicit AppDataStore(const QString &baseDirPath);

    // 从磁盘加载完整应用数据
    bool load();
    // 将当前内存数据保存到磁盘
    bool save() const;
    // 初始化默认数据（首次启动或数据缺失时）
    void ensureDefaultData();
    // 创建默认总计划模板（不自动写入容器）
    MasterPlan createMouseDefaultPresetPlan(const QString &ownerUserId) const;
    // 为指定用户添加默认总计划
    bool addMouseDefaultPresetPlan(const QString &ownerUserId,
                                   QString *createdPlanId = nullptr,
                                   bool setActiveIfEmpty = true);
    // 保存用户 package1 到本地文件
    bool saveUserPackage1Local(const QString &userId) const;
    // 保存用户 package1 到远端模拟文件
    bool saveUserPackage1RemoteMock(const QString &userId) const;
    // 加载用户最佳 package1 快照（本地/在线模式）
    bool loadBestUserPackage1(const QString &userId, bool isOnlineMode);
    // 构建用户 package1 的 JSON 快照
    QJsonObject buildUserPackage1Snapshot(const QString &userId) const;
    // 应用并落地用户 package1 快照
    bool applyUserPackage1Snapshot(const QString &userId,
                                   const QJsonObject &root,
                                   bool saveRemoteSnapshot = true);
    // 导出总计划 package2 文件
    bool exportPlanPackage2(const QString &planId, const QString &outputFilePath) const;
    // 导入总计划 package2 文件
    bool importPlanPackage2(const QString &ownerUserId,
                            const QString &filePath,
                            QString *createdPlanId = nullptr);

    QVector<User> users;                            //全部用户列表
    QVector<RegistrationCode> registrationCodes;    //注册码池
    QVector<MasterPlan> plans;                      //总计划列表
    QVector<TrainingRecord> records;                //训练记录列表

private:
    QString dataFilePath;                           //主数据文件路径

    // 获取用户 package1 本地路径
    QString package1LocalFilePath(const QString &userId) const;
    // 获取用户 package1 远端模拟路径
    QString package1RemoteMockFilePath(const QString &userId) const;
    // 构建用户 package1 JSON 对象
    QJsonObject buildUserPackage1Json(const QString &userId) const;
    // 应用用户 package1 JSON 内容
    bool applyUserPackage1Json(const QString &userId, const QJsonObject &root);
    // 读取 package 快照时间戳
    static QDateTime packageTimestamp(const QJsonObject &root);

    // 以下为各数据结构的 JSON 序列化
    static QJsonObject toJson(const PlanSet &value);
    static QJsonObject toJson(const WorkoutItem &value);
    static QJsonObject toJson(const DayPlan &value);
    static QJsonObject toJson(const MasterPlan &value);
    static QJsonObject toJson(const RecordItem &value);
    static QJsonObject toJson(const RecordDay &value);
    static QJsonObject toJson(const TrainingRecord &value);
    static QJsonObject toJson(const RegistrationCode &value);
    static QJsonObject toJson(const User &value);

    // 以下为各数据结构的 JSON 反序列化
    static PlanSet planSetFromJson(const QJsonObject &obj);
    static WorkoutItem workoutItemFromJson(const QJsonObject &obj);
    static DayPlan dayPlanFromJson(const QJsonObject &obj);
    static MasterPlan masterPlanFromJson(const QJsonObject &obj);
    static RecordItem recordItemFromJson(const QJsonObject &obj);
    static RecordDay recordDayFromJson(const QJsonObject &obj);
    static TrainingRecord trainingRecordFromJson(const QJsonObject &obj);
    static RegistrationCode registrationCodeFromJson(const QJsonObject &obj);
    static User userFromJson(const QJsonObject &obj);
};

#endif // APPDATA_H
