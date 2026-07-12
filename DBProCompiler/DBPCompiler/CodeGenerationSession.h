#pragma once

#include <optional>
#include <string>

class ICodeGenerator;

enum class CodeGenerationErrorCode {
    NotInitialized,
    InitializationFailed,
    InvalidTransition
};

struct CodeGenerationError {
    CodeGenerationErrorCode code;
    std::string message;
    std::string operation;
};

class CodeGenerationResult {
public:
    static CodeGenerationResult Success();
    static CodeGenerationResult Failure(CodeGenerationError error);

    bool has_value() const noexcept { return success_; }
    explicit operator bool() const noexcept { return has_value(); }
    const CodeGenerationError& error() const { return error_.value(); }

private:
    explicit CodeGenerationResult(bool success, std::optional<CodeGenerationError> error);

    bool success_;
    std::optional<CodeGenerationError> error_;
};

enum class CodeGenerationState {
    Created,
    Initialized,
    Finished,
    Failed
};

class CodeGenerationSession {
public:
    explicit CodeGenerationSession(ICodeGenerator& generator);

    CodeGenerationResult Begin();
    CodeGenerationResult RequireInitialized(const std::string& operation) const;
    CodeGenerationResult Finish();
    CodeGenerationState state() const noexcept { return state_; }

private:
    ICodeGenerator& generator_;
    CodeGenerationState state_ = CodeGenerationState::Created;
};
