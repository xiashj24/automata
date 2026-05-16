#pragma once

#include <stream.hpp>
#include <samplerate.hpp>

/**
 * @brief sequencer in audio thread
 *
 */

// one pole lag filter
// [[nodiscard]] inline Stream lag(Stream x, Stream a, float& yz) {
//   return Stream([x = std::move(x), a = std::move(a), &yz]() mutable {
//     float ac = std::clamp(a.next(), 0.f, 1.f);
//     float xn = x.next();
//     float yn = (1.f - ac) * xn + ac * yz;
//     yz = yn;
//     return yn;
//   });
// }

namespace automata {

// (60 / bpm) * SampleRate = samples per beat

inline Stream metro(Stream bpm) {
  return Stream([bpm = std::move(bpm), phase = 0]() mutable {
    float period = (60.f / bpm.next()) * SampleRate;
    bool pulse = (phase == 0);
    if (++phase >= period)
      phase = 0;
    return pulse ? 1.f : 0.f;
  });
}

}  // namespace automata
