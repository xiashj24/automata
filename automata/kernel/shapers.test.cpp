#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/shapers.hpp"

#include <cmath>

using Catch::Approx;
using namespace automata;

TEST_CASE("sine and saw map phase to their waveforms", "[kernel][shapers]") {
  REQUIRE(sine_from_phase(0.f) == Approx(0.f).margin(1e-6));
  REQUIRE(sine_from_phase(0.25f) == Approx(1.f).margin(1e-6));
  REQUIRE(sine_from_phase(0.75f) == Approx(-1.f).margin(1e-6));

  REQUIRE(saw_from_phase(0.f) == Approx(-1.f));
  REQUIRE(saw_from_phase(0.5f) == Approx(0.f));
  REQUIRE(saw_from_phase(0.75f) == Approx(0.5f));
}

TEST_CASE("naive triangle traces its corners and wraps", "[kernel][shapers]") {
  REQUIRE(tri_from_phase(0.f) == Approx(1.f));
  REQUIRE(tri_from_phase(0.25f) == Approx(0.f));
  REQUIRE(tri_from_phase(0.5f) == Approx(-1.f));
  REQUIRE(tri_from_phase(0.75f) == Approx(0.f));
  REQUIRE(tri_from_phase(1.25f) == Approx(tri_from_phase(0.25f)));
  REQUIRE(tri_from_phase(-0.25f) == Approx(tri_from_phase(0.75f)));
}

TEST_CASE(
    "antialiased triangle matches the naive shape between corners and "
    "rounds the corners",
    "[kernel][shapers]") {
  const float dt = 0.01f;
  REQUIRE(tri_from_phase_aa(0.25f, dt) == Approx(tri_from_phase(0.25f)));
  REQUIRE(tri_from_phase_aa(0.6f, dt) == Approx(tri_from_phase(0.6f)));

  // The polyBLAMP pulls the peak and valley off the rails by 4/3 dt.
  REQUIRE(tri_from_phase_aa(0.f, dt) == Approx(1.f - 4.f / 3.f * dt));
  REQUIRE(tri_from_phase_aa(0.5f, dt) == Approx(-1.f + 4.f / 3.f * dt));

  for (float p = 0.f; p < 1.f; p += 0.001f) {
    REQUIRE(std::abs(tri_from_phase_aa(p, dt)) <= 1.f);
  }
}

TEST_CASE("swing splits the pair at 0.5 plus half the amount",
          "[kernel][shapers]") {
  // Straight: two identical sub-ramps.
  REQUIRE(swing_shape(0.25f, 0.f) == Approx(0.5f));
  REQUIRE(swing_shape(0.5f, 0.f) == Approx(0.f));
  REQUIRE(swing_shape(0.75f, 0.f) == Approx(0.5f));

  // amount 0.5 → the second onset lands at pair phase 0.75.
  REQUIRE(swing_shape(0.375f, 0.5f) == Approx(0.5f));
  REQUIRE(swing_shape(0.75f, 0.5f) == Approx(0.f));
  REQUIRE(swing_shape(0.875f, 0.5f) == Approx(0.5f));
}

TEST_CASE("swing amount is clamped", "[kernel][shapers]") {
  // Split pinned at 0.95 / 0.05; probe well inside each region.
  REQUIRE(swing_shape(0.975f, 5.f) == Approx(0.5f).margin(1e-4));
  REQUIRE(swing_shape(0.04f, -5.f) == Approx(0.8f).margin(1e-4));
}

TEST_CASE("exp_curve pins its endpoints and bows by k", "[kernel][shapers]") {
  REQUIRE(exp_curve(0.f, 2.f) == Approx(0.f));
  REQUIRE(exp_curve(1.f, 2.f) == Approx(1.f));
  REQUIRE(exp_curve(0.5f, 2.f) < 0.5f);   // k > 0 bows below the identity
  REQUIRE(exp_curve(0.5f, -2.f) > 0.5f);  // k < 0 above
  REQUIRE(exp_curve(0.3f, 0.f) == Approx(0.3f));  // k -> 0 is the identity
}
