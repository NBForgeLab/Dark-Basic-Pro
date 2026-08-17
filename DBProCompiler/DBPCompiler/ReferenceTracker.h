#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class CEXEBlock;

/** Serialized reference kinds consumed by CEXEBlock::RunProgram. */
enum class ReferenceKind : std::uint32_t
{
    Command = 1,
    StringLiteral = 2,
    Variable = 3,
    Immediate = 4,
    CodeLabel = 5,
    DataLabel = 6,
};

/** A validated reference label, independent of compiler-global symbol tables. */
struct ParsedReference
{
    ReferenceKind kind{};
    std::uint32_t index{};
    std::string symbol;
    bool isArray{};
    std::uint32_t memoryOffset{};
};

/**
 * Parses the textual reference syntax emitted by CASMWriter.
 *
 * Numeric references are fully resolved in the returned index. Symbolic
 * variable and label references retain their normalized symbol for resolution
 * against the compiler tables at the executable-preparation boundary.
 */
[[nodiscard]] std::optional<ParsedReference> ParseReferenceLabel(std::string_view label);

/**
 * Owns forward-reference records emitted alongside the x86 machine code.
 *
 * Labels are stored as std::string values. This is intentionally pointer-width
 * independent: host addresses must never become part of the serialized target
 * reference format.
 */
class CReferenceTracker
{
public:
    struct Record
    {
        std::uint32_t machineCodeOffset{};
        std::string label;
        // Size of the operand slot in the generated machine code (4 for
        // disp32/rel32 slots, 8 for imm64 slots). The boot-time ref patch
        // needs this to write either an absolute 64-bit address or a
        // relative 32-bit displacement, and to avoid clobbering bytes that
        // belong to the following instruction.
        std::uint32_t slotBytes{8u};
        // Absolute MCB offset of the end of the enclosing instruction. For
        // RIP-relative disp32 and rel32 slots the CPU computes the effective
        // address from the address of the *next* instruction, so the patched
        // displacement must be target - relEnd. Zero falls back to
        // machineCodeOffset + slotBytes (operand at the tail of the
        // instruction, e.g. imm64 slots).
        std::uint32_t relEnd{0u};
    };

    using SymbolResolver =
        std::function<std::optional<std::uint32_t>(const ParsedReference&)>;

    CReferenceTracker();
    ~CReferenceTracker() = default;

    CReferenceTracker(const CReferenceTracker&) = delete;
    CReferenceTracker& operator=(const CReferenceTracker&) = delete;
    CReferenceTracker(CReferenceTracker&&) noexcept = default;
    CReferenceTracker& operator=(CReferenceTracker&&) noexcept = default;

    void Reset() noexcept;

    /** Reserves additional storage when fewer than 100 slots remain. */
    bool CheckAndExpandREFMemory();

    void AddReference(std::uint32_t machineCodeOffset, std::string_view label,
                      std::uint32_t slotBytes = 8u, std::uint32_t relEnd = 0u);

    [[nodiscard]] std::uint32_t GetRefPointer() const noexcept;
    [[nodiscard]] std::uint32_t GetRefBufferSize() const noexcept;
    [[nodiscard]] std::uint32_t GetRef(std::size_t index) const noexcept;
    [[nodiscard]] std::uint32_t GetRefWidth(std::size_t index) const noexcept;
    [[nodiscard]] std::uint32_t GetRefRelEnd(std::size_t index) const noexcept;
    [[nodiscard]] const std::string* GetRefLabel(std::size_t index) const noexcept;
    bool SetRefLabel(std::size_t index, std::string_view label);

    [[nodiscard]] const std::vector<Record>& GetRecords() const noexcept { return records_; }

    /**
     * Resolves and appends all records to an executable block atomically.
     * Existing arrays remain unchanged when parsing or symbol resolution fails.
     */
    bool UpdateMCBRefData(
        CEXEBlock* executable,
        std::uint32_t machineCodeBaseOffset,
        const SymbolResolver& resolveSymbol) const;

private:
    static constexpr std::size_t InitialCapacity = 1024;
    static constexpr std::size_t GrowthAmount = 1024;

    std::vector<Record> records_;
};
