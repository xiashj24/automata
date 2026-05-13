#pragma once
#include <cstdint>

/**
 * @brief state-less euclid rhythm
 * @ref https://paulbatchelor.github.io/sndkit/euclid/
 * @param p number of pulses
 * @param n number of total steps
 * @param r rotation in [0, n)
 * @param i step index to query
 * @return true if queried step is on.
 * @return false if queried step is off.
 */
constexpr bool euclid(uint32_t pulses,
                      uint32_t steps,
                      uint32_t rotation,
                      uint32_t index) {
  return (((pulses * (index + rotation)) % steps) + pulses) >= steps;
}

// 1st step is always on
constexpr bool euclid_simple(uint32_t pulses, uint32_t steps, uint32_t index) {
  return euclid(pulses, steps, steps / pulses, index);
}