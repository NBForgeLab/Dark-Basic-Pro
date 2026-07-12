#include "DiagnosticEngine.h"
#include "DBPLogger.h"
#include "Error.h"
#include "DBPCompiler.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>

extern CError* g_pErrorReport;
extern CDBPCompiler* g_pDBPCompiler;

std::string DiagnosticEngine::Format(const SourceLocation& loc, 
                                     const std::string& message, 
                                     const std::string& hint, 
                                     bool useColor) {
    std::string fileContent;
    std::ifstream f(loc.filePath, std::ios::in | std::ios::binary);
    if (f) {
        fileContent = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    } else {
        // Fallback to reading the compiler's currently loaded file data if available
        extern CDBPCompiler* g_pDBPCompiler;
        if (g_pDBPCompiler && g_pDBPCompiler->m_pFileData) {
            fileContent = std::string(g_pDBPCompiler->m_pFileData, g_pDBPCompiler->m_FileDataSize);
        }
    }

    // Resolve character index for loc.line and loc.column
    size_t currentLine = 1;
    size_t pos = 0;
    while (pos < fileContent.size() && currentLine < loc.line) {
        if (fileContent[pos] == '\n') {
            currentLine++;
        } else if (fileContent[pos] == '\r') {
            if (pos + 1 < fileContent.size() && fileContent[pos + 1] == '\n') {
                pos++;
            }
            currentLine++;
        }
        pos++;
    }

    size_t charPos = pos + (loc.column > 0 ? loc.column - 1 : 0);
    std::string lineContent;
    size_t resolvedCol = 1;
    GetLineContext(fileContent, charPos, lineContent, resolvedCol);

    // If source file is not found, keep column
    if (resolvedCol == 1 && loc.column > 1 && lineContent.empty()) {
        resolvedCol = loc.column;
    }

    std::string redBold = useColor ? "\033[1;31m" : "";
    std::string cyanBold = useColor ? "\033[1;36m" : "";
    std::string reset = useColor ? "\033[0m" : "";
    
    std::string result = "";
    result += redBold + "Error" + reset + ": " + message + "\n";
    result += "  --> " + loc.filePath + ":" + std::to_string(loc.line) + ":" + std::to_string(loc.column) + "\n";
    result += "   |\n";
    result += " " + std::to_string(loc.line) + " | " + lineContent + "\n";
    result += "   | ";
    
    // Build caret alignment, preserving tabs for terminal display alignment
    for (size_t i = 0; i < resolvedCol - 1; ++i) {
        if (i < lineContent.size() && lineContent[i] == '\t') {
            result += '\t';
        } else {
            result += ' ';
        }
    }
    
    result += redBold + "^";
    size_t underlineLen = loc.length > 0 ? loc.length : 1;
    for (size_t i = 1; i < underlineLen; ++i) {
        result += "~";
    }
    result += reset + "\n";
    
    if (!hint.empty()) {
        result += "   = " + cyanBold + "Help" + reset + ": " + hint + "\n";
    }
    return result;
}

void DiagnosticEngine::Report(const SourceLocation& loc, 
                             const std::string& message, 
                             const std::string& hint) {
    std::string colored = Format(loc, message, hint, true);
    std::string clean = StripAnsi(Format(loc, message, hint, false));

    // Output to stderr and logger
    std::cerr << colored;
    DBP_ERROR("\n{}", clean);
}

std::string DiagnosticEngine::StripAnsi(const std::string& input) {
    std::string result;
    bool inEscape = false;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\033') {
            inEscape = true;
        } else if (inEscape) {
            if (input[i] == 'm') {
                inEscape = false;
            }
        } else {
            result += input[i];
        }
    }
    return result;
}

void DiagnosticEngine::GetLineContext(const std::string& fileContent, 
                                     size_t charPos, 
                                     std::string& lineContent, 
                                     size_t& column) {
    if (fileContent.empty()) {
        lineContent = "";
        column = 1;
        return;
    }

    if (charPos >= fileContent.size()) {
        charPos = fileContent.size() - 1;
    }

    // Scan backward to find beginning of line
    size_t lineStartPos = charPos;
    while (lineStartPos > 0 && fileContent[lineStartPos - 1] != '\n' && fileContent[lineStartPos - 1] != '\r') {
        lineStartPos--;
    }

    // Scan forward to find end of line
    size_t lineEndPos = charPos;
    while (lineEndPos < fileContent.size() && fileContent[lineEndPos] != '\n' && fileContent[lineEndPos] != '\r') {
        lineEndPos++;
    }

    lineContent = fileContent.substr(lineStartPos, lineEndPos - lineStartPos);
    column = charPos - lineStartPos + 1;
}
