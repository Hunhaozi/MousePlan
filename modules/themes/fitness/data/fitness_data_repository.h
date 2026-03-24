#ifndef FITNESS_DATA_REPOSITORY_H
#define FITNESS_DATA_REPOSITORY_H

#include "appdata.h"
#include "modules/themes/fitness/data/fitness_data_models.h"

namespace mouseplan {
namespace fitness {

// 健身主题数据仓储适配层。
// 目标：将主题业务看到的数据与 AppDataStore 原始结构解耦。
class FitnessDataRepository {
public:
    explicit FitnessDataRepository(const AppDataStore &storeRef);

    // 查询某用户某天的记录视图。
    FitnessCalendarDayView queryCalendarDayView(const QString &userId, const QDate &date) const;

private:
    const AppDataStore &store;
};

} // namespace fitness
} // namespace mouseplan

#endif // FITNESS_DATA_REPOSITORY_H
