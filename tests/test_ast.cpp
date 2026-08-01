#include <gtest/gtest.h>
#include "ASTNode.h"
#include "ASTVisitor.h"
#include "ASTNodes.h"
#include "ASTPrinter.h"
#include "ASTPipelineParser.h"
#include "CodeGenVisitor.h"
#include "SemanticVisitor.h"
#include "IRLoweringVisitor.h"
#include "TargetCodegen.h"
#include "ASTOptimizer.h"
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
    void Visit(ASTIfNode* node) override {
        if (node->m_condition) node->m_condition->Accept(this);
        if (node->m_thenBranch) node->m_thenBranch->Accept(this);
        if (node->m_elseBranch) node->m_elseBranch->Accept(this);
    }
    void Visit(ASTWhileNode* node) override {
        if (node->m_condition) node->m_condition->Accept(this);
        if (node->m_body) node->m_body->Accept(this);
    }
    void Visit(ASTForNode* node) override {
        if (node->m_startExpr) node->m_startExpr->Accept(this);
        if (node->m_endExpr) node->m_endExpr->Accept(this);
        if (node->m_stepExpr) node->m_stepExpr->Accept(this);
        if (node->m_body) node->m_body->Accept(this);
    }
    void Visit(ASTFunctionCallNode* node) override {
        for (auto& arg : node->m_arguments) {
            if (arg) arg->Accept(this);
        }
    }
    void Visit(ASTFunctionDeclNode* node) override {
        if (node->m_body) node->m_body->Accept(this);
        if (node->m_returnExpr) node->m_returnExpr->Accept(this);
    }
    void Visit(ASTArrayDimNode* node) override {
        for (auto& dim : node->m_dimensions) {
            if (dim) dim->Accept(this);
        }
    }
    void Visit(ASTArrayAccessNode* node) override {
        for (auto& idx : node->m_indices) {
            if (idx) idx->Accept(this);
        }
    }
    void Visit(ASTStructDeclNode* node) override {}
    void Visit(ASTStructAccessNode* node) override {}
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

