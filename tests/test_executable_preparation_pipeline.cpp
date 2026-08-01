#include <gtest/gtest.h>

#include "ExecutablePreparationPipeline.h"

#include <optional>
#include <vector>

namespace {

class RecordingPreparationServices final
    : public IExecutablePreparationServices {
public:
    bool ValidateTarget() noexcept override {
        return Record(ExecutablePreparationStage::targetValidation);
    }
    bool UpdateMachineCode() noexcept override {
        return Record(ExecutablePreparationStage::machineCode);
    }
    bool UpdateReferences() noexcept override {
        return Record(ExecutablePreparationStage::references);
    }
    bool UpdateDllData() noexcept override {
        return Record(ExecutablePreparationStage::dllData);
    }
    bool UpdateCommandData() noexcept override {
        return Record(ExecutablePreparationStage::commandData);
    }
    bool UpdateStringData() noexcept override {
        return Record(ExecutablePreparationStage::stringData);
    }
    bool UpdateDataData() noexcept override {
        return Record(ExecutablePreparationStage::dataData);
    }
    bool UpdateDynamicData() noexcept override {
        return Record(ExecutablePreparationStage::dynamicData);
    }
    bool UpdateStructurePatterns() noexcept override {
        return Record(ExecutablePreparationStage::structurePatterns);
    }
    bool ValidateRuntime() noexcept override {
        return Record(ExecutablePreparationStage::runtimeValidation);
    }
    bool FinalizeSpaceSizes() noexcept override {
        return Record(ExecutablePreparationStage::spaceSizes);
    }
    bool RunDebug(const ExecutablePreparationRequest&) noexcept override {
        return Record(ExecutablePreparationStage::debugExecution);
    }
    bool PackageStandalone(
        const ExecutablePreparationRequest&) noexcept override {
        return Record(ExecutablePreparationStage::standalonePackaging);
    }

    std::vector<ExecutablePreparationStage> calls;
    std::optional<ExecutablePreparationStage> failedStage;

private:
    bool Record(const ExecutablePreparationStage stage) {
        calls.push_back(stage);
        return failedStage != stage;
    }
};

TEST(ExecutablePreparationPipelineTest,
     RejectsNullOutputNameBeforeCallingServices) {
    RecordingPreparationServices services;
    const ExecutablePreparationRequest request{
        nullptr, true, true, ExecutableOutputMode::standalone};

    const auto result =
        ExecutablePreparationPipeline{}.Run(request, services);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.failedStage,
              ExecutablePreparationStage::requestValidation);
    EXPECT_TRUE(services.calls.empty());
}

TEST(ExecutablePreparationPipelineTest,
     RejectsEmptyOutputNameBeforeCallingServices) {
    RecordingPreparationServices services;
    const ExecutablePreparationRequest request{
        "", true, true, ExecutableOutputMode::standalone};

    const auto result =
        ExecutablePreparationPipeline{}.Run(request, services);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.failedStage,
              ExecutablePreparationStage::requestValidation);
    EXPECT_TRUE(services.calls.empty());
}

TEST(ExecutablePreparationPipelineTest,
     NewCodeRunsEveryMaterializationStageInOrder) {
    using Stage = ExecutablePreparationStage;
    RecordingPreparationServices services;
    const ExecutablePreparationRequest request{
        "game.exe", true, true, ExecutableOutputMode::standalone};

    const auto result =
        ExecutablePreparationPipeline{}.Run(request, services);

    ASSERT_TRUE(result);
    EXPECT_EQ(services.calls, (std::vector<Stage>{
        Stage::targetValidation,
        Stage::machineCode,
        Stage::references,
        Stage::dllData,
        Stage::commandData,
        Stage::stringData,
        Stage::dataData,
        Stage::dynamicData,
        Stage::structurePatterns,
        Stage::runtimeValidation,
        Stage::spaceSizes,
        Stage::standalonePackaging,
    }));
}

TEST(ExecutablePreparationPipelineTest,
     ExistingCodeSkipsMaterializationStages) {
    using Stage = ExecutablePreparationStage;
    RecordingPreparationServices services;
    const ExecutablePreparationRequest request{
        "game.exe", false, false, ExecutableOutputMode::debug};

    const auto result =
        ExecutablePreparationPipeline{}.Run(request, services);

    ASSERT_TRUE(result);
    EXPECT_EQ(services.calls, (std::vector<Stage>{
        Stage::targetValidation,
        Stage::runtimeValidation,
        Stage::spaceSizes,
        Stage::debugExecution,
    }));
}

class ExecutablePreparationFailureTest
    : public ::testing::TestWithParam<ExecutablePreparationStage> {};

TEST_P(ExecutablePreparationFailureTest, StopsAtFirstFailedStage) {
    const auto failedStage = GetParam();
    RecordingPreparationServices services;
    services.failedStage = failedStage;
    const auto outputMode =
        failedStage == ExecutablePreparationStage::debugExecution
            ? ExecutableOutputMode::debug
            : ExecutableOutputMode::standalone;

    const auto result = ExecutablePreparationPipeline{}.Run(
        {"game.exe", true, true, outputMode}, services);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.failedStage, failedStage);
    ASSERT_FALSE(services.calls.empty());
    EXPECT_EQ(services.calls.back(), failedStage);
}

INSTANTIATE_TEST_SUITE_P(
    EveryServiceStage,
    ExecutablePreparationFailureTest,
    ::testing::Values(
        ExecutablePreparationStage::targetValidation,
        ExecutablePreparationStage::machineCode,
        ExecutablePreparationStage::references,
        ExecutablePreparationStage::dllData,
        ExecutablePreparationStage::commandData,
        ExecutablePreparationStage::stringData,
        ExecutablePreparationStage::dataData,
        ExecutablePreparationStage::dynamicData,
        ExecutablePreparationStage::structurePatterns,
        ExecutablePreparationStage::runtimeValidation,
        ExecutablePreparationStage::spaceSizes,
        ExecutablePreparationStage::debugExecution,
        ExecutablePreparationStage::standalonePackaging));

} // namespace
