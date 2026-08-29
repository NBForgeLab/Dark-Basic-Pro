#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <cstring>
#include <sstream>

namespace DBPDebugger {

enum class MessageType {
    DebugMapData = 1,
    VariableTable = 2,
    ProgramName = 3,
    StepHit = 11,
    VariableInspectResponse = 12,
    BreakpointHit = 13,
    DiagnosticLog = 21,
    RuntimeError = 31
};

// ---------------------------------------------------------------------------
// Native 64-bit variable transport formats.
//
// These layouts are the wire contract between the compiler (CASMWriter::
// MakeVarDataForTransfer / MakeVarValuesForTransfer) and the debugger. They
// are intentionally little-endian and versioned so the 64-bit variable-space
// offsets (and 64-bit string offsets) survive intact instead of being
// truncated to 32-bit.
// ---------------------------------------------------------------------------

constexpr uint32_t kVariableTableFormatVersion = 1;
struct VariableEntry {
    uint32_t type = 0;
    uint64_t offset = 0;
    std::string name;
};

constexpr uint32_t kVariableValuesFormatVersion = 1;
struct VariableValueBlob {
    uint32_t varSpaceSize = 0;
    std::vector<uint8_t> varSpace;
    struct StringEntry {
        uint64_t offset = 0;
        uint32_t length = 0;
        std::string chars;
    };
    std::vector<StringEntry> strings;
};

struct DebugEvent {
    MessageType type{MessageType::StepHit};
    uint32_t line{0};
    uint32_t fileIndex{0};
    std::string message;
    std::vector<uint8_t> rawData;
    std::vector<VariableEntry> variables;
    std::optional<VariableValueBlob> values;
};

// --- VariableTable (msg 2) --------------------------------------------------
//   u32 version | u32 count | entries[count]:
//     u32 type | u64 offset | u32 nameLen | name[nameLen]
inline std::optional<std::vector<VariableEntry>> ParseVariableTable(
    const uint8_t* payload, size_t payloadSize) {
    if (!payload || payloadSize < 8) return std::nullopt;
    uint32_t version = 0;
    uint32_t count = 0;
    std::memcpy(&version, payload, sizeof(version));
    std::memcpy(&count, payload + 4, sizeof(count));
    if (version != kVariableTableFormatVersion) return std::nullopt;

    size_t pos = 8;
    std::vector<VariableEntry> out;
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 16 > payloadSize) return std::nullopt; // type+offset+nameLen
        VariableEntry e;
        std::memcpy(&e.type, payload + pos, 4); pos += 4;
        std::memcpy(&e.offset, payload + pos, 8); pos += 8;
        uint32_t nameLen = 0;
        std::memcpy(&nameLen, payload + pos, 4); pos += 4;
        if (pos + nameLen > payloadSize) return std::nullopt;
        e.name.assign(reinterpret_cast<const char*>(payload + pos), nameLen);
        pos += nameLen;
        out.push_back(std::move(e));
    }
    return out;
}

inline void SerializeVariableTable(
    std::vector<uint8_t>& out, const std::vector<VariableEntry>& vars) {
    auto writeU32 = [&](uint32_t v) {
        uint8_t b[4];
        std::memcpy(b, &v, 4);
        out.insert(out.end(), b, b + 4);
    };
    auto writeU64 = [&](uint64_t v) {
        uint8_t b[8];
        std::memcpy(b, &v, 8);
        out.insert(out.end(), b, b + 8);
    };
    writeU32(kVariableTableFormatVersion);
    writeU32(static_cast<uint32_t>(vars.size()));
    for (const auto& e : vars) {
        writeU32(e.type);
        writeU64(e.offset);
        writeU32(static_cast<uint32_t>(e.name.size()));
        out.insert(out.end(), e.name.begin(), e.name.end());
    }
}

// --- VariableValues (msg 21) ------------------------------------------------
//   u32 version | u32 varSpaceSize | rawVarSpace[varSpaceSize] |
//   u32 stringCount | entries[stringCount]:
//     u8 marker(1) | u64 offset | u32 length | chars[length]
inline std::optional<VariableValueBlob> ParseVariableValues(
    const uint8_t* payload, size_t payloadSize) {
    if (!payload || payloadSize < 8) return std::nullopt;
    uint32_t version = 0;
    uint32_t varSpaceSize = 0;
    std::memcpy(&version, payload, 4);
    std::memcpy(&varSpaceSize, payload + 4, 4);
    if (version != kVariableValuesFormatVersion) return std::nullopt;

    size_t pos = 8;
    if (pos + varSpaceSize > payloadSize) return std::nullopt;
    VariableValueBlob blob;
    blob.varSpaceSize = varSpaceSize;
    blob.varSpace.assign(payload + pos, payload + pos + varSpaceSize);
    pos += varSpaceSize;

    if (pos + 4 > payloadSize) return std::nullopt;
    uint32_t stringCount = 0;
    std::memcpy(&stringCount, payload + pos, 4);
    pos += 4;
    for (uint32_t i = 0; i < stringCount; ++i) {
        if (pos + 13 > payloadSize) return std::nullopt; // marker+offset+len
        uint8_t marker = 0;
        std::memcpy(&marker, payload + pos, 1); pos += 1;
        if (marker != 1) return std::nullopt;
        VariableValueBlob::StringEntry s;
        std::memcpy(&s.offset, payload + pos, 8); pos += 8;
        std::memcpy(&s.length, payload + pos, 4); pos += 4;
        if (pos + s.length > payloadSize) return std::nullopt;
        s.chars.assign(reinterpret_cast<const char*>(payload + pos), s.length);
        pos += s.length;
        blob.strings.push_back(std::move(s));
    }
    return blob;
}

