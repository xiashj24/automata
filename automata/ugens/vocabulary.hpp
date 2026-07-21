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
  registry.add_data_kernel(&detail::ParamInfo);
  registry.add_data_kernel(&detail::ClockInfo);
  registry.add_data_kernel(&detail::SeqInfo);
  registry.add_data_kernel(&detail::EuclidInfo);
  registry.add_data_kernel(&detail::StepInfo);

  const GraphDef probe = describe([](GraphBuilder& g) {
    const Signal osc = sine(440.f) + saw(110.f) - phasor(1.f) +
                       triangle(220.f) + tri(phasor(2.f)) +
                       simple_fm(110.f, 2.f);
    const Signal env =
        ar(metro(2.f), 0.01f, 0.1f) + are(pulsen(0.1f, 0.5f), 0.01f, 0.2f);
    const Signal filtered = svf_lp(osc * env, 800.f, 0.7f);
    const Signal shaped = soft_clip(filtered / 2.f) + (-filtered) +
                          hard_clip(bipolar(clip(unipolar(osc)))) +
                          curve(smooth(gain(osc, -6.f)), 2.f) + (osc >= 0.5f);
    const Clock c = beat();
    const Signal rhythm =
        (c / 2).swing(0.25f).trig() + c.gate(0.25f) + (c >> 0.5f).ramp() +
        seq(c, {1.f, 2.f}) + euclid(bar(), 3.f, 8.f) +
        step(c.trig(), {1.f, 2.f, 3.f}) + latch(osc, c.trig());
    g.out(shaped + rhythm * 0.f, shaped);
  });
  registry.add_probe(probe);
  return registry;
}

}  // namespace automata
