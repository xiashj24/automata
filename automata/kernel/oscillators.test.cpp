#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/oscillators.hpp"

#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Phasor>);
static_assert(std::is_trivially_copyable_v<automata::SineShaper>);
static_assert(std::is_trivially_copyable_v<automata::SawShaper>);
static_assert(std::is_trivially_copyable_v<automata::Metro>);

TEST_CASE("phasor ramps and wraps", "[kernel][oscillators]") {
  automata::Phasor p;
  p.set_freq(0.25f);

  REQUIRE(p.process() == Approx(0.f));
  REQUIRE(p.process() == Approx(0.25f));
  REQUIRE(p.process() == Approx(0.5f));
  REQUIRE(p.process() == Approx(0.75f));
  REQUIRE(p.process() == Approx(0.f));  // wrapped
}

TEST_CASE("sine shaper maps phase to a unit sine", "[kernel][oscillators]") {
  automata::SineShaper s;

  REQUIRE(s.process(0.f) == Approx(0.f).margin(1e-6));
  REQUIRE(s.process(0.25f) == Approx(1.f).margin(1e-6));
  REQUIRE(s.process(0.5f) == Approx(0.f).margin(1e-6));
  REQUIRE(s.process(0.75f) == Approx(-1.f).margin(1e-6));
}

TEST_CASE("saw shaper maps phase to a bipolar ramp", "[kernel][oscillators]") {
  automata::SawShaper s;

  REQUIRE(s.process(0.f) == Approx(-1.f));
  REQUIRE(s.process(0.25f) == Approx(-0.5f));
  REQUIRE(s.process(0.5f) == Approx(0.f));
  REQUIRE(s.process(0.75f) == Approx(0.5f));
}

TEST_CASE("metro fires on its first sample, then once per period",
          "[kernel][oscillators]") {
  automata::Metro m;
  m.set_freq(0.1f);  // period of 10 samples

  REQUIRE(m.process() == 1.f);
  for (int i = 1; i < 10; ++i) {
    REQUIRE(m.process() == 0.f);
  }
  REQUIRE(m.process() == 1.f);
}

TEST_CASE("reset returns an oscillator to its initial state",
          "[kernel][oscillators]") {
  automata::Phasor p;
  p.set_freq(0.3f);
  (void)p.process();
  (void)p.process();

  p.reset();

  REQUIRE(p.process() == Approx(0.f));
}