inline void SerializeVariableValues(
    std::vector<uint8_t>& out, const VariableValueBlob& blob) {
    auto writeU32 = [&](uint32_t v) {
        uint8_t b[4];
        std::memcpy(b, &v, 4);
        out.insert(out.end(), b, b + 4);
    };
    auto writeU64 = [&](uint64_t v) {
        uint8_t b[8];
        std::memcpy(b, &v, 8);
        out.insert(out.end(), b, b + 8);
    };
    writeU32(kVariableValuesFormatVersion);
    writeU32(blob.varSpaceSize);
    out.insert(out.end(), blob.varSpace.begin(), blob.varSpace.end());
    writeU32(static_cast<uint32_t>(blob.strings.size()));
    for (const auto& s : blob.strings) {
        uint8_t marker = 1;
        out.push_back(marker);
        writeU64(s.offset);
        writeU32(s.length);
        out.insert(out.end(), s.chars.begin(), s.chars.end());
    }
}

inline std::string FormatEventForCLI(const DebugEvent& evt) {
    std::ostringstream oss;
    oss << "[HEADLESS DEBUGGER] ";
    switch (evt.type) {
        case MessageType::RuntimeError:
            oss << "[RUNTIME ERROR] " << evt.message;
            break;
        case MessageType::DiagnosticLog:
            if (evt.values) {
                oss << "[VARIABLE VALUES] varspace=" << evt.values->varSpaceSize
                    << " strings=" << evt.values->strings.size();
                for (const auto& s : evt.values->strings) {
                    oss << "\n  - @" << s.offset << " \"" << s.chars << "\"";
                }
            } else {
                oss << "[LOG] " << evt.message;
            }
            break;
        case MessageType::StepHit:
            oss << "[STEP] Line " << evt.line << " (File #" << evt.fileIndex << ")";
            break;
        case MessageType::BreakpointHit:
            oss << "[BREAKPOINT] Hit line " << evt.line;
            break;
        case MessageType::ProgramName:
            oss << "[PROGRAM] " << evt.message;
            break;
        case MessageType::VariableTable:
            oss << "[VARIABLE TABLE] " << evt.variables.size() << " variables";
            for (const auto& v : evt.variables) {
                oss << "\n  - " << v.name << " (type " << v.type
                    << ") offset " << v.offset;
            }
            break;
        default:
            oss << "[EVENT " << static_cast<int>(evt.type)
                << "] Payload size: " << evt.rawData.size();
            break;
    }
    return oss.str();
}

class ProtocolParser {
public:
    std::optional<DebugEvent> ParseMessage(int msgType, const uint8_t* buffer, size_t bufferSize) {
        if (!buffer || bufferSize < sizeof(uint32_t)) {
            return std::nullopt;
        }

        uint32_t payloadSize = 0;
        std::memcpy(&payloadSize, buffer, sizeof(uint32_t));

        if (bufferSize < sizeof(uint32_t) + payloadSize) {
            return std::nullopt;
        }

        const uint8_t* payload = buffer + sizeof(uint32_t);
        DebugEvent evt;
        evt.type = static_cast<MessageType>(msgType);

        switch (evt.type) {
            case MessageType::StepHit:
            case MessageType::BreakpointHit: {
                if (payloadSize >= sizeof(uint32_t) * 2) {
                    std::memcpy(&evt.line, payload, sizeof(uint32_t));
                    std::memcpy(&evt.fileIndex, payload + sizeof(uint32_t), sizeof(uint32_t));
                }
                break;
            }
            case MessageType::ProgramName:
            case MessageType::RuntimeError: {
                if (payloadSize > 0) {
                    evt.message.assign(reinterpret_cast<const char*>(payload), payloadSize);
                }
                break;
            }
            case MessageType::VariableTable: {
                auto vars = ParseVariableTable(payload, payloadSize);
                if (vars) {
                    evt.variables = std::move(*vars);
                } else if (payloadSize > 0) {
                    evt.rawData.assign(payload, payload + payloadSize);
                }
                break;
            }
            case MessageType::DiagnosticLog: {
                // The compiler tunnels 64-bit variable values through this
                // channel; try to interpret the payload as a value blob, and
                // fall back to a plain string log otherwise.
                auto vals = ParseVariableValues(payload, payloadSize);
                if (vals) {
                    evt.values = std::move(*vals);
                } else if (payloadSize > 0) {
                    evt.message.assign(reinterpret_cast<const char*>(payload), payloadSize);
                }
                break;
            }
            default: {
                if (payloadSize > 0) {
                    evt.rawData.assign(payload, payload + payloadSize);
                }
                break;
            }
        }

        return evt;
    }
};

} // namespace DBPDebugger
