#include <gtest/gtest.h>

#include "dbp/package/ApplicationPublisher.h"
#include "dbp/package/CryptoProvider.h"
#include "dbp/package/KeyProvider.h"
#include "dbp/package/PackageWriter.h"
#include "dbp/package/PublicationCheckpoint.h"
#include "dbp/package/CompressionCodec.h"

#include <cstdint>
#include <filesystem>
#include <type_traits>
#include <vector>

namespace {

using namespace dbp::package;

TEST(ApplicationPublisherTest, RequestAndResultHaveValueSemantics) {
    static_assert(
        std::is_move_constructible_v<ApplicationPublishRequest>);
    static_assert(
        std::is_move_constructible_v<ApplicationPublishResult>);
    static_assert(!std::is_copy_constructible_v<SecureBuffer>);
}

TEST(ApplicationPublisherTest, RejectsHostAndOutputPathAlias) {
    CngCryptoProvider crypto;
    ZstdCompressionCodec compression;
    Win32AtomicFilePublisher filePublisher;
    NoopPublicationCheckpoint checkpoint;
    ApplicationPublisher publisher(
        crypto,
        compression,
        filePublisher,
        checkpoint);

    KeyId keyId{};
    keyId.front() = 0x42;
    std::vector<std::uint8_t> keyBytes(
        kPackageMasterKeySize,
        0x5A);
    MemoryKeyProvider keys(
        keyId,
        SecureBuffer::FromBytes(keyBytes));

    ApplicationPublishRequest request;
    request.hostExecutable =
        std::filesystem::path(L"C:\\build\\same.exe");
    request.outputExecutable = request.hostExecutable;
    request.keyId = keyId;

    const auto result = publisher.Publish(request, keys);

    ASSERT_FALSE(result);
    EXPECT_EQ(
        PackageErrorCode::PublicationFailed,
        result.error().code);
}

} // namespace
