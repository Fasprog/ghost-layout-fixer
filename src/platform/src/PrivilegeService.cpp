#include <src/platform/PrivilegeService.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ghost::platform
{

bool PrivilegeService::isRunningAsAdmin() const
{
#if defined(_WIN32)
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;
    if (!AllocateAndInitializeSid(
            &ntAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0,
            0,
            0,
            0,
            0,
            0,
            &adminGroup))
    {
        return false;
    }

    const BOOL checked = CheckTokenMembership(nullptr, adminGroup, &isMember);
    FreeSid(adminGroup);
    if (!checked)
    {
        return false;
    }

    return isMember == TRUE;
#else
    return geteuid() == 0;
#endif
}

} // namespace ghost::platform
