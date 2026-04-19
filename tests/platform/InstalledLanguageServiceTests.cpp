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
    const ghost::platform::InstalledLayoutCodesResult layoutsResult = service.listInstalledLayoutCodes();

    bool ok = true;
    ok = expect(layoutsResult.success, "installed language query succeeds for valid runner response") && ok;
    ok = expect(layoutsResult.values.size() == 2, "installed language parser keeps unique tags") && ok;
    ok = expect(layoutsResult.values[0] == "en-US", "installed language parser keeps first tag") && ok;
    ok = expect(layoutsResult.values[1] == "ru-RU", "installed language parser trims spaces and tabs") && ok;
    return ok;
}

bool testInstalledLanguageEmptyOnFailure()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"(Get-WinUserLanguageList).LanguageTag", 1, "failed to query languages"});

    const ghost::platform::CommandResult runResult = runner.run(
        "powershell -NoProfile -Command \"(Get-WinUserLanguageList).LanguageTag\"");

    const ghost::platform::InstalledLanguageService service(&runner);
    const ghost::platform::InstalledLayoutCodesResult layoutsResult = service.listInstalledLayoutCodes();

    bool ok = true;
    ok = expect(runResult.exitCode != 0, "installed language command fails with non-zero exit code in fake runner") && ok;
    ok = expect(!runResult.outputText.empty(), "installed language command failure keeps error text in output") && ok;
    ok = expect(!layoutsResult.success, "installed language result marks command failure") && ok;
    ok = expect(layoutsResult.values.empty(), "installed language result keeps values empty on failure") && ok;
    ok = expect(!layoutsResult.error.empty(), "installed language result includes error details") && ok;
    return ok;
}
