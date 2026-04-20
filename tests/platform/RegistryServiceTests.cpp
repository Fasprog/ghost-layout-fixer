#include <src/platform/RegistryService.h>

#include <tests/test_support/TestFakes.h>
#include <tests/test_support/TestHelpers.h>

#include <vector>

using ghost::tests::FakeCommandRunner;
using ghost::tests::expect;

bool testRegistryFailures()
{
    FakeCommandRunner queryFailRunner;
    queryFailRunner.rules.push_back({"GetCultures", 0, "0409=en-US\n"});
    queryFailRunner.rules.push_back({"reg query", 1, "query failed"});
    const ghost::platform::RegistryService registryQueryFail(&queryFailRunner);

    const ghost::platform::RegistryLayoutsResult layoutsResult = registryQueryFail.listLayoutCodesFromRegistry();

    FakeCommandRunner deleteFailRunner;
    deleteFailRunner.rules.push_back({"reg delete", 1, "delete failed"});
    const ghost::platform::RegistryService registryDeleteFail(&deleteFailRunner);
    ghost::platform::RegistryMatch match;
    match.branchPath = "HKEY_CURRENT_USER\\Keyboard Layout\\Preload";
    match.valueName = "1";
    match.valueData = "00000409";
    const std::vector<ghost::platform::RegistryMatch> matches = {match};
    const std::vector<std::string> errors = registryDeleteFail.deleteMatches(matches);

    bool ok = true;
    ok = expect(!layoutsResult.success, "registry list reports failure when reg query fails") && ok;
    ok = expect(layoutsResult.values.empty(), "registry list contains no values when query fails") && ok;
    ok = expect(!layoutsResult.error.empty(), "registry list includes error details on query failure") && ok;
    ok = expect(!errors.empty(), "registry delete returns error details on failure") && ok;
    return ok;
}

bool testRegistryMissingBranchIsSkipped()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"GetCultures", 0, "0409=en-US\n"});
    runner.rules.push_back({"reg query", 1, "ERROR: The system was unable to find the specified registry key or value."});
    const ghost::platform::RegistryService registryService(&runner);

    const ghost::platform::RegistryLayoutsResult layoutsResult = registryService.listLayoutCodesFromRegistry();

    bool ok = true;
    ok = expect(layoutsResult.success, "missing whitelist branch is treated as empty state, not fatal error") && ok;
    ok = expect(layoutsResult.values.empty(), "missing whitelist branches produce empty registry layout list") && ok;
    return ok;
}

bool testRegistryLcidLookupIsCaseInsensitive()
{
    FakeCommandRunner upperCaseRunner;
    upperCaseRunner.rules.push_back({"GetCultures", 0, "040C=fr-FR\n"});
    upperCaseRunner.rules.push_back({"reg query", 0, "    1    REG_SZ    0000040C\n"});
    const ghost::platform::RegistryService upperCaseService(&upperCaseRunner);

    const ghost::platform::RegistryLayoutsResult upperCaseResult = upperCaseService.listLayoutCodesFromRegistry();

    FakeCommandRunner lowerCaseRunner;
    lowerCaseRunner.rules.push_back({"GetCultures", 0, "040C=fr-FR\n"});
    lowerCaseRunner.rules.push_back({"reg query", 0, "    1    REG_SZ    0000040c\n"});
    const ghost::platform::RegistryService lowerCaseService(&lowerCaseRunner);

    const ghost::platform::RegistryLayoutsResult lowerCaseResult = lowerCaseService.listLayoutCodesFromRegistry();

    bool ok = true;
    ok = expect(upperCaseResult.success, "uppercase HKL LCID is parsed successfully") && ok;
    ok = expect(lowerCaseResult.success, "lowercase HKL LCID is parsed successfully") && ok;
    ok = expect(upperCaseResult.values.size() == 1, "uppercase HKL yields one registry entry") && ok;
    ok = expect(lowerCaseResult.values.size() == 1, "lowercase HKL yields one registry entry") && ok;
    ok = expect(upperCaseResult.values[0] == "fr-FR", "0000040C resolves to fr-FR") && ok;
    ok = expect(lowerCaseResult.values[0] == "fr-FR", "0000040c resolves to fr-FR") && ok;
    ok = expect(
             upperCaseResult.values[0] == lowerCaseResult.values[0],
             "uppercase and lowercase HKL LCID variants resolve identically") &&
        ok;
    return ok;
}


