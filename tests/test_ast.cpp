#include <gtest/gtest.h>
#include "ASTNode.h"
#include "ASTVisitor.h"
#include "ASTNodes.h"
#include "CodeGenVisitor.h"
#include "SemanticVisitor.h"
#include "IRLoweringVisitor.h"
#include "TargetCodegen.h"
#include "CompilerContext.h"
#include "VarTable.h"
#include "StatementList.h"
#include "ASMWriter.h"
#include "StructTable.h"
#include "InstructionTable.h"
#include "DBPLogger.h"

extern ICodeGenerator* g_pASMWriter;
extern CVarTable* g_pVarTable;
extern CStructTable* g_pStructTable;
extern CStatementList* g_pStatementList;
extern CInstructionTable* g_pInstructionTable;

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
    void Visit(ASTBinaryOpNode* node) override {
        if (node->m_left) node->m_left->Accept(this);
        if (node->m_right) node->m_right->Accept(this);
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

TEST(ASTTest, BinaryOpConstruction) {
    auto left = std::make_unique<ASTLiteralNode>("10", 1);
    auto right = std::make_unique<ASTLiteralNode>("20", 1);
    auto binaryOp = std::make_unique<ASTBinaryOpNode>(BinaryOpType::Add, std::move(left), std::move(right));
    
    NodeCounterVisitor visitor;
    binaryOp->Accept(&visitor);
    EXPECT_EQ(visitor.literalCount, 2);
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

TEST(ASTParsingRegressionTest, SimpleAssignmentDoesNotEmitBeforeBackendInitialization) {
    DBPLogger::Initialize("test_ast_parse.log");
    CompilerContext context;
    context.Initialize();
    g_pStructTable->SetStructDefaults();
    g_pInstructionTable->SetInternalInstructionDatabase();
    char program[] =
        "global gloadreportstate\r\n"
        "gloadreportstate=0\r\n";

    const bool parsed = g_pStatementList->MakeStatements(
        program, static_cast<DWORD>(sizeof(program) - 1));

    EXPECT_TRUE(parsed);
    context.Cleanup();
    spdlog::shutdown();
}

TEST_F(ASTCodeGenTest, SemanticTypeCheck) {
    g_pStatementList->SetVariableAddParse(true);
    DWORD dwAction = 0;
    g_pVarTable->AddVariable("myIntVar", "integer", 0, 1, true, &dwAction, false);

    // Assign literal integer: myIntVar = 42
    auto literal = std::make_unique<ASTLiteralNode>("42", 1);
    auto assignment = std::make_unique<ASTAssignmentNode>("myIntVar", std::move(literal));

    SemanticVisitor visitor;
    assignment->Accept(&visitor);
    EXPECT_FALSE(visitor.HasErrors());
}

TEST(ASTTest, IRLowering) {
    auto left = std::make_unique<ASTLiteralNode>("10", 1);
    auto right = std::make_unique<ASTLiteralNode>("20", 1);
    auto binaryOp = std::make_unique<ASTBinaryOpNode>(BinaryOpType::Add, std::move(left), std::move(right));
    auto assignment = std::make_unique<ASTAssignmentNode>("x", std::move(binaryOp));

    IRLoweringVisitor lowering;
    assignment->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();
    
    ASSERT_EQ(ir.instructions.size(), 4u);
    EXPECT_EQ(ir.instructions[0].opCode, IROpCode::LoadConst);
    EXPECT_EQ(ir.instructions[1].opCode, IROpCode::LoadConst);
    EXPECT_EQ(ir.instructions[2].opCode, IROpCode::BinaryOp);
    EXPECT_EQ(ir.instructions[3].opCode, IROpCode::StoreVar);
}

TEST_F(ASTCodeGenTest, PipelineCodegen) {
    g_pStatementList->SetVariableAddParse(true);
    DWORD dwAction = 0;
    g_pVarTable->AddVariable("testTargetVar", "integer", 0, 1, true, &dwAction, false);

    // myIntVar = 42
    auto literal = std::make_unique<ASTLiteralNode>("42", 1);
    auto assignment = std::make_unique<ASTAssignmentNode>("testTargetVar", std::move(literal));

    // Lower
    IRLoweringVisitor lowering;
    assignment->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();

    // Compile
    TargetCodegen codegen(g_pASMWriter, 1);
    EXPECT_TRUE(codegen.Generate(ir));
}

#include "ASTExpressionParser.h"

TEST(ASTExpressionParserTest, ParseLiteral) {
    auto node = ASTExpressionParser::Parse("123");
    ASSERT_NE(node, nullptr);
    auto literal = dynamic_cast<ASTLiteralNode*>(node.get());
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->m_value, "123");
    EXPECT_EQ(literal->m_type, 1);
}

TEST(ASTExpressionParserTest, ParseVariable) {
    auto node = ASTExpressionParser::Parse("myVar");
    ASSERT_NE(node, nullptr);
    auto var = dynamic_cast<ASTVariableNode*>(node.get());
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->m_varName, "myVar");
}

