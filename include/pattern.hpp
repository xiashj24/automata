#pragma once

#include <algorithm>
#include <array>
#include <string_view>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <vector>

#include <euclid.hpp>
#include <stream.hpp>
#include <tempo.hpp>

namespace automata {

namespace detail {

constexpr uint32_t lcg_mul = 196314165u;
constexpr uint32_t lcg_add = 907633515u;

inline uint32_t lcg_next(uint32_t state) {
  return state * lcg_mul + lcg_add;
}

// Recursive L-system interpreter. Expands 'N' nodes to depth levels.
// Symbols: N=note, +=up, -=down, ?=random±1, [=push pitch, ]=pop pitch.
struct LSystemExpander {
  std::string_view axiom;
  std::vector<float>& output;
  uint32_t& rng;

  void run(int depth, int& pitch, std::vector<int>& stack) {
    for (char c : axiom) {
      switch (c) {
        case 'N':
          if (depth <= 0)
            output.push_back(static_cast<float>(pitch));
          else
            run(depth - 1, pitch, stack);
          break;
        case '+': ++pitch; break;
        case '-': --pitch; break;
        case '?':
          rng = lcg_next(rng);
          pitch += (rng & 1u) ? 1 : -1;
          break;
        case '[': stack.push_back(pitch); break;
        case ']':
          if (!stack.empty()) {
            pitch = stack.back();
            stack.pop_back();
          }
          break;
        default: break;
      }
    }
  }
};

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

// A scale as semitone intervals from the root (up to 12 notes).
struct Scale {
  std::array<int, 12> intervals{};
  int size = 0;

  constexpr Scale(std::initializer_list<int> ints)
      : size(static_cast<int>(ints.size())) {
    auto it = ints.begin();
    for (int i = 0; i < size; ++i)
      intervals[static_cast<std::size_t>(i)] = *it++;
  }

  // Map a scale degree (0=root, negative = below root) to semitone offset.
  [[nodiscard]] constexpr int semitones(int degree) const {
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
  static const Scale major_penta;
  static const Scale minor_penta;
  static const Scale hirajoshi;    // Japanese koto scale
  static const Scale ritusen;      // Japanese pentatonic subset
  static const Scale hex_major6;   // major hexatonic
};

inline constexpr Scale Scale::major{0, 2, 4, 5, 7, 9, 11};
inline constexpr Scale Scale::minor{0, 2, 3, 5, 7, 8, 10};
inline constexpr Scale Scale::major_penta{0, 2, 4, 7, 9};
inline constexpr Scale Scale::minor_penta{0, 3, 5, 7, 10};
inline constexpr Scale Scale::hirajoshi{0, 2, 3, 7, 8};
inline constexpr Scale Scale::ritusen{0, 2, 7, 9};
inline constexpr Scale Scale::hex_major6{0, 2, 4, 5, 7, 9};

// Map a stream of scale degrees to semitone offsets (add a root note to get
// MIDI note numbers).
[[nodiscard]] inline Stream degree(Stream degrees, Scale scale) {
  return Stream([degrees = std::move(degrees),
                 scale = std::move(scale)]() mutable -> float {
    int d = static_cast<int>(std::round(degrees.next()));
    return static_cast<float>(scale.semitones(d));
  });
}

// Euclidean rhythm: distributes `pulses` evenly across `steps`, returns 1/0.
[[nodiscard]] inline Stream euclid(uint32_t pulses,
                                      uint32_t steps,
                                      Clock clock,
                                      uint32_t rotation = 0u) {
  auto raw = Stream([pulses, steps, rotation, i = 0u]() mutable -> float {
    float v = ::euclid(pulses, steps, rotation, i) ? 1.f : 0.f;
    i = (i + 1u) % steps;
    return v;
  });
  return hold(std::move(raw), clock.trigger());
}

// Shuffle values randomly on each cycle, then step through in that order.
[[nodiscard]] inline Stream shuffle(std::vector<float> values,
                                    Clock clock,
                                    uint32_t seed = 1u) {
  auto n = values.size();
  std::vector<std::size_t> indices(n);
  std::iota(indices.begin(), indices.end(), 0u);
  auto raw = Stream([vals = std::move(values), indices = std::move(indices),
                     state = seed, i = std::size_t(0)]() mutable -> float {
    if (i == 0) {
      for (std::size_t j = vals.size() - 1; j > 0; --j) {
        state = detail::lcg_next(state);
        std::size_t k = detail::lcg_index(state, j + 1);
        std::swap(indices[j], indices[k]);
      }
    }
    float v = vals[indices[i]];
    i = (i + 1u) % vals.size();
    return v;
  });
  return hold(std::move(raw), clock.trigger());
}

// Generate all n! permutations of values in lexicographic order, flattened.
[[nodiscard]] inline Stream permut(std::vector<float> values, Clock clock) {
  std::vector<float> all;
  auto perm = values;
  std::sort(perm.begin(), perm.end());
  do {
    std::copy(perm.begin(), perm.end(), std::back_inserter(all));
  } while (std::next_permutation(perm.begin(), perm.end()));
  return seq(std::move(all), clock);
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

// L-system sequence: expand axiom recursively to `depth` levels, then cycle.
// Symbols: N=emit pitch, +=pitch up, -=pitch down, ?=random ±1,
//          [=save pitch, ]=restore pitch.
[[nodiscard]] inline Stream lsystem(std::string_view axiom,
                                    int depth,
                                    Clock clock,
                                    uint32_t seed = 1u) {
  std::vector<float> output;
  uint32_t rng = seed;
  int cursor = 0;
  std::vector<int> cursor_stack;
  detail::LSystemExpander{axiom, output, rng}.run(depth, cursor, cursor_stack);
  return seq(std::move(output), clock);
}

}  // namespace automata
