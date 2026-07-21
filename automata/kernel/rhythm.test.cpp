#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/rhythm.hpp"

#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::ClockTrig>);
static_assert(std::is_trivially_copyable_v<automata::SwingShaper>);

TEST_CASE("a fresh trig fires the downbeat at cycle start",
          "[kernel][rhythm]") {
  automata::ClockTrig t;
  REQUIRE(t.process(0.f) == 1.f);
  REQUIRE(t.process(0.1f) == 0.f);
}

TEST_CASE("a trig created mid-cycle waits for the wrap", "[kernel][rhythm]") {
  automata::ClockTrig t;
  REQUIRE(t.process(0.6f) == 0.f);
  REQUIRE(t.process(0.8f) == 0.f);
  REQUIRE(t.process(0.0f) == 1.f);  // the wrap
  REQUIRE(t.process(0.2f) == 0.f);
}

TEST_CASE("small backward jitter is not a wrap", "[kernel][rhythm]") {
  automata::ClockTrig t;
  (void)t.process(0.5f);
  REQUIRE(t.process(0.45f) == 0.f);
}

TEST_CASE("swing splits the pair at 0.5 plus half the amount",
          "[kernel][rhythm]") {
  automata::SwingShaper s;

  // Straight: two identical sub-ramps.
  REQUIRE(s.process(0.25f, 0.f) == Approx(0.5f));
  REQUIRE(s.process(0.5f, 0.f) == Approx(0.f));
  REQUIRE(s.process(0.75f, 0.f) == Approx(0.5f));

  // amount 0.5 → the second onset lands at pair phase 0.75.
  REQUIRE(s.process(0.375f, 0.5f) == Approx(0.5f));
  REQUIRE(s.process(0.75f, 0.5f) == Approx(0.f));
  REQUIRE(s.process(0.875f, 0.5f) == Approx(0.5f));
}

TEST_CASE("swing amount is clamped", "[kernel][rhythm]") {
  automata::SwingShaper s;
  // Split pinned at 0.95 / 0.05; probe well inside each region.
  REQUIRE(s.process(0.975f, 5.f) == Approx(0.5f).margin(1e-4));
  REQUIRE(s.process(0.04f, -5.f) == Approx(0.8f).margin(1e-4));
}
