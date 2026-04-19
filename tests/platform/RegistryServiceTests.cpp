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
