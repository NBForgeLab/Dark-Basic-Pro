#include <gtest/gtest.h>

#include "../DBProCompiler/DBPCompiler/ReferenceTracker.h"

#include <cstdint>
#include <string>

namespace
{
void ExpectParsed(
    const std::string_view source,
    const ReferenceKind expectedKind,
    const std::uint32_t expectedIndex,
    const std::string_view expectedSymbol = {},
    const bool expectedArray = false,
    const std::uint32_t expectedMemoryOffset = 0)
{
    const auto parsed = ParseReferenceLabel(source);
    ASSERT_TRUE(parsed.has_value()) << source;
    EXPECT_EQ(parsed->kind, expectedKind);
    EXPECT_EQ(parsed->index, expectedIndex);
    EXPECT_EQ(parsed->symbol, expectedSymbol);
    EXPECT_EQ(parsed->isArray, expectedArray);
    EXPECT_EQ(parsed->memoryOffset, expectedMemoryOffset);
}
} // namespace

TEST(ReferenceTrackerTest, InitialStateIsEmpty)
{
    CReferenceTracker tracker;
    EXPECT_EQ(tracker.GetRefPointer(), 0u);
    EXPECT_GE(tracker.GetRefBufferSize(), 1024u);
}

TEST(ReferenceTrackerTest, OwnsReferenceLabelStorage)
{
    CReferenceTracker tracker;
    std::string label = "$labelend";

    tracker.AddReference(0x1000u, label);
    label.assign("overwritten");

    ASSERT_NE(tracker.GetRefLabel(0), nullptr);
    EXPECT_EQ(tracker.GetRef(0), 0x1000u);
    EXPECT_EQ(*tracker.GetRefLabel(0), "$labelend");
}

TEST(ReferenceTrackerTest, ReplacesLeapMarkerLabelWithoutRawPointerOwnership)
{
    CReferenceTracker tracker;
    tracker.AddReference(12u, "$labelpending");

    EXPECT_TRUE(tracker.SetRefLabel(0, "37"));
    ASSERT_NE(tracker.GetRefLabel(0), nullptr);
    EXPECT_EQ(*tracker.GetRefLabel(0), "37");
    EXPECT_FALSE(tracker.SetRefLabel(1, "invalid"));
}

TEST(ReferenceTrackerTest, GrowsBeyondInitialReservation)
{
    CReferenceTracker tracker;
    const auto initialCapacity = tracker.GetRefBufferSize();

    for (std::uint32_t index = 0; index < initialCapacity + 1u; ++index)
    {
        tracker.AddReference(index, "0");
    }

    EXPECT_GT(tracker.GetRefBufferSize(), initialCapacity);
    EXPECT_EQ(tracker.GetRefPointer(), initialCapacity + 1u);
}

TEST(ReferenceTrackerTest, ResetReleasesEntries)
{
    CReferenceTracker tracker;
    tracker.AddReference(10u, "20");
    tracker.AddReference(30u, "40");

    tracker.Reset();

    EXPECT_EQ(tracker.GetRefPointer(), 0u);
    EXPECT_EQ(tracker.GetRefLabel(0), nullptr);
}

TEST(ReferenceTrackerTest, BoundsCheckingReturnsNoEntry)
{
    CReferenceTracker tracker;
    tracker.AddReference(100u, "200");

    EXPECT_EQ(tracker.GetRef(0), 100u);
    EXPECT_EQ(tracker.GetRef(9999), 0u);
    EXPECT_EQ(tracker.GetRefLabel(9999), nullptr);
}

TEST(ReferenceLabelParserTest, ParsesSerializedIndexesAndSignedImmediates)
{
    ExpectParsed("[1", ReferenceKind::Command, 0u);
    ExpectParsed("$$2", ReferenceKind::StringLiteral, 1u);
    ExpectParsed("42", ReferenceKind::Immediate, 42u);
    ExpectParsed("-1", ReferenceKind::Immediate, 0xFFFFFFFFu);
}

TEST(ReferenceLabelParserTest, ParsesVariableForms)
{
    ExpectParsed("@score", ReferenceKind::Variable, 0u, "score");
    ExpectParsed("@&items", ReferenceKind::Variable, 0u, "&items", true);
    ExpectParsed("+@record", ReferenceKind::Variable, 0u, "record", false, 4u);
}

TEST(ReferenceLabelParserTest, NormalizesCodeAndDataLabels)
{
    ExpectParsed("$labelend", ReferenceKind::CodeLabel, 0u, "$labelend");
    ExpectParsed("$labelloop@scope", ReferenceKind::CodeLabel, 0u, "$labelloop");
    ExpectParsed("$dabelitems@scope", ReferenceKind::DataLabel, 0u, "$labelitems");
}

TEST(ReferenceLabelParserTest, RejectsMalformedOrOutOfRangeReferences)
{
    EXPECT_FALSE(ParseReferenceLabel("").has_value());
    EXPECT_FALSE(ParseReferenceLabel("[0").has_value());
    EXPECT_FALSE(ParseReferenceLabel("$$0").has_value());
    EXPECT_FALSE(ParseReferenceLabel("@").has_value());
    EXPECT_FALSE(ParseReferenceLabel("not-a-number").has_value());
    EXPECT_FALSE(ParseReferenceLabel("4294967296").has_value());
    EXPECT_FALSE(ParseReferenceLabel("-2147483649").has_value());
}
