#include <gtest/gtest.h>

#include "PreparedExecutablePackager.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace {

enum class PackagingCall {
    resolveRuntime,
    stageExecutable,
    customizeResources,
    publish,
};

class RecordingPackagingServices final
    : public IStandalonePackagingServices {
public:
    bool ResolveRuntimeBundle() noexcept override {
        return Record(PackagingCall::resolveRuntime);
    }
    bool StageExecutable(
        const std::filesystem::path&) noexcept override {
        return Record(PackagingCall::stageExecutable);
    }
    bool CustomizeResources() noexcept override {
        return Record(PackagingCall::customizeResources);
    }
    bool Publish() noexcept override {
        return Record(PackagingCall::publish);
    }

    std::vector<PackagingCall> calls;
    std::optional<PackagingCall> failedCall;

private:
    bool Record(const PackagingCall call) {
        calls.push_back(call);
        return failedCall != call;
    }
};

TEST(PreparedExecutablePackagerTest,
     EmptyOutputPathFailsBeforeCallingServices) {
    RecordingPackagingServices services;

    const bool result = PreparedExecutablePackager{}.Package(
        {}, services);

    EXPECT_FALSE(result);
    EXPECT_TRUE(services.calls.empty());
}

class PreparedExecutablePackagingFailureTest
    : public ::testing::TestWithParam<PackagingCall> {};

TEST_P(PreparedExecutablePackagingFailureTest,
       StopsAtTheFirstFailedPackagingBoundary) {
    const auto failedCall = GetParam();
    RecordingPackagingServices services;
    services.failedCall = failedCall;

    const bool result = PreparedExecutablePackager{}.Package(
        {std::filesystem::path{"game.exe"}}, services);

    EXPECT_FALSE(result);
    ASSERT_FALSE(services.calls.empty());
    EXPECT_EQ(services.calls.back(), failedCall);
}

INSTANTIATE_TEST_SUITE_P(
    EveryPackagingBoundary,
    PreparedExecutablePackagingFailureTest,
    ::testing::Values(
        PackagingCall::resolveRuntime,
        PackagingCall::stageExecutable,
        PackagingCall::customizeResources,
        PackagingCall::publish));

TEST(PreparedExecutablePackagerTest,
     SuccessfulPackagingRunsEveryBoundaryInOrder) {
    RecordingPackagingServices services;

    const bool result = PreparedExecutablePackager{}.Package(
        {std::filesystem::path{"game.exe"}}, services);

    EXPECT_TRUE(result);
    EXPECT_EQ(services.calls, (std::vector<PackagingCall>{
        PackagingCall::resolveRuntime,
        PackagingCall::stageExecutable,
        PackagingCall::customizeResources,
        PackagingCall::publish,
    }));
}

} // namespace
