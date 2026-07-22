#include "ASTPipelineParser.h"
#include "ASTExpressionParser.h"
#include "DBPLogger.h"
#include <sstream>
#include <vector>
#include <algorithm>

static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::vector<std::string> SplitLines(const std::string& source) {
    std::vector<std::string> lines;
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::unique_ptr<ASTProgramNode> ASTPipelineParser::ParseProgram(const std::string& source) {
    auto program = std::make_unique<ASTProgramNode>();
    auto lines = SplitLines(source);

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = Trim(lines[i]);
        if (line.empty() || line.rfind("rem", 0) == 0 || line.rfind("REM", 0) == 0 || line.rfind("//", 0) == 0) {
            continue;
        }

        SourceLocation loc;
        loc.line = i + 1;
        loc.column = 1;

        // Parse assignment: var = expr
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos && line.find("==") == std::string::npos) {
            std::string varName = Trim(line.substr(0, eqPos));
            std::string exprStr = Trim(line.substr(eqPos + 1));
            if (!varName.empty() && !exprStr.empty()) {
                auto exprNode = ASTExpressionParser::Parse(exprStr);
                if (exprNode) {
                    exprNode->SetLocation(loc);
                }
                auto assignNode = std::make_unique<ASTAssignmentNode>(varName, std::move(exprNode));
                assignNode->SetLocation(loc);
                program->m_statements.push_back(std::move(assignNode));
            }
        }
    }

    return program;
}
