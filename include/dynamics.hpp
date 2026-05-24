#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <samplerate.hpp>
#include <stream.hpp>

namespace automata {

// RMS power follower over a sliding window of `window_s` seconds.
[[nodiscard]] inline Stream rms(Stream x, float window_s = 0.1f) {
  std::size_t n =
      std::max(std::size_t(1), static_cast<std::size_t>(window_s * SampleRate));
  struct State {
    std::vector<float> buf;
    std::size_t pos = 0;
    float sum_sq = 0.f;
  };
  State st;
  st.buf.assign(n, 0.f);
  return Stream([x, n, st = std::move(st)]() mutable -> float {
    float xn = x.next();
    st.sum_sq -= st.buf[st.pos] * st.buf[st.pos];
    st.buf[st.pos] = xn;
    st.sum_sq = std::max(st.sum_sq + xn * xn, 0.f);
    st.pos = (st.pos + 1) % n;
    return std::sqrt(st.sum_sq / static_cast<float>(n));
  });
}

// Feed-forward peak compressor. attack_s / release_s are the smoothing
// time constants for the peak envelope.
[[nodiscard]] inline Stream compressor(Stream x,
                                        float threshold_db = -12.f,
                                        float ratio = 4.f,
                                        float attack_s = 0.005f,
                                        float release_s = 0.1f) {
  float slope = 1.f - 1.f / ratio;
  float att = std::exp(-1.f / (attack_s * SampleRate));
  float rel = std::exp(-1.f / (release_s * SampleRate));
  return Stream([x, threshold_db, slope, att, rel, env = 0.f]() mutable -> float {
    float xn = x.next();
    float level = std::abs(xn);
    env = level > env ? att * env + (1.f - att) * level
                      : rel * env + (1.f - rel) * level;
    float gain_db = -slope * std::max(
        20.f * std::log10(std::max(env, 1e-10f)) - threshold_db, 0.f);
    return xn * std::pow(10.f, gain_db * 0.05f);
  });
}

}  // namespace automata
