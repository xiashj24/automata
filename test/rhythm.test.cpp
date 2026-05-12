#include <catch2/catch_test_macros.hpp>
#include <rhythm.hpp>

using automata::Rhythm;
using automata::interleave;

TEST_CASE("string constructor", "[rhythm]") {
  constexpr Rhythm<8> rhy("x.x.x.x.");
  REQUIRE(rhy.get(0));
  REQUIRE_FALSE(rhy.get(1));
  REQUIRE(rhy.get(2));
  REQUIRE_FALSE(rhy.get(3));
}

TEST_CASE("integer constructor", "[rhythm]") {
  constexpr Rhythm<4> rhy(0b1010u);
  REQUIRE(rhy.get(1));
  REQUIRE(rhy.get(3));
  REQUIRE_FALSE(rhy.get(0));
  REQUIRE_FALSE(rhy.get(2));
}

TEST_CASE("boolean ops", "[rhythm]") {
  constexpr Rhythm<4> aa("x.x.");
  constexpr Rhythm<4> bb("xx..");

  REQUIRE((aa & bb).get(0));
  REQUIRE_FALSE((aa & bb).get(1));

  REQUIRE((aa | bb).get(0));
  REQUIRE((aa | bb).get(1));
  REQUIRE((aa | bb).get(2));
  REQUIRE_FALSE((aa | bb).get(3));

  REQUIRE_FALSE((~aa).get(0));
  REQUIRE((~aa).get(1));

  // aa - bb: hits in aa but not in bb
  REQUIRE_FALSE((aa - bb).get(0));
  REQUIRE((aa - bb).get(2));

  REQUIRE_FALSE((aa ^ bb).get(0));
  REQUIRE((aa ^ bb).get(1));
  REQUIRE((aa ^ bb).get(2));
}

TEST_CASE("rotation", "[rhythm]") {
  constexpr Rhythm<4> rhy("x...");

  // shift left by 1 moves step 0 → step 1
  REQUIRE((rhy << 1).get(1));
  REQUIRE_FALSE((rhy << 1).get(0));

  // shift right by 1 wraps step 0 → step 3
  REQUIRE((rhy >> 1).get(3));
  REQUIRE_FALSE((rhy >> 1).get(0));
}

TEST_CASE("concatenation", "[rhythm]") {
  constexpr Rhythm<2> aa("x.");
  constexpr Rhythm<2> bb(".x");
  constexpr auto cat = aa + bb;

  static_assert(cat.steps() == 4);
  REQUIRE(cat.get(0));
  REQUIRE_FALSE(cat.get(1));
  REQUIRE_FALSE(cat.get(2));
  REQUIRE(cat.get(3));
}

TEST_CASE("interleave", "[rhythm]") {
  constexpr Rhythm<2> aa("x.");
  constexpr Rhythm<2> bb(".x");
  constexpr auto zipped = interleave(aa, bb);

  static_assert(zipped.steps() == 4);
  // result[j*2 + chan]: aa[0]=x at pos 0, bb[0]=. at pos 1, aa[1]=. at pos 2, bb[1]=x at pos 3
  REQUIRE(zipped.get(0));
  REQUIRE_FALSE(zipped.get(1));
  REQUIRE_FALSE(zipped.get(2));
  REQUIRE(zipped.get(3));
}

TEST_CASE("dac", "[rhythm]") {
  constexpr Rhythm<4> rhy("x.x.");
  constexpr auto val = rhy.dac<4>();
  // bit 0 = step 0 (hit), bit 1 = step 1 (rest), bit 2 = step 2 (hit), bit 3 = step 3 (rest)
  REQUIRE(val == 0b0101u);
}
