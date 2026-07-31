// test_ubsan_detection.cpp — UBSan characterization probes
//
// These tests deliberately exercise patterns that constitute undefined
// behavior in C/C++.  When compiled with a compiler that supports UBSan
// (Clang, GCC, or a future MSVC version) and UBSAN_OPTIONS includes
// abort_on_error=1, the runtime aborts the process → CTest reports FAILURE.
//
// When the compiler does NOT support UBSan (current MSVC), the tests
// detect the absence and GTEST_SKIP() gracefully.

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

// ---------------------------------------------------------------------------
// Runtime detection: is UBSan actually active?
//
// __has_feature(undefined_behavior_sanitizer) is defined by Clang and GCC 14+.
// MSVC does not define it.  We also check the UBSAN_OPTIONS env variable as
// a secondary signal.
// ---------------------------------------------------------------------------
static bool ubsan_is_active() {
#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
    return true;
#endif
#endif
    // Fallback: if UBSAN_OPTIONS is set and non-empty, assume UBSan is active.
    // This handles compilers that don't expose __has_feature but do ship UBSan.
    const char* opts = std::getenv("UBSAN_OPTIONS");
    return opts != nullptr && opts[0] != '\0';
}

// ---------------------------------------------------------------------------
// 1. Signed integer overflow  (UB: [expr.add]/4)
//    x86 ADD wraps silently → test passes without UBSan.
//    UBSan inserts a check → abort on overflow.
// ---------------------------------------------------------------------------
TEST(UBSanDetectionTest, SignedIntegerOverflowIsDetected) {
    if (!ubsan_is_active()) {
        GTEST_SKIP() << "UBSan not active — compiler does not instrument UB";
    }
    volatile int32_t a = std::numeric_limits<int32_t>::max();
    volatile int32_t b = 1;
    // The addition overflows INT32_MAX → undefined behavior.
    volatile int32_t result = a + b;
    // Should never reach here under UBSan with abort_on_error=1.
    (void)result;
}

// ---------------------------------------------------------------------------
// 2. Shift exponent too large  (UB: [expr.shift]/1)
//    x86 SHL masks the count to 5 bits → shift by 32 becomes shift by 0.
//    UBSan catches the out-of-range exponent.
// ---------------------------------------------------------------------------
TEST(UBSanDetectionTest, ShiftExponentOverflowIsDetected) {
    if (!ubsan_is_active()) {
        GTEST_SKIP() << "UBSan not active — compiler does not instrument UB";
    }
    volatile int32_t base = 1;
    volatile unsigned int shift = 32; // equal to bit-width → UB
    volatile int32_t result = base << shift;
    (void)result;
}

// ---------------------------------------------------------------------------
// 3. Signed integer multiplication overflow  (UB: [expr.mul]/4)
// ---------------------------------------------------------------------------
TEST(UBSanDetectionTest, SignedMultiplicationOverflowIsDetected) {
    if (!ubsan_is_active()) {
        GTEST_SKIP() << "UBSan not active — compiler does not instrument UB";
    }
    volatile int32_t a = 100000;
    volatile int32_t b = 100000;
    // 100000 * 100000 = 10^10 which exceeds INT32_MAX
    volatile int32_t result = a * b;
    (void)result;
}

// ---------------------------------------------------------------------------
// 4. Negation overflow  (UB: [expr.unary.op]/8)
//    Negating INT32_MIN overflows because +INT32_MIN is not representable.
// ---------------------------------------------------------------------------
TEST(UBSanDetectionTest, NegationOverflowIsDetected) {
    if (!ubsan_is_active()) {
        GTEST_SKIP() << "UBSan not active — compiler does not instrument UB";
    }
    volatile int32_t v = std::numeric_limits<int32_t>::min();
    volatile int32_t neg = -v; // UB
    (void)neg;
}

// ---------------------------------------------------------------------------
// 5. Misaligned pointer access  (UB: [basic.align])
//    We create a pointer with guaranteed bad alignment via offset.
//    UBSan's alignment checker flags this.
// ---------------------------------------------------------------------------
TEST(UBSanDetectionTest, MisalignedAccessIsDetected) {
    if (!ubsan_is_active()) {
        GTEST_SKIP() << "UBSan not active — compiler does not instrument UB";
    }
    alignas(16) char buffer[64] = {};
    // Offset by 1 byte → not 4-byte aligned.
    char* misaligned = buffer + 1;
    volatile int* ip = reinterpret_cast<int*>(misaligned);
    volatile int val = *ip;
    (void)val;
}

// ---------------------------------------------------------------------------
// Minimal main — no CrashHandler / diagnostic-overrides so that UBSan's
// SIGABRT propagates to CTest as a non-zero exit code.
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
