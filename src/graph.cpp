#include "graph.hpp"

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include <envelope.hpp>
#include <filter.hpp>
#include <delay.hpp>
#include <note_number.hpp>
#include <pattern.hpp>
#include <stream.hpp>
#include <units.hpp>

using namespace automata;
using namespace literals;

namespace {

Stream simple_fm(Stream freq, Stream fm_index) {
  auto modulator = osc(freq * fm_index);
  return osc(freq, modulator);
}

// Square-wave arpeggio voice with filter envelope and chord transposition.
Stream arp_voice(Clock beat,
                 Clock chord_clock,
                 std::vector<float> degrees,
                 float root_note) {
  auto fundamental = seq({0.f, -4.f, -5.f}, chord_clock);
  auto voices = note_to_frequency(seq(degrees, beat) + fundamental + root_note);
  auto osc_width = osc(0.1_hz).unipolar() * 0.45f + 0.05f;
  auto sq = sqr(voices, osc_width);
  auto filter_env = lin_exp(asr(0.001f, 0.01f, 0.1f, beat.trigger()).vpow(3.f),
                            0.f, 1.f, 100.f, 5000.f);
  return svf_lp(sq, filter_env, 0.2f) * 0.2f;
}

// ported from https://supercollider.github.io/sc-140.html
// {LocalOut.ar(a=CombN.ar(BPF.ar(LocalIn.ar(2)*7.5+Saw.ar([32,33],0.2),2**LFNoise0.kr(4/3,4)*300,0.1).distort,2,2,40));a}.play
// it sounded completely different...

// Delay out_z;
// auto saw_osc = (saw(32_hz) + saw(33_hz)) * 0.1f;
// auto mix = out_z.read() * 7.5f + saw_osc;
// auto freq = (lf_noise_0(1.33_hz) * 4.f).exp2() * 300.f;
// auto filtered = svf_bp(mix, freq, 0.9f);
// auto clipped = distort(filtered);
// auto comb = comb_n(clipped, 2_s, 40_s);
// auto out = out_z.write(comb);

// 00
// auto notes = range(60, 73, 1, Clock(120_bpm, 4.f));
// auto out = simple_fm(note_to_frequency(notes), 1.f);

// 01 - isobar ex-basics
// Clock beat(120_bpm, 4);
// Clock sixteenth(120_bpm, 16);

// auto arpeggio = note_to_frequency(
//     scale<minor>(ping_pong({0, 2, 4, 6, 8, 10}, sixteenth), 72));
// auto amplitude = seq({50.f, 35.f, 25.f, 35.f}, sixteenth) +
//                  walk(-20.f, 20.f, 3.f, sixteenth);
// auto bassline =
//     note_to_frequency(stutter({0.f, 2.f, 7.f, 3.f}, 3, beat) +
//                              seq({0.f, 12.f, 24.f}, beat) + 36.f);

// auto out = simple_fm(arpeggio, 2.f) * (amplitude / 127.f) +
//            simple_fm(bassline, 1.f) * 0.3f;

// 03 - isobar ex-euclid
// Clock beat(100_bpm, 4);
// auto out = (osc(note_to_frequency(60)) * euclid(5, 8, beat.at(16)) +
//             osc(note_to_frequency(62)) * euclid(5, 13, beat.at(8)) +
//             osc(note_to_frequency(64)) * euclid(7, 15, beat.at(8)) +
//             osc(note_to_frequency(67)) * euclid(6, 19, beat.at(16)) +
//             osc(note_to_frequency(71)) * euclid(7, 23, beat.at(8))) *
//            0.15f;

// 04 - isobar ex-permutations
// Clock beat(60_bpm, 4);
// auto pitch = note_to_frequency(
//     scale<major_penta>(permut({-3, -2, -1, 0, 1, 2}, beat), 60));
// auto pitch_lo = note_to_frequency(
//     scale<major_penta>(permut({-3, -2, -1, 0, 1, 2}, beat / 2), 48));
// auto out = (simple_fm(pitch, 1.5f) + simple_fm(pitch_lo, 2.f)) * 0.2f;

// 05 - isobar ex-piano-phase (Steve Reich, 1967)
Clock beat(160_bpm, 8);
const std::vector<float> piano_phase = {-7, -5, 0, 2,  3, -5,
                                        -7, 2,  0, -5, 3, 2};
auto voice1 = note_to_frequency(seq(piano_phase, beat) + 60);
auto voice2 = note_to_frequency(seq(piano_phase, beat / 1.01f) + 72);
auto out = (simple_fm(voice1, 1.f) + simple_fm(voice2, 1.f)) * 0.3f;

// 06 - isobar ex-walk
// Clock beat(170_bpm, 16);
// auto notes = note_to_frequency(scale<hex_minor>(walk(-8, 16, 2, beat),
// 60)); auto amplitude = seq({40.f, 30.f, 20.f, 25.f}, beat) +
//                  walk(-10, 10, 2, beat, 0.f, 2u);
// auto out = osc(notes) * (amplitude / 127.f);

// 07 - isobar ex-static-pattern (Cm → Gm → Bb → F)
// Clock beat(120_bpm, 4);
// Clock chord(120_bpm, 1);
// const std::vector<float> roots = {0, 7, -2, 5};  // C G Bb F
// auto melody = note_to_frequency(
//     scale<minor>(walk(0, 6, 2, beat), seq(roots, chord) + 60));
// auto bass = note_to_frequency(seq(roots, chord) + 48);
// auto out = (osc(melody) + osc(bass)) * 0.3f;

// 10 - isobar ex-lsystem-stochastic
// Clock beat(180_bpm, 16);

// auto pitch = scale<major_penta>(
//     wrap(lsystem("N[+N--?N]+N[+?N]", 4, beat), 0, 36), 50);
// auto out = osc(note_to_frequency(pitch)) * 0.4f;

// 11 - isobar ex-lsystem-rhythm
// Clock beat(120_bpm, 16);
// auto notes = lsystem("N+[+N+N--N+N]+N[++N]", 4, beat) + 60;
// auto out = osc(note_to_frequency(notes)) * 0.4f;

// signalflow: modulation-example
// Triangle wave with S&H frequency and ASR envelope, into comb delay.
// Clock impulse(120_bpm, 16);  // 8 triggers/sec
// auto mod_freq = lin_exp(saw(0.2_hz).unipolar(), 0.f, 1.f, 200.f, 2000.f);
// auto sample_held = hold(mod_freq, impulse.trigger());
// auto signal =
//     tri(sample_held) * 0.5f * asr(0.001f, 0.001f, 0.1f, impulse.trigger());
// auto out = comb_n(signal, 0.1f, 3.1f) * 0.5f;

// signalflow: chaotic-feedback
// op0 self-modulates via 1-sample-delayed feedback; op1 is modulated by op0.
// Clock chaotic_clock(60_bpm, 4);
// auto chaotic_f0 = rand_exp(40.f, 2000.f, chaotic_clock.trigger());
// auto chaotic_level =
//     lin_exp(lf_noise_0(0.5_hz).unipolar(), 0.f, 1.f, 0.0001f, 1.f);
// Delay op0_z;
// auto op0 =
//     op0_z.write(osc(chaotic_f0 * (1.f + op0_z.read() * chaotic_level
//     * 14.f)));
// auto out = osc(chaotic_f0 * (1.f + op0 * 14.f)) * 0.3f;

// signalflow: sequencing-example
// Three square-wave arpeggios with filter envelopes + hi-hat.
// Clock seq_beat(110_bpm, 16);
// Clock seq_chord(110_bpm, 1);  // chord change every 4 beats
// auto bass = arp_voice(seq_beat, seq_chord, {0, 0, 0, 0, 7, 12, 0, 7}, 36.f);
// auto mid = arp_voice(seq_beat, seq_chord, {3, 2, 0, -2, 0}, 60.f);
// auto high = arp_voice(seq_beat, seq_chord, {7, 3, 5, 2, 3, 0}, 72.f);
// auto hihat_env = asr(0.001f, 0.01f, 0.05f, seq_beat.trigger());
// auto hihat_amp = seq({0.1f, 0.25f, 0.5f, 0.1f}, seq_beat);
// auto hihat = svf_hp(noise(), 8000.f, 0.f) * hihat_env * hihat_amp;
// auto out = (bass + mid + high + hihat) * 0.5f;

// sc ch7_08: RLPF-Saw motif loop (C4 E4 F4 C5, rhythm 4:1:1:2 in 16th notes)
// Clock motif_clock(120_bpm, 16);  // 16th notes
// auto motif_notes =
//     note_to_frequency(seq({60, 60, 60, 60, 64, 65, 72, 72}, motif_clock));
// auto motif_cutoff = lf_noise_1(1.0_hz).unipolar() * 3800.f + 200.f;
// auto out = svf_lp(saw(motif_notes), motif_cutoff, 0.7f) * 0.25f;

// sc ch7_11: two-voice polyphony — bass RLPF-Saw at 8th notes, sine at 16th
// Clock bass_clock(132_bpm, 8);     // half-beat (8th notes)
// Clock treble_clock(132_bpm, 16);  // quarter-beat (16th notes)
// auto bass_notes = note_to_frequency(seq({60, 63, 65, 67}, bass_clock));
// auto treble_notes = note_to_frequency(seq({70, 72, 75, 77}, treble_clock));
// auto bass_cutoff = lf_noise_1(0.5_hz).unipolar() * 3000.f + 300.f;
// auto bass_filtered = svf_lp(saw(bass_notes), bass_cutoff, 0.5f);
// auto out = (bass_filtered + osc(treble_notes)) * 0.3f;

// sc ch7_27: hirajoshi scale, random degree + octave, RLPF-Saw
// Clock clock(120_bpm, 8);  // 8th notes
// auto hira_degree = xchoose({0, 1, 2, 3, 4}, clock);
// auto hira_octave = xchoose({36, 48, 60}, clock, 2u);
// auto hira_notes = note_to_frequency(scale<hirajoshi>(hira_degree,
// hira_octave)); auto hira_cutoff = lf_noise_1(1.0_hz).unipolar() * 4000.f +
// 200.f; auto out = svf_lp(saw(hira_notes), hira_cutoff, 0.3f) * 0.3f;

// sc ch8_22/23: ritusen scale, random degree + octave, random filter cutoff
// Clock clock(120_bpm, 8);  // 8th notes
// auto ritu_degree = xchoose({0, 1, 2, 3, 4, 5, 6, 7}, clock);
// auto ritu_octave = xchoose({36, 48, 60}, clock, 2u);
// auto ritu_notes = note_to_frequency(scale<ritusen>(ritu_degree,
// ritu_octave)); auto ritu_cutoff = rand_exp(200.f, 15000.f, clock.trigger(),
// 3u); auto out = svf_lp(saw(ritu_notes), ritu_cutoff, 0.3f) * 0.25f;

// sc ch8_26: FM synthesis with hex_major6 scale, sequenced index and ratio
// Clock fm_clock(120_bpm, 4);  // quarter notes
// auto fm_degree = choose({0, 1, 2, 3, 4, 5}, fm_clock);
// auto fm_octave = choose({48, 60, 72}, fm_clock, 2u);
// auto fm_notes = note_to_frequency(scale<hex_major6>(fm_degree, fm_octave));
// auto fm_index = choose({0.f, 1.f, 10.f, 40.f}, fm_clock, 3u);
// auto out = simple_fm(fm_notes, fm_index) * 0.3f;

}  // namespace

void render(std::span<float> buf) {
  out.render(buf);
}
