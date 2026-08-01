#include <gtest/gtest.h>

#include "PreparedExecutableDebugger.h"

#include <optional>
#include <vector>

namespace {

enum class DebugCall {
    beginSession,
    initializeMain,
    runMain,
    initializeMini,
    runNewCode,
    resumeMain,
    endSession,
};

class RecordingDebugRuntime final : public IPreparedExecutableDebugRuntime {
public:
    bool BeginSession() noexcept override {
        return Record(DebugCall::beginSession);
    }
    bool InitializeMain() noexcept override {
        return Record(DebugCall::initializeMain);
    }
    bool RunMain() noexcept override {
        return Record(DebugCall::runMain);
    }
    bool InitializeMini() noexcept override {
        return Record(DebugCall::initializeMini);
    }
    bool RunNewCode() noexcept override {
        return Record(DebugCall::runNewCode);
    }
    bool ResumeMain() noexcept override {
        return Record(DebugCall::resumeMain);
    }
    bool EndSession() noexcept override {
        return Record(DebugCall::endSession);
    }

    std::vector<DebugCall> calls;
    std::optional<DebugCall> failedCall;

private:
    bool Record(const DebugCall call) {
        calls.push_back(call);
        return failedCall != call;
    }
};

TEST(PreparedExecutableDebuggerTest,
     MainProgramInitializesAndRunsExactlyOnce) {
    RecordingDebugRuntime runtime;

    const bool result = PreparedExecutableDebugger{}.Run(
        {true, true}, runtime);

    EXPECT_TRUE(result);
    EXPECT_EQ(runtime.calls, (std::vector<DebugCall>{
        DebugCall::beginSession,
        DebugCall::initializeMain,
        DebugCall::runMain,
        DebugCall::endSession,
    }));
}

TEST(PreparedExecutableDebuggerTest,
     MiniProgramRunsNewCodeThenResumesMainProgram) {
    RecordingDebugRuntime runtime;

    const bool result = PreparedExecutableDebugger{}.Run(
        {false, true}, runtime);

    EXPECT_TRUE(result);
    EXPECT_EQ(runtime.calls, (std::vector<DebugCall>{
        DebugCall::beginSession,
        DebugCall::initializeMini,
        DebugCall::runNewCode,
        DebugCall::resumeMain,
        DebugCall::endSession,
    }));
}

TEST(PreparedExecutableDebuggerTest,
     MiniProgramWithoutNewCodeOnlyResumesMainProgram) {
    RecordingDebugRuntime runtime;

    const bool result = PreparedExecutableDebugger{}.Run(
        {false, false}, runtime);

    EXPECT_TRUE(result);
    EXPECT_EQ(runtime.calls, (std::vector<DebugCall>{
        DebugCall::beginSession,
        DebugCall::initializeMini,
        DebugCall::resumeMain,
        DebugCall::endSession,
    }));
}

TEST(PreparedExecutableDebuggerTest,
     FailedExecutionStillClosesAnOpenedSession) {
    RecordingDebugRuntime runtime;
    runtime.failedCall = DebugCall::runMain;

    const bool result = PreparedExecutableDebugger{}.Run(
        {true, true}, runtime);

    EXPECT_FALSE(result);
    ASSERT_FALSE(runtime.calls.empty());
    EXPECT_EQ(runtime.calls.back(), DebugCall::endSession);
}

TEST(PreparedExecutableDebuggerTest,
     FailedSessionOpenDoesNotAttemptSessionCleanup) {
    RecordingDebugRuntime runtime;
    runtime.failedCall = DebugCall::beginSession;

    const bool result = PreparedExecutableDebugger{}.Run(
        {true, true}, runtime);

    EXPECT_FALSE(result);
    EXPECT_EQ(runtime.calls,
              (std::vector<DebugCall>{DebugCall::beginSession}));
}

} // namespace
