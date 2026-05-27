#pragma once

#include <memory>

#include <samplerate.hpp>
#include <v2/node.hpp>

namespace automata::v2 {

// ─── ImpulseNode
// ──────────────────────────────────────────────────────────
//
// Outputs 1.f for one sample at the start of each period, 0.f otherwise.
// Matches SC's Impulse.ar and sapf's `freq phase impulse`.
// Phase starts at 1.f so the impulse fires on the very first sample.

class ImpulseNode : public Node {
public:
  explicit ImpulseNode(Signal freq) : freq(freq) {}

  void reset() override {
    phase = 1.f;
    freq.reset();
  }

protected:
  float process() override {
    phase += freq.next() / SampleRate;
    if (phase >= 1.f) {
      phase -= 1.f;
      return 1.f;
    }
    return 0.f;
  }

private:
  Signal freq;
  float phase = 1.f;
};

// freq_hz may be any Signal — tempo modulation is free.
inline Signal impulse(Signal freq_hz) {
  return Signal(std::make_shared<ImpulseNode>(freq_hz));
}

// Convenience: BPM + subdivision rate (subdivisions per whole note).
// clock(120, 8) fires at eighth-note rate at 120 bpm.
inline Signal clock(float bpm, float rate) {
  return impulse(bpm * rate / 240.f);
}

}  // namespace automata::v2
