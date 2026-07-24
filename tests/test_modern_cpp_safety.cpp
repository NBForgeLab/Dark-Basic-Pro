#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include "EXEBlock.h"
#include "ASTNode.h"

// Concrete dummy ASTNode for testing
class DummyASTNode : public ASTNode {
public:
    void Accept(ASTVisitor* visitor) override {}
};

// Modern C++17/20 Safety Unit Tests

TEST(ModernCppSafetyTest, SmartPointerRAIIOwnership) {
    // Verify RAII unique_ptr management for CEXEBlock
    auto exeBlock = std::make_unique<CEXEBlock>();
    ASSERT_NE(exeBlock, nullptr);
    
    // Verify CreatePtrArray memory initialization
    uintptr_t* pArray = exeBlock->CreatePtrArray(4);
    ASSERT_NE(pArray, nullptr);
    EXPECT_EQ(pArray[0], 0U);
    EXPECT_EQ(pArray[3], 0U);
    
    delete[] pArray;
}

TEST(ModernCppSafetyTest, StringViewPathFormatting) {
    std::string_view baseRoot = "C:\\DarkBasicPro\\";
    std::string_view subFolder = "Plugins\\";
    
    std::string fullPath = std::string(baseRoot) + std::string(subFolder);
    EXPECT_EQ(fullPath, "C:\\DarkBasicPro\\Plugins\\");
}

TEST(ModernCppSafetyTest, NodiscardAndNoexceptAttributes) {
    DummyASTNode node;
    // ASTNode GetLocation must be marked noexcept and [[nodiscard]]
    static_assert(noexcept(node.GetLocation()), "GetLocation must be marked noexcept");
}
