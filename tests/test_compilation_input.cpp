#include <gtest/gtest.h>

#include "CompilationInput.h"
#include "Str.h"
#include "DBPCompiler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class CompilationInputFixture {
public:
    CompilationInputFixture() {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        directory_ = std::filesystem::temp_directory_path() /
            ("dbpro_compilation_input_test_" + suffix);
        std::filesystem::create_directories(directory_);
    }

    ~CompilationInputFixture() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    std::filesystem::path Write(const std::string& name, const std::string& text) {
        const auto path = directory_ / name;
        std::ofstream stream(path, std::ios::binary);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        return path;
    }

    const std::filesystem::path& directory() const noexcept { return directory_; }

private:
    std::filesystem::path directory_;
};

std::string InputText(const CompilationInput& input) {
    const auto& bytes = input.bytes();
    return std::string(
        reinterpret_cast<const char*>(bytes.data()),
        reinterpret_cast<const char*>(bytes.data()) + bytes.size());
}

} // namespace

TEST(CompilationInputTest, OwnsDirectSourceBytesAndBaseDirectory) {
    CompilationInputFixture fixture;
    const auto source = fixture.Write("Direct.dba", "print \"hello\"\r\n");

    const auto result = CompilationInput::FromSourceFile(source);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(InputText(result.value()), "print \"hello\"\r\n");
    EXPECT_EQ(result.value().baseDirectory(), fixture.directory());
    ASSERT_EQ(result.value().sourceMap().size(), 1u);
}

TEST(CompilationInputTest, OwnsAssembledProjectBytes) {
    CompilationInputFixture fixture;
    const auto main = fixture.Write("Main.dba", "alpha");
    const auto include = fixture.Write("Include.dba", "beta\r\n");
    ProjectManifest manifest;
    manifest.projectPath = fixture.directory() / "Project.dbpro";
    manifest.projectDirectory = fixture.directory();
    manifest.sources.push_back({"main", "Main.dba", main});
    manifest.sources.push_back({"include1", "Include.dba", include});

    const auto result = CompilationInput::FromProject(manifest, {});

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(InputText(result.value()), "alpha\r\nbeta\r\n");
    EXPECT_EQ(result.value().baseDirectory(), fixture.directory());
    ASSERT_EQ(result.value().sourceMap().size(), 2u);
}

TEST(CompilationInputTest, PreparesProjectFileWithoutExistingFinalSource) {
    CompilationInputFixture fixture;
    fixture.Write("Main.dba", "alpha");
    fixture.Write("Include.dba", "beta\r\n");
    const auto project = fixture.Write(
        "Project.dbpro",
        "main=Main.dba\r\n"
        "include1=Include.dba\r\n"
        "final source=_Temp.dbsource\r\n");

    const auto result = CompilationInput::FromProjectFile(project, {});

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_EQ(InputText(result.value()), "alpha\r\nbeta\r\n");
    EXPECT_FALSE(std::filesystem::exists(fixture.directory() / "_Temp.dbsource"));
}

TEST(CompilationInputTest, LoadsPreparedProjectIntoLegacyCompilerBuffer) {
    CompilationInputFixture fixture;
    fixture.Write("Main.dba", "alpha");
    fixture.Write("Include.dba", "beta\r\n");
    const auto project = fixture.Write(
        "Project.dbpro",
        "main=Main.dba\r\ninclude1=Include.dba\r\nfinal source=missing.dbsource\r\n");
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);

    ASSERT_TRUE(compiler.PrepareCompilationInput(project.string().c_str()));
    ASSERT_TRUE(compiler.LoadPreparedSource());
    ASSERT_NE(compiler.GetFilePtr(), nullptr);
    EXPECT_EQ(
        std::string(compiler.GetFilePtr(), compiler.GetFileData()),
        "alpha\r\nbeta");
}
