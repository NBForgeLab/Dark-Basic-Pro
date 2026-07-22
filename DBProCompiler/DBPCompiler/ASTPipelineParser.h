#pragma once
#include <string>
#include <memory>
#include "ASTNodes.h"

class ASTPipelineParser {
public:
    static std::unique_ptr<ASTProgramNode> ParseProgram(const std::string& source);
};
