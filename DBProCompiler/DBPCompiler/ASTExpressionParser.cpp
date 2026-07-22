#include "ASTExpressionParser.h"
#include "ASTNodes.h"
#include <algorithm>
#include <cctype>

static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool IsSimpleIdentifier(const std::string& name) {
    if (name.empty()) return false;
    if (!isalpha(name[0]) && name[0] != '_') return false;
    for (size_t i = 1; i < name.size(); ++i) {
        char c = name[i];
        if (i == name.size() - 1 && (c == '$' || c == '#' || c == '%')) {
            continue;
        }
        if (!isalnum(c) && c != '_') return false;
    }
    return true;
}

static bool IsSimpleNumeric(const std::string& val) {
    if (val.empty()) return false;
    size_t start = 0;
    if (val[0] == '-' || val[0] == '+') {
        start = 1;
    }
    if (start >= val.size()) return false;
    for (size_t i = start; i < val.size(); ++i) {
        if (!isdigit(val[i])) return false;
    }
    return true;
}

static bool IsStringLiteral(const std::string& val) {
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        return true;
    }
    return false;
}

static std::unique_ptr<ASTNode> ParseSubExpr(const std::string& str);

static std::unique_ptr<ASTNode> ParsePrimary(const std::string& str) {
    std::string trimmed = Trim(str);
    if (trimmed.empty()) return nullptr;

    // Parentheses: ( sub_expr )
    if (trimmed.size() >= 2 && trimmed.front() == '(' && trimmed.back() == ')') {
        int depth = 0;
        bool validParens = true;
        for (size_t i = 0; i < trimmed.size() - 1; ++i) {
            if (trimmed[i] == '(') depth++;
            else if (trimmed[i] == ')') depth--;
            if (depth == 0) {
                validParens = false;
                break;
            }
        }
        if (validParens && depth == 1) {
            return ParseSubExpr(trimmed.substr(1, trimmed.size() - 2));
        }
    }

    if (IsSimpleNumeric(trimmed)) {
        return std::make_unique<ASTLiteralNode>(trimmed, 1); // 1 = integer
    }
    if (IsStringLiteral(trimmed)) {
        std::string unquoted = trimmed.substr(1, trimmed.size() - 2);
        return std::make_unique<ASTLiteralNode>(unquoted, 3); // 3 = string
    }
    if (IsSimpleIdentifier(trimmed)) {
        return std::make_unique<ASTVariableNode>(trimmed);
    }

    return nullptr;
}

// Level 2: Multiplicative (*, /)
static std::unique_ptr<ASTNode> ParseMultiplicative(const std::string& str) {
    std::string trimmed = Trim(str);
    int depth = 0;
    for (int i = (int)trimmed.size() - 1; i > 0; --i) {
        char c = trimmed[i];
        if (c == ')') depth++;
        else if (c == '(') depth--;
        else if (depth == 0) {
            if (c == '*' || c == '/') {
                std::string leftStr = Trim(trimmed.substr(0, i));
                std::string rightStr = Trim(trimmed.substr(i + 1));
                auto left = ParseMultiplicative(leftStr);
                auto right = ParsePrimary(rightStr);
                if (left && right) {
                    BinaryOpType op = (c == '*') ? BinaryOpType::Multiply : BinaryOpType::Divide;
                    return std::make_unique<ASTBinaryOpNode>(op, std::move(left), std::move(right));
                }
            }
        }
    }
    return ParsePrimary(trimmed);
}

// Level 1: Additive (+, -)
static std::unique_ptr<ASTNode> ParseAdditive(const std::string& str) {
    std::string trimmed = Trim(str);
    int depth = 0;
    for (int i = (int)trimmed.size() - 1; i > 0; --i) {
        char c = trimmed[i];
        if (c == ')') depth++;
        else if (c == '(') depth--;
        else if (depth == 0) {
            if (c == '+' || c == '-') {
                std::string leftStr = Trim(trimmed.substr(0, i));
                std::string rightStr = Trim(trimmed.substr(i + 1));
                auto left = ParseAdditive(leftStr);
                auto right = ParseMultiplicative(rightStr);
                if (left && right) {
                    BinaryOpType op = (c == '+') ? BinaryOpType::Add : BinaryOpType::Subtract;
                    return std::make_unique<ASTBinaryOpNode>(op, std::move(left), std::move(right));
                }
            }
        }
    }
    return ParseMultiplicative(trimmed);
}

// Level 0: Relational / Comparison (<, >, =)
static std::unique_ptr<ASTNode> ParseSubExpr(const std::string& str) {
    std::string trimmed = Trim(str);
    int depth = 0;
    for (int i = (int)trimmed.size() - 1; i > 0; --i) {
        char c = trimmed[i];
        if (c == ')') depth++;
        else if (c == '(') depth--;
        else if (depth == 0) {
            if (c == '<' || c == '>' || c == '=') {
                BinaryOpType op = BinaryOpType::Equal;
                int opLen = 1;
                if (c == '<') op = BinaryOpType::LessThan;
                else if (c == '>') op = BinaryOpType::GreaterThan;
                else if (c == '=') op = BinaryOpType::Equal;

                std::string leftStr = Trim(trimmed.substr(0, i));
                std::string rightStr = Trim(trimmed.substr(i + opLen));
                auto left = ParseSubExpr(leftStr);
                auto right = ParseAdditive(rightStr);
                if (left && right) {
                    return std::make_unique<ASTBinaryOpNode>(op, std::move(left), std::move(right));
                }
            }
        }
    }
    return ParseAdditive(trimmed);
}

std::unique_ptr<ASTNode> ASTExpressionParser::Parse(const std::string& exprStr) {
    return ParseSubExpr(exprStr);
}

