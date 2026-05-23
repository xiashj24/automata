#include <graph.hpp>
#include <filter.hpp>
#include <io.hpp>
#include <note_number.hpp>
#include <stream.hpp>
#include <units.hpp>
#include <pattern.hpp>

namespace automata {
using namespace literals;

// ported from https://supercollider.github.io/sc-140.html
// {LocalOut.ar(a=CombN.ar(BPF.ar(LocalIn.ar(2)*7.5+Saw.ar([32,33],0.2),2**LFNoise0.kr(4/3,4)*300,0.1).distort,2,2,40));a}.play
// it sounded completely different...

// auto in_signal = local_in();
// auto saw_osc = (saw(32_hz) + saw(33_hz)) * 0.1f;
// auto mix = in_signal * 7.5f + saw_osc;
// auto freq = (lf_noise0(1.33_hz) * 4.f).exp2() * 300.f;
// auto filtered = svf_bp(mix, freq, 0.9f);
// auto clipped = distort(filtered);
// auto comb = comb_n(clipped, 2_s, 40_s);
// auto out = local_out(comb);

Stream simple_fm(Stream freq, Stream fm_index) {
  auto modulator = osc(freq * fm_index);
  return osc(freq, modulator);
}

// 00
auto notes = range(60, 73, 1, Clock(120_bpm, 4.f));
auto out = simple_fm(note_number_to_frequency(notes), 1.f);

void render(std::span<float> buf) {
  out.render(buf);
}

}  // namespace automata
