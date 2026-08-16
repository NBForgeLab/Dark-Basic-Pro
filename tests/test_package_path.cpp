#include <gtest/gtest.h>

#include "dbp/package/PackagePath.h"

#include <string>
#include <vector>

namespace {

using dbp::package::NormalizePackageInputPath;
using dbp::package::ValidateAndSortPackagePaths;
using dbp::package::ValidatePersistedPackagePath;

TEST(PackagePathTest, NormalizesInputSeparatorsAndUnicodeToCanonicalUtf8) {
    const auto result =
        NormalizePackageInputPath("media\\شخصيات\\e\u0301lite.png");

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result.value(), "media/شخصيات/élite.png");
}

TEST(PackagePathTest, AcceptsCanonicalRelativeUtf8Path) {
    const auto result =
        ValidatePersistedPackagePath("media/أصوات/تنبيه.wav");

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result.value(), "media/أصوات/تنبيه.wav");
}

class UnsafePackagePathTest
    : public testing::TestWithParam<std::string> {};

TEST_P(UnsafePackagePathTest, RejectsUnsafeOrAmbiguousPath) {
    const auto result = ValidatePersistedPackagePath(GetParam());
    EXPECT_FALSE(result) << GetParam();
}

INSTANTIATE_TEST_SUITE_P(
    PackagePathTest,
    UnsafePackagePathTest,
    testing::Values(
        "",
        "/media/file.dat",
        "C:/media/file.dat",
        "//server/share/file.dat",
        R"(\\?\C:\media\file.dat)",
        "media\\file.dat",
        "media//file.dat",
        "media/./file.dat",
        "media/../file.dat",
        "media/file.dat/",
        "media/file. ",
        "media/trailing.",
        "media/CON",
        "media/con.txt",
        "media/PRN.bin",
        "media/AUX",
        "media/NUL.dat",
        "media/COM1.log",
        "media/LPT9",
        "media/file:stream.dat",
        "media/file?.dat",
        std::string("media/control") + static_cast<char>(0x1F) + ".dat",
        std::string("media/nul") + std::string(1, '\0') + "byte.dat",
        std::string("media/") + static_cast<char>(0xC3) + ".dat",
        std::string(1025, 'a')));

TEST(PackagePathTest, SortsPathsByCanonicalUtf8Bytes) {
    const auto result = ValidateAndSortPackagePaths({
        "z/file.dat",
        "a/file.dat",
        "media/صوت.wav",
    });

    ASSERT_TRUE(result) << result.error().message;
    EXPECT_EQ(result.value(), (std::vector<std::string>{
        "a/file.dat",
        "media/صوت.wav",
        "z/file.dat",
    }));
}

TEST(PackagePathTest, RejectsExactDuplicatePaths) {
    const auto result = ValidateAndSortPackagePaths({
        "media/file.dat",
        "media/file.dat",
    });

    EXPECT_FALSE(result);
}

TEST(PackagePathTest, RejectsWindowsCaseInsensitiveCollision) {
    const auto result = ValidateAndSortPackagePaths({
        "Media/Hero.PNG",
        "media/hero.png",
    });

    EXPECT_FALSE(result);
}

TEST(PackagePathTest, RejectsUnicodeNormalizationCollision) {
    const auto result = ValidateAndSortPackagePaths({
        "media/élite.png",
        "media/e\u0301lite.png",
    });

    EXPECT_FALSE(result);
}

} // namespace
