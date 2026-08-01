#include <gtest/gtest.h>

#include "ASMWriter.h"
#include "ASMWriterPreparationServices.h"

class CDBPCompiler;
class CEXEBlock;
class CStatementList;

extern CDBPCompiler* g_pDBPCompiler;
extern CEXEBlock* g_pEXE;
extern CStatementList* g_pStatementList;

namespace {

class ScopedCoreCompilerGlobals final {
public:
    ScopedCoreCompilerGlobals() noexcept
        : compiler_(g_pDBPCompiler), exe_(g_pEXE),
          statements_(g_pStatementList) {
        g_pDBPCompiler = nullptr;
        g_pEXE = nullptr;
        g_pStatementList = nullptr;
    }

    ~ScopedCoreCompilerGlobals() {
        g_pDBPCompiler = compiler_;
        g_pEXE = exe_;
        g_pStatementList = statements_;
    }

    ScopedCoreCompilerGlobals(const ScopedCoreCompilerGlobals&) = delete;
    ScopedCoreCompilerGlobals& operator=(
        const ScopedCoreCompilerGlobals&) = delete;

private:
    CDBPCompiler* compiler_;
    CEXEBlock* exe_;
    CStatementList* statements_;
};

TEST(ASMWriterPreparationServicesTest,
     MissingCoreCollaboratorsFailClosedWithoutDereference) {
    CASMWriter writer;
    ASMWriterPreparationServices services(writer);
    const ScopedCoreCompilerGlobals missingGlobals;

    EXPECT_FALSE(services.ValidateTarget());
    EXPECT_FALSE(services.ValidateRuntime());
    EXPECT_FALSE(services.FinalizeSpaceSizes());
}

} // namespace
