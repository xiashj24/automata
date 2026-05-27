#include "graph.hpp"

#include <vector>

#include <clock.hpp>
#include <delay.hpp>
#include <dynamics.hpp>
#include <envelope.hpp>
#include <euclid.hpp>
#include <filter.hpp>
#include <note_number.hpp>
#include <ops.hpp>
#include <pattern.hpp>
#include <random.hpp>
#include <stream.hpp>
#include <units.hpp>
#include <v2/clock.hpp>
#include <v2/node.hpp>
#include <v2/osc.hpp>
#include <v2/pattern.hpp>

using namespace automata;
using namespace literals;

namespace {

Stream simple_fm(Stream freq, Stream fm_index) {
  auto modulator = osc(freq * fm_index);
  return osc(freq, modulator);
}

Stream arp_voice(Clock beat,
                 Clock chord_clock,
                 std::vector<float> degrees,
                 float root_note) {
  auto fundamental = seq({0, -4, -5}, chord_clock);
  auto voices = note_to_frequency(seq(degrees, beat) + fundamental + root_note);
  auto osc_width = osc(0.1_hz).unipolar() * 0.45f + 0.05f;
  auto sq = sqr(voices, osc_width);
  auto filter_env =
      lin_exp(asr(0.001_s, 0.01_s, 0.1_s, beat.trigger()).vpow(3.f), 0.f, 1.f,
              100_hz, 5000_hz);
  return svf_lp(sq, filter_env, 0.2f).gain(-12_db);
}

// ─── Patches
// ──────────────────────────────────────────────────────────────────

// 440 Hz sine at −12 dB
Stream patch_hello_world() {
  return osc(440_hz).gain(-12_db);
}

// signalflow: euclidean-rhythm-example — four Euclidean voices, tanh-clipped,
// ping-pong delay (allpass + tap).
// https://signalflow.dev/examples/euclidean-rhythm/
Stream patch_euclidean_rhythm() {
  Clock clock(120_bpm, 16);
  auto voice = [&](float div, uint32_t steps, uint32_t pulses, float cutoff,
                   float res, float amp) {
    return svf_lp(euclid(pulses, steps, clock / div), cutoff, res) * amp;
  };
  auto mix = voice(2, 23, 7, 80_hz, 0.99f, 10.f) +
             voice(3, 13, 9, 800_hz, 0.98f, 0.2f) +
             voice(4, 16, 11, 8000_hz, 0.97f, 0.05f) +
             voice(2, 19, 12, 480_hz, 0.99f, 0.2f);
  auto clipped = tanh(mix * 10.f);
  auto ap = allpass_delay(clipped, 0.125_s, 0.7f);
  auto tap = delay_n(ap, 1.f / 16.f);
  return clipped * 0.7f + (ap + tap) * 0.15f;
}

// ported from https://supercollider.github.io/sc-140.html
// SC:
// {LocalOut.ar(a=CombN.ar(BPF.ar(LocalIn.ar(2)*7.5+Saw.ar([32,33],0.2),2**LFNoise0.kr(4/3,4)*300,0.1).distort,2,2,40));a}.play
Stream patch_sc_comb() {
  Delay out_z;
  auto saw_osc = (saw(32_hz) + saw(33_hz)) * 0.2f;
  auto mix = out_z.read() * 7.5f + saw_osc;
  auto freq = (lf_noise_0(1.333_hz) * 4.f).exp2() * 300_hz;
  auto filtered = svf_bp(mix, freq, 0.95f);
  auto clipped = distort(filtered);
  auto comb = comb_n(clipped, 2_s, 40_s, 2_s);
  return out_z.write(comb).gain(-12_db);
}

Stream patch_fm() {
  namespace v2 = automata::v2;
  auto notes = range(60, 73, 1, Clock(120_bpm, 4));
  v2::Signal freq = v2::from_stream(note_to_frequency(notes));
  return v2::simple_fm(freq, 1.f).to_stream();
}

// isobar ex-basics
// https://github.com/ideoforms/isobar/blob/master/examples/ex-basics.py
Stream patch_basics() {
  Clock beat(120_bpm, 4);
  Clock sixteenth(120_bpm, 16);
  auto arpeggio = note_to_frequency(
      scale<minor>(ping_pong({0, 2, 4, 6, 8, 10}, sixteenth), 72));
  auto amplitude =
      seq({50, 35, 25, 35}, sixteenth) + walk(-20, 20, 3, sixteenth);
  auto bassline = note_to_frequency(stutter({0, 2, 7, 3}, 3, beat) +
                                    seq({0, 12, 24}, beat) + 36);
  return simple_fm(arpeggio, 2.f) * (amplitude / 127.f) +
         simple_fm(bassline, 1.f) * 0.3f;
}

// isobar ex-euclid
// https://github.com/ideoforms/isobar/blob/master/examples/ex-euclidean-rhythm.py
Stream patch_euclid() {
  Clock beat(100_bpm, 4);
  return (osc(note_to_frequency(60)) * euclid(5, 8, beat.at(16)) +
          osc(note_to_frequency(62)) * euclid(5, 13, beat.at(8)) +
          osc(note_to_frequency(64)) * euclid(7, 15, beat.at(8)) +
          osc(note_to_frequency(67)) * euclid(6, 19, beat.at(16)) +
          osc(note_to_frequency(71)) * euclid(7, 23, beat.at(8))) *
         0.15f;
}

// isobar ex-permutations
// https://github.com/ideoforms/isobar/blob/master/examples/ex-permutations.py
Stream patch_permutations() {
  Clock beat(60, 4);
  auto pitch = note_to_frequency(
      scale<major_penta>(permut({-3, -2, -1, 0, 1, 2}, beat), 60));
  auto pitch_lo = note_to_frequency(
      scale<major_penta>(permut({-3, -2, -1, 0, 1, 2}, beat / 2), 48));
  return (simple_fm(pitch, 1.5f) + simple_fm(pitch_lo, 2.f)).gain(-18_db);
}

// isobar ex-piano-phase (Steve Reich, 1967)
// https://github.com/ideoforms/isobar/blob/master/examples/ex-piano-phase.py
Stream patch_piano_phase() {
  Clock beat(160_bpm, 8);
  const std::vector<float> piano_phase = {-7, -5, 0, 2,  3, -5,
                                          -7, 2,  0, -5, 3, 2};
  auto voice1 = note_to_frequency(seq(piano_phase, beat) + 60);
  auto voice2 = note_to_frequency(seq(piano_phase, beat / 1.01f) + 72);
  return (simple_fm(voice1, 1.f) + simple_fm(voice2, 1.f)).gain(-18_db);
}

Stream patch_piano_phase_v2() {
  using namespace v2;
  const std::vector<float> piano_phase = {-7, -5, 0, 2,  3, -5,
                                          -7, 2,  0, -5, 3, 2};
  auto beat1 = clock(160.f, 8.f);
  auto beat2 = impulse(160.f * 8.f / (240.f * 1.01f));
  auto voice1 = note_to_frequency(seq(piano_phase, beat1) + 60.f);
  auto voice2 = note_to_frequency(seq(piano_phase, beat2) + 72.f);
  return (simple_fm(voice1, 1.f) + simple_fm(voice2, 1.f))
      .to_stream()
      .gain(-18_db);
}

// isobar ex-walk
// https://github.com/ideoforms/isobar/blob/master/examples/ex-walk.py
Stream patch_walk() {
  Clock beat(170_bpm, 16);
  auto notes = note_to_frequency(scale<hex_minor>(walk(-8, 16, 2, beat), 60));
  auto amplitude =
      seq({40.f, 30.f, 20.f, 25.f}, beat) + walk(-10, 10, 2, beat, 0.f, 2u);
  return osc(notes) * (amplitude / 127.f);
}

// isobar ex-static-pattern (Cm → Gm → Bb → F)
// https://github.com/ideoforms/isobar/blob/master/examples/ex-static-pattern.py
Stream patch_static_pattern() {
  Clock beat(120_bpm, 4);
  Clock chord(120_bpm, 1);
  const std::vector<float> roots = {0, 7, -2, 5};
  auto melody = note_to_frequency(
      scale<minor>(walk(0, 6, 2, beat), seq(roots, chord) + 60));
  auto bass = note_to_frequency(seq(roots, chord) + 48);
  return (osc(melody) + osc(bass)) * 0.3f;
}

// isobar ex-lsystem-stochastic
// https://github.com/ideoforms/isobar/blob/master/examples/ex-lsystem-stochastic.py
Stream patch_lsystem_stochastic() {
  Clock beat(180_bpm, 16);
  auto pitch =
      scale<major_penta>(wrap(lsystem("N[+N--?N]+N[+?N]", 4, beat), 0, 36), 50);
  return osc(note_to_frequency(pitch)) * 0.4f;
}

// isobar ex-lsystem-rhythm
// https://github.com/ideoforms/isobar/blob/master/examples/ex-lsystem-rhythm.py
Stream patch_lsystem_rhythm() {
  Clock beat(120_bpm, 16);
  auto notes = lsystem("N+[+N+N--N+N]+N[++N]", 4, beat) + 60;
  return osc(note_to_frequency(notes)) * 0.4f;
}

// signalflow: karplus-strong-example — random-freq string.
// https://signalflow.dev/examples/karplus-strong/
Stream patch_karplus_strong() {
  Clock clock(80_bpm, 4);
  auto freq = rand_exp(50.f, 800.f, clock.trigger());
  auto excitation = noise() * asr(0.f, 0.002f, 0.001f, clock.trigger());
  return karplus_strong(excitation, freq) * 0.5f;
}

// signalflow: audio-through-example — AudioIn unavailable; noise + comb delay.
// https://signalflow.dev/examples/audio-through/
Stream patch_audio_through() {
  auto signal = noise() * 0.1f;
  return signal + comb_n(signal, 0.1f, 3.1f) * 0.8f;
}

// signalflow: buffer-play-example — BufferPlayer requires file I/O;
// approximated with a band-limited saw tone.
// https://signalflow.dev/examples/buffer-play/
Stream patch_buffer_play() {
  return svf_lp(saw(110), 2000.f, 0.3f) * 0.4f;
}

// signalflow: granulation-example — Granulator requires a sample buffer;
// approximated with overlapping noise grains at varying clock rates.
// https://signalflow.dev/examples/granulation/
Stream patch_granulation() {
  Clock g1(120_bpm, 16), g2(120_bpm, 12);
  auto grain = [](Clock c) {
    return noise() * asr(0.01f, 0.1f, 0.08f, c.trigger());
  };
  return (grain(g1) + grain(g2)) * 0.3f;
}

// signalflow: midi-fm-voicer-example — 3-op DX7-style FM with LFO, sequenced.
// https://signalflow.dev/examples/midi-fm-voicer/
// DX7-style exp amplitude: level in [0,1] → amplitude in [0.0005, 1].
// fm_in is a phase modulation signal from a prior operator.
Stream fm_op(Stream f0,
             float coarse,
             float level,
             float attack,
             float sustain_s,
             float release,
             Stream gate,
             Stream fm_in = 0.f) {
  float amp = std::exp(std::log(0.0005f) + level * std::log(2000.f));
  return osc(f0 * coarse, fm_in * 14.f) *
         asr(attack, sustain_s, release, gate).vpow(2.f) * amp;
}

Stream patch_fm_voicer() {
  Clock beat(100_bpm, 4);
  auto gate = beat.trigger();
  auto degree = choose({0.f, 2.f, 4.f, 7.f, 9.f, 11.f}, beat);
  auto freq = note_to_frequency(scale<major>(degree, 60.f));
  auto lfo = tri(4);
  auto f = freq * (1.f + 0.01f * lfo);
  auto op2 = fm_op(f, 9.1f, 0.95f, 0.0f, 0.2f, 0.3f, gate);
  auto op1 = fm_op(f, 2.0f, 0.75f, 0.0f, 0.4f, 0.5f, gate, op2);
  auto op0 = fm_op(f, 1.0f, 1.0f, 0.0f, 0.3f, 0.7f, gate, op1 + op2);
  return (op0 + op1) * 0.1f * (1.f + lfo * 0.2f);
}

// signalflow: midi-keyboard-example — MIDI + bit-crushing omitted; sequenced
// NotePatch signal chain: saw → SVF LP with envelope + LFO cutoff sweep.
// https://signalflow.dev/examples/midi-keyboard/
Stream patch_midi_keyboard() {
  Clock beat(120_bpm, 8);
  auto degree = seq({0.f, 2.f, 4.f, 7.f, 9.f, 7.f, 4.f, 2.f}, beat);
  auto freq = note_to_frequency(scale<major>(degree, 60.f));
  auto gate = beat.trigger();
  auto env = asr(0.001f, 0.3f, 0.2f, gate);
  auto filter_env = lin_exp(env.vpow(4.f), 0.f, 1.f, 800.f, 8000.f);
  auto lfo_rate = lf_noise_0(0.5).unipolar() * 5.f + 3.f;
  auto filter_lfo = osc(lfo_rate).unipolar() * 0.3f + 0.7f;
  auto res = lf_noise_0(0.3).unipolar() * 0.49f + 0.5f;
  return svf_lp(saw(freq), filter_env * filter_lfo, res) * env * 0.2f;
}

// signalflow: wavetable-2d-example — Buffer2D/Wavetable2D approximated by
// crossfading sqr↔saw with an LFO.
// https://signalflow.dev/examples/wavetable-2d/
Stream patch_wavetable_2d() {
  auto hz = note_to_frequency(osc(0.1).unipolar() + 60.f);
  auto crossfade = osc(0.5).unipolar();
  return (sqr(hz, 0.5f) * (1.f - crossfade) + saw(hz) * crossfade) * 0.1f;
}

// signalflow: modulation-example — triangle wave with S&H freq + ASR, into comb
// https://signalflow.dev/examples/modulation/
Stream patch_modulation() {
  Clock impulse(120_bpm, 16);
  auto mod_freq = lin_exp(saw(0.2).unipolar(), 0.f, 1.f, 200.f, 2000.f);
  auto sample_held = hold(mod_freq, impulse.trigger());
  auto signal =
      tri(sample_held) * 0.5f * asr(0.001f, 0.001f, 0.1f, impulse.trigger());
  return comb_n(signal, 0.1f, 3.1f) * 0.5f;
}

// signalflow: chaotic-feedback — FM self-modulation via 1-sample delay
// https://signalflow.dev/examples/chaotic-feedback/
Stream patch_chaotic_feedback() {
  Clock chaotic_clock(60_bpm, 4);
  auto chaotic_f0 = rand_exp(40.f, 2000.f, chaotic_clock.trigger());
  auto chaotic_level =
      lin_exp(lf_noise_0(0.5).unipolar(), 0.f, 1.f, 0.0001f, 1.f);
  Delay op0_z;
  auto op0 = op0_z.write(
      osc(chaotic_f0 * (1.f + op0_z.read() * chaotic_level * 14.f)));
  return osc(chaotic_f0 * (1.f + op0 * 14.f)) * 0.3f;
}

// signalflow: sequencing-example — three square-wave arpeggios + hi-hat
// https://signalflow.dev/examples/sequencing/
Stream patch_sequencing() {
  Clock seq_beat(110_bpm, 16);
  Clock seq_chord(110_bpm, 1);
  auto bass = arp_voice(seq_beat, seq_chord, {0, 0, 0, 0, 7, 12, 0, 7}, 36.f);
  auto mid = arp_voice(seq_beat, seq_chord, {3, 2, 0, -2, 0}, 60.f);
  auto high = arp_voice(seq_beat, seq_chord, {7, 3, 5, 2, 3, 0}, 72.f);
  auto hihat_env = asr(0.001f, 0.01f, 0.05f, seq_beat.trigger());
  auto hihat_amp = seq({0.1f, 0.25f, 0.5f, 0.1f}, seq_beat);
  auto hihat = svf_hp(noise(), 8000.f, 0.f) * hihat_env * hihat_amp;
  return (bass + mid + high + hihat) * 0.5f;
}

// sc ch7_08: RLPF-Saw motif loop (C4 E4 F4 C5, rhythm 4:1:1:2 in 16th notes)
// The SuperCollider Book, ch.7
Stream patch_sc_motif() {
  Clock motif_clock(120_bpm, 16);
  auto motif_notes =
      note_to_frequency(seq({60, 60, 60, 60, 64, 65, 72, 72}, motif_clock));
  auto motif_cutoff = lf_noise_1(1.0).unipolar() * 3800.f + 200.f;
  return svf_lp(saw(motif_notes), motif_cutoff, 0.7f) * 0.25f;
}

// sc ch7_11: two-voice polyphony — bass RLPF-Saw at 8th notes, sine at 16th
// The SuperCollider Book, ch.7
Stream patch_sc_polyphony() {
  Clock bass_clock(132_bpm, 8);
  Clock treble_clock(132_bpm, 16);
  auto bass_notes = note_to_frequency(seq({60, 63, 65, 67}, bass_clock));
  auto treble_notes = note_to_frequency(seq({70, 72, 75, 77}, treble_clock));
  auto bass_cutoff = lf_noise_1(0.5).unipolar() * 3000.f + 300.f;
  auto bass_filtered = svf_lp(saw(bass_notes), bass_cutoff, 0.5f);
  return (bass_filtered + osc(treble_notes)) * 0.3f;
}

// sc ch7_27: hirajoshi scale, random degree + octave, RLPF-Saw
// The SuperCollider Book, ch.7
Stream patch_hirajoshi() {
  Clock clock(120_bpm, 8);
  auto hira_degree = xchoose({0, 1, 2, 3, 4}, clock);
  auto hira_octave = xchoose({36, 48, 60}, clock, 2u);
  auto hira_notes =
      note_to_frequency(scale<hirajoshi>(hira_degree, hira_octave));
  auto hira_cutoff = lf_noise_1(1.0).unipolar() * 4000.f + 200.f;
  return svf_lp(saw(hira_notes), hira_cutoff, 0.3f) * 0.3f;
}

// sc ch8_22/23: ritusen scale, random degree + octave, random filter cutoff
// The SuperCollider Book, ch.8
Stream patch_ritusen() {
  Clock clock(120_bpm, 8);
  auto ritu_degree = xchoose({0, 1, 2, 3, 4, 5, 6, 7}, clock);
  auto ritu_octave = xchoose({36, 48, 60}, clock, 2);
  auto ritu_notes = note_to_frequency(scale<ritusen>(ritu_degree, ritu_octave));
  auto ritu_cutoff = rand_exp(200, 15000, clock.trigger(), 3);
  return svf_lp(saw(ritu_notes), ritu_cutoff, 0.3f).gain(-12_db);
}

// sc ch8_26: FM synthesis with hex_major6 scale, sequenced index and ratio
// The SuperCollider Book, ch.8
Stream patch_fm_scale() {
  Clock fm_clock(120_bpm, 4);
  auto fm_degree = choose({0, 1, 2, 3, 4, 5}, fm_clock);
  auto fm_octave = choose({48, 60, 72}, fm_clock, 2);
  auto fm_notes = note_to_frequency(scale<hex_major6>(fm_degree, fm_octave));
  auto fm_index = choose({0, 1, 10, 40}, fm_clock, 3);
  return simple_fm(fm_notes, fm_index).gain(-12_db);
}

// Stochastic Garden: evolving ambient patch that exercises every new UGen.
//
// Structure:
//   Bass   — saw → fold → Moog VCF (logistic chaos cutoff) → gated ADSR
//   Melody — probabilistic (rand_coin), Gaussian pitch drift, tremolo via abs
//   Perc   — Poisson impulse (rand_impulse), rect_env, clip
//   Pad    — pink noise → SVF LP (mod-wrapped sweep + line accent)
//   Output — side-chain duck from bass RMS, compressor, dc_filter
Stream patch_stochastic_garden() {
  constexpr float pi = std::numbers::pi_v<float>;
  Clock beat(85_bpm, 4);
  Clock sixteenth(85_bpm, 16);

  // Quadrature LFO pair via sin/cos of a common saw phase ramp.
  auto lfo_phase = saw(0.08) * pi;  // [-π, +π] linear ramp
  auto lfo_s = sin(lfo_phase);      // sine  LFO  at 0.08 Hz
  auto lfo_c = cos(lfo_phase);      // cosine LFO, 90° offset

  // ── Bass ──────────────────────────────────────────────────────────────
  // logistic chaos → lin_lin → Moog VCF cutoff for timbral drift
  auto chaos = logistic(3.97f, 0.4f);
  auto bass_cut = lin_lin(chaos, -1.f, 1.f, 100.f, 2400.f);
  auto bass_note = seq({36.f, 36.f, 43.f, 41.f, 36.f, 38.f, 43.f, 41.f}, beat);
  auto bass_gate = rect_env(0.3f, beat.trigger());
  auto bass_env = adsr(0.004f, 0.15f, 0.55f, 0.4f, bass_gate);
  auto bass_raw = fold(saw(note_to_frequency(bass_note)) * 1.8f);
  auto bass = dc_filter(moog_vcf(bass_raw, bass_cut, 0.38f)) * bass_env;

  // ── Melody ────────────────────────────────────────────────────────────
  // rand_coin gates: ~38% of sixteenth notes trigger a note
  auto mel_trig = rand_coin(0.38f, sixteenth.trigger());
  auto mel_deg = xchoose({0.f, 2.f, 4.f, 7.f, 9.f, 11.f}, sixteenth);
  // gaussian drift → round keeps pitches on semitone grid
  auto mel_drift = mel_deg + gaussian_noise(0.f, 0.18f);
  auto mel_pitch = scale<major>(round(mel_drift), 60.f);
  auto mel_env = adsr(0.002f, 0.06f, 0.55f, 0.28f, mel_trig);
  // pow sharpens the attack transient; abs(lfo_s) gives pulsing tremolo
  auto tremolo = abs(lfo_s) * 0.4f + 0.6f;
  auto mel_hz = note_to_frequency(mel_pitch);
  auto mel = osc(mel_hz) * pow(mel_env, 1.5f) * tremolo;

  // ── Percussion ────────────────────────────────────────────────────────
  // lfo_c slowly sweeps the Poisson hit rate between 2 and 5 Hz
  auto hit_rate = lin_lin(lfo_c, -1.f, 1.f, 2.f, 5.f);
  auto perc_trig = rand_impulse(hit_rate);
  auto perc_env = rect_env(0.018f, perc_trig);
  auto perc = clip(noise() * 12.f) * perc_env;

  // ── Pad ───────────────────────────────────────────────────────────────
  // mod wraps a slow ramp into [0,1) five times per LFO period →
  // repeating sawtooth sweep of filter cutoff (~3 s per sweep)
  auto sweep = mod(osc(0.04).unipolar() * 5.f, 1.f);
  auto sweep_cut = lin_lin(sweep, 0.f, 1.f, 200.f, 4000.f);
  // line adds a beat-sync accent sweep mixed in at 40%
  auto accent = line(600.f, 3200.f, 0.8f, beat.trigger());
  auto pad = svf_lp(pink_noise(), sweep_cut * 0.6f + accent * 0.4f, 0.15f);

  // ── Side-chain duck ───────────────────────────────────────────────────
  // rms + amp_to_db measures bass level; lin_lin maps it to a duck gain
  auto bass_db = amp_to_db(rms(bass, 0.04f));
  auto duck = lin_lin(bass_db, -36_db, -6_db, 1.f, 0.25f);

  // ── Mix ───────────────────────────────────────────────────────────────
  auto mix = bass.gain(-4_db) + mel.gain(-10_db) + perc.gain(-13_db) +
             (pad * duck).gain(-5_db);
  return compressor(mix, -18.f, 3.f, 0.008f, 0.12f);
}

}  // namespace

void define_graph(Graph& g) {
  g.add_output("main", patch_piano_phase_v2());
}
