#include <gtest/gtest.h>
#include "DBPDebuggerProtocol.h"

TEST(DBPDebuggerTest, DecodeStepEventMessage) {
    // Shared memory layout: uint32_t size followed by payload
    struct StepPayload {
        uint32_t line;
        uint32_t fileIndex;
    } stepData{ 42, 1 };

    std::vector<uint8_t> buffer(sizeof(uint32_t) + sizeof(StepPayload));
    uint32_t dataSize = sizeof(StepPayload);
    std::memcpy(buffer.data(), &dataSize, sizeof(uint32_t));
    std::memcpy(buffer.data() + 4, &stepData, sizeof(StepPayload));

    DBPDebugger::ProtocolParser parser;
    auto eventOpt = parser.ParseMessage(11, buffer.data(), buffer.size());

    ASSERT_TRUE(eventOpt.has_value());
    EXPECT_EQ(eventOpt->type, DBPDebugger::MessageType::StepHit);
    EXPECT_EQ(eventOpt->line, 42u);
    EXPECT_EQ(eventOpt->fileIndex, 1u);
}

TEST(DBPDebuggerTest, DecodeDiagnosticConsoleMessage) {
    std::string text = "Runtime Error 100: Array out of bounds";
    uint32_t dataSize = static_cast<uint32_t>(text.size());

    std::vector<uint8_t> buffer(sizeof(uint32_t) + dataSize);
    std::memcpy(buffer.data(), &dataSize, sizeof(uint32_t));
    std::memcpy(buffer.data() + 4, text.data(), dataSize);

    DBPDebugger::ProtocolParser parser;
    auto eventOpt = parser.ParseMessage(31, buffer.data(), buffer.size());

    ASSERT_TRUE(eventOpt.has_value());
    EXPECT_EQ(eventOpt->type, DBPDebugger::MessageType::RuntimeError);
    EXPECT_EQ(eventOpt->message, text);
}

TEST(DBPDebuggerTest, FormatHeadlessCLILogOutput) {
    DBPDebugger::DebugEvent evt;
    evt.type = DBPDebugger::MessageType::RuntimeError;
    evt.message = "Array Index Out of Range";

    std::string formatted = DBPDebugger::FormatEventForCLI(evt);
    EXPECT_NE(formatted.find("[HEADLESS DEBUGGER] [RUNTIME ERROR] Array Index Out of Range"), std::string::npos);
}

TEST(DBPDebuggerTest, RejectMalformedBuffer) {
    std::vector<uint8_t> shortBuffer = { 0x05, 0x00 }; // Buffer smaller than header (4 bytes)
    DBPDebugger::ProtocolParser parser;
    auto eventOpt = parser.ParseMessage(11, shortBuffer.data(), shortBuffer.size());

    EXPECT_FALSE(eventOpt.has_value());
}
