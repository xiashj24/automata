#include <catch2/catch_test_macros.hpp>

#include "automata/engine/offline.hpp"
#include "automata/graph/rebind.hpp"
#include "automata/patch.hpp"
#include "automata/ugens/vocabulary.hpp"

#include <cmath>

// The patch boundary exercised in-binary: AUTOMATA_PATCH's exports are
// ordinary symbols here, called exactly the way the host calls them after
// GetProcAddress — stamp first, then describe into a caller-owned builder.

using namespace automata;

AUTOMATA_PATCH(g) {
  auto lfo = sine(0.25f);
  auto env = ar(metro(2.0f), 0.005f, 0.12f);
  auto voice = svf_lp(saw(110.0f), 800.0f + lfo * 600.0f, 0.7f) * env;
  auto fb = g.tap("fb");
  auto wet = voice + fb.read(0.375f) * 0.35f;
  fb.write(wet);
  g.out(soft_clip(wet), soft_clip(wet));
}

TEST_CASE("the patch macro exports a verifiable stamp", "[patch]") {
  REQUIRE(automata_patch_stamp.magic == automata::PatchMagic);
  REQUIRE(automata_patch_stamp.abi == automata::PatchAbiVersion);
  REQUIRE(automata::stamp_compatible(automata_patch_stamp));

  automata::PatchStamp drifted = automata_patch_stamp;
  drifted.build ^= 1;
  REQUIRE_FALSE(automata::stamp_compatible(drifted));
}

TEST_CASE("the describe entry point builds a playable, rebindable def",
          "[patch]") {
  automata::GraphBuilder builder;
  automata_patch_describe(&builder);
  REQUIRE(builder.has_out());
  automata::GraphDef def = builder.build();
  REQUIRE(!def.nodes.empty());
  REQUIRE(def.def_hash != 0);

  // Vocabulary-only patch: nothing pins its generation (ADR 0001).
  const automata::KernelRegistry registry = automata::vocabulary_registry();
  REQUIRE(rebind_kernels(def, registry) == 0);

  const auto pcm = automata::render_interleaved(def, 4800);
  bool finite = true;
  bool audible = false;
  for (const float s : pcm) {
    finite = finite && std::isfinite(s);
    audible = audible || s != 0.f;
  }
  CHECK(finite);
  CHECK(audible);
}
