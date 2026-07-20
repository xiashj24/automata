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
    const Signal osc =
        sine(440.f) + saw(110.f) - phasor(1.f) + sine(220.f, 0.25f);
    const Signal env = ar(metro(2.f), 0.01f, 0.1f);
    const Signal filtered = svf_lp(osc * env, 800.f, 0.7f) +
                            svf_bp(osc, 500.f, 2.f) + svf_hp(osc, 100.f, 1.f);
    const Signal wet =
        soft_clip(filtered / 2.f) + (-filtered) + fb.read(0.25f) * 0.5f;
    fb.write(wet);
    const Signal controlled = wet * param("gain", 0.5f) + mouse_x();
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
    node.kernel = infos.back().get();
    const KernelRegistry::Entry* entry = registry.find(node.kernel->type_hash);
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
