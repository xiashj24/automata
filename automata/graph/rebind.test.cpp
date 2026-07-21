#include <catch2/catch_test_macros.hpp>

#include "automata/engine/offline.hpp"
#include "automata/graph/rebind.hpp"
#include "automata/ugens/vocabulary.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

using namespace automata;

namespace {

// Every factory in ugens.hpp plus taps — the coverage guard: a factory the
// vocabulary probe misses would leave a foreign node below.
GraphDef full_vocabulary_def() {
  return describe([](GraphBuilder& g) {
    auto fb = g.tap("fb");
    const Signal osc = sine(440.f) + saw(110.f) - phasor(1.f) +
                       sine(220.f, 0.25f) + triangle(220.f) + tri(phasor(2.f)) +
                       simple_fm(110.f, 2.f);
    const Signal env =
        ar(metro(2.f), 0.01f, 0.1f) + are(pulsen(0.1f, 0.5f), 0.01f, 0.2f);
    const Signal filtered = svf_lp(osc * env, 800.f, 0.7f) +
                            svf_bp(osc, 500.f, 2.f) + svf_hp(osc, 100.f, 1.f);
    const Signal wet = soft_clip(filtered / 2.f) + (-filtered) +
                       fb.read(0.25f) * 0.5f +
                       hard_clip(bipolar(clip(unipolar(osc)))) +
                       curve(smooth(gain(osc, -6.f)), 2.f) + (osc >= 0.5f);
    fb.write(wet);
    const Clock c = beat();
    const Signal rhythm =
        (c / 2).swing(0.25f).trig() + c.gate(0.3f) + (c >> 0.5f).ramp() +
        seq(c, {1.f, 2.f}) + euclid(bar(), 3.f, 8.f, 1.f) + frac(osc) +
        step(c.trig(), {1.f, 2.f, 3.f}) + latch(osc, c.trig());
    const Signal controlled =
        wet * param("gain", 0.5f) + mouse_x() + rhythm * 0.1f;
    g.out(controlled, controlled);
  });
}

// What a def looks like arriving from another generation: kernel pointers
// into that library's statics, bound-setter op bytes meaningless here.
std::vector<std::unique_ptr<KernelInfo>> make_foreign(
    GraphDef& def,
    const KernelRegistry& registry) {
  std::vector<std::unique_ptr<KernelInfo>> infos;
  for (GraphDef::Node& node : def.nodes) {
    infos.push_back(std::make_unique<KernelInfo>(*node.kernel));
    const KernelRegistry::Entry* entry = registry.find(node.type_hash);
    if (entry == nullptr) {
      entry = registry.find(node.kernel->type_hash);
    }
    node.kernel = infos.back().get();
    if (entry != nullptr && entry->rewrite_op) {
      std::fill_n(def.op_data.begin() + node.op_begin, node.op_size,
                  std::byte{0xAB});
    }
  }
  return infos;
}

}  // namespace

TEST_CASE("rebinding restores every vocabulary kernel to host statics",
          "[rebind]") {
  const GraphDef reference = full_vocabulary_def();
  GraphDef foreign_def = full_vocabulary_def();
  const KernelRegistry registry = vocabulary_registry();
  const auto foreign_infos = make_foreign(foreign_def, registry);

  REQUIRE(rebind_kernels(foreign_def, registry) == 0);

  for (std::size_t i = 0; i < reference.nodes.size(); ++i) {
    CHECK(foreign_def.nodes[i].kernel == reference.nodes[i].kernel);
  }
  // Bound ops restored, tap-id ops untouched: byte-identical overall.
  CHECK(foreign_def.op_data == reference.op_data);
  CHECK(foreign_def.def_hash == reference.def_hash);

  // The rebound def renders exactly what the native one does.
  CHECK(render_interleaved(foreign_def, 4800) ==
        render_interleaved(reference, 4800));
}

TEST_CASE("rebinding a host-built def changes nothing", "[rebind]") {
  const GraphDef reference = full_vocabulary_def();
  GraphDef def = full_vocabulary_def();
  const KernelRegistry registry = vocabulary_registry();

  REQUIRE(rebind_kernels(def, registry) == 0);
  CHECK(def.op_data == reference.op_data);
  for (std::size_t i = 0; i < reference.nodes.size(); ++i) {
    CHECK(def.nodes[i].kernel == reference.nodes[i].kernel);
  }
}

TEST_CASE("an unknown kernel stays foreign and is counted", "[rebind]") {
  const KernelInfo custom{
      .type_hash = hash_string("test.Custom"),
      .state_size = detail::ConstInfo.state_size,
      .state_align = detail::ConstInfo.state_align,
      .output_count = 1,
      .construct = detail::ConstInfo.construct,
      .reset = detail::ConstInfo.reset,
      .process = detail::ConstInfo.process,
  };
  GraphDef def = describe([&](GraphBuilder& g) {
    const float values[1] = {0.f};
    const Signal out =
        g.add_node(&custom, custom.type_hash, {}, values, {}, {});
    g.out(out, out * 2.f);
  });

  const KernelRegistry registry = vocabulary_registry();
  CHECK(rebind_kernels(def, registry) == 1);
  CHECK(def.nodes[0].kernel == &custom);
}

TEST_CASE("a probed fn name rebinds; a patch-local one stays foreign",
          "[rebind]") {
  GraphDef def = describe([](GraphBuilder& g) {
    const Signal known = soft_clip(0.5f);  // fn-based vocabulary
    const Signal local =
        fn("patch_only", [](float x) { return x + 1.f; }, known);
    g.out(local, local);
  });

  const KernelRegistry registry = vocabulary_registry();
  CHECK(rebind_kernels(def, registry) == 1);

  // The vocabulary fn was rewritten to the host entry; the local one kept
  // its own kernel and pointer.
  const KernelRegistry::Entry* entry =
      registry.find(def.nodes[1].type_hash);  // soft_clip node
  REQUIRE(entry != nullptr);
  CHECK(def.nodes[1].kernel == entry->kernel);
}
