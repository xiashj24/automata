#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/core/transport.hpp"
#include "automata/engine/graph.hpp"
#include "automata/engine/offline.hpp"
#include "automata/ugens/ugens.hpp"

#include <cstddef>
#include <vector>

using namespace automata;
using Catch::Approx;

namespace {

GraphDef cycle_def(float beats) {
  return describe([beats](GraphBuilder& g) {
    const Signal ramp = cycle(beats).ramp();
    g.out(ramp, ramp);
  });
}

// Left channel of an offline render, one float per frame.
std::vector<float> left_of(GraphDef def, std::size_t nframes) {
  const std::vector<float> stereo = render_interleaved(std::move(def), nframes);
  std::vector<float> mono(nframes);
  for (std::size_t i = 0; i < nframes; ++i) {
    mono[i] = stereo[2 * i];
  }
  return mono;
}

}  // namespace

TEST_CASE("a cycle derives its phase from the transport", "[engine][clock]") {
  Transport t;  // 120 bpm → 24000 samples per beat
  Graph graph(cycle_def(1.f), nullptr, &t);

  graph.process_block();
  REQUIRE(graph.left()[0] == Approx(0.f));
  REQUIRE(graph.left()[127] == Approx(127.0 / 24000.0));

  t.advance(BlockSize);
  graph.process_block();
  REQUIRE(graph.left()[0] == Approx(128.0 / 24000.0));
}

TEST_CASE("a cycle created mid-performance is already on the grid",
          "[engine][clock]") {
  Transport t;
  t.advance(60000);  // beat 2.5

  // A brand-new graph — the post-swap situation — needs no state transfer
  // to be in phase: position is derived, not stored.
  Graph graph(cycle_def(1.f), nullptr, &t);
  graph.process_block();
  REQUIRE(graph.left()[0] == Approx(0.5f));

  Graph bar_graph(cycle_def(4.f), nullptr, &t);
  bar_graph.process_block();
  REQUIRE(bar_graph.left()[0] == Approx(2.5 / 4.0));
}

TEST_CASE("a tempo change re-derives the phase without a jump",
          "[engine][clock]") {
  Transport t;
  Graph graph(cycle_def(1.f), nullptr, &t);
  t.advance(12000);  // half a beat
  t.bpm = 60.f;      // beat position is unchanged; only the rate halves

  graph.process_block();
  REQUIRE(graph.left()[0] == Approx(0.5f));
  REQUIRE(graph.left()[1] == Approx(0.5 + 1.0 / 48000.0));
}

TEST_CASE("unbound cycles emit zero", "[engine][clock]") {
  Graph graph(cycle_def(1.f));
  graph.process_block();
  REQUIRE(graph.left()[0] == 0.f);
  REQUIRE(graph.left()[127] == 0.f);
}

TEST_CASE("retiming a cycle is a value patch", "[engine][clock]") {
  const GraphDef one = cycle_def(1.f);
  const GraphDef two = cycle_def(2.f);
  REQUIRE(one.def_hash == two.def_hash);
}

TEST_CASE("seq steps through its values over one cycle", "[engine][clock]") {
  GraphDef def = describe([](GraphBuilder& g) {
    const Signal s = seq(bar(), {1.f, 2.f, 3.f, 4.f});
    g.out(s, s);
  });

  // One 4/4 bar at 120 bpm = 96000 samples, one step per beat. Boundaries
  // land within a sample of exact, so probe one sample inside each step.
  const std::vector<float> out = left_of(std::move(def), 96256);
  REQUIRE(out[0] == 1.f);
  REQUIRE(out[23999] == 1.f);
  REQUIRE(out[24001] == 2.f);
  REQUIRE(out[48001] == 3.f);
  REQUIRE(out[72001] == 4.f);
  REQUIRE(out[95999] == 4.f);
  REQUIRE(out[96001] == 1.f);  // the next bar wraps to the first step
}

TEST_CASE("editing a seq step is a value patch; resizing is structural",
          "[engine][clock]") {
  const auto with_steps = [](std::initializer_list<float> steps) {
    return describe([&](GraphBuilder& g) {
      const Signal s = seq(bar(), steps);
      g.out(s, s);
    });
  };
  REQUIRE(with_steps({1.f, 2.f, 3.f}).def_hash ==
          with_steps({1.f, 9.f, 3.f}).def_hash);
  REQUIRE(with_steps({1.f, 2.f, 3.f}).def_hash !=
          with_steps({1.f, 2.f, 3.f, 4.f}).def_hash);
}

TEST_CASE("euclid fires single-sample triggers on the euclidean grid",
          "[engine][clock]") {
  GraphDef def = describe([](GraphBuilder& g) {
    const Signal s = euclid(bar(), 3.f, 8.f);
    g.out(s, s);
  });

  // 8 steps over a 96000-sample bar = 12000 samples per step. The closed
  // form puts E(3,8)'s hits at steps {2, 5, 7} for rotation 0.
  const std::vector<float> out = left_of(std::move(def), 96000);
  std::vector<std::size_t> onsets;
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (out[i] != 0.f) {
      onsets.push_back(i);
    }
  }
  const std::size_t expected[] = {24000, 60000, 84000};
  REQUIRE(onsets.size() == 3);
  for (std::size_t i = 0; i < 3; ++i) {
    REQUIRE(onsets[i] >= expected[i] - 1);
    REQUIRE(onsets[i] <= expected[i] + 1);
  }
}

TEST_CASE("swing delays every second onset", "[engine][clock]") {
  GraphDef def = describe([](GraphBuilder& g) {
    const Signal s = beat().swing(0.5f).trig();
    g.out(s, s);
  });

  // The pair spans two beats (48000 samples); amount 0.5 puts the second
  // onset at 3/4 of the pair instead of 1/2.
  const std::vector<float> out = left_of(std::move(def), 48000);
  std::vector<std::size_t> onsets;
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (out[i] != 0.f) {
      onsets.push_back(i);
    }
  }
  REQUIRE(onsets.size() == 2);
  REQUIRE(onsets[0] == 0);
  REQUIRE(onsets[1] >= 35999);
  REQUIRE(onsets[1] <= 36001);
}

TEST_CASE("rate algebra and rotation stay phase-locked to the grid",
          "[engine][clock]") {
  Transport t;
  Graph graph(describe([](GraphBuilder& g) {
                const Clock c = beat();
                g.out((c * 2.f).ramp(), (c >> 0.5f).ramp());
              }),
              nullptr, &t);

  t.advance(12000);  // beat 0.5: the doubled clock wraps, rotation is at 0
  graph.process_block();
  REQUIRE(graph.left()[0] == Approx(0.f).margin(1e-6));
  REQUIRE(graph.right()[0] == Approx(0.f).margin(1e-6));
}
