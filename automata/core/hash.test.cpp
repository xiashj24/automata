#include <catch2/catch_test_macros.hpp>

#include "automata/core/hash.hpp"

#include <array>
#include <cstddef>

using automata::Hash;
using automata::HashSeed;

// Structural identity is usable at compile time (stable type-name hashing).
static_assert(automata::hash_string("svf") != automata::hash_string("saw"));
static_assert(automata::hash_value(1.0f) != automata::hash_value(-1.0f));

TEST_CASE("hash_string matches published FNV-1a 64-bit vectors",
          "[core][hash]") {
  REQUIRE(automata::hash_string("") == HashSeed);
  REQUIRE(automata::hash_string("a") == 0xaf63dc4c8601ec8cull);
  REQUIRE(automata::hash_string("foobar") == 0x85944171f73967e8ull);
}

TEST_CASE("hash_bytes agrees with hash_string on the same bytes",
          "[core][hash]") {
  constexpr std::array<std::byte, 3> bytes{std::byte{'a'}, std::byte{'b'},
                                           std::byte{'c'}};
  REQUIRE(automata::hash_bytes(bytes) == automata::hash_string("abc"));
}

TEST_CASE("seeding chains hashes incrementally", "[core][hash]") {
  const Hash prefix = automata::hash_string("a");
  REQUIRE(automata::hash_string("b", prefix) == automata::hash_string("ab"));
  REQUIRE(automata::hash_string("ab") == 0x089c4407b545986aull);
}

TEST_CASE("hash_combine is order-sensitive", "[core][hash]") {
  const Hash a = automata::hash_string("saw");
  const Hash b = automata::hash_string("sine");
  REQUIRE(automata::hash_combine(a, b) != automata::hash_combine(b, a));
}

TEST_CASE("hash_value hashes the bit pattern of scalars", "[core][hash]") {
  REQUIRE(automata::hash_value(440.0f) != automata::hash_value(441.0f));
  REQUIRE(automata::hash_value(1) != automata::hash_value(1u << 16));

  // Bit-pattern hashing distinguishes signed zero: a config edit from 0.f
  // to -0.f is a structural change, documented rather than special-cased.
  REQUIRE(automata::hash_value(0.0f) != automata::hash_value(-0.0f));

  enum class Interp { None, Linear };
  REQUIRE(automata::hash_value(Interp::None) !=
          automata::hash_value(Interp::Linear));
}
