# x64 PEBuilder64 & Runner Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement PE32+ 64-bit executable header validation, relocation traits, and dual-target runner build rules.

**Architecture:** Extend `CPEBuilder` with `ValidatePE64HeaderRequirements` for 64-bit magic `0x020B`, high image bases, and `IMAGE_REL_BASED_DIR64` verification.

**Tech Stack:** C++17, CMake 4.2+, GoogleTest, MSVC v145.

---

### Task 1: Extend `CPEBuilder` with PE32+ 64-bit Header Validation and Relocation Trait Checks

**Files:**
- Modify: `DBProCompiler/DBPCompiler/PEBuilder.h`
- Modify: `DBProCompiler/DBPCompiler/PEBuilder.cpp`
- Modify: `DBProCompiler/DBPCompilerEXE/CMakeLists.txt`
- Modify: `tests/test_pe_builder_headers.cpp`

- [ ] **Step 1: Write failing unit tests for PE32+ 64-bit header requirement validation**

```cpp
TEST(PEBuilderHeadersTest, ValidatesPE64HeaderRequirements) {
    CPEBuilder builder;
    EXPECT_TRUE(builder.ValidatePE64HeaderRequirements(0x140000000ULL, 4096, 512));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0ULL, 4096, 512));
    EXPECT_FALSE(builder.ValidatePE64HeaderRequirements(0x140000000ULL, 0, 512));
}

TEST(PEBuilderHeadersTest, IdentifiesPeMagicBitness) {
    EXPECT_EQ(CPEBuilder::GetPeMagic(false), IMAGE_NT_OPTIONAL_HDR32_MAGIC);
    EXPECT_EQ(CPEBuilder::GetPeMagic(true), IMAGE_NT_OPTIONAL_HDR64_MAGIC);
}
```

- [ ] **Step 2: Run test to verify build/pass failure**

- [ ] **Step 3: Implement `ValidatePE64HeaderRequirements` and `GetPeMagic` in `PEBuilder.h` & `PEBuilder.cpp`**

- [ ] **Step 4: Build and run test suite to verify PASS**

Run: `cmake --build --preset windows-x86-debug --target dbp_tests`
Run: `.\out\build\windows-x86-debug\bin\Debug\dbp_tests.exe --gtest_filter=PEBuilderHeadersTest.*`

- [ ] **Step 5: Commit**

```bash
git add DBProCompiler/DBPCompiler/PEBuilder.h DBProCompiler/DBPCompiler/PEBuilder.cpp DBProCompiler/DBPCompilerEXE/CMakeLists.txt tests/test_pe_builder_headers.cpp
git commit -m "feat(x64): implement PEBuilder64 header validation and PE32+ relocation traits"
```
