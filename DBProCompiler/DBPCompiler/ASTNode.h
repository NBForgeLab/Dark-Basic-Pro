#pragma once
#include <string>
#include <memory>
#include <vector>

class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void Accept(ASTVisitor* visitor) = 0;
};
