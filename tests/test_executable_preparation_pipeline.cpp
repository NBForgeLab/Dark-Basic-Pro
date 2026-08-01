#include <gtest/gtest.h>

#include "ExecutablePreparationPipeline.h"

#include <vector>

namespace {

class RecordingPreparationServices final
    : public IExecutablePreparationServices {
public:
    bool ValidateTarget() noexcept override { return Record(); }
    bool UpdateMachineCode() noexcept override { return Record(); }
    bool UpdateReferences() noexcept override { return Record(); }
    bool UpdateDllData() noexcept override { return Record(); }
    bool UpdateCommandData() noexcept override { return Record(); }
    bool UpdateStringData() noexcept override { return Record(); }
    bool UpdateDataData() noexcept override { return Record(); }
    bool UpdateDynamicData() noexcept override { return Record(); }
    bool UpdateStructurePatterns() noexcept override { return Record(); }
    bool ValidateRuntime() noexcept override { return Record(); }
    bool FinalizeSpaceSizes() noexcept override { return Record(); }
    bool RunDebug(const ExecutablePreparationRequest&) noexcept override {
        return Record();
    }
    bool PackageStandalone(
        const ExecutablePreparationRequest&) noexcept override {
        return Record();
    }

    std::vector<int> calls;

private:
    bool Record() {
        calls.push_back(1);
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

} // namespace
