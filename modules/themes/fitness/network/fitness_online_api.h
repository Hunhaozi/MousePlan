#ifndef FITNESS_ONLINE_API_H
#define FITNESS_ONLINE_API_H

#include <QString>

namespace mouseplan {
namespace fitness {

// 健身主题在线接口路径集中定义。
// 后续如果不同主题接不同后端，可在主题模块内分别维护。
class FitnessOnlineApi {
public:
    static QString authLoginPath();
    static QString syncUserPath();
    static QString updateLatestPath();
    static QString feedbackSubmitPath();
};

} // namespace fitness
} // namespace mouseplan

#endif // FITNESS_ONLINE_API_H
