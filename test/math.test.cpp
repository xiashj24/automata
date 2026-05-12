#include <math.hpp>

#include <catch2/catch_test_macros.hpp>

using automata::gcd;
using automata::lcm;

// Remaining Test List
// interleave, concatenation, etc.

TEST_CASE("gcd of two equal numbers", "[math][gcd]") {
  STATIC_REQUIRE(gcd(6, 6) == 6);
}

TEST_CASE("gcd basic cases", "[math][gcd]") {
  STATIC_REQUIRE(gcd(12, 8) == 4);
  STATIC_REQUIRE(gcd(8, 12) == 4);
  STATIC_REQUIRE(gcd(17, 5) == 1);
  STATIC_REQUIRE(gcd(100, 75) == 25);
}

TEST_CASE("gcd with zero", "[math][gcd]") {
  STATIC_REQUIRE(gcd(0, 7) == 7);
  STATIC_REQUIRE(gcd(7, 0) == 7);
  STATIC_REQUIRE(gcd(0, 0) == 0);
}

TEST_CASE("gcd with one", "[math][gcd]") {
  STATIC_REQUIRE(gcd(1, 99) == 1);
  STATIC_REQUIRE(gcd(99, 1) == 1);
}

TEST_CASE("lcm basic cases", "[math][lcm]") {
  STATIC_REQUIRE(lcm(4, 6) == 12);
  STATIC_REQUIRE(lcm(6, 4) == 12);
  STATIC_REQUIRE(lcm(3, 5) == 15);
  STATIC_REQUIRE(lcm(12, 8) == 24);
}

TEST_CASE("lcm of two equal numbers", "[math][lcm]") {
  STATIC_REQUIRE(lcm(7, 7) == 7);
}

TEST_CASE("lcm with zero", "[math][lcm]") {
  STATIC_REQUIRE(lcm(0, 5) == 0);
  STATIC_REQUIRE(lcm(5, 0) == 0);
  STATIC_REQUIRE(lcm(0, 0) == 0);
}

TEST_CASE("lcm with one", "[math][lcm]") {
  STATIC_REQUIRE(lcm(1, 13) == 13);
  STATIC_REQUIRE(lcm(13, 1) == 13);
}

TEST_CASE("lcm of coprimes is their product", "[math][lcm]") {
  STATIC_REQUIRE(lcm(7, 11) == 77);
  STATIC_REQUIRE(lcm(4, 9) == 36);
}
