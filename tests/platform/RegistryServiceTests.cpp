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

    const std::vector<std::string> layouts = registryQueryFail.listLayoutCodesFromRegistry();

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
    ok = expect(layouts.empty(), "registry list is empty when reg query fails") && ok;
    ok = expect(!errors.empty(), "registry delete returns error details on failure") && ok;
    return ok;
}
