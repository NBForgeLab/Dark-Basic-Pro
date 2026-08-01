#include <gtest/gtest.h>

#include "CompilationInput.h"
#include "Str.h"
#include "DBPCompiler.h"

#include <array>
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

TEST(CompilationInputTest, BreakpointScanDoesNotReadBeforeSourceBuffer) {
    CompilationInputFixture fixture;
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);
    compiler.m_FileDataSize = 3;
    compiler.m_pFileData = static_cast<LPSTR>(
        GlobalAlloc(GMEM_FIXED, compiler.m_FileDataSize));
    ASSERT_NE(compiler.m_pFileData, nullptr);
    memcpy(compiler.m_pFileData, "abc", compiler.m_FileDataSize);

    EXPECT_TRUE(compiler.RemoveAndRecordBreakpoints());
    EXPECT_EQ(compiler.GetBreakPointMax(), 0u);
}

TEST(CompilationInputTest, EmptyMediaRootResolvesToProjectDirectory) {
    CompilationInputFixture fixture;
    const auto project = fixture.Write(
        "Project.dbpro", "main=Main.dba\r\nmedia root path=\r\n");
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);
    std::string projectPath = project.string();

    ASSERT_TRUE(compiler.LoadProjectFile(&projectPath[0]));
    LPSTR mediaRoot = compiler.GetProjectMediaRoot();
    ASSERT_NE(mediaRoot, nullptr);
    EXPECT_TRUE(std::filesystem::equivalent(mediaRoot, fixture.directory()));
    delete[] mediaRoot;
}

TEST(CompilationInputTest, ProjectFieldsAcceptModernAndLegacyLineEndings) {
    CompilationInputFixture fixture;
    const std::array<std::pair<const char*, const char*>, 3> variants{{
        {"ProjectLF.dbpro", "\n"},
        {"ProjectCRLF.dbpro", "\r\n"},
        {"ProjectCR.dbpro", "\r"},
    }};

    for (const auto& [name, newline] : variants) {
        const auto project = fixture.Write(
            name,
            std::string{"main=Main.dba"} + newline +
                "executable=Game.exe" + newline +
                "final source=_Temp.dbsource" + newline);
        std::string compilerPath =
            (fixture.directory() / "DBPCompiler.exe").string();
        CDBPCompiler compiler(&compilerPath[0]);
        std::string mutableProject = project.string();

        ASSERT_TRUE(compiler.LoadProjectFile(&mutableProject[0])) << name;
        std::unique_ptr<char[]> executable(
            compiler.GetProjectField("executable"));
        ASSERT_NE(executable, nullptr) << name;
        EXPECT_STREQ(executable.get(), "Game.exe") << name;
    }
}

TEST(CompilationInputTest, CompilerPreparesDirectDbaAsOwnedInput) {
    CompilationInputFixture fixture;
    const auto source = fixture.Write("Direct.dba", "print 42\n");
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);

    ASSERT_TRUE(compiler.PrepareCompilationInput(source.string().c_str()));
    ASSERT_TRUE(compiler.LoadPreparedSource());
    EXPECT_EQ(
        std::string(compiler.GetFilePtr(), compiler.GetFileData()),
        "print 42");
}

TEST(CompilationInputTest, PreservesManifestErrorCategoryAndDiagnosticCode) {
    CompilationInputFixture fixture;
    const auto project = fixture.Write("Project.dbpro", "include1=Part.dba\r\n");

    const auto result = CompilationInput::FromProjectFile(project, {});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SourceAssemblyErrorCode::ProjectMissingMain);
    EXPECT_NE(result.error().message.find("DBP1001"), std::string::npos);
    EXPECT_EQ(result.error().sourcePath, project);
}

TEST(CompilationInputTest, RelativeProjectKeepsExecutableInProjectDirectory) {
    CompilationInputFixture fixture;
    const auto project = fixture.Write(
        "Project.dbpro",
        "main=Main.dba\r\nfinal source=_Temp.dbsource\r\n"
        "executable=bin\\Game.exe\r\n");
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);
    const auto originalDirectory = std::filesystem::current_path();
    std::filesystem::current_path(fixture.directory());
    std::string mutableRelative = "Project.dbpro";
    const bool loaded = compiler.LoadProjectFile(&mutableRelative[0]);
    const bool projectExists = compiler.ProjectExists();
    std::filesystem::current_path(originalDirectory);

    ASSERT_TRUE(loaded);
    ASSERT_TRUE(projectExists);
    EXPECT_TRUE(std::filesystem::equivalent(
        compiler.m_pRelativePathToProjectFile->GetStr(), fixture.directory()));
    LPSTR executable = compiler.GetProjectFile("executable");
    ASSERT_NE(executable, nullptr);
    EXPECT_EQ(
        std::filesystem::path(executable).lexically_normal(),
        (fixture.directory() / "bin" / "Game.exe").lexically_normal());
    delete[] executable;
}

TEST(CompilationInputTest, ExecutableOutputOverrideDoesNotChangeProjectBaseDirectory) {
    CompilationInputFixture fixture;
    fixture.Write("Main.dba", "end\n");
    const auto project = fixture.Write(
        "Project.dbpro",
        "main=Main.dba\r\nfinal source=_Temp.dbsource\r\n"
        "executable=bin\\ManifestGame.exe\r\n");
    const auto output = fixture.directory() / "isolated out" / "Override.exe";
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);
    std::string mutableProject = project.string();

    ASSERT_TRUE(compiler.LoadProjectFile(&mutableProject[0]));
    compiler.SetExecutableOutputOverride(output);
    ASSERT_TRUE(compiler.GetAllProjectFields(&mutableProject[0]));

    EXPECT_EQ(
        std::filesystem::path(compiler.GetProgramName()).lexically_normal(),
        output.lexically_normal());
    EXPECT_TRUE(std::filesystem::equivalent(
        compiler.m_pRelativePathToProjectFile->GetStr(), fixture.directory()));
    EXPECT_FALSE(std::filesystem::exists(fixture.directory() / "bin"));
    EXPECT_FALSE(std::filesystem::exists(output.parent_path()));
}

TEST(CompilationInputTest, PreparesOnlyExecutableOutputParentDirectory) {
    CompilationInputFixture fixture;
    const auto output = fixture.directory() / "isolated out" / "nested" / "Game.exe";
    std::string compilerPath = (fixture.directory() / "DBPCompiler.exe").string();
    CDBPCompiler compiler(&compilerPath[0]);
    compiler.SetExecutableOutputOverride(output);

    ASSERT_TRUE(compiler.PrepareExecutableOutputDirectory());

    EXPECT_TRUE(std::filesystem::is_directory(output.parent_path()));
    EXPECT_FALSE(std::filesystem::exists(output));
}
