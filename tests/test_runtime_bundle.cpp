#include <gtest/gtest.h>

#include "RuntimeContract.h"

TEST(RuntimeContractTest, ReportsOnlyMissingRequiredCapabilities) {
    const RuntimeCapabilities available{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreDataStatementsV1};
    const ProgramRuntimeRequirements required{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreStructurePatternsV1};

    EXPECT_EQ(
        MissingCapabilities(available, required),
        RuntimeCapabilities{RuntimeCapability::CoreStructurePatternsV1});
}

TEST(RuntimeContractTest, StructurePatternsAreRequiredOnlyForNonEmptyMetadata) {
    EXPECT_EQ(
        DeriveProgramRuntimeRequirements(0).count(
            RuntimeCapability::CoreStructurePatternsV1),
        0u);
    EXPECT_EQ(
        DeriveProgramRuntimeRequirements(1).count(
            RuntimeCapability::CoreStructurePatternsV1),
        1u);
}

TEST(RuntimeContractTest, AlwaysRequiresTheBaselineCoreContract) {
    const auto requirements = DeriveProgramRuntimeRequirements(0);

    EXPECT_EQ(requirements.count(RuntimeCapability::CoreBootstrapV1), 1u);
    EXPECT_EQ(requirements.count(RuntimeCapability::CoreDataStatementsV1), 1u);
    EXPECT_EQ(requirements.count(RuntimeCapability::CoreRuntimeErrorsV1), 1u);
}
