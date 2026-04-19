#include <src/core/LayoutFixService.h>

#include <tests/test_support/TestFakes.h>
#include <tests/test_support/TestHelpers.h>

using ghost::tests::FakeCommandRunner;
using ghost::tests::expect;

bool testLayoutFixFailurePathsAndScanCases()
{
    FakeCommandRunner runner;
    runner.rules.push_back({"powershell", 1, "powershell failed"});
    runner.rules.push_back({"reg delete", 1, "delete failed"});

    const ghost::core::LayoutFixService layoutFixService(&runner);
    const ghost::core::FixReport fixReport =
        layoutFixService.executeFix("en-US", "backup.reg");

    const ghost::core::ScanResult emptyGhostScan =
        layoutFixService.scan({"EN_us", "ru"}, {"en-US", "ru-RU"});
    const ghost::core::ScanResult ghostScan =
        layoutFixService.scan({"en-US", "de-DE"}, {"en-US"});
    const ghost::core::ScanResult enGbGhostScan =
        layoutFixService.scan({"en-GB"}, {"en-US"});
    const ghost::core::ScanResult ruRuGhostScan =
        layoutFixService.scan({"ru-RU"}, {"ru"});
    const bool enUsGhostWithOnlyEnGbInRegistry =
        layoutFixService.isGhostLayout("en-US", {"en-GB"}, {"ru"});
    const bool ruRuGhostWithInstalledRu = layoutFixService.isGhostLayout("ru-RU", {"ru-RU"}, {"ru"});
    const bool deDeGhost = layoutFixService.isGhostLayout("de-DE", {"de-DE"}, {"en-US"});

    bool ok = true;
    ok = expect(!fixReport.success, "fix fails when powershell/delete fail") && ok;
    ok = expect(!fixReport.errors.empty(), "fix returns failure details") && ok;
    ok = expect(emptyGhostScan.ghostLayouts.empty(), "scan handles mixed case and underscore normalization") && ok;
    ok = expect(ghostScan.ghostLayouts.size() == 1 && ghostScan.ghostLayouts.front() == "de-DE", "scan detects non-installed layout") && ok;
    ok = expect(enGbGhostScan.ghostLayouts.size() == 1 && enGbGhostScan.ghostLayouts.front() == "en-GB", "scan treats en-GB as ghost when only en-US is installed") && ok;
    ok = expect(ruRuGhostScan.ghostLayouts.size() == 1 && ruRuGhostScan.ghostLayouts.front() == "ru-RU", "scan treats ru-RU as ghost when only neutral ru is installed") && ok;
    ok = expect(!enUsGhostWithOnlyEnGbInRegistry, "ghost check requires exact normalized layout match between request and registry") && ok;
    ok = expect(ruRuGhostWithInstalledRu, "ghost check treats ru and ru-RU as different layout codes") && ok;
    ok = expect(deDeGhost, "ghost check returns true when layout exists in registry and is not installed") && ok;
    return ok;
}
