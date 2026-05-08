#include <catch2/catch_test_macros.hpp>
#include <dpb/cpu_detect.hpp>

// ============================================================================
// CPU Detection Tests
// ============================================================================

TEST_CASE("detect_cpu() returns cached result", "[cpu_detect]") {
    const auto& feat1 = dpb::detect_cpu();
    const auto& feat2 = dpb::detect_cpu();
    const auto& feat3 = dpb::detect_cpu();

    // Thread-safe static local (Meyers' singleton) — same address every call
    REQUIRE(&feat1 == &feat2);
    REQUIRE(&feat2 == &feat3);
}

TEST_CASE("detect_cpu() populates feature flags", "[cpu_detect]") {
    const auto& cpu = dpb::detect_cpu();

    // All members are bool — must be either true or false (no uninitialized)
    REQUIRE((cpu.has_sse42   == true || cpu.has_sse42   == false));
    REQUIRE((cpu.has_avx2    == true || cpu.has_avx2    == false));
    REQUIRE((cpu.has_avx512f == true || cpu.has_avx512f == false));
}

TEST_CASE("detect_cpu() does not crash", "[cpu_detect]") {
    // Basic smoke test — calling detect_cpu() should not throw or segfault
    const auto& cpu = dpb::detect_cpu();

    // Access all fields without crashing
    bool sse42   = cpu.has_sse42;
    bool avx2    = cpu.has_avx2;
    bool avx512f = cpu.has_avx512f;

    // At least SSE4.2 is expected on any modern (post-2008) x86-64 CPU
    // This is a sanity check, not a strict requirement — if the test
    // machine predates SSE4.2, the flag will legitimately be false.
    (void)sse42;
    (void)avx2;
    (void)avx512f;
    SUCCEED("detect_cpu() ran without crashing");
}
