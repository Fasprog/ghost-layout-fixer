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

class ApplicationService
{
public:
    ApplicationService(
        const ghost::platform::IPrivilegeService& privilegeService,
        const ghost::core::BackupService& backupService,
        const ghost::platform::RegistryService& registryService,
        const ghost::platform::InstalledLanguageService& installedLanguageService,
        const ghost::core::LayoutFixService& layoutFixService,
        const ghost::report::ReportPrinter& printer);

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
