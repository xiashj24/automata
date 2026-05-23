#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <vector>

#include <stream.hpp>
#include <tempo.hpp>

namespace automata {

namespace detail {

constexpr uint32_t lcg_mul = 196314165u;
constexpr uint32_t lcg_add = 907633515u;

inline uint32_t lcg_next(uint32_t state) {
  return state * lcg_mul + lcg_add;
}

// Pick a random index in [0, n).
inline std::size_t lcg_index(uint32_t state, std::size_t n) {
  return static_cast<std::size_t>(
      (static_cast<uint64_t>(state) * static_cast<uint64_t>(n)) >> 32);
}

}  // namespace detail

// Cycle through values endlessly, one per trigger pulse.
[[nodiscard]] inline Stream seq(std::vector<float> values, Clock clock) {
  auto raw = Stream([vals = std::move(values), i = 0u]() mutable -> float {
    float v = vals[i];
    i = (i + 1u) % vals.size();
    return v;
  });
  return hold(std::move(raw), clock.trigger());
}

// Pick a random value with replacement on each trigger pulse.
[[nodiscard]] inline Stream choose(std::vector<float> values,
                                   Clock clock,
                                   uint32_t seed = 1u) {
  auto raw =
      Stream([vals = std::move(values), state = seed]() mutable -> float {
        state = detail::lcg_next(state);
        return vals[detail::lcg_index(state, vals.size())];
      });
  return hold(std::move(raw), clock.trigger());
}

// Pick a random value with no consecutive repeats.
[[nodiscard]] inline Stream xchoose(std::vector<float> values,
                                    Clock clock,
                                    uint32_t seed = 1u) {
  auto raw = Stream([vals = std::move(values), state = seed,
                     last = std::size_t(-1)]() mutable -> float {
    std::size_t idx;
    do {
      state = detail::lcg_next(state);
      idx = detail::lcg_index(state, vals.size());
    } while (idx == last && vals.size() > 1);
    last = idx;
    return vals[idx];
  });
  return hold(std::move(raw), clock.trigger());
}

// Pick a random value according to (unnormalized) weights.
[[nodiscard]] inline Stream wchoose(std::vector<float> values,
                                    std::vector<float> weights,
                                    Clock clock,
                                    uint32_t seed = 1u) {
  float total = std::accumulate(weights.begin(), weights.end(), 0.f);

  auto raw = Stream([vals = std::move(values), wts = std::move(weights), total,
                     state = seed]() mutable -> float {
    state = detail::lcg_next(state);
    float target =
        static_cast<float>(state) / static_cast<float>(UINT32_MAX) * total;
    float cumulative = 0.f;
    for (std::size_t i = 0; i < wts.size(); ++i) {
      cumulative += wts[i];
      if (target < cumulative)
        return vals[i];
    }
    return vals.back();
  });
  return hold(std::move(raw), clock.trigger());
}

// Finite range cycling: start, start+step, …, up to (but not including) stop,
// then loops.
[[nodiscard]] inline Stream range(float start,
                                  float stop,
                                  float step,
                                  Clock clock) {
  auto raw = Stream([start, stop, step, value = start]() mutable -> float {
    float out = value;
    value += step;
    if (step > 0.f ? value >= stop : value <= stop)
      value = start;
    return out;
  });
  return hold(std::move(raw), clock.trigger());
}

// Arithmetic sequence: start, start+step, start+2·step, …
[[nodiscard]] inline Stream series(float start, float step, Clock clock) {
  auto raw = Stream([value = start, step]() mutable -> float {
    float out = value;
    value += step;
    return out;
  });
  return hold(std::move(raw), clock.trigger());
}

// Geometric sequence: start, start·ratio, start·ratio², …
[[nodiscard]] inline Stream geom(float start, float ratio, Clock clock) {
  auto raw = Stream([value = start, ratio]() mutable -> float {
    float out = value;
    value *= ratio;
    return out;
  });
  return hold(std::move(raw), clock.trigger());
}

// Play values forward then backward, cycling: [a,b,c,d] → a,b,c,d,c,b,a,b,…
[[nodiscard]] inline Stream ping_pong(std::vector<float> values, Clock clock) {
  auto n = static_cast<int>(values.size());
  std::vector<float> bounced;
  bounced.reserve(n > 1 ? static_cast<std::size_t>(2 * n - 2) : 1u);
  std::copy(values.begin(), values.end(), std::back_inserter(bounced));
  for (int i = n - 2; i > 0; --i)
    bounced.push_back(values[static_cast<std::size_t>(i)]);
  return seq(std::move(bounced), clock);
}

// Repeat each value n times before advancing: [a,b,c], n=2 → a,a,b,b,c,c,…
[[nodiscard]] inline Stream stutter(std::vector<float> values,
                                    int n,
                                    Clock clock) {
  auto raw = Stream(
      [vals = std::move(values), n, i = 0u, count = 0]() mutable -> float {
        float v = vals[i];
        if (++count >= n) {
          count = 0;
          i = (i + 1u) % vals.size();
        }
        return v;
      });
  return hold(std::move(raw), clock.trigger());
}

// A diatonic scale as semitone intervals from the root.
struct Scale {
  std::array<int, 7> intervals;

  // Map a scale degree (0=root, 7=octave above root, negative = below) to
  // semitone offset from the root.
  [[nodiscard]] constexpr int semitones(int degree) const {
    constexpr int size = 7;
    int octave = degree / size;
    int idx = degree % size;
    if (idx < 0) {
      idx += size;
      --octave;
    }
    return octave * 12 + intervals[static_cast<std::size_t>(idx)];
  }

  static const Scale major;
  static const Scale minor;
};

inline constexpr Scale Scale::major{{0, 2, 4, 5, 7, 9, 11}};
inline constexpr Scale Scale::minor{{0, 2, 3, 5, 7, 8, 10}};

// Map a stream of scale degrees to semitone offsets (add a root note to get
// MIDI note numbers).
[[nodiscard]] inline Stream degree(Stream degrees, Scale scale) {
  return Stream([degrees = std::move(degrees),
                 scale = std::move(scale)]() mutable -> float {
    int d = static_cast<int>(std::round(degrees.next()));
    return static_cast<float>(scale.semitones(d));
  });
}

// Brownian random walk clamped to [lo, hi], stepping by a random amount in
// [-step, +step].
[[nodiscard]] inline Stream walk(float lo,
                                 float hi,
                                 float step,
                                 Clock clock,
                                 float init = 0.f,
                                 uint32_t seed = 1u) {
  auto raw =
      Stream([lo, hi, step, value = init, state = seed]() mutable -> float {
        state = detail::lcg_next(state);
        float delta =
            (static_cast<float>(state) / static_cast<float>(UINT32_MAX) * 2.f -
             1.f) *
            step;
        value += delta;
        if (value < lo)
          value = lo;
        if (value > hi)
          value = hi;
        return value;
      });
  return hold(std::move(raw), clock.trigger());
}

}  // namespace automata
