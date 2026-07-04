# Phase 3: Automated Testing Environment

## 🎯 Goal
Build a robust testing environment to automatically verify the correctness of the compiler and engine. Automated unit and integration tests create a "safety net" that prevents regressions when refactoring code or adding new compiler features.

---

## 🛠️ Framework Integrated: Google Test (GTest)
Google Test (v1.11.0) is integrated into the CMake build system:
* **Why GTest?**: It is the industry standard for C++, provides excellent IDE integration (like Visual Studio's Test Explorer), and separates test logic from the main application binary.

---

## 🏗️ CMake Configuration for Testing

Unit tests are enabled conditionally in the `CMakeLists.txt` configuration:

```cmake
# --- Automated Testing Configuration (GTest Integration) ---
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # Fetch Google Test framework automatically
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/release-1.11.0.zip
    )
    # Ensure GTest uses the same runtime configuration as the rest of the project
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    # Declare the test executable containing unit & integration tests
    add_executable(dbp_tests
        tests/test_main.cpp
        tests/test_logger.cpp
        tests/test_vartable.cpp
    )
    
    # Link with GTest and the compiler static library
    target_link_libraries(dbp_tests PRIVATE 
        gtest 
        dbp_compiler_lib
    )
    
    # Register the test suite in CMake's test runner
    add_test(NAME dbp_unit_tests COMMAND dbp_tests)
endif()
```

---

## 💻 Implemented Unit Tests

The test suite is organized inside the `tests` directory:

### 1. Global Entry Point (`tests/test_main.cpp`)
Initializes the Google Test runner.
```cpp
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

### 2. Logger Tests (`tests/test_logger.cpp`)
Verifies file logging, log levels (`trace`, `warning`, `error`), formatting, and compiler startup logging integration.

### 3. Variable Table Tests (`tests/test_vartable.cpp`)
Tests `CVarTable::AddVariable` and `CVarTable::FindVariable`. It allocates the global compiler environment objects safely without C++ layout-breaking macros, ensuring perfect ABI compatibility with MSVC.
```cpp
TEST_F(VarTableTest, AddAndFindVariable) {
    g_pStatementList->SetVariableAddParse(true);
    
    DWORD dwAction = 0;
    // Add variable using the global g_pVarTable
    bool result = g_pVarTable->AddVariable("myIntegerVar", "integer", 0, 10, true, &dwAction, false);
    
    ASSERT_TRUE(result);
    
    // Find variable
    CVarTable* pVar = g_pVarTable->FindVariable(nullptr, "myIntegerVar", 0);
    ASSERT_NE(pVar, nullptr);
}
```

---

## 🚀 Build and Run Instructions

To compile and run the tests, execute the following commands in the shell:

```powershell
# Create build directory and run CMake configuration
mkdir build
cd build
cmake ..

# Build in Release mode (or Debug)
cmake --build . --config Release

# Run the test runner executable
cd ../bin/Release
.\dbp_tests.exe
```

All tests pass successfully under both Debug and Release build configurations.
