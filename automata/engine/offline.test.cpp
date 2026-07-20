#include <catch2/catch_test_macros.hpp>

#include "automata/engine/offline.hpp"
#include "automata/ugens/ugens.hpp"

#include <filesystem>

using namespace automata;

namespace {
GraphDef test_patch() {
  return describe([](GraphBuilder& g) {
    auto lfo = sine(0.25f);
    auto env = ar(metro(2.f), 0.005f, 0.12f);
    auto voice = svf_lp(saw(110.f), 800.f + lfo * 600.f, 0.7f) * env;
    auto wet = soft_clip(voice);
    g.out(wet, wet);
  });
}
}  // namespace

TEST_CASE("offline rendering is deterministic", "[engine][offline]") {
  const auto a = render_interleaved(test_patch(), 10'000);
  const auto b = render_interleaved(test_patch(), 10'000);

  REQUIRE(a.size() == 20'000);
  REQUIRE(a == b);
}

TEST_CASE("a rendered patch actually makes sound", "[engine][offline]") {
  const auto pcm = render_interleaved(test_patch(), 48'000);

  float peak = 0.f;
  for (const float s : pcm) {
    peak = std::max(peak, s > 0.f ? s : -s);
  }
  REQUIRE(peak > 0.05f);
  REQUIRE(peak <= 1.f);  // soft_clip bounds the output
}

TEST_CASE("render_wav writes a 32-bit float stereo file", "[engine][offline]") {
  const char* path = "automata_offline_test.wav";
  const auto result = render_wav(test_patch(), 4800, path);

  REQUIRE(result.has_value());
  REQUIRE(std::filesystem::exists(path));
  // 4800 stereo f32 frames plus headers.
  REQUIRE(std::filesystem::file_size(path) >= 4800 * 2 * sizeof(float));
  std::filesystem::remove(path);
}
