#include <cstddef>

#include <rhythm.hpp>
#include <math.hpp>

#include <catch2/catch_test_macros.hpp>

using automata::lcm;
using automata::Rhythm;
// using automata::interleave;

TEST_CASE("noneness", "[rhythm]") {
  STATIC_REQUIRE(Rhythm<8>{}.none());
  STATIC_REQUIRE_FALSE(Rhythm<8>{"x......."}.none());
  STATIC_REQUIRE_FALSE(Rhythm<8>{"x.x.x.x."}.none());
}

TEST_CASE("fill", "[rhythm]") {
  STATIC_REQUIRE(Rhythm<1>{}.fill().rests() == 0);
  STATIC_REQUIRE(Rhythm<2>{}.fill().rests() == 0);
  STATIC_REQUIRE(Rhythm<4>{}.fill().rests() == 0);
  STATIC_REQUIRE(Rhythm<8>{}.fill().rests() == 0);
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
  constexpr auto result = lhs * rhs;
  STATIC_REQUIRE(result.length() == 4);
  STATIC_REQUIRE(result == Rhythm{"x..."});
}

TEST_CASE("binary operator of different length auto expand to lcm",
          "[rhythm]") {
  constexpr Rhythm<4> lhs("x.x.");
  constexpr Rhythm<9> rhs = Rhythm{"x..."}.tile<9>();
  constexpr auto result = lhs * rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 9));
}

TEST_CASE("operator+ different length", "[rhythm]") {
  constexpr Rhythm<4> lhs{"x..."};
  constexpr Rhythm<6> rhs{"...x.."};
  constexpr auto result = lhs + rhs;
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
  constexpr auto result = first | second;
  STATIC_REQUIRE(result.length() == 4);
  STATIC_REQUIRE(result == Rhythm{"x..x"});
}

TEST_CASE("concatenate 3 rhythms", "[rhythm]") {
  constexpr Rhythm<3> first("x..");
  constexpr Rhythm<3> second(".x.");
  constexpr Rhythm<3> third("..x");
  constexpr auto result = first | second | third;
  STATIC_REQUIRE(result.length() == 9);
  STATIC_REQUIRE(result == Rhythm{"x...x...x"});
}

TEST_CASE("dac", "[rhythm]") {
  constexpr Rhythm<4> beat("x.x.");
  constexpr auto val = beat.dac<4>();
  STATIC_REQUIRE(val == 0b0101);
}

TEST_CASE("stretch", "[rhythm]") {
  constexpr Rhythm<4> original{"xxxx"};
  constexpr auto stretched_2x = original.stretch<2>();
  constexpr auto stretched_4x = original.stretch<4>();
  STATIC_REQUIRE(stretched_2x.length() == 8);
  STATIC_REQUIRE(stretched_4x.length() == 16);
  STATIC_REQUIRE(stretched_2x == Rhythm{"x.x.x.x."});
  STATIC_REQUIRE(stretched_4x == Rhythm{"x...x...x...x..."});
}

TEST_CASE("compress takes every K-th step", "[rhythm]") {
  constexpr Rhythm<8> original{"x.x.x.x."};
  constexpr auto compressed = original.compress<2>();
  STATIC_REQUIRE(compressed.length() == 4);
  STATIC_REQUIRE(compressed == Rhythm{"xxxx"});
}

TEST_CASE("compress with non-divisor K", "[rhythm]") {
  constexpr Rhythm<8> original{"x......."};
  constexpr auto compressed = original.compress<3>();
  STATIC_REQUIRE(compressed.length() == 3);
  STATIC_REQUIRE(compressed == Rhythm{"x.."});
}

TEST_CASE("compress discards intermediate steps", "[rhythm]") {
  constexpr Rhythm<4> original{".x.x"};
  constexpr auto compressed = original.compress<2>();
  STATIC_REQUIRE(compressed.length() == 2);
  STATIC_REQUIRE(compressed == Rhythm{".."});
}

TEST_CASE("stretch then compress is identity", "[rhythm]") {
  constexpr Rhythm<4> original{"x.x."};
  STATIC_REQUIRE(original.stretch<2>().compress<2>() == original);
}

TEST_CASE("compress then stretch is lossy", "[rhythm]") {
  constexpr Rhythm<4> original{"xxx."};
  STATIC_REQUIRE(original.compress<2>().stretch<2>() != original);
}

TEST_CASE("permute<1> advances one permutation", "[rhythm][permute]") {
  constexpr Rhythm<4> first{"..xx"};
  STATIC_REQUIRE(first.permute<1>() == Rhythm<4>{".x.x"});
  STATIC_REQUIRE(first.permute<1>().permute<1>() == Rhythm<4>{".xx."});
}

TEST_CASE("permute<K> advances K permutations at once", "[rhythm][permute]") {
  constexpr Rhythm<4> first{"..xx"};
  STATIC_REQUIRE(first.permute<3>() == Rhythm<4>{"x..x"});
  STATIC_REQUIRE(first.permute<6>() == first);  // C(4,2)=6 full cycle
}

TEST_CASE("permute preserves hit count", "[rhythm][permute]") {
  constexpr Rhythm<4> original{"x.x."};
  STATIC_REQUIRE(original.hits() == original.permute<1>().hits());
  STATIC_REQUIRE(original.hits() == original.permute<2>().hits());
  STATIC_REQUIRE(original.hits() == original.permute<3>().hits());
}

TEST_CASE("permute of trivial rhythms returns itself", "[rhythm][permute]") {
  constexpr Rhythm<3> all_hits{"xxx"};
  STATIC_REQUIRE(all_hits.permute<1>() == all_hits);
  constexpr Rhythm<3> no_hits{"..."};
  STATIC_REQUIRE(no_hits.permute<1>() == no_hits);
}

TEST_CASE("euclid(3) on 8 steps produces tresillo", "[rhythm][euclid]") {
  constexpr auto beat = Rhythm<8>::euclid(3);
  STATIC_REQUIRE(beat == Rhythm<8>{"x..x.x.."});
}

TEST_CASE("euclid(4) on 16 steps produces four on the flour", "[rhythm][euclid]") {
  constexpr auto beat = Rhythm<16>::euclid(4);
  STATIC_REQUIRE(beat == Rhythm<16>{"x...x...x...x..."});
}

TEST_CASE("euclid(5) on 8 steps produces son clave", "[rhythm][euclid]") {
  constexpr auto beat = Rhythm<8>::euclid(5);
  STATIC_REQUIRE(beat == Rhythm<8>{"x.xx.xx."});
}

TEST_CASE("euclid hit count equals pulse count", "[rhythm][euclid]") {
  STATIC_REQUIRE(Rhythm<16>::euclid(5).hits() == 5);
  STATIC_REQUIRE(Rhythm<12>::euclid(7).hits() == 7);
}
