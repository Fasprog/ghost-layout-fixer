#pragma once

#include <src/cli/CliTypes.h>
#include <src/core/BackupService.h>
#include <src/core/LayoutFixService.h>
#include <src/platform/InstalledLanguageService.h>
#include <src/platform/PrivilegeService.h>
#include <src/platform/RegistryService.h>
#include <src/report/ReportPrinter.h>

namespace ghost::app
{

/// @brief Оркестрирует выполнение CLI-команд приложения.
class ApplicationService
{
public:
    /// @brief Создаёт сервис приложения с необходимыми зависимостями.
    /// @param[in] privilegeService Сервис проверки административных прав.
    /// @param[in] backupService Сервис создания и восстановления backup.
    /// @param[in] registryService Сервис чтения и изменения реестра.
    /// @param[in] installedLanguageService Сервис получения установленных раскладок.
    /// @param[in] layoutFixService Сервис устранения ghost-layout проблем.
    /// @param[in] printer Сервис печати отчётов в консоль.
    ApplicationService(
        const ghost::platform::IPrivilegeService& privilegeService,
        const ghost::core::BackupService& backupService,
        const ghost::platform::RegistryService& registryService,
        const ghost::platform::InstalledLanguageService& installedLanguageService,
        const ghost::core::LayoutFixService& layoutFixService,
        const ghost::report::ReportPrinter& printer);

    /// @brief Выполняет команду согласно разобранным CLI-опциям.
    /// @param[in] options Набор опций командной строки.
    /// @return Код завершения приложения.
    int run(const ghost::cli::CliOptions& options) const;

private:
    const ghost::platform::IPrivilegeService& privilegeService_;
    const ghost::core::BackupService& backupService_;
    const ghost::platform::RegistryService& registryService_;
    const ghost::platform::InstalledLanguageService& installedLanguageService_;
    const ghost::core::LayoutFixService& layoutFixService_;
    const ghost::report::ReportPrinter& printer_;
};

} // namespace ghost::app
