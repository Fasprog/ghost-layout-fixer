#pragma once

#include <string>
#include <vector>

namespace ghost::core {

struct ScanResult {
    std::vector<std::string> checkedLocations;
    std::vector<std::string> matches;
    std::string status;
};

struct FixPlan {
    std::vector<std::string> plannedSteps;
};

struct FixReport {
    std::vector<std::string> executedSteps;
    std::vector<std::string> errors;
    std::string backupPath;
};

} // namespace ghost::core
