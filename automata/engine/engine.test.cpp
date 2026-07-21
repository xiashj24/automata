#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/engine/engine.hpp"
#include "automata/engine/offline.hpp"
#include "automata/engine/reconcile.hpp"
#include "automata/ugens/ugens.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace automata;
using Catch::Approx;

namespace {

GraphDef const_def(float value) {
  return describe([&](GraphBuilder& g) {
    Signal v = value;
    g.out(v, v);
  });
}

// Structurally distinct from const_def: a Mul of two consts.
GraphDef mul_def(float a, float b) {
  return describe([&](GraphBuilder& g) {
    auto v = Signal{a} * Signal{b};
    g.out(v, v);
  });
}

std::vector<float> run(Engine& engine, std::size_t nframes) {
  std::vector<float> buf(nframes * 2, 0.f);
  engine.render(buf.data(), nframes);
  return buf;
}

constexpr float TwoPi = 6.28318530717958647692f;

}  // namespace

TEST_CASE("the engine is silent until a graph arrives, then renders it",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);

  auto silent = run(engine, 64);
  REQUIRE(silent[0] == 0.f);

  rec.update(const_def(0.5f));
  const auto buf = run(engine, 256);
  rec.drain();
  REQUIRE(rec.idle());
  // First partial block was already mixed as silence; from the next block
  // boundary on, the graph is live.
  REQUIRE(buf[2 * 70] == 0.5f);
  REQUIRE(buf[2 * 255] == 0.5f);
}

TEST_CASE("a value edit glides instead of stepping", "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(const_def(0.25f));
  (void)run(engine, BlockSize);
  rec.drain();

  rec.update(const_def(0.75f));  // same structure: value patch
  const auto buf = run(engine, 768);
  rec.drain();

  constexpr std::size_t Ramp = 480;  // 10 ms at 48 kHz
  REQUIRE(buf[0] > 0.25f);
  REQUIRE(buf[0] < 0.26f);  // one ramp step, not a jump
  bool monotonic = true;
  for (std::size_t i = 1; i < Ramp; ++i) {
    monotonic = monotonic && buf[2 * i] >= buf[2 * (i - 1)];
  }
  REQUIRE(monotonic);
  REQUIRE(buf[2 * Ramp] == Approx(0.75f));
  REQUIRE(buf[2 * 767] == 0.75f);
}

TEST_CASE("a structural swap transfers state: the sine keeps its phase",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(describe([](GraphBuilder& g) {
    auto s = sine(375.f);
    g.out(s, s);
  }));
  (void)run(engine, 1280);
  rec.drain();

  rec.update(describe([](GraphBuilder& g) {
               auto s = soft_clip(sine(375.f));
               g.out(s, s);
             }),
             Quantize::Immediate, 0.f);
  const auto buf = run(engine, 1280);
  rec.drain();

  // Reference: the same phasor run uninterrupted, setter-then-process per
  // sample exactly as the graph executes it.
  Phasor k;
  const float w = 375.f * (1.f / 48000.f);
  for (int n = 0; n < 1280; ++n) {
    k.set_freq(w);
    (void)k.process();
  }
  for (int n = 0; n < 1280; ++n) {
    k.set_freq(w);
    const float expected = std::tanh(std::sin(TwoPi * k.process()));
    REQUIRE(buf[2 * n] == expected);
  }
}

TEST_CASE("a waveform edit keeps the phase: sine becomes saw",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(describe([](GraphBuilder& g) {
    auto s = sine(375.f);
    g.out(s, s);
  }));
  (void)run(engine, 1280);
  rec.drain();

  rec.update(describe([](GraphBuilder& g) {
               auto s = saw(375.f);
               g.out(s, s);
             }),
             Quantize::Immediate, 0.f);
  const auto buf = run(engine, 1280);
  rec.drain();

  // Only the stateless shaper changed; the phasor pairs in the global match
  // pass, so the saw continues from the sine's accumulated phase.
  Phasor k;
  const float w = 375.f * (1.f / 48000.f);
  for (int n = 0; n < 1280; ++n) {
    k.set_freq(w);
    (void)k.process();
  }
  for (int n = 0; n < 1280; ++n) {
    k.set_freq(w);
    const float expected = 2.f * k.process() - 1.f;
    REQUIRE(buf[2 * n] == expected);
  }
}

