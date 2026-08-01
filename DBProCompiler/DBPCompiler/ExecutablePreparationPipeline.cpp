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

    if (!services.ValidateTarget()) {
        return Failure(
            ExecutablePreparationStage::targetValidation,
            "The executable target configuration is invalid.");
    }

    if (request.hasNewCode) {
        if (!services.UpdateMachineCode()) {
            return Failure(
                ExecutablePreparationStage::machineCode,
                "Updating the executable machine code failed.");
        }
        if (!services.UpdateReferences()) {
            return Failure(
                ExecutablePreparationStage::references,
                "Updating machine-code references failed.");
        }
        if (!services.UpdateDllData()) {
            return Failure(
                ExecutablePreparationStage::dllData,
                "Updating DLL metadata failed.");
        }
        if (!services.UpdateCommandData()) {
            return Failure(
                ExecutablePreparationStage::commandData,
                "Updating command metadata failed.");
        }
        if (!services.UpdateStringData()) {
            return Failure(
                ExecutablePreparationStage::stringData,
                "Updating string metadata failed.");
        }
        if (!services.UpdateDataData()) {
            return Failure(
                ExecutablePreparationStage::dataData,
                "Updating DATA metadata failed.");
        }
        if (!services.UpdateDynamicData()) {
            return Failure(
                ExecutablePreparationStage::dynamicData,
                "Updating dynamic-variable metadata failed.");
        }
        if (!services.UpdateStructurePatterns()) {
            return Failure(
                ExecutablePreparationStage::structurePatterns,
                "Updating structure-pattern metadata failed.");
        }
    }

    if (!services.ValidateRuntime()) {
        return Failure(
            ExecutablePreparationStage::runtimeValidation,
            "Validating the runtime bundle failed.");
    }
    if (!services.FinalizeSpaceSizes()) {
        return Failure(
            ExecutablePreparationStage::spaceSizes,
            "Finalizing runtime space sizes failed.");
    }

    if (request.outputMode == ExecutableOutputMode::debug) {
        if (!services.RunDebug(request)) {
            return Failure(
                ExecutablePreparationStage::debugExecution,
                "Running the prepared executable in the debugger failed.");
        }
    } else {
        if (!services.PackageStandalone(request)) {
            return Failure(
                ExecutablePreparationStage::standalonePackaging,
                "Packaging the prepared executable failed.");
        }
    }

    return {true, ExecutablePreparationStage::none, nullptr};
}
