#pragma once

namespace ghost::platform {

class PrivilegeService {
public:
    bool isRunningAsAdmin() const;
};

} // namespace ghost::platform
