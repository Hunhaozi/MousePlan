#include "modules/themes/fitness/network/fitness_online_api.h"

namespace mouseplan {
namespace fitness {

QString FitnessOnlineApi::authLoginPath()
{
    return QStringLiteral("/auth/login");
}

QString FitnessOnlineApi::syncUserPath()
{
    return QStringLiteral("/sync/user");
}

QString FitnessOnlineApi::updateLatestPath()
{
    return QStringLiteral("/app/update/latest");
}

QString FitnessOnlineApi::feedbackSubmitPath()
{
    return QStringLiteral("/app/feedback/submit");
}

} // namespace fitness
} // namespace mouseplan