TEST_F(ASTCodeGenTest, AssignmentPipelineEmitsLoweredTargetCode) {
    g_pStatementList->SetVariableAddParse(true);
    DWORD action = 0;
    ASSERT_TRUE(g_pVarTable->AddVariable(
        "pipelineTarget", "integer", 0, 1, true, &action, false));
    auto* const writer = static_cast<CASMWriter*>(g_pASMWriter);
    const auto before = writer->GetCurrentMCPosition();
    CASTAssignment assignment("pipelineTarget", "42", 1);

    EXPECT_TRUE(assignment.WriteDBM());
    EXPECT_GT(writer->GetCurrentMCPosition(), before);
    const auto* const machineCode = reinterpret_cast<const unsigned char*>(
        writer->GetMachineCodeBuffer().GetProgramStart());
    ASSERT_NE(machineCode, nullptr);
    EXPECT_EQ(machineCode[before], 0xB8u)
        << "loading an integer literal must emit MOV EAX, imm32";
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

TEST(ASTControlFlowTest, IfElseNodeConstructionAndTraversal) {
    auto cond = std::make_unique<ASTBinaryOpNode>(
        BinaryOpType::LessThan,
        std::make_unique<ASTVariableNode>("x"),
        std::make_unique<ASTLiteralNode>("10", 1)
    );

    auto thenBlock = std::make_unique<ASTBlockNode>();
    thenBlock->m_statements.push_back(
        std::make_unique<ASTAssignmentNode>("y", std::make_unique<ASTLiteralNode>("1", 1))
    );

    auto elseBlock = std::make_unique<ASTBlockNode>();
    elseBlock->m_statements.push_back(
        std::make_unique<ASTAssignmentNode>("y", std::make_unique<ASTLiteralNode>("2", 1))
    );

    auto ifNode = std::make_unique<ASTIfNode>(std::move(cond), std::move(thenBlock), std::move(elseBlock));

    ASTPrinter printer;
    std::string printed = printer.Print(ifNode.get());
    EXPECT_NE(printed.find("If:"), std::string::npos);
    EXPECT_NE(printed.find("Then:"), std::string::npos);
    EXPECT_NE(printed.find("Else:"), std::string::npos);

    IRLoweringVisitor lowering;
    ifNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();

    bool hasJumpIfFalse = false;
    bool hasJump = false;
    bool hasLabel = false;

    for (const auto& inst : ir.instructions) {
        if (inst.opCode == IROpCode::JumpIfFalse) hasJumpIfFalse = true;
        if (inst.opCode == IROpCode::Jump) hasJump = true;
        if (inst.opCode == IROpCode::Label) hasLabel = true;
    }

    EXPECT_TRUE(hasJumpIfFalse);
    EXPECT_TRUE(hasJump);
    EXPECT_TRUE(hasLabel);
}

TEST(ASTLoopTest, WhileLoopIRLowering) {
    auto cond = std::make_unique<ASTBinaryOpNode>(
        BinaryOpType::LessThan,
        std::make_unique<ASTVariableNode>("i"),
        std::make_unique<ASTLiteralNode>("10", 1)
    );

    auto body = std::make_unique<ASTBlockNode>();
    body->m_statements.push_back(
        std::make_unique<ASTAssignmentNode>("sum", std::make_unique<ASTLiteralNode>("1", 1))
    );

    auto whileNode = std::make_unique<ASTWhileNode>(std::move(cond), std::move(body));

    ASTPrinter printer;
    std::string printed = printer.Print(whileNode.get());
    EXPECT_NE(printed.find("While:"), std::string::npos);

    IRLoweringVisitor lowering;
    whileNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();

    int labelCount = 0;
    bool hasJumpIfFalse = false;
    bool hasJump = false;

    for (const auto& inst : ir.instructions) {
        if (inst.opCode == IROpCode::Label) labelCount++;
        if (inst.opCode == IROpCode::JumpIfFalse) hasJumpIfFalse = true;
        if (inst.opCode == IROpCode::Jump) hasJump = true;
    }

    EXPECT_GE(labelCount, 2);
    EXPECT_TRUE(hasJumpIfFalse);
    EXPECT_TRUE(hasJump);
}

TEST(ASTLoopTest, ForLoopIRLowering) {
    auto body = std::make_unique<ASTBlockNode>();
    body->m_statements.push_back(
        std::make_unique<ASTAssignmentNode>("acc", std::make_unique<ASTLiteralNode>("5", 1))
    );

    auto forNode = std::make_unique<ASTForNode>(
        "i",
        std::make_unique<ASTLiteralNode>("1", 1),
        std::make_unique<ASTLiteralNode>("10", 1),
        std::make_unique<ASTLiteralNode>("1", 1),
        std::move(body)
    );

    ASTPrinter printer;
    std::string printed = printer.Print(forNode.get());
    EXPECT_NE(printed.find("For: i"), std::string::npos);

    IRLoweringVisitor lowering;
    forNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();

    bool hasStoreVar = false;
    bool hasJumpIfFalse = false;

    for (const auto& inst : ir.instructions) {
        if (inst.opCode == IROpCode::StoreVar && inst.operandStr == "i") hasStoreVar = true;
        if (inst.opCode == IROpCode::JumpIfFalse) hasJumpIfFalse = true;
    }

    EXPECT_TRUE(hasStoreVar);
    EXPECT_TRUE(hasJumpIfFalse);
}

TEST(ASTFunctionTest, FunctionDeclAndCallLowering) {
    std::vector<std::unique_ptr<ASTNode>> args;
    args.push_back(std::make_unique<ASTLiteralNode>("5", 1));
    args.push_back(std::make_unique<ASTLiteralNode>("10", 1));
    auto callNode = std::make_unique<ASTFunctionCallNode>("my_add", std::move(args));

    ASTPrinter printer;
    std::string printedCall = printer.Print(callNode.get());
    EXPECT_NE(printedCall.find("FunctionCall: my_add"), std::string::npos);

    IRLoweringVisitor lowering;
    callNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();

    bool hasCall = false;
    for (const auto& inst : ir.instructions) {
        if (inst.opCode == IROpCode::Call && inst.operandStr == "my_add") hasCall = true;
    }
    EXPECT_TRUE(hasCall);

    std::vector<std::string> params = {"x", "y"};
    auto body = std::make_unique<ASTBlockNode>();
    auto retExpr = std::make_unique<ASTBinaryOpNode>(
        BinaryOpType::Add,
        std::make_unique<ASTVariableNode>("x"),
        std::make_unique<ASTVariableNode>("y")
    );
    auto declNode = std::make_unique<ASTFunctionDeclNode>("my_add", params, std::move(body), std::move(retExpr));

    std::string printedDecl = printer.Print(declNode.get());
    EXPECT_NE(printedDecl.find("FunctionDecl: my_add"), std::string::npos);
}

TEST(ASTPipelineParserTest, ParseAssignmentsAndLocations) {
    std::string source = "x = 100\ny = x + 50 * 2\n";
    auto progNode = ASTPipelineParser::ParseProgram(source);
    ASSERT_NE(progNode, nullptr);
    EXPECT_EQ(progNode->m_statements.size(), 2);

    auto assign1 = dynamic_cast<ASTAssignmentNode*>(progNode->m_statements[0].get());
    ASSERT_NE(assign1, nullptr);
    EXPECT_EQ(assign1->m_varName, "x");
    EXPECT_EQ(assign1->GetLocation().line, 1);

    auto assign2 = dynamic_cast<ASTAssignmentNode*>(progNode->m_statements[1].get());
    ASSERT_NE(assign2, nullptr);
    EXPECT_EQ(assign2->m_varName, "y");
    EXPECT_EQ(assign2->GetLocation().line, 2);

    SemanticVisitor semantic;
    progNode->Accept(&semantic);
    EXPECT_FALSE(semantic.HasErrors());

    IRLoweringVisitor lowering;
    progNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();
    EXPECT_GT(ir.instructions.size(), 0);
}

TEST(ASTArrayAndStructTest, ArrayAndStructNodeConstruction) {
    std::vector<std::unique_ptr<ASTNode>> dims;
    dims.push_back(std::make_unique<ASTLiteralNode>("10", 1));
    dims.push_back(std::make_unique<ASTLiteralNode>("20", 1));
    auto dimNode = std::make_unique<ASTArrayDimNode>("grid", std::move(dims), 1);

    ASTPrinter printer;
    std::string printedDim = printer.Print(dimNode.get());
    EXPECT_NE(printedDim.find("ArrayDim: grid"), std::string::npos);

    std::vector<ASTStructField> fields = {{"x", 1}, {"y", 1}, {"name", 3}};
    auto structDecl = std::make_unique<ASTStructDeclNode>("PlayerType", fields);
    std::string printedStruct = printer.Print(structDecl.get());
    EXPECT_NE(printedStruct.find("StructDecl: PlayerType"), std::string::npos);

    auto structAccess = std::make_unique<ASTStructAccessNode>("player1", "health");
    std::string printedAccess = printer.Print(structAccess.get());
    EXPECT_NE(printedAccess.find("StructAccess: player1.health"), std::string::npos);
}

TEST(ASTExpressionParserTest, ParseStructPropertyAccess) {
    auto node = ASTExpressionParser::Parse("player.health");
    ASSERT_NE(node, nullptr);
    auto structAccess = dynamic_cast<ASTStructAccessNode*>(node.get());
    ASSERT_NE(structAccess, nullptr);
    EXPECT_EQ(structAccess->m_varName, "player");
    EXPECT_EQ(structAccess->m_fieldName, "health");
}

TEST(ASTExpressionParserTest, ParseArrayElementAccess) {
    auto node = ASTExpressionParser::Parse("grid(x, 10)");
    ASSERT_NE(node, nullptr);
    auto arrAccess = dynamic_cast<ASTArrayAccessNode*>(node.get());
    ASSERT_NE(arrAccess, nullptr);
    EXPECT_EQ(arrAccess->m_arrayName, "grid");
    EXPECT_EQ(arrAccess->m_indices.size(), 2);
}

TEST(ASTControlFlowTest, ControlFlowNodesConstructionAndLowering) {
    auto cond = std::make_unique<ASTBinaryOpNode>(BinaryOpType::GreaterThan, std::make_unique<ASTVariableNode>("score"), std::make_unique<ASTLiteralNode>("100", 1));
    
    std::vector<std::unique_ptr<ASTNode>> thenStmts;
    thenStmts.push_back(std::make_unique<ASTAssignmentNode>("highScore", std::make_unique<ASTLiteralNode>("1", 1)));
    auto thenBlock = std::make_unique<ASTBlockNode>(std::move(thenStmts));

    std::vector<std::unique_ptr<ASTNode>> elseStmts;
    elseStmts.push_back(std::make_unique<ASTAssignmentNode>("highScore", std::make_unique<ASTLiteralNode>("0", 1)));
    auto elseBlock = std::make_unique<ASTBlockNode>(std::move(elseStmts));

    auto ifNode = std::make_unique<ASTIfNode>(std::move(cond), std::move(thenBlock), std::move(elseBlock));

    ASTPrinter printer;
    std::string printedIf = printer.Print(ifNode.get());
    EXPECT_NE(printedIf.find("If"), std::string::npos);

    SemanticVisitor semantic;
    ifNode->Accept(&semantic);
    EXPECT_FALSE(semantic.HasErrors());

    IRLoweringVisitor lowering;
    ifNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();
    EXPECT_GT(ir.instructions.size(), 0);
}

TEST(ASTOptimizerTest, ConstantFoldingFoldsArithmeticLiterals) {
    // 5 + 10 * 2 -> 5 + 20 -> 25
    auto expr = std::make_unique<ASTBinaryOpNode>(
        BinaryOpType::Add,
        std::make_unique<ASTLiteralNode>("5", 1),
        std::make_unique<ASTBinaryOpNode>(
            BinaryOpType::Multiply,
            std::make_unique<ASTLiteralNode>("10", 1),
            std::make_unique<ASTLiteralNode>("2", 1)
        )
    );

    ASTOptimizer optimizer;
    auto optimized = optimizer.Optimize(std::move(expr));
    ASSERT_NE(optimized, nullptr);

    auto literal = dynamic_cast<ASTLiteralNode*>(optimized.get());
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->m_value, "25");
    EXPECT_EQ(literal->m_type, 1);
}

