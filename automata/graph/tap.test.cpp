#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/graph/tap.hpp"

#include <cmath>

using automata::detail::TapState;
using Catch::Approx;

TEST_CASE("integer reads return written history exactly", "[graph][tap]") {
  TapState t;
  for (int i = 0; i < 16; ++i) {
    t.write(static_cast<float>(i));
  }
  REQUIRE(t.read(0.f) == 15.f);
  REQUIRE(t.read(1.f) == 14.f);
  REQUIRE(t.read(7.f) == 8.f);
}

TEST_CASE("fractional reads reproduce a linear ramp exactly", "[graph][tap]") {
  // Hermite is exact on polynomials up to cubic; a ramp must come back as
  // the line itself, in both the Hermite region and the linear fallback.
  TapState t;
  for (int i = 0; i < 64; ++i) {
    t.write(static_cast<float>(i));
  }
  REQUIRE(t.read(2.5f) == 60.5f);
  REQUIRE(t.read(10.25f) == 52.75f);
  REQUIRE(t.read(0.25f) == 62.75f);  // linear fallback inside newest interval
}

TEST_CASE("interpolation is continuous at the fallback boundary",
          "[graph][tap]") {
  TapState t;
  for (int i = 0; i < 64; ++i) {
    t.write(std::sin(0.3f * static_cast<float>(i)));
  }
  REQUIRE(t.read(0.999f) == Approx(t.read(1.001f)).margin(2e-3f));
}
