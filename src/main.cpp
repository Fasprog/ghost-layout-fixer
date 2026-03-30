#include <src/cli/CliParser.h>
#include <src/core/ExitCodes.h>
#include <src/core/LayoutFixService.h>
#include <src/platform/InstalledLanguageService.h>
#include <src/platform/RegistryService.h>
#include <src/report/ReportPrinter.h>

#include <cstddef>
#include <string>
#include <vector>

namespace
{

std::string joinLayouts(const std::vector<std::string>& layouts)
{
    if (layouts.empty())
    {
        return "<empty>";
    }

    std::string result;
    for (std::size_t index = 0; index < layouts.size(); ++index)
    {
        result += layouts[index];
        if (index + 1 < layouts.size())
        {
            result += ", ";
        }
    }

    return result;
}

} // namespace

int main(int argc, const char* const argv[])
{
    const ghost::cli::CliParser parser;
    const ghost::cli::CliOptions options = parser.parse(argc, argv);

    const ghost::report::ReportPrinter printer;
    if (options.command == ghost::cli::CommandType::Unknown)
    {
        printer.print("ghost-layout-fixer skeleton: command is not implemented yet");
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    if (options.command == ghost::cli::CommandType::Scan)
    {
        const ghost::platform::RegistryService registryService;
        const ghost::platform::InstalledLanguageService installedLanguageService;
        const ghost::core::LayoutFixService layoutFixService;

        const std::vector<std::string> registryLayouts = registryService.listLayoutCodesFromRegistry();
        const std::vector<std::string> installedLayouts = installedLanguageService.listInstalledLayoutCodes();
        const ghost::core::ScanResult scanResult = layoutFixService.scan(registryLayouts, installedLayouts);

        printer.print("[scan] registry layouts: " + joinLayouts(scanResult.registryLayouts));
        printer.print("[scan] installed layouts: " + joinLayouts(scanResult.installedLayouts));
        printer.print("[scan] ghost layouts: " + joinLayouts(scanResult.ghostLayouts));
        printer.print("[scan] status: " + scanResult.status);

        return static_cast<int>(ghost::core::ExitCode::Success);
    }

    printer.print("ghost-layout-fixer skeleton: command recognized");
    return static_cast<int>(ghost::core::ExitCode::Success);
}
