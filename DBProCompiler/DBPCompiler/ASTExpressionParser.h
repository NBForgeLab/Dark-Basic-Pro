#pragma once
#include <memory>
#include <string>
#include "ASTNode.h"

class ASTExpressionParser {
public:
    static std::unique_ptr<ASTNode> Parse(const std::string& exprStr);
};
