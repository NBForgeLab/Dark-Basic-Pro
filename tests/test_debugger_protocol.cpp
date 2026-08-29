#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include "../DBProCompiler/DBPDebugger/DBPDebuggerProtocol.h"

using namespace DBPDebugger;

// The compiler (CASMWriter::MakeVarDataForTransfer) and the headless
// debugger (DBPDebuggerProtocol.h) must agree on the native 64-bit variable
// table format. These tests pin that contract end-to-end.

TEST(DebuggerProtocol, VariableTableRoundTripPreserves64BitOffset) {
    std::vector<VariableEntry> in;
    in.push_back({1, 0x123456789ABCDEF0ULL, "player_x"});
    in.push_back({3, 42, "greeting"});

    std::vector<uint8_t> buf;
    SerializeVariableTable(buf, in);

    auto out = ParseVariableTable(buf.data(), buf.size());
    ASSERT_TRUE(out.has_value());
    ASSERT_EQ(out->size(), 2u);
    EXPECT_EQ((*out)[0].type, 1u);
    EXPECT_EQ((*out)[0].offset, 0x123456789ABCDEF0ULL);
    EXPECT_EQ((*out)[0].name, "player_x");
    EXPECT_EQ((*out)[1].offset, 42u);
    EXPECT_EQ((*out)[1].name, "greeting");
}

TEST(DebuggerProtocol, VariableTableRejectsWrongVersion) {
    uint8_t framed[12] = {};
    // payload = version 99 (unknown) + count 0, size field = 8
    const uint32_t payloadSize = 8;
    const uint32_t badVersion = 99;
    std::memcpy(framed, &payloadSize, 4);
    std::memcpy(framed + 4, &badVersion, 4);
    // ParseMessage expects [u32 size][payload]; feed payload only.
    EXPECT_FALSE(ParseVariableTable(framed + 4, 8));
}

TEST(DebuggerProtocol, VariableValuesRoundTripPreserves64BitStringOffset) {
    VariableValueBlob in;
    in.varSpaceSize = 4;
    in.varSpace = {0xDE, 0xAD, 0xBE, 0xEF};
    VariableValueBlob::StringEntry s;
    s.offset = 0x100000000ULL + 7; // high bit set: must not truncate
    s.length = 5;
    s.chars = "hello";
    in.strings.push_back(s);

    std::vector<uint8_t> buf;
    SerializeVariableValues(buf, in);

    auto out = ParseVariableValues(buf.data(), buf.size());
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->varSpaceSize, 4u);
    ASSERT_EQ(out->varSpace.size(), 4u);
    ASSERT_EQ(out->strings.size(), 1u);
    EXPECT_EQ(out->strings[0].offset, 0x100000007ULL);
    EXPECT_EQ(out->strings[0].chars, "hello");
}

TEST(DebuggerProtocol, EndToEndThroughParserWithFraming) {
    // Mimic CDebuggerInterface::SendDataToDebugger framing: [u32 size][payload]
    std::vector<VariableEntry> vars;
    vars.push_back({2, 0xABULL, "score"});

    std::vector<uint8_t> payload;
    SerializeVariableTable(payload, vars);

    std::vector<uint8_t> framed;
    uint32_t sz = static_cast<uint32_t>(payload.size());
    uint8_t sizeHdr[4];
    std::memcpy(sizeHdr, &sz, 4);
    framed.insert(framed.end(), sizeHdr, sizeHdr + 4);
    framed.insert(framed.end(), payload.begin(), payload.end());

    ProtocolParser parser;
    auto evt = parser.ParseMessage(static_cast<int>(MessageType::VariableTable),
                                    framed.data(), framed.size());
    ASSERT_TRUE(evt.has_value());
    EXPECT_EQ(evt->type, MessageType::VariableTable);
    ASSERT_EQ(evt->variables.size(), 1u);
    EXPECT_EQ(evt->variables[0].name, "score");
    EXPECT_EQ(evt->variables[0].offset, 0xABULL);
}
