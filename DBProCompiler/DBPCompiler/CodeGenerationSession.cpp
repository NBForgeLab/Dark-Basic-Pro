#include "CodeGenerationSession.h"

#include "ICodeGenerator.h"

#include <utility>

CodeGenerationResult::CodeGenerationResult(
    bool success,
    std::optional<CodeGenerationError> error)
    : success_(success), error_(std::move(error)) {}

CodeGenerationResult CodeGenerationResult::Success() {
    return CodeGenerationResult(true, std::nullopt);
}

CodeGenerationResult CodeGenerationResult::Failure(CodeGenerationError error) {
    return CodeGenerationResult(false, std::move(error));
}

CodeGenerationSession::CodeGenerationSession(ICodeGenerator& generator)
    : generator_(generator) {}

CodeGenerationResult CodeGenerationSession::Begin() {
    if (state_ != CodeGenerationState::Created) {
        return CodeGenerationResult::Failure({
            CodeGenerationErrorCode::InvalidTransition,
            "Code-generation session can only begin once.",
            "begin"});
    }
    if (!generator_.CreateASMHeader()) {
        state_ = CodeGenerationState::Failed;
        return CodeGenerationResult::Failure({
            CodeGenerationErrorCode::InitializationFailed,
            "Backend machine-code storage could not be initialized.",
            "begin"});
    }
    state_ = CodeGenerationState::Initialized;
    return CodeGenerationResult::Success();
}

CodeGenerationResult CodeGenerationSession::RequireInitialized(
    const std::string& operation) const {
    if (state_ != CodeGenerationState::Initialized) {
        return CodeGenerationResult::Failure({
            CodeGenerationErrorCode::NotInitialized,
            "DBP2001: code emission attempted before backend initialization.",
            operation});
    }
    return CodeGenerationResult::Success();
}

CodeGenerationResult CodeGenerationSession::Finish() {
    if (state_ != CodeGenerationState::Initialized) {
        return CodeGenerationResult::Failure({
            CodeGenerationErrorCode::InvalidTransition,
            "Only an initialized code-generation session can finish.",
            "finish"});
    }
    state_ = CodeGenerationState::Finished;
    return CodeGenerationResult::Success();
}
