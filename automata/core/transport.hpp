#pragma once

#include <cmath>
#include <cstdint>
#include <utility>

#include "automata/config.hpp"

// Musical time (ADR 0004/0008): advances by samples rendered, never wall
// clock, so offline and live renders land on identical beats. beat_pos is
// the continuous position clocks re-derive from; a bpm change only alters
// future increments, so the grid never jumps.

namespace automata {

enum class Quantize { Immediate, NextBeat, NextBar };

struct Transport {
  float bpm = 120.f;
  std::uint32_t beats_per_bar = 4;
  std::uint64_t sample_pos = 0;
  double beat_pos = 0.0;

  void advance(std::uint64_t nframes) {
    sample_pos += nframes;
    beat_pos += static_cast<double>(nframes) / samples_per_beat();
  }

  [[nodiscard]] double samples_per_beat() const {
    return static_cast<double>(SampleRate) * 60.0 / static_cast<double>(bpm);
  }

  // The first gated sample at or after the current position; a swap lands at
  // the first block boundary past it.
  [[nodiscard]] std::uint64_t next_boundary(Quantize quantize) const {
    switch (quantize) {
      case Quantize::Immediate:
        return sample_pos;
      case Quantize::NextBeat:
        return sample_at(std::floor(beat_pos) + 1.0);
      case Quantize::NextBar: {
        const double bar = static_cast<double>(beats_per_bar);
        return sample_at((std::floor(beat_pos / bar) + 1.0) * bar);
      }
    }
    std::unreachable();
  }

private:
  [[nodiscard]] std::uint64_t sample_at(double beat) const {
    const double delta = (beat - beat_pos) * samples_per_beat();
    return sample_pos + static_cast<std::uint64_t>(std::llround(delta));
  }
};

}  // namespace automata
