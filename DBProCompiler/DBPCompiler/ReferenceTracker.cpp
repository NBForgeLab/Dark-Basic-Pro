#include "ReferenceTracker.h"

#include "EXEBlock.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <memory>
#include <system_error>

namespace
{
std::optional<std::uint32_t> ParseOneBasedIndex(const std::string_view text)
{
    std::uint32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0)
    {
        return std::nullopt;
    }
    return value - 1u;
}

std::optional<std::uint32_t> ParseImmediate(const std::string_view text)
{
    std::int64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value < (std::numeric_limits<std::int32_t>::min)() ||
        value > (std::numeric_limits<std::uint32_t>::max)())
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::string NormalizeLabel(const std::string_view label)
{
    return std::string(label.substr(0, label.find('@')));
}

bool IsSymbolic(const ReferenceKind kind) noexcept
{
    return kind == ReferenceKind::Variable || kind == ReferenceKind::CodeLabel ||
           kind == ReferenceKind::DataLabel;
}
} // namespace

std::optional<ParsedReference> ParseReferenceLabel(const std::string_view label)
{
    if (label.empty())
    {
        return std::nullopt;
    }

    ParsedReference parsed;
    if (label.front() == '[')
    {
        const auto index = ParseOneBasedIndex(label.substr(1));
        if (!index)
        {
            return std::nullopt;
        }
        parsed.kind = ReferenceKind::Command;
        parsed.index = *index;
        return parsed;
    }

    if (label.size() >= 2 && label[0] == '$' && label[1] == '$')
    {
        const auto index = ParseOneBasedIndex(label.substr(2));
        if (!index)
        {
            return std::nullopt;
        }
        parsed.kind = ReferenceKind::StringLiteral;
        parsed.index = *index;
        return parsed;
    }

    if (label.size() >= 2 && label[0] == '@' && label[1] == '&')
    {
        if (label.size() == 2)
        {
            return std::nullopt;
        }
        parsed.kind = ReferenceKind::Variable;
        // Array symbols are stored in CVarTable with their leading '&'.
        parsed.symbol = label.substr(1);
        parsed.isArray = true;
        return parsed;
    }

    if (label.front() == '@')
    {
        if (label.size() == 1)
        {
            return std::nullopt;
        }
        parsed.kind = ReferenceKind::Variable;
        parsed.symbol = label.substr(1);
        return parsed;
    }

    if (label.size() >= 2 && label[0] == '+' && label[1] == '@')
    {
        if (label.size() == 2)
        {
            return std::nullopt;
        }
        parsed.kind = ReferenceKind::Variable;
        parsed.symbol = label.substr(2);
        parsed.memoryOffset = 4;
        return parsed;
    }

    constexpr std::string_view CodeLabelPrefix = "$label";
    if (label.compare(0, CodeLabelPrefix.size(), CodeLabelPrefix) == 0)
    {
        parsed.kind = ReferenceKind::CodeLabel;
        parsed.symbol = NormalizeLabel(label);
        return parsed;
    }

    constexpr std::string_view DataLabelPrefix = "$dabel";
    if (label.compare(0, DataLabelPrefix.size(), DataLabelPrefix) == 0)
    {
        parsed.kind = ReferenceKind::DataLabel;
        parsed.symbol = NormalizeLabel(label);
        parsed.symbol[1] = 'l';
        return parsed;
    }

    const auto immediate = ParseImmediate(label);
    if (!immediate)
    {
        return std::nullopt;
    }
    parsed.kind = ReferenceKind::Immediate;
    parsed.index = *immediate;
    return parsed;
}

CReferenceTracker::CReferenceTracker()
{
    records_.reserve(InitialCapacity);
}

void CReferenceTracker::Reset() noexcept
{
    records_.clear();
}

bool CReferenceTracker::CheckAndExpandREFMemory()
{
    if (records_.capacity() - records_.size() >= 100)
    {
        return false;
    }

    records_.reserve(records_.capacity() + GrowthAmount);
    return true;
}

void CReferenceTracker::AddReference(
    const std::uint32_t machineCodeOffset,
    const std::string_view label,
    const std::uint32_t slotBytes,
    const std::uint32_t relEnd)
{
    CheckAndExpandREFMemory();
    records_.push_back({machineCodeOffset, std::string(label), slotBytes, relEnd});
}

std::uint32_t CReferenceTracker::GetRefPointer() const noexcept
{
    return static_cast<std::uint32_t>(records_.size());
}

std::uint32_t CReferenceTracker::GetRefBufferSize() const noexcept
{
    return static_cast<std::uint32_t>(records_.capacity());
}

