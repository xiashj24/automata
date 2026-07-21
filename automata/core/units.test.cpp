#include <catch2/catch_test_macros.hpp>

#include "automata/core/units.hpp"

using namespace automata;

static_assert(440_hz == 440.f);
static_assert(432.5_hz == 432.5f);
static_assert(120_bpm == 120.f);
static_assert(2_s == 2.f);
static_assert(250_ms == 0.25f);  // milliseconds convert to seconds
static_assert(0.5_ms == 0.0005f);
static_assert(-6_db == -6.f);  // decibels stay decibels

TEST_CASE("unit literals annotate plain floats", "[core][units]") {
  CHECK(440_hz == 440.f);
  CHECK(10_ms == 0.01f);
}
