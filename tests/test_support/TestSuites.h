#pragma once

bool testBackupPathFormat();
bool testBackupAggregatesAllBranches();
bool testBackupPartialFailure();
bool testBackupCreatesDirectoryWhenMissing();
bool testRestoreBackupValidationAndFailure();

bool testLayoutFixFailurePathsAndScanCases();

bool testRegistryFailures();
bool testRegistryMissingBranchIsSkipped();
bool testRegistryLcidLookupIsCaseInsensitive();

bool testInstalledLanguageParsing();
bool testInstalledLanguageEmptyOnFailure();

bool testFixRejectsNonGhostLayoutWithNeutralInstalledTagAndAllowsGhostLayout();
bool testCliAndNoAdmin();
