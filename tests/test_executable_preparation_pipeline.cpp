#include <gtest/gtest.h>

#include "ExecutablePreparationPipeline.h"

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

private:
    bool Record(const ExecutablePreparationStage stage) {
        calls.push_back(stage);
        return true;
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

} // namespace
