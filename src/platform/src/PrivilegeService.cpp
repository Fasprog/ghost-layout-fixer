#include <src/platform/PrivilegeService.h>

#include <windows.h>

namespace ghost::platform
{

bool PrivilegeService::isRunningAsAdmin() const
{
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
}

} // namespace ghost::platform
