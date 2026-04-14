#pragma once

#include <vector>
#include <string>

namespace ghost::platform
{
    
    inline const std::vector<std::string> kRegistryBranches = {
        "HKEY_CURRENT_USER\\Keyboard Layout\\Preload",
        "HKEY_CURRENT_USER\\Keyboard Layout\\Substitutes",
        "HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Preload",
        "HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Substitutes",
        "HKEY_CURRENT_USER\\Control Panel\\International\\User Profile",
        "HKEY_USERS\\.DEFAULT\\Control Panel\\International\\User Profile",
        "HKEY_USERS\\.DEFAULT\\Control Panel\\International\\User Profile System Backup"
    };

} // namespace ghost::platform
