#include <graph.hpp>
#include <filter.hpp>
#include <stream.hpp>

namespace automata {

float base_freq = 110.0f;
float fm_index = 10.f;
float fm_depth = 5.f;
float lfo_freq = 0.3f;

SvfState svf_state;

auto lfo = osc(lfo_freq);

// 2-op phase modulation
auto modulator = osc(base_freq * fm_index);
auto carrier = osc(base_freq, modulator * fm_depth * lfo);
auto sawtooth = saw(base_freq);
auto svf_out = svf_lp(sawtooth, 2000.f + lfo * 1800.f, 0.9f, svf_state);

auto out = svf_out * modulator * 0.2f;

void render(std::span<float> buf) {
  out.render(buf);
}

}  // namespace automata
