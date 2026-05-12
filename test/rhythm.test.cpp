#include <cstddef>

#include <rhythm.hpp>
#include <math.hpp>

#include <catch2/catch_test_macros.hpp>

using automata::lcm;
using automata::next_permutation;
using automata::Rhythm;
// using automata::interleave;

TEST_CASE("noneness", "[rhythm]") {
  STATIC_REQUIRE(Rhythm<8>{}.none());
  STATIC_REQUIRE_FALSE(Rhythm<8>{"x......."}.none());
  STATIC_REQUIRE_FALSE(Rhythm<8>{"x.x.x.x."}.none());
}

TEST_CASE("default constructed rhythm is none", "[rhythm]") {
  constexpr Rhythm<4> beat;
  STATIC_REQUIRE(beat.none());
  STATIC_REQUIRE_FALSE(beat[0]);
  STATIC_REQUIRE_FALSE(beat[1]);
  STATIC_REQUIRE_FALSE(beat[2]);
  STATIC_REQUIRE_FALSE(beat[3]);
}

TEST_CASE("rhythm length", "[rhythm]") {
  constexpr Rhythm<1> beat_1;
  constexpr Rhythm<2> beat_2;
  constexpr Rhythm<8> beat_8;
  STATIC_REQUIRE(beat_1.length() == 1);
  STATIC_REQUIRE(beat_2.length() == 2);
  STATIC_REQUIRE(beat_8.length() == 8);
}

TEST_CASE("string constructor", "[rhythm]") {
  constexpr Rhythm<8> beat{"x.x.x.x."};
  STATIC_REQUIRE(beat[0]);
  STATIC_REQUIRE(beat[2]);
  STATIC_REQUIRE(beat[4]);
  STATIC_REQUIRE(beat[6]);
  STATIC_REQUIRE_FALSE(beat[1]);
  STATIC_REQUIRE_FALSE(beat[3]);
  STATIC_REQUIRE_FALSE(beat[5]);
  STATIC_REQUIRE_FALSE(beat[7]);
}

TEST_CASE("integer constructor", "[rhythm]") {
  constexpr Rhythm<4> beat(0b1010);
  STATIC_REQUIRE(beat[1]);
  STATIC_REQUIRE(beat[3]);
  STATIC_REQUIRE_FALSE(beat[0]);
  STATIC_REQUIRE_FALSE(beat[2]);
}

TEST_CASE("equality", "[rhythm]") {
  constexpr Rhythm<8> beat_1{"x.x.x.x."};
  constexpr Rhythm<8> beat_2{0b01010101};
  STATIC_REQUIRE(beat_1 == beat_2);
}

TEST_CASE("zero padding", "[rhythm]") {
  constexpr Rhythm<4> short_beat{"x.x."};
  constexpr Rhythm<8> long_beat = short_beat;
  STATIC_REQUIRE(long_beat == Rhythm{"x.x....."});
}

TEST_CASE("truncation", "[rhythm]") {
  constexpr Rhythm<8> long_beat{"x.x.x.x."};
  constexpr Rhythm<4> short_beat = long_beat;
  STATIC_REQUIRE(short_beat == Rhythm{"x.x."});
}

TEST_CASE("tiling", "[rhythm]") {
  constexpr Rhythm<4> short_beat{"x.x."};
  constexpr auto long_beat = short_beat.tile<8>();
  STATIC_REQUIRE(long_beat == Rhythm{"x.x.x.x."});
}

TEST_CASE("binary operator of same length)", "[rhythm]") {
  constexpr Rhythm<4> lhs("x.x.");
  constexpr Rhythm<4> rhs("xx..");
  constexpr auto result = lhs & rhs;
  STATIC_REQUIRE(result.length() == 4);
  STATIC_REQUIRE(result == Rhythm{"x..."});
}

TEST_CASE("binary operator of different length auto expand to lcm",
          "[rhythm]") {
  constexpr Rhythm<4> lhs("x.x.");
  constexpr Rhythm<9> rhs = Rhythm{"x..."}.tile<9>();
  constexpr auto result = lhs & rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 9));
}

TEST_CASE("operator| different length", "[rhythm]") {
  constexpr Rhythm<4> lhs{"x..."};
  constexpr Rhythm<6> rhs{"...x.."};
  constexpr auto result = lhs | rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 6));
  STATIC_REQUIRE(result[0]);
  STATIC_REQUIRE(result[3]);
  STATIC_REQUIRE(result[4]);
  STATIC_REQUIRE(result[9]);
  STATIC_REQUIRE_FALSE(result[1]);
  STATIC_REQUIRE_FALSE(result[2]);
}

TEST_CASE("operator^ different length expands to lcm", "[rhythm]") {
  constexpr Rhythm<4> lhs{"x.x."};
  constexpr Rhythm<6> rhs{"x.x.x."};
  constexpr auto result = lhs ^ rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 6));
}

TEST_CASE("operator- different length expands to lcm", "[rhythm]") {
  constexpr Rhythm<4> lhs{"xxxx"};
  constexpr Rhythm<6> rhs{"x.x.x."};
  constexpr auto result = lhs - rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 6));
  STATIC_REQUIRE_FALSE(result[0]);
  STATIC_REQUIRE(result[1]);
  STATIC_REQUIRE_FALSE(result[2]);
  STATIC_REQUIRE(result[3]);
}

