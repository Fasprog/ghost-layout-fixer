#include <src/platform/PrivilegeService.h>

namespace ghost::platform
{

bool PrivilegeService::isRunningAsAdmin() const
{
    return true;
}

} // namespace ghost::platform
