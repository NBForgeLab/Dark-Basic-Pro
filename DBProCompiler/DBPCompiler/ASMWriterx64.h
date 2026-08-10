#pragma once
#include "ASMWriter.h"
#include <cstdint>
#include <cstddef>
#include <vector>

enum class X64Register : uint8_t {
    None = 0,
    RAX,
    RCX,
    RDX,
    RBX,
    RSP,
    RBP,
    RSI,
    RDI,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15
};

class CASMWriterx64 : public CASMWriter {
public:
    CASMWriterx64();
    ~CASMWriterx64() override = default;

    static X64Register GetArgumentRegister(size_t index) noexcept {
        switch (index) {
            case 0: return X64Register::RCX;
            case 1: return X64Register::RDX;
            case 2: return X64Register::R8;
            case 3: return X64Register::R9;
            default: return X64Register::None;
        }
    }

    static constexpr uint32_t GetShadowSpaceSize() noexcept {
        return 32U;
    }

    static constexpr uint32_t AlignStackFrame(uint32_t bytes) noexcept {
        const uint32_t aligned = (bytes + 15U) & ~15U;
        return aligned < 32U ? 32U : aligned;
    }

    // Instruction Emission Helpers
    void EmitByte(uint8_t b);
    void EmitDword(uint32_t dw);
    void EmitQword(uint64_t qw);

    void EmitMovRegImm64(X64Register reg, uint64_t val);
    void EmitPushReg(X64Register reg);
    void EmitPopReg(X64Register reg);
    void EmitSubRegImm32(X64Register reg, uint32_t val);
    void EmitAddRegImm32(X64Register reg, uint32_t val);
    void EmitRet();

    [[nodiscard]] const std::vector<uint8_t>& GetCodeBuffer() const noexcept { return m_codeBuffer; }
    [[nodiscard]] size_t GetCodeSize() const noexcept { return m_codeBuffer.size(); }
    void ClearCodeBuffer() noexcept { m_codeBuffer.clear(); }

private:
    std::vector<uint8_t> m_codeBuffer;
};