TEST_CASE("rotation", "[rhythm]") {
  constexpr Rhythm<4> beat("x...");

  constexpr auto left_1 = beat << 1;
  STATIC_REQUIRE(left_1 == Rhythm{"...x"});
  constexpr auto left_2 = beat << 2;
  STATIC_REQUIRE(left_2 == Rhythm{"..x."});
  constexpr auto left_3 = beat << 3;
  STATIC_REQUIRE(left_3 == Rhythm{".x.."});
  constexpr auto left_4 = beat << 4;
  STATIC_REQUIRE(left_4 == beat);

  constexpr auto right_1 = beat >> 1;
  STATIC_REQUIRE(right_1 == Rhythm{".x.."});
  constexpr auto right_2 = beat >> 2;
  STATIC_REQUIRE(right_2 == Rhythm{"..x."});
  constexpr auto right_3 = beat >> 3;
  STATIC_REQUIRE(right_3 == Rhythm{"...x"});
  constexpr auto right_4 = beat >> 4;
  STATIC_REQUIRE(right_4 == beat);
}

TEST_CASE("concatenation", "[rhythm]") {
  constexpr Rhythm<2> first("x.");
  constexpr Rhythm<2> second(".x");
  constexpr auto result = cat(first, second);
  STATIC_REQUIRE(result.length() == 4);
  STATIC_REQUIRE(result == Rhythm{"x..x"});
}

TEST_CASE("dac", "[rhythm]") {
  constexpr Rhythm<4> beat("x.x.");
  constexpr auto val = beat.dac<4>();
  STATIC_REQUIRE(val == 0b0101);
}

// TEST_CASE("interleave", "[rhythm]") {
//   constexpr Rhythm<2> aa("x.");
//   constexpr Rhythm<2> bb(".x");
//   constexpr auto zipped = interleave(aa, bb);

//   static_assert(zipped.steps() == 4);
//   // result[j*2 + chan]: aa[0]=x at pos 0, bb[0]=. at pos 1, aa[1]=. at pos
//   2, bb[1]=x at pos 3 REQUIRE(zipped[0]); REQUIRE_FALSE(zipped[1]);
//   REQUIRE_FALSE(zipped[2]);
//   REQUIRE(zipped[3]);
// }

TEST_CASE("next_permutation 2-step cycle", "[rhythm][next_permutation]") {
  // "x." is the last permutation; wraps to ".x" (first, ascending order)
  constexpr Rhythm<2> hit_first{"x."};
  STATIC_REQUIRE(next_permutation(hit_first) == Rhythm<2>{".x"});

  // ".x" advances to "x."
  constexpr Rhythm<2> rest_first{".x"};
  STATIC_REQUIRE(next_permutation(rest_first) == Rhythm<2>{"x."});
}

TEST_CASE("next_permutation 3-step full cycle with 1 hit", "[rhythm][next_permutation]") {
  // C(3,1) = 3 permutations: "..x" -> ".x." -> "x.." -> "..x"
  constexpr Rhythm<3> first{"..x"};
  constexpr auto second = next_permutation(first);
  STATIC_REQUIRE(second == Rhythm<3>{".x."});
  constexpr auto third = next_permutation(second);
  STATIC_REQUIRE(third == Rhythm<3>{"x.."});
  constexpr auto wrapped = next_permutation(third);
  STATIC_REQUIRE(wrapped == first);
}

TEST_CASE("next_permutation 4-step count with 2 hits equals C(4,2)=6", "[rhythm][next_permutation]") {
  constexpr Rhythm<4> first{"..xx"};
  constexpr auto p1 = next_permutation(first);
  constexpr auto p2 = next_permutation(p1);
  constexpr auto p3 = next_permutation(p2);
  constexpr auto p4 = next_permutation(p3);
  constexpr auto p5 = next_permutation(p4);
  constexpr auto wrapped = next_permutation(p5);
  STATIC_REQUIRE(wrapped == first);
  STATIC_REQUIRE_FALSE(p1 == first);
  STATIC_REQUIRE_FALSE(p2 == first);
  STATIC_REQUIRE_FALSE(p3 == first);
  STATIC_REQUIRE_FALSE(p4 == first);
  STATIC_REQUIRE_FALSE(p5 == first);
}

TEST_CASE("next_permutation of all-hits or no-hits returns itself", "[rhythm][next_permutation]") {
  constexpr Rhythm<3> all_hits{"xxx"};
  STATIC_REQUIRE(next_permutation(all_hits) == all_hits);

  constexpr Rhythm<3> no_hits{"..."};
  STATIC_REQUIRE(next_permutation(no_hits) == no_hits);
}

TEST_CASE("next_permutation preserves hit count", "[rhythm][next_permutation]") {
  constexpr Rhythm<4> original{"x.x."};
  constexpr auto permuted = next_permutation(original);
  STATIC_REQUIRE(original.hits() == permuted.hits());
}

