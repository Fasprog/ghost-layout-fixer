#pragma once

#include <string>
#include <vector>

namespace ghost::platform {

struct RegistryMatch {
    std::string location;
    std::string valueName;
    std::string valueData;
};

class RegistryService {
public:
    std::vector<RegistryMatch> findLayoutMatches(const std::string& layoutCode) const;
};

} // namespace ghost::platform
