#include <graph.hpp>
#include <filter.hpp>
#include <stream.hpp>
#include <tempo.hpp>
#include <units.hpp>

namespace automata {
using namespace literals;

constexpr float base_freq = 110_hz;
constexpr float fm_index = 2.f;
constexpr float fm_depth = 5.f;
constexpr float lfo_freq = 0.3_hz;
constexpr float lfo_res_freq = 0.7_hz;
constexpr float amp = 0.2f;
constexpr float cutoff = 2_khz;
constexpr float cutoff_mod_depth = 1800.f;

float tempo = 130_bpm;

SvfState svf_state;
float slew_state;

auto lfo = osc(lfo_freq);
auto lfo_res = osc(lfo_res_freq).to_unipolar();

auto rhythmic_modulation = slew(metro(&tempo), 1.f, 0.001f, slew_state);

// auto noise_out = noise(1) * 100.f;

auto modulator = osc(base_freq * fm_index * (1 + rhythmic_modulation));
auto carrier = osc(base_freq, modulator * fm_depth * rhythmic_modulation);
auto sawtooth = saw(base_freq);

auto svf_out =
    svf_lp(sawtooth, cutoff + lfo * cutoff_mod_depth, lfo_res, svf_state);

SvfState svf_state_2;

auto noise_bpf = svf_bp(noise(1), osc(0.5f) * 200.f + 500.f, 0.5f, svf_state_2);
// auto out = svf_out * modulator * amp;
auto out = carrier + noise_bpf;

void render(std::span<float> buf) {
  out.render(buf);
}

}  // namespace automata
