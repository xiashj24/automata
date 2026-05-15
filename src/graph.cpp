#include <graph.hpp>
#include <filter.hpp>
#include <stream.hpp>

namespace automata {

float base_freq = 110.0f;
float fm_index = 10.f;
float fm_depth = 5.f;
float lfo_freq = 0.3f;
float lfo_res_freq = 0.7f;
float amp = 0.2f;

float cutoff = 2000.f;
float cutoff_mod_depth = 1800.f;

SvfState svf_state;

auto lfo = osc(lfo_freq);
auto lfo_res = to_unipolar(osc(lfo_res_freq));

auto modulator = osc(base_freq * fm_index);
auto carrier = osc(base_freq, modulator * fm_depth * lfo);
auto sawtooth = saw(base_freq);

auto svf_out =
    svf_lp(sawtooth, cutoff + lfo * cutoff_mod_depth, lfo_res, svf_state);

auto out = svf_out * modulator * amp;

void render(std::span<float> buf) {
  out.render(buf);
}

}  // namespace automata