TEST_CASE("a structural swap crossfades linearly", "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(const_def(1.f));
  (void)run(engine, BlockSize);
  rec.drain();

  const float fade_seconds = 480.f / 48000.f;
  rec.update(mul_def(-0.5f, 2.f), Quantize::Immediate, fade_seconds);
  const auto buf = run(engine, 640);
  rec.drain();

  REQUIRE(buf[0] == 1.f);  // fade starts on the old graph
  REQUIRE(buf[2 * 240] == Approx(0.f).margin(0.01f));  // halfway
  REQUIRE(buf[2 * 500] == -1.f);  // settled on the new graph
}

TEST_CASE("a swap gated on the next beat lands past the beat boundary",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(const_def(1.f));
  (void)run(engine, BlockSize);
  rec.drain();

  rec.update(mul_def(-0.5f, 2.f), Quantize::NextBeat, 0.f);
  const auto buf = run(engine, 25000);
  rec.drain();

  // 120 bpm → beat at 24000; first block boundary past it is 24064.
  // This render started at absolute sample 128.
  const std::size_t land = 24064 - BlockSize;
  REQUIRE(buf[2 * (land - 1)] == 1.f);
  REQUIRE(buf[2 * land] == -1.f);
}

TEST_CASE("a swap landing mid-fade snaps the older fade",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(const_def(1.f));
  (void)run(engine, BlockSize);
  rec.drain();

  rec.update(mul_def(-0.5f, 2.f), Quantize::Immediate, 0.5f);  // long fade
  (void)run(engine, BlockSize);                                // fade begins
  rec.drain();                                                 // acks the swap

  rec.update(describe([](GraphBuilder& g) {
               auto v = Signal{1.f} + Signal{-1.f};
               g.out(v, v);
             }),
             Quantize::Immediate, 0.f);
  const auto buf = run(engine, 256);
  rec.drain();
  REQUIRE(buf[0] == 0.f);  // third graph, instantly, no residue of the fade
  REQUIRE(buf[2 * 255] == 0.f);
  REQUIRE(engine.dropped_msgs() == 0);
}

TEST_CASE("tap feedback echoes with sample-exact delay",
          "[engine][reconcile]") {
  const auto def = describe([](GraphBuilder& g) {
    auto fb = g.tap("fb");
    auto in = metro(0.4f);  // one impulse at t = 0 in this window
    auto wet = in + fb.read(0.25f) * 0.5f;
    fb.write(wet);
    g.out(wet, wet);
  });

  const auto pcm = render_interleaved(def, 48'000);

  REQUIRE(pcm[0] == 1.f);
  REQUIRE(pcm[2 * 12000] == Approx(0.5f));
  REQUIRE(pcm[2 * 24000] == Approx(0.25f));
  REQUIRE(pcm[2 * 36000] == Approx(0.125f));
  REQUIRE(pcm[2 * 6000] == 0.f);
  REQUIRE(pcm[2 * 18000] == 0.f);
}

TEST_CASE("a tapped patch swaps with its delay memory intact",
          "[engine][reconcile]") {
  const auto echo_def = [](float gain) {
    return describe([&](GraphBuilder& g) {
      auto fb = g.tap("fb");
      auto in = metro(0.4f);
      auto wet = in + fb.read(0.25f) * Signal{gain};
      fb.write(wet);
      g.out(wet, wet);
    });
  };

  Engine engine;
  Reconciler rec(engine);
  rec.update(echo_def(0.5f));
  (void)run(engine, 6000);  // impulse is in the tap buffer now
  rec.drain();

  // Same structure — the gain edit is a value patch; but also prove the
  // buffer survives a *structural* swap by wrapping the output.
  rec.update(describe([&](GraphBuilder& g) {
               auto fb = g.tap("fb");
               auto in = metro(0.4f);
               auto wet = in + fb.read(0.25f) * 0.5f;
               fb.write(wet);
               g.out(soft_clip(wet), soft_clip(wet));
             }),
             Quantize::Immediate, 0.f);
  const auto buf = run(engine, 10000);
  rec.drain();

  // The echo of the pre-swap impulse arrives on schedule: absolute sample
  // 12000 is index 6000 here, amplitude tanh(0.5).
  REQUIRE(buf[2 * 6000] == Approx(std::tanh(0.5f)));
}

TEST_CASE("a param plays its fallback until the bus is written, then glides",
          "[engine][param]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(describe([](GraphBuilder& g) {
    auto p = param("gain", 0.25f);
    g.out(p, p);
  }));
  auto buf = run(engine, 2 * BlockSize);
  rec.drain();
  REQUIRE(buf[2 * (2 * BlockSize - 1)] == 0.25f);

  engine.bus().set("gain", 0.75f);
  buf = run(engine, 768);
  REQUIRE(buf[0] > 0.25f);
  REQUIRE(buf[0] < 0.26f);  // one ramp step, not a jump
  REQUIRE(buf[2 * 700] == 0.75f);
}

