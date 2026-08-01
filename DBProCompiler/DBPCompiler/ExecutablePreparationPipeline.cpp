#include "ExecutablePreparationPipeline.h"

ExecutablePreparationResult ExecutablePreparationPipeline::Run(
    const ExecutablePreparationRequest& request,
    IExecutablePreparationServices&) const noexcept
{
    if (request.outputFilename == nullptr || request.outputFilename[0] == '\0') {
        return {
            false,
            ExecutablePreparationStage::requestValidation,
            "The executable output filename is empty.",
        };
    }

    return {true, ExecutablePreparationStage::none, nullptr};
}