TEST(ASTExpressionParserTest, ParseAddition) {
    auto node = ASTExpressionParser::Parse("x + 10");
    ASSERT_NE(node, nullptr);
    auto binOp = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(binOp, nullptr);
    EXPECT_EQ(binOp->m_op, BinaryOpType::Add);
    
    auto left = dynamic_cast<ASTVariableNode*>(binOp->m_left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->m_varName, "x");

    auto right = dynamic_cast<ASTLiteralNode*>(binOp->m_right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->m_value, "10");
}

TEST(ASTExpressionParserTest, ParseSubtraction) {
    auto node = ASTExpressionParser::Parse("val - other_val");
    ASSERT_NE(node, nullptr);
    auto binOp = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(binOp, nullptr);
    EXPECT_EQ(binOp->m_op, BinaryOpType::Subtract);

    auto left = dynamic_cast<ASTVariableNode*>(binOp->m_left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->m_varName, "val");

    auto right = dynamic_cast<ASTVariableNode*>(binOp->m_right.get());
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->m_varName, "other_val");
}

TEST(ASTExpressionParserTest, ParseMultiplication) {
    auto node = ASTExpressionParser::Parse("a * b");
    ASSERT_NE(node, nullptr);
    auto binOp = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(binOp, nullptr);
    EXPECT_EQ(binOp->m_op, BinaryOpType::Multiply);
}

TEST(ASTExpressionParserTest, ParseDivision) {
    auto node = ASTExpressionParser::Parse("a / b");
    ASSERT_NE(node, nullptr);
    auto binOp = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(binOp, nullptr);
    EXPECT_EQ(binOp->m_op, BinaryOpType::Divide);
}

TEST(ASTExpressionParserTest, ParsePrecedence) {
    // a + b * c should parse as a + (b * c)
    auto node = ASTExpressionParser::Parse("a + b * c");
    ASSERT_NE(node, nullptr);
    auto rootAdd = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(rootAdd, nullptr);
    EXPECT_EQ(rootAdd->m_op, BinaryOpType::Add);

    auto leftVar = dynamic_cast<ASTVariableNode*>(rootAdd->m_left.get());
    ASSERT_NE(leftVar, nullptr);
    EXPECT_EQ(leftVar->m_varName, "a");

    auto rightMul = dynamic_cast<ASTBinaryOpNode*>(rootAdd->m_right.get());
    ASSERT_NE(rightMul, nullptr);
    EXPECT_EQ(rightMul->m_op, BinaryOpType::Multiply);
}

TEST(ASTExpressionParserTest, ParseParentheses) {
    // (a + b) * c should parse as ((a + b) * c)
    auto node = ASTExpressionParser::Parse("(a + b) * c");
    ASSERT_NE(node, nullptr);
    auto rootMul = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(rootMul, nullptr);
    EXPECT_EQ(rootMul->m_op, BinaryOpType::Multiply);

    auto leftAdd = dynamic_cast<ASTBinaryOpNode*>(rootMul->m_left.get());
    ASSERT_NE(leftAdd, nullptr);
    EXPECT_EQ(leftAdd->m_op, BinaryOpType::Add);
}

TEST(ASTExpressionParserTest, ParseComparisons) {
    auto node = ASTExpressionParser::Parse("a < b");
    ASSERT_NE(node, nullptr);
    auto binOp = dynamic_cast<ASTBinaryOpNode*>(node.get());
    ASSERT_NE(binOp, nullptr);
    EXPECT_EQ(binOp->m_op, BinaryOpType::LessThan);
}