TEST_CASE("editing a param fallback is a value patch and glides",
          "[engine][param]") {
  const auto def_with = [](float fallback) {
    return describe([&](GraphBuilder& g) {
      auto p = param("q", fallback);
      g.out(p, p);
    });
  };
  REQUIRE(def_with(0.25f).def_hash == def_with(0.75f).def_hash);

  Engine engine;
  Reconciler rec(engine);
  rec.update(def_with(0.25f));
  (void)run(engine, BlockSize);
  rec.drain();

  rec.update(def_with(0.75f));
  const auto buf = run(engine, 768);
  rec.drain();
  REQUIRE(buf[0] > 0.25f);
  REQUIRE(buf[0] < 0.26f);
  REQUIRE(buf[2 * 700] == 0.75f);
}

TEST_CASE("a param's live value survives a structural swap",
          "[engine][param]") {
  Engine engine;
  Reconciler rec(engine);
  rec.update(describe([](GraphBuilder& g) {
    auto p = param("gain", 0.25f);
    g.out(p, p);
  }));
  (void)run(engine, BlockSize);
  rec.drain();
  engine.bus().set("gain", 0.75f);
  (void)run(engine, 768);  // settled on the live value

  rec.update(describe([](GraphBuilder& g) {
               auto p = param("gain", 0.25f);
               g.out(soft_clip(p), soft_clip(p));
             }),
             Quantize::Immediate, 0.f);
  const auto buf = run(engine, 2 * BlockSize);
  rec.drain();
  // No re-glide from the fallback: the transferred ramp is already there.
  REQUIRE(buf[0] == Approx(std::tanh(0.75f)));
}

TEST_CASE("offline, a param renders its fallback", "[engine][param]") {
  const auto pcm = render_interleaved(describe([](GraphBuilder& g) {
                                        auto p = param("cutoff", 0.5f);
                                        g.out(p, p);
                                      }),
                                      256);
  REQUIRE(pcm[0] == 0.5f);
  REQUIRE(pcm[2 * 255] == 0.5f);
}

TEST_CASE("an owner token is released only when its graph retires",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  auto owner = std::make_shared<int>(0);

  rec.update(const_def(0.5f), Quantize::Immediate, 0.f, owner);
  (void)run(engine, BlockSize);
  rec.drain();
  REQUIRE(owner.use_count() == 2);  // held for the live graph

  // A pinned def-equal update swaps (ADR 0009): the new graph holds its
  // token, the retired first graph releases the old one.
  auto patch_owner = std::make_shared<int>(0);
  rec.update(const_def(0.9f), Quantize::Immediate, 0.f, patch_owner);
  (void)run(engine, 2 * BlockSize);
  rec.drain();
  REQUIRE(patch_owner.use_count() == 2);
  REQUIRE(owner.use_count() == 1);

  // An ownerless def-equal update stays a value patch: no graph is built,
  // and the live pinned graph keeps its token.
  rec.update(const_def(0.7f), Quantize::Immediate, 0.f);
  (void)run(engine, BlockSize);
  rec.drain();
  REQUIRE(patch_owner.use_count() == 2);

  // A structural swap retires the pinned graph and with it the token.
  rec.update(mul_def(0.5f, 1.f), Quantize::Immediate, 0.f);
  (void)run(engine, 2 * BlockSize);
  rec.drain();
  REQUIRE(patch_owner.use_count() == 1);
}

TEST_CASE("a pinned body edit lands even when the def hash is unchanged",
          "[engine][reconcile]") {
  Engine engine;
  Reconciler rec(engine);
  const auto def_with = [](float (*f)(float)) {
    return describe([&](GraphBuilder& g) {
      const Signal s = fn("gain", f, 0.5f);
      g.out(s, s);
    });
  };

  GraphDef doubled = def_with(+[](float x) { return x * 2.f; });
  GraphDef tripled = def_with(+[](float x) { return x * 3.f; });
  REQUIRE(doubled.def_hash == tripled.def_hash);  // the edit is body-only

  rec.update(std::move(doubled), Quantize::Immediate, 0.f,
             std::make_shared<int>(1));
  (void)run(engine, 256);
  rec.drain();
  REQUIRE(run(engine, BlockSize)[0] == 1.f);

  // An owner marks the def as carrying its own code: the reconciler must
  // swap, not value-patch, or the new body never runs.
  rec.update(std::move(tripled), Quantize::Immediate, 0.f,
             std::make_shared<int>(2));
  (void)run(engine, 256);
  rec.drain();
  REQUIRE(run(engine, BlockSize)[0] == 1.5f);
}
