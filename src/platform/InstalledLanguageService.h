#pragma once

#include <src/platform/SystemCommandRunner.h>

#include <string>
#include <vector>

namespace ghost::platform
{

struct InstalledLayoutCodesResult
{
    bool success{false};
    std::vector<std::string> values;
    std::string error;
};

/// @brief Возвращает список установленных языков/раскладок Windows.
class InstalledLanguageService
{
public:
    explicit InstalledLanguageService(const ICommandRunner* runner = nullptr);

    /// @brief Получает коды установленных раскладок из системы.
    /// @return Результат чтения с признаком успеха, данными и ошибкой.
    InstalledLayoutCodesResult listInstalledLayoutCodes() const;

private:
    const ICommandRunner* runner_;
};

} // namespace ghost::platform
