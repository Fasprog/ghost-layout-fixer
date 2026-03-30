#pragma once

#include <string>
#include <vector>

namespace ghost::core
{

/// @brief Результат диагностического сканирования.
struct ScanResult
{
    std::vector<std::string> registryLayouts;
    std::vector<std::string> installedLayouts;
    std::vector<std::string> ghostLayouts;
    std::string status;
};

/// @brief План действий для dry-run и fix.
struct FixPlan
{
    std::vector<std::string> plannedSteps;
};

/// @brief Отчёт о выполнении операции fix.
struct FixReport
{
    std::vector<std::string> executedSteps;
    std::vector<std::string> errors;
    std::string backupPath;
};

} // namespace ghost::core
