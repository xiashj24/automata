#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/smooth.hpp"

#include <cmath>
#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Smooth>);

TEST_CASE("smooth converges exponentially on its tau", "[kernel][smooth]") {
  automata::Smooth s;
  s.set_tau(100.f / automata::SampleRateF);  // tau of 100 samples

  float out = 0.f;
  for (int i = 0; i < 100; ++i) {
    out = s.process(1.f);
  }
  // A one-pole step response reaches 1 - 1/e after tau samples.
  REQUIRE(out == Approx(1.f - std::exp(-1.f)).margin(1e-3));
}

TEST_CASE("smooth with tau <= 0 passes through", "[kernel][smooth]") {
  automata::Smooth s;
  s.set_tau(0.f);
  REQUIRE(s.process(0.7f) == Approx(0.7f));
  REQUIRE(s.process(-0.2f) == Approx(-0.2f));
}
