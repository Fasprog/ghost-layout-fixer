#include "cli/CliParser.h"
#include "core/ExitCodes.h"
#include "report/ReportPrinter.h"

int main(int argc, const char* const argv[]) {
    const ghost::cli::CliParser parser;
    const ghost::cli::CliOptions options = parser.parse(argc, argv);

    const ghost::report::ReportPrinter printer;
    if (options.command == ghost::cli::CommandType::Unknown) {
        printer.print("ghost-layout-fixer skeleton: command is not implemented yet");
        return static_cast<int>(ghost::core::ExitCode::GeneralError);
    }

    printer.print("ghost-layout-fixer skeleton: command recognized");
    return static_cast<int>(ghost::core::ExitCode::Success);
}