bool testRegistryRecursiveUserProfileSubkeys()
{
    FakeCommandRunner listRunner;
    listRunner.rules.push_back({"GetCultures", 0, "0809=en-GB\n0409=en-US\n"});
    listRunner.rules.push_back(
        {"reg query \"HKEY_CURRENT_USER\\Keyboard Layout\\Preload\"", 0, "    1    REG_SZ    00000409\n"});
    listRunner.rules.push_back(
        {"reg query \"HKEY_CURRENT_USER\\Keyboard Layout\\Substitutes\"", 0, ""});
    listRunner.rules.push_back(
        {"reg query \"HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Preload\"", 0, ""});
    listRunner.rules.push_back(
        {"reg query \"HKEY_USERS\\.DEFAULT\\Keyboard Layout\\Substitutes\"", 0, ""});
    listRunner.rules.push_back(
        {"reg query \"HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\" /s", 0,
         "HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\n"
         "    Languages    REG_SZ    00000409\n"
         "\n"
         "HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\\en-GB\n"
         "    1    REG_SZ    00000809\n"});
    listRunner.rules.push_back(
        {"reg query \"HKEY_USERS\\.DEFAULT\\Control Panel\\International\\User Profile\" /s", 1,
         "ERROR: The system was unable to find the specified registry key or value."});
    listRunner.rules.push_back(
        {"reg query \"HKEY_USERS\\.DEFAULT\\Control Panel\\International\\User Profile System Backup\" /s", 1,
         "ERROR: The system was unable to find the specified registry key or value."});

    const ghost::platform::RegistryService listService(&listRunner);
    const ghost::platform::RegistryLayoutsResult layoutsResult = listService.listLayoutCodesFromRegistry();

    FakeCommandRunner matchRunner = listRunner;
    matchRunner.commands.clear();
    const ghost::platform::RegistryService matchService(&matchRunner);
    const ghost::platform::RegistryMatchesResult matchesResult = matchService.findLayoutMatches("en-GB");

    FakeCommandRunner deleteRunner;
    deleteRunner.rules.push_back(
        {"reg delete \"HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\\en-GB\" /v \"1\" /f", 0, ""});
    const ghost::platform::RegistryService deleteService(&deleteRunner);

    std::vector<ghost::platform::RegistryMatch> matchesToDelete;
    if (!matchesResult.values.empty())
    {
        matchesToDelete.push_back(matchesResult.values.front());
    }
    const std::vector<std::string> deleteErrors = deleteService.deleteMatches(matchesToDelete);

    bool usedRecursiveUserProfileQuery = false;
    bool usedNonRecursivePreloadQuery = false;
    for (const std::string& command : listRunner.commands)
    {
        if (command == "reg query \"HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\" /s")
        {
            usedRecursiveUserProfileQuery = true;
        }

        if (command == "reg query \"HKEY_CURRENT_USER\\Keyboard Layout\\Preload\"")
        {
            usedNonRecursivePreloadQuery = true;
        }
    }

    bool ok = true;
    ok = expect(layoutsResult.success, "registry list succeeds when recursive user profile query returns child key values") && ok;
    ok = expect(layoutsResult.values.size() == 2, "registry list keeps values from recursive and non-recursive branches") && ok;
    ok = expect(layoutsResult.values[0] == "en-US", "non-recursive preload branch still works in original mode") && ok;
    ok = expect(layoutsResult.values[1] == "en-GB", "recursive user profile child key contributes layout code") && ok;
    ok = expect(usedRecursiveUserProfileQuery, "user profile branch uses recursive reg query /s") && ok;
    ok = expect(usedNonRecursivePreloadQuery, "preload branch remains non-recursive") && ok;

    ok = expect(matchesResult.success, "find layout matches succeeds for recursive branch output") && ok;
    ok = expect(matchesResult.values.size() == 1, "find layout matches returns child key match") && ok;
    if (!matchesResult.values.empty())
    {
        ok = expect(
                 matchesResult.values[0].branchPath ==
                     "HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\\en-GB",
                 "match branchPath points to actual child key path") &&
             ok;
        ok = expect(matchesResult.values[0].valueName == "1", "match keeps child key value name") && ok;
    }

    ok = expect(deleteErrors.empty(), "delete matches succeeds for child key path") && ok;
    ok = expect(
             !deleteRunner.commands.empty() &&
                 deleteRunner.commands.front() ==
                     "reg delete \"HKEY_CURRENT_USER\\Control Panel\\International\\User Profile\\en-GB\" /v \"1\" /f",
             "delete matches uses actual child key path") &&
        ok;
    return ok;
}
