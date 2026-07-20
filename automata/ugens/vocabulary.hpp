#pragma once

#include "automata/graph/builder.hpp"
#include "automata/graph/rebind.hpp"
#include "automata/graph/tap.hpp"
#include "automata/ugens/ugens.hpp"

// The host's rebinding registry: the graph-owned data kernels plus one probe
// describe that instantiates every factory in ugens.hpp, harvesting each
// canonical KernelInfo and setter binding from this binary. A factory added
// to ugens.hpp must join the probe — a forgotten one still plays correctly
// but pins the generations of the patches that use it.

namespace automata {

[[nodiscard]] inline KernelRegistry vocabulary_registry() {
  KernelRegistry registry;
  registry.add_data_kernel(&detail::ConstInfo);
  registry.add_data_kernel(&detail::TapWriteInfo);
  registry.add_data_kernel(&detail::TapReadInfo);

  const GraphDef probe = describe([](GraphBuilder& g) {
    const Signal osc = sine(440.f) + saw(110.f) - phasor(1.f);
    const Signal env = ar(metro(2.f), 0.01f, 0.1f);
    const Signal filtered = svf_lp(osc * env, 800.f, 0.7f);
    const Signal shaped = soft_clip(filtered / 2.f) + (-filtered);
    g.out(shaped, shaped);
  });
  registry.add_probe(probe);
  return registry;
}

}  // namespace automata