std::uint32_t CReferenceTracker::GetRef(const std::size_t index) const noexcept
{
    return index < records_.size() ? records_[index].machineCodeOffset : 0u;
}

const std::string* CReferenceTracker::GetRefLabel(const std::size_t index) const noexcept
{
    return index < records_.size() ? &records_[index].label : nullptr;
}

std::uint32_t CReferenceTracker::GetRefWidth(const std::size_t index) const noexcept
{
    return index < records_.size() ? records_[index].slotBytes : 8u;
}

std::uint32_t CReferenceTracker::GetRefRelEnd(const std::size_t index) const noexcept
{
    return index < records_.size() ? records_[index].relEnd : 0u;
}

bool CReferenceTracker::SetRefLabel(const std::size_t index, const std::string_view label)
{
    if (index >= records_.size())
    {
        return false;
    }
    records_[index].label.assign(label);
    return true;
}

bool CReferenceTracker::UpdateMCBRefData(
    CEXEBlock* const executable,
    const std::uint32_t machineCodeBaseOffset,
    const SymbolResolver& resolveSymbol) const
{
    if (executable == nullptr || !resolveSymbol)
    {
        return false;
    }

    struct ResolvedRecord
    {
        std::uint32_t position;
        ReferenceKind kind;
        std::uint32_t index;
        std::uint32_t slotBytes;
        std::uint32_t relEnd;
    };

    std::vector<ResolvedRecord> resolved;
    resolved.reserve(records_.size());
    for (const auto& record : records_)
    {
        const auto parsed = ParseReferenceLabel(record.label);
        if (!parsed)
        {
            return false;
        }

        std::uint32_t resolvedIndex = parsed->index;
        if (IsSymbolic(parsed->kind))
        {
            const auto symbolIndex = resolveSymbol(*parsed);
            if (!symbolIndex)
            {
                return false;
            }
            resolvedIndex = *symbolIndex;
        }

        const std::uint64_t absolutePosition =
            static_cast<std::uint64_t>(record.machineCodeOffset) + machineCodeBaseOffset;
        if (absolutePosition > (std::numeric_limits<std::uint32_t>::max)())
        {
            return false;
        }
        resolved.push_back(
            {static_cast<std::uint32_t>(absolutePosition), parsed->kind, resolvedIndex,
             record.slotBytes, record.relEnd});
    }

    if (resolved.empty())
    {
        return true;
    }

    const auto oldCount = static_cast<std::size_t>(executable->m_dwNumberOfReferences);
    if (resolved.size() > (std::numeric_limits<std::uint32_t>::max)() - oldCount)
    {
        return false;
    }
    const auto newCount = oldCount + resolved.size();
    auto positions = std::make_unique<DWORD[]>(newCount);
    auto types = std::make_unique<DWORD[]>(newCount);
    auto indexes = std::make_unique<DWORD[]>(newCount);
    auto widths = std::make_unique<DWORD[]>(newCount);
    auto relEnds = std::make_unique<DWORD[]>(newCount);

    if (oldCount != 0)
    {
        if (executable->m_pRefArray == nullptr || executable->m_pRefTypeArray == nullptr ||
            executable->m_pRefIndexArray == nullptr || executable->m_pRefWidthArray == nullptr ||
            executable->m_pRefRelEndArray == nullptr)
        {
            return false;
        }
        std::copy_n(executable->m_pRefArray, oldCount, positions.get());
        std::copy_n(executable->m_pRefTypeArray, oldCount, types.get());
        std::copy_n(executable->m_pRefIndexArray, oldCount, indexes.get());
        std::copy_n(executable->m_pRefWidthArray, oldCount, widths.get());
        std::copy_n(executable->m_pRefRelEndArray, oldCount, relEnds.get());
    }

    for (std::size_t index = 0; index < resolved.size(); ++index)
    {
        const auto target = oldCount + index;
        positions[target] = resolved[index].position;
        types[target] = static_cast<std::uint32_t>(resolved[index].kind);
        indexes[target] = resolved[index].index;
        widths[target] = resolved[index].slotBytes;
        relEnds[target] = resolved[index].relEnd;
    }

    delete[] executable->m_pRefArray;
    delete[] executable->m_pRefTypeArray;
    delete[] executable->m_pRefIndexArray;
    delete[] executable->m_pRefWidthArray;
    delete[] executable->m_pRefRelEndArray;
    executable->m_pRefArray = positions.release();
    executable->m_pRefTypeArray = types.release();
    executable->m_pRefIndexArray = indexes.release();
    executable->m_pRefWidthArray = widths.release();
    executable->m_pRefRelEndArray = relEnds.release();
    executable->m_dwNumberOfReferences = static_cast<DWORD>(newCount);
    return true;
}
