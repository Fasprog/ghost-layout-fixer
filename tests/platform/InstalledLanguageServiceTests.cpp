#include <src/platform/InstalledLanguageService.h>

#include <tests/test_support/TestFakes.h>
#include <tests/test_support/TestHelpers.h>

#include <vector>

using ghost::tests::FakeCommandRunner;
using ghost::tests::expect;

bool testInstalledLanguageParsing()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 0, "en-US\n ru-RU\n\ten-US\r\n"});

    const ghost::platform::InstalledLanguageService service(&runner);
    const std::vector<std::string> layouts = service.listInstalledLayoutCodes();

    bool ok = true;
    ok = expect(layouts.size() == 2, "installed language parser keeps unique tags") && ok;
    ok = expect(layouts[0] == "en-US", "installed language parser keeps first tag") && ok;
    ok = expect(layouts[1] == "ru-RU", "installed language parser trims spaces and tabs") && ok;
    return ok;
}

bool testInstalledLanguageEmptyOnFailure()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 1, "failed"});

    const ghost::platform::InstalledLanguageService service(&runner);
    const std::vector<std::string> layouts = service.listInstalledLayoutCodes();

    return expect(layouts.empty(), "installed language parser returns empty list on command failure output");
}
