#include <src/app/ApplicationService.h>
#include <src/cli/CliParser.h>
#include <src/core/BackupService.h>
#include <src/core/LayoutFixService.h>
#include <src/platform/InstalledLanguageService.h>
#include <src/platform/PrivilegeService.h>
#include <src/platform/RegistryService.h>
#include <src/report/ReportPrinter.h>

int main(int argc, const char* const argv[])
{
    const ghost::cli::CliParser parser;
    const ghost::cli::CliOptions options = parser.parse(argc, argv);

    const ghost::report::ReportPrinter printer;
    const ghost::platform::PrivilegeService privilegeService;
    const ghost::core::BackupService backupService;
    const ghost::platform::RegistryService registryService;
    const ghost::platform::InstalledLanguageService installedLanguageService;
    const ghost::core::LayoutFixService layoutFixService;

    const ghost::app::ApplicationService application(
        privilegeService,
        backupService,
        registryService,
        installedLanguageService,
        layoutFixService,
        printer);

    return application.run(options);
}
