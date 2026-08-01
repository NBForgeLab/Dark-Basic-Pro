#include "ExecutablePreparationPipeline.h"

namespace {

ExecutablePreparationResult Failure(
    const ExecutablePreparationStage stage,
    const char* const message) noexcept
{
    return {false, stage, message};
}

} // namespace

ExecutablePreparationResult ExecutablePreparationPipeline::Run(
    const ExecutablePreparationRequest& request,
    IExecutablePreparationServices& services) const noexcept
{
    if (request.outputFilename == nullptr || request.outputFilename[0] == '\0') {
        return Failure(
            ExecutablePreparationStage::requestValidation,
            "The executable output filename is empty.");
    }

    (void)services.ValidateTarget();
    (void)services.UpdateMachineCode();
    (void)services.UpdateReferences();
    (void)services.UpdateDllData();
    (void)services.UpdateCommandData();
    (void)services.UpdateStringData();
    (void)services.UpdateDataData();
    (void)services.UpdateDynamicData();
    (void)services.UpdateStructurePatterns();
    (void)services.ValidateRuntime();
    (void)services.FinalizeSpaceSizes();

    if (request.outputMode == ExecutableOutputMode::debug) {
        (void)services.RunDebug(request);
    } else {
        (void)services.PackageStandalone(request);
    }

    return {true, ExecutablePreparationStage::none, nullptr};
}
