#include <gtest/gtest.h>
#include "ASTNode.h"
#include "ASTVisitor.h"
#include "ASTNodes.h"
#include "CodeGenVisitor.h"
#include "CompilerContext.h"
#include "VarTable.h"
#include "StatementList.h"
#include "ASMWriter.h"
#include "StructTable.h"
#include "DBPLogger.h"

extern ICodeGenerator* g_pASMWriter;
extern CVarTable* g_pVarTable;
extern CStructTable* g_pStructTable;
extern CStatementList* g_pStatementList;

// Define a simple test visitor to count traversed node types
class NodeCounterVisitor : public ASTVisitor {
public:
    int programCount = 0;
    int blockCount = 0;
    int assignmentCount = 0;
    int literalCount = 0;
    int variableCount = 0;

    void Visit(ASTProgramNode* node) override {
        programCount++;
        for (auto& stmt : node->m_statements) {
            stmt->Accept(this);
        }
    }
    void Visit(ASTBlockNode* node) override {
        blockCount++;
        for (auto& stmt : node->m_statements) {
            stmt->Accept(this);
        }
    }
    void Visit(ASTAssignmentNode* node) override {
        assignmentCount++;
        if (node->m_expression) {
            node->m_expression->Accept(this);
        }
    }
    void Visit(ASTLiteralNode* node) override {
        literalCount++;
    }
    void Visit(ASTVariableNode* node) override {
        variableCount++;
    }
};

TEST(ASTTest, ConstructionAndTraversal) {
    // Construct an AST for:
    // x = 100
    auto literal = std::make_unique<ASTLiteralNode>("100", 1); // 1 = integer type
    auto assignment = std::make_unique<ASTAssignmentNode>("x", std::move(literal));
    
    auto program = std::make_unique<ASTProgramNode>();
    program->m_statements.push_back(std::move(assignment));

    NodeCounterVisitor visitor;
    program->Accept(&visitor);

    EXPECT_EQ(visitor.programCount, 1);
    EXPECT_EQ(visitor.assignmentCount, 1);
    EXPECT_EQ(visitor.literalCount, 1);
    EXPECT_EQ(visitor.blockCount, 0);
    EXPECT_EQ(visitor.variableCount, 0);
}

class ASTCodeGenTest : public ::testing::Test {
protected:
    CompilerContext* m_pContext;

    void SetUp() override {
        DBPLogger::Initialize("test_ast.log");
        m_pContext = new CompilerContext();
        m_pContext->Initialize();
        g_pStructTable->SetStructDefaults();
        
        char dummyProg[] = "";
        g_pStatementList->MakeStatements(dummyProg, 0);
        g_pASMWriter->CreateASMHeader();
    }

    void TearDown() override {
        if (m_pContext) {
            m_pContext->Cleanup();
            delete m_pContext;
            m_pContext = nullptr;
        }
        spdlog::shutdown();
    }
};

TEST_F(ASTCodeGenTest, GenerateAssignmentCode) {
    // 1. Add variable to symbol table so code generator can resolve it
    g_pStatementList->SetVariableAddParse(true);
    DWORD dwAction = 0;
    bool addRes = g_pVarTable->AddVariable("myTestVar", "integer", 0, 1, true, &dwAction, false);
    ASSERT_TRUE(addRes);

    // 2. Build AST: myTestVar = 42
    auto literal = std::make_unique<ASTLiteralNode>("42", 1); // 1 = integer type
    auto assignment = std::make_unique<ASTAssignmentNode>("myTestVar", std::move(literal));
    auto program = std::make_unique<ASTProgramNode>();
    program->m_statements.push_back(std::move(assignment));

    // 3. Compile AST to Assembly
    CodeGenVisitor visitor(g_pASMWriter, 1);
    
    // Accept shouldn't crash and should execute cleanly
    EXPECT_NO_THROW(program->Accept(&visitor));
}
