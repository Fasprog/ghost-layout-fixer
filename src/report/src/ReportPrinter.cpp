#include <src/report/ReportPrinter.h>

#include <iostream>

namespace ghost::report
{

void ReportPrinter::print(const std::string& message) const
{
    std::cout << message << '\n';
}

} // namespace ghost::report
