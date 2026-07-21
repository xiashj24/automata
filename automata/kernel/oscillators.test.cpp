#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/oscillators.hpp"

#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Phasor>);
static_assert(std::is_trivially_copyable_v<automata::Metro>);

TEST_CASE("phasor ramps and wraps", "[kernel][oscillators]") {
  automata::Phasor p;
  p.set_freq(12000.f);  // 0.25 cycles per sample

  REQUIRE(p.process() == Approx(0.f));
  REQUIRE(p.process() == Approx(0.25f));
  REQUIRE(p.process() == Approx(0.5f));
  REQUIRE(p.process() == Approx(0.75f));
  REQUIRE(p.process() == Approx(0.f));  // wrapped
}

TEST_CASE("metro fires on its first sample, then once per period",
          "[kernel][oscillators]") {
  automata::Metro m;
  m.set_freq(4800.f);  // period of 10 samples

  REQUIRE(m.process() == 1.f);
  for (int i = 1; i < 10; ++i) {
    REQUIRE(m.process() == 0.f);
  }
  REQUIRE(m.process() == 1.f);
}

TEST_CASE("reset returns an oscillator to its initial state",
          "[kernel][oscillators]") {
  automata::Phasor p;
  p.set_freq(14400.f);
  (void)p.process();
  (void)p.process();

  p.reset();

  REQUIRE(p.process() == Approx(0.f));
}
