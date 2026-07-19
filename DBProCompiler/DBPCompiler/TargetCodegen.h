#pragma once
#include "TypedIR.h"
#include "ICodeGenerator.h"

class TargetCodegen {
public:
    TargetCodegen(ICodeGenerator* codeGen, DWORD lineNumber = 1);
    ~TargetCodegen() = default;

    bool Generate(const IRProgram& ir);

private:
    ICodeGenerator* m_codeGen;
    DWORD m_lineNumber;
};
