#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "EXEBlock.h"
#include "ASTNode.h"
#include "Declaration.h"
#include "StatementList.h"
#include "StructTable.h"
#include "VarTable.h"

// Concrete dummy ASTNode for testing
class DummyASTNode : public ASTNode {
public:
    void Accept(ASTVisitor*) override {}
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

TEST(ModernCppSafetyTest, CoreAccessorsPreserveConstness) {
    static_assert(std::is_same_v<
        decltype(std::declval<CDeclaration&>().GetName()), CStr*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CDeclaration&>().GetName()),
        const CStr*>);

    static_assert(std::is_same_v<
        decltype(std::declval<CVarTable&>().GetVarName()), CStr*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CVarTable&>().GetVarName()),
        const CStr*>);

    static_assert(std::is_same_v<
        decltype(std::declval<CStructTable&>().GetTypeName()), CStr*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CStructTable&>().GetTypeName()),
        const CStr*>);

    static_assert(std::is_same_v<
        decltype(std::declval<CStatementList&>().GetProgramStatements()),
        CStatement*>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CStatementList&>().GetProgramStatements()),
        const CStatement*>);

    static_assert(std::is_same_v<
        decltype(std::declval<CStatementList&>().GetFileDataPointer()),
        LPSTR>);
    static_assert(std::is_same_v<
        decltype(std::declval<const CStatementList&>().GetFileDataPointer()),
        LPCSTR>);
}

#include "macros.h"

// Tracker struct for testing SafeDelete / SafeDeleteArray RAII behavior
struct DestructionTracker {
    static inline int s_destructorCount = 0;
    int value = 0;
    DestructionTracker() = default;
    explicit DestructionTracker(int v) : value(v) {}
    ~DestructionTracker() { ++s_destructorCount; }
};

// Mock COM interface for testing SafeRelease
struct MockComObject {
    int releaseCount = 0;
    ULONG Release() {
        ++releaseCount;
        return 0;
    }
};

// C++20 <=> Spaceship operator test struct
struct SpaceshipItem {
    int id = 0;
    auto operator<=>(const SpaceshipItem&) const = default;
    bool operator==(const SpaceshipItem&) const = default;
};

TEST(ModernCppSafetyTest, SafeDeleteTemplateSafety) {
    DestructionTracker::s_destructorCount = 0;
    auto* pObj = new DestructionTracker(42);
    ASSERT_NE(pObj, nullptr);
    EXPECT_EQ(pObj->value, 42);

    SafeDelete(pObj);
    EXPECT_EQ(pObj, nullptr);
    EXPECT_EQ(DestructionTracker::s_destructorCount, 1);

    // Null safety: calling SafeDelete on nullptr must be a safe no-op
    DestructionTracker* pNull = nullptr;
    SafeDelete(pNull);
    EXPECT_EQ(pNull, nullptr);
    EXPECT_EQ(DestructionTracker::s_destructorCount, 1);
}

TEST(ModernCppSafetyTest, SafeDeleteArrayTemplateSafety) {
    DestructionTracker::s_destructorCount = 0;
    constexpr size_t kArraySize = 5;
    auto* pArray = new DestructionTracker[kArraySize];
    ASSERT_NE(pArray, nullptr);

    SafeDeleteArray(pArray);
    EXPECT_EQ(pArray, nullptr);
    EXPECT_EQ(DestructionTracker::s_destructorCount, 5);

    // Null safety: calling SafeDeleteArray on nullptr must be a safe no-op
    DestructionTracker* pNullArray = nullptr;
    SafeDeleteArray(pNullArray);
    EXPECT_EQ(pNullArray, nullptr);
    EXPECT_EQ(DestructionTracker::s_destructorCount, 5);
}

TEST(ModernCppSafetyTest, SafeReleaseTemplateSafety) {
    MockComObject mock;
    MockComObject* pCom = &mock;
    EXPECT_EQ(mock.releaseCount, 0);

    SafeRelease(pCom);
    EXPECT_EQ(pCom, nullptr);
    EXPECT_EQ(mock.releaseCount, 1);

    // Null safety: calling SafeRelease on nullptr must be a safe no-op
    MockComObject* pNullCom = nullptr;
    SafeRelease(pNullCom);
    EXPECT_EQ(pNullCom, nullptr);
    EXPECT_EQ(mock.releaseCount, 1);
}

TEST(ModernCppSafetyTest, Cpp20SpaceshipThreeWayComparison) {
    SpaceshipItem a{1};
    SpaceshipItem b{2};
    SpaceshipItem c{1};

    // Equality
    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);

    // Three-way ordering
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a <= c);
    EXPECT_TRUE(a >= c);

    // Standard container sorting with C++20 spaceship operator
    std::vector<SpaceshipItem> items = { SpaceshipItem{10}, SpaceshipItem{2}, SpaceshipItem{7}, SpaceshipItem{1} };
    std::sort(items.begin(), items.end());

    EXPECT_EQ(items[0].id, 1);
    EXPECT_EQ(items[1].id, 2);
    EXPECT_EQ(items[2].id, 7);
    EXPECT_EQ(items[3].id, 10);
}

