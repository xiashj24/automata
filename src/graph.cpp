#include <graph.hpp>
#include <filter.hpp>
#include <io.hpp>
#include <stream.hpp>
#include <units.hpp>

namespace automata {
using namespace literals;

// ported from https://supercollider.github.io/sc-140.html
// {LocalOut.ar(a=CombN.ar(BPF.ar(LocalIn.ar(2)*7.5+Saw.ar([32,33],0.2),2**LFNoise0.kr(4/3,4)*300,0.1).distort,2,2,40));a}.play
// it sounded completely different...

// LocalIn.ar(2)
auto in_signal = local_in();

// Saw.ar([32, 33], 0.2) — stereo detuned saws, mul=0.2, summed to mono
auto saw_osc = (saw(32_hz) + saw(33_hz)) * 0.1f;

// mix = in * 7.5 + saw
auto mix = in_signal * 7.5f + saw_osc;

// freq = 2 ** LFNoise0.kr(4/3, 4) * 300 → stepped random in [~19, ~4800] Hz
auto freq = (lf_noise0(1.33_hz) * 4.f).exp2() * 300.f;

// BPF with rq=0.1: res = 1 - rq/2 = 0.95
auto filtered = svf_bp(mix, freq, 0.9f);

// .distort
auto clipped = distort(filtered);

// CombN.ar(bpf, 2, 2, 40)
auto comb = comb_n(clipped, 2_s, 40_s);

// LocalOut.ar(out)
auto out = local_out(comb);

void render(std::span<float> buf) {
  out.render(buf);
}

}  // namespace automata
