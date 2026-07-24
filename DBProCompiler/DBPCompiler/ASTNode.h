#pragma once
#include <string>
#include <memory>
#include <vector>

struct SourceLocation {
    std::string filePath;
    size_t line = 0;
    size_t column = 0;
    size_t length = 0;
};

class ASTVisitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void Accept(ASTVisitor* visitor) = 0;

    [[nodiscard]] const SourceLocation& GetLocation() const noexcept { return m_location; }
    void SetLocation(const SourceLocation& loc) noexcept { m_location = loc; }

protected:
    SourceLocation m_location;
};
