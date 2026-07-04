# Phase 8: Structural Cleanup

## 🎯 Goal
Eliminate fragile data representation patterns, hardcoded magic numbers, and highly-coupled structures. This refactoring standardizes C++ code interfaces, prevents silent data corruption, and makes adding or modifying engine modules simple and isolated.

---

## 🏗️ Refactoring Areas

### 1. Scoped Enums (`enum class`) instead of Magic Numbers
* **Problem**: The codebase uses raw integer values to denote data types and state flags. For example, in `CEXEBlock::FreeUptoDisplay`, `3` is checked to identify string variables, and `1` is used for array types. This impairs code readability and causes errors when values collide.
* **Solution**:
  * Replace magic constants with strongly-typed scoped enums (`enum class`):
    ```cpp
    enum class DataType : uint8_t {
        Unknown = 0,
        Array   = 1,
        Integer = 2,
        String  = 3,
        Float   = 4
    };
    
    // Usage
    if (m_pDataSpace[dv] == static_cast<DWORD>(DataType::String)) { ... }
    ```

### 2. Service Locator (`PluginRegistry`) instead of Monolithic `GlobStruct`
* **Problem**: The shared struct [GlobStruct](file:///d:/GitHub-repo/Dark-Basic-Pro/Dark%20Basic%20Public%20Shared/Dark%20Basic%20Pro%20SDK/Shared/Core/globstruct.h#L83) is a monolith containing hardcoded `HINSTANCE` handles for every single supported DLL in the engine (`g_GFX`, `g_Text`, `g_Sound`...). Adding a new DLL plugin requires editing this shared core struct and rebuilding all dependent projects.
* **Solution**:
  * Implement a dynamic `PluginRegistry` mapping plugin names to handles:
    ```cpp
    #include <string>
    #include <unordered_map>
    #include <windows.h>
    
    class PluginRegistry {
    public:
        void RegisterPlugin(const std::string& name, HINSTANCE handle) {
            m_plugins[name] = handle;
        }
        
        HINSTANCE GetPlugin(const std::string& name) const {
            auto it = m_plugins.find(name);
            return (it != m_plugins.end()) ? it->second : nullptr;
        }
        
    private:
        std::unordered_map<std::string, HINSTANCE> m_plugins;
    };
    ```
  * This decouples the engine core from specific plugins, allowing developers to load custom DLLs dynamically.

### 3. Transition to standard `std::filesystem`
* **Problem**: Path operations and directory listing use outdated C/Win32 functions (like `_chdir`, `FindFirstFile`, `FindNextFile`). These functions are error-prone under non-ASCII character paths and hard to maintain.
* **Solution**:
  * Use C++17 `<filesystem>` for path validations and directory lookups:
    ```cpp
    #include <filesystem>
    namespace fs = std::filesystem;

    if (fs::exists(pTryDLLName)) {
        // Load library safely
    }
    ```

---

## 🚀 Benefits
* **Type Safety**: The compiler automatically prevents type-mixing errors using scoped enums.
* **Loose Coupling**: Introduce new DLL modules to the engine without modifying the core `GlobStruct`.
* **Path Safety**: Standardizes filesystem operations, avoiding path-resolution bugs and directory leaks.
