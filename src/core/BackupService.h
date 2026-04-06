#pragma once

#include <src/core/Models.h>
#include <src/platform/SystemCommandRunner.h>

#include <string>

namespace ghost::core
{

/// @brief Создаёт backup whitelist-веток реестра для безопасных операций.
class BackupService
{
public:
    explicit BackupService(const ghost::platform::ICommandRunner* runner = nullptr);

    /// @brief Генерирует имя backup-файла в рабочей директории.
    std::string makeBackupPath() const;

    /// @brief Выполняет экспорт реестра в backup-файл.
    /// @param[in] backupPath Путь до целевого .reg файла.
    /// @return Отчёт о выполнении backup-команд.
    BackupReport createBackup(const std::string& backupPath) const;

    /// @brief Восстанавливает реестр из backup-файла.
    /// @param[in] backupPath Путь до .reg файла.
    /// @return Отчёт о выполнении операции восстановления.
    RestoreReport restoreBackup(const std::string& backupPath) const;

private:
    const ghost::platform::ICommandRunner* runner_;
};

} // namespace ghost::core
