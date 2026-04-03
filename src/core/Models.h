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
    bool success{false};
};

/// @brief Отчёт о создании backup.
struct BackupReport
{
    bool success{false};
    std::string backupPath;
    std::vector<std::string> executedCommands;
    std::vector<std::string> errors;
};

/// @brief Отчёт о восстановлении backup.
struct RestoreReport
{
    bool success{false};
    std::string sourcePath;
    std::vector<std::string> executedCommands;
    std::vector<std::string> errors;
};

} // namespace ghost::core
