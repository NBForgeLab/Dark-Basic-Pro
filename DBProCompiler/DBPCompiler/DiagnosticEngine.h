#pragma once
#include <string>
#include "ASTNode.h" // For SourceLocation

class DiagnosticEngine {
public:
    // Formats the diagnostic message with ANSI escape sequences (optional color)
    static std::string Format(const SourceLocation& loc, 
                              const std::string& message, 
                              const std::string& hint = "", 
                              bool useColor = true);

    // Logs to the system, console, and standard streams
    static void Report(const SourceLocation& loc, 
                       const std::string& message, 
                       const std::string& hint = "");

    // Helper to strip ANSI escape codes for file logging
    static std::string StripAnsi(const std::string& input);

    // Extract exact source line context and column details
    static void GetLineContext(const std::string& fileContent, 
                               size_t charPos, 
                               std::string& lineContent, 
                               size_t& column);
};
