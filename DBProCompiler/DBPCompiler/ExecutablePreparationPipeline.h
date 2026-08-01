#pragma once

#include <string>

enum class ExecutableOutputMode {
    debug,
    standalone,
};

enum class ExecutablePreparationStage {
    none,
    requestValidation,
    targetValidation,
    machineCode,
    references,
    dllData,
    commandData,
    stringData,
    dataData,
    dynamicData,
    structurePatterns,
    runtimeValidation,
    spaceSizes,
    debugExecution,
    standalonePackaging,
};

struct ExecutablePreparationRequest {
    ExecutablePreparationRequest(
        const char* filename,
        bool isParsingMainProgram,
        bool containsNewCode,
        ExecutableOutputMode mode)
        : outputFilename(filename != nullptr ? filename : ""),
          parsingMainProgram(isParsingMainProgram),
          hasNewCode(containsNewCode),
          outputMode(mode) {}

    std::string outputFilename;
    bool parsingMainProgram;
    bool hasNewCode;
    ExecutableOutputMode outputMode;
};

struct ExecutablePreparationResult {
    bool succeeded;
    ExecutablePreparationStage failedStage;
    const char* message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return succeeded;
    }
};

class IExecutablePreparationServices {
public:
    virtual ~IExecutablePreparationServices() = default;

    [[nodiscard]] virtual bool ValidateTarget() noexcept = 0;
    [[nodiscard]] virtual bool UpdateMachineCode() noexcept = 0;
    [[nodiscard]] virtual bool UpdateReferences() noexcept = 0;
    [[nodiscard]] virtual bool UpdateDllData() noexcept = 0;
    [[nodiscard]] virtual bool UpdateCommandData() noexcept = 0;
    [[nodiscard]] virtual bool UpdateStringData() noexcept = 0;
    [[nodiscard]] virtual bool UpdateDataData() noexcept = 0;
    [[nodiscard]] virtual bool UpdateDynamicData() noexcept = 0;
    [[nodiscard]] virtual bool UpdateStructurePatterns() noexcept = 0;
    [[nodiscard]] virtual bool ValidateRuntime() noexcept = 0;
    [[nodiscard]] virtual bool FinalizeSpaceSizes() noexcept = 0;
    [[nodiscard]] virtual bool RunDebug(
        const ExecutablePreparationRequest& request) noexcept = 0;
    [[nodiscard]] virtual bool PackageStandalone(
        const ExecutablePreparationRequest& request) noexcept = 0;
};

class ExecutablePreparationPipeline {
public:
    [[nodiscard]] ExecutablePreparationResult Run(
        const ExecutablePreparationRequest& request,
        IExecutablePreparationServices& services) const noexcept;
};
