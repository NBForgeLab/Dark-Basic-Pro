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

struct DebugEvent {
    MessageType type{MessageType::StepHit};
    uint32_t line{0};
    uint32_t fileIndex{0};
    std::string message;
    std::vector<uint8_t> rawData;
};

inline std::string FormatEventForCLI(const DebugEvent& evt) {
    std::ostringstream oss;
    oss << "[HEADLESS DEBUGGER] ";
    switch (evt.type) {
        case MessageType::RuntimeError:
            oss << "[RUNTIME ERROR] " << evt.message;
            break;
        case MessageType::DiagnosticLog:
            oss << "[LOG] " << evt.message;
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
        default:
            oss << "[EVENT " << static_cast<int>(evt.type) << "] Payload size: " << evt.rawData.size();
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
            case MessageType::DiagnosticLog:
            case MessageType::RuntimeError: {
                if (payloadSize > 0) {
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
