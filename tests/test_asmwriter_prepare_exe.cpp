#include <gtest/gtest.h>

#include "ASMWriter.h"
#include "CompilerContext.h"
#include "DBPCompiler.h"
#include "EXEBlock.h"
#include "StatementList.h"
#include "StructTable.h"

extern CDBPCompiler* g_pDBPCompiler;
extern CEXEBlock* g_pEXE;
extern ICodeGenerator* g_pASMWriter;
extern CStatementList* g_pStatementList;
extern CStructTable* g_pStructTable;

namespace {

TEST(ASMWriterPrepareEXETest,
     RuntimeFailureOccursAfterMaterializationAndReleasesTransientCode) {
    CompilerContext context;
    context.Initialize();
    g_pStructTable->SetStructDefaults();
    char emptyProgram[] = "";
    ASSERT_TRUE(g_pStatementList->MakeStatements(emptyProgram, 0U));

    auto& writer = static_cast<CASMWriter&>(*g_pASMWriter);
    ASSERT_TRUE(writer.CreateASMHeader());
    ASSERT_TRUE(writer.CreateASMMiddle(-1, 0x90, -1, nullptr));
    ASSERT_GT(writer.GetCurrentMCPosition(), 0U);

    char compilerPath[] = "DBPCompiler.exe";
    CDBPCompiler compiler(compilerPath);
    CDBPCompiler* const previousCompiler = g_pDBPCompiler;
    g_pDBPCompiler = &compiler;

    char outputPath[] = "unpublished-test-output.exe";
    const bool result = writer.PrepareEXE(
        outputPath, true, true);

    g_pDBPCompiler = previousCompiler;

    EXPECT_FALSE(result);
    ASSERT_NE(g_pEXE->m_pMachineCodeBlock, nullptr);
    EXPECT_GT(g_pEXE->m_dwSizeOfMCB, 0U);
    EXPECT_EQ(writer.GetCurrentMCPosition(), 0U);
}

} // namespace
