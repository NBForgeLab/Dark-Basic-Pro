#pragma once

#include <memory>

class CASMWriter;

struct PreparedExecutableDebugRequest {
    bool parsingMainProgram;
    bool hasNewCode;
};

class IPreparedExecutableDebugRuntime {
public:
    virtual ~IPreparedExecutableDebugRuntime() = default;

    [[nodiscard]] virtual bool BeginSession() noexcept = 0;
    [[nodiscard]] virtual bool InitializeMain() noexcept = 0;
    [[nodiscard]] virtual bool RunMain() noexcept = 0;
    [[nodiscard]] virtual bool InitializeMini() noexcept = 0;
    [[nodiscard]] virtual bool RunNewCode() noexcept = 0;
    [[nodiscard]] virtual bool ResumeMain() noexcept = 0;
    [[nodiscard]] virtual bool EndSession() noexcept = 0;
};

class PreparedExecutableDebugger {
public:
    [[nodiscard]] bool Run(
        const PreparedExecutableDebugRequest& request,
        IPreparedExecutableDebugRuntime& runtime) const noexcept;
};

class ASMWriterDebugRuntime final : public IPreparedExecutableDebugRuntime {
public:
    explicit ASMWriterDebugRuntime(CASMWriter& writer) noexcept;
    ~ASMWriterDebugRuntime() override;

    [[nodiscard]] bool BeginSession() noexcept override;
    [[nodiscard]] bool InitializeMain() noexcept override;
    [[nodiscard]] bool RunMain() noexcept override;
    [[nodiscard]] bool InitializeMini() noexcept override;
    [[nodiscard]] bool RunNewCode() noexcept override;
    [[nodiscard]] bool ResumeMain() noexcept override;
    [[nodiscard]] bool EndSession() noexcept override;

private:
    CASMWriter& writer_;
    bool executionResult_{true};
    void* statementHook_{nullptr};
    void* jumpHook_{nullptr};
    void* returnHook_{nullptr};
    char* returnError_{nullptr};
    std::unique_ptr<char[]> returnErrorOwner_;
};
