#pragma once

#include <cmath>
#include <cstdint>
#include <utility>

#include "automata/config.hpp"

// Musical time (ADR 0004): advances by samples rendered, never wall clock,
// so offline and live renders land on identical beats.

namespace automata {

enum class Quantize { Immediate, NextBeat, NextBar };

struct Transport {
  float bpm = 120.f;
  std::uint32_t beats_per_bar = 4;
  std::uint64_t sample_pos = 0;

  void advance(std::uint64_t nframes) { sample_pos += nframes; }

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
        return next_multiple(samples_per_beat());
      case Quantize::NextBar:
        return next_multiple(samples_per_beat() *
                             static_cast<double>(beats_per_bar));
    }
    std::unreachable();
  }

private:
  [[nodiscard]] std::uint64_t next_multiple(double period) const {
    const double index =
        std::floor(static_cast<double>(sample_pos) / period) + 1.0;
    return static_cast<std::uint64_t>(index * period);
  }
};

}  // namespace automata
