#pragma once
#include <cstddef>

namespace automata {

inline constexpr float SampleRate = 48000.f;
inline constexpr std::size_t MaxDelaySamples = 4096;  // ~85 ms at 48 kHz

}  // namespace automata
