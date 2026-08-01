#pragma once

#include "ExecutablePreparationPipeline.h"

class CASMWriter;

class IPreparedExecutableOutputServices {
public:
    virtual ~IPreparedExecutableOutputServices() = default;

    [[nodiscard]] virtual bool RunDebug(
        CASMWriter& writer,
        const ExecutablePreparationRequest& request) noexcept = 0;
    [[nodiscard]] virtual bool PackageStandalone(
        CASMWriter& writer,
        const ExecutablePreparationRequest& request) noexcept = 0;
};

class ASMWriterPreparationServices final
    : public IExecutablePreparationServices {
public:
    explicit ASMWriterPreparationServices(
        CASMWriter& writer,
        IPreparedExecutableOutputServices* outputServices = nullptr) noexcept;

    [[nodiscard]] bool ValidateTarget() noexcept override;
    [[nodiscard]] bool UpdateMachineCode() noexcept override;
    [[nodiscard]] bool UpdateReferences() noexcept override;
    [[nodiscard]] bool UpdateDllData() noexcept override;
    [[nodiscard]] bool UpdateCommandData() noexcept override;
    [[nodiscard]] bool UpdateStringData() noexcept override;
    [[nodiscard]] bool UpdateDataData() noexcept override;
    [[nodiscard]] bool UpdateDynamicData() noexcept override;
    [[nodiscard]] bool UpdateStructurePatterns() noexcept override;
    [[nodiscard]] bool ValidateRuntime() noexcept override;
    [[nodiscard]] bool FinalizeSpaceSizes() noexcept override;
    [[nodiscard]] bool RunDebug(
        const ExecutablePreparationRequest& request) noexcept override;
    [[nodiscard]] bool PackageStandalone(
        const ExecutablePreparationRequest& request) noexcept override;

    void ReportFailure(const ExecutablePreparationResult& result) const noexcept;

private:
    CASMWriter& writer_;
    IPreparedExecutableOutputServices* outputServices_;
};