TEST(ASTPipelineOptimizerIntegrationTest, AssignmentOptimizationTrace) {
    CASTAssignment assignment("x", "10 + 20 * 2", 1);
    
    // Test that optimization passes before lowering
    auto parsedExpr = ASTExpressionParser::Parse("10 + 20 * 2");
    ASSERT_NE(parsedExpr, nullptr);

    ASTOptimizer optimizer;
    auto optimizedExpr = optimizer.Optimize(std::move(parsedExpr));
    ASSERT_NE(optimizedExpr, nullptr);

    auto assignNode = std::make_unique<ASTAssignmentNode>("x", std::move(optimizedExpr));

    SemanticVisitor semantic;
    assignNode->Accept(&semantic);
    EXPECT_FALSE(semantic.HasErrors());

    IRLoweringVisitor lowering;
    assignNode->Accept(&lowering);
    IRProgram ir = lowering.GetProgram();

    // Verification: constant folding reduced expression to 1 LoadConst(50) and 1 StoreVar(x)
    ASSERT_EQ(ir.instructions.size(), 2);
    EXPECT_EQ(ir.instructions[0].opCode, IROpCode::LoadConst);
    EXPECT_EQ(ir.instructions[0].operandStr, "50");
    EXPECT_EQ(ir.instructions[1].opCode, IROpCode::StoreVar);
    EXPECT_EQ(ir.instructions[1].operandStr, "x");
}





