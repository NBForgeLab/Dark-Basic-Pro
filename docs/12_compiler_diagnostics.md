# Phase 12: Modern Compiler Diagnostics

## 🎯 Goal
Replace basic compile-time errors with a rich diagnostic engine similar to modern compiler interfaces (like Rust, Swift, and Clang). A robust diagnostic engine prints exact source file contexts, underlines the source of the error, and suggests corrections, reducing development time for game programmers.

---

## ⚠️ Current Issues
The legacy compiler prints generic errors:
`Error on line 45: Syntax Error` or displays simple popups.
* **Problem**:
  * Developers have to manually inspect line 45 to figure out what symbol, parenthese, or token is missing.
  * If a project uses multiple files via `#include`, the compiler doesn't indicate which file caused the crash.

---

## 🛠️ Design: Source Manager & Diagnostics

We introduce a `SourceManager` to track character offsets and file indexes, and a `DiagnosticEngine` to format output errors:

### 1. Source Location Tracking
Store detailed coordinates for every parsed token:
```cpp
struct SourceLocation {
    std::string filePath; // Absolute path to source file
    size_t line;          // 1-based line number
    size_t column;        // 1-based character column
    size_t length;        // Character length of the token
};
```

### 2. Highlighting Code Locations
The diagnostic engine extracts the source line and prints it with caret indicators:

```cpp
#include <iostream>
#include <string>

class DiagnosticEngine {
public:
    static void ReportError(const SourceLocation& loc, const std::string& message, const std::string& hint = "") {
        std::cerr << "\033[1;31mError\033[0m: " << message << "\n";
        std::cerr << "  --> " << loc.filePath << ":" << loc.line << ":" << loc.column << "\n";
        
        std::string sourceLine = GetSourceLine(loc.filePath, loc.line);
        std::cerr << "   |\n";
        std::cerr << " " << loc.line << " | " << sourceLine << "\n";
        
        // Print underline indicators
        std::cerr << "   | ";
        for (size_t i = 1; i < loc.column; ++i) std::cerr << " ";
        std::cerr << "\033[1;31m^";
        for (size_t i = 1; i < loc.length; ++i) std::cerr << "~";
        std::cerr << "\033[0m\n";
        
        // Print hint context
        if (!hint.empty()) {
            std::cerr << "   = \033[1;36mHelp\033[0m: " << hint << "\n";
        }
        std::cerr << "\n";
    }

private:
    static std::string GetSourceLine(const std::string& file, size_t lineNum) {
        return "myVariable = 5 + * 10"; // Dummy example line
    }
};
```

---

## 💻 Output Example
When a syntax error occurs, the compiler output displays in the command-line interface as follows:

```text
Error: Expected expression after operator
  --> src/main.dba:12:18
   |
12 | myVariable = 5 + * 10
   |                  ^
   = Help: Check for missing operands or remove the extra '*' operator.
```

---

## 🚀 Benefits
* **Professional Experience**: Provides clear, informative console reports.
* **Rapid Iteration**: Correct errors immediately based on suggested fixes (e.g., spelling suggestions like "did you mean 'myVariable'?").
