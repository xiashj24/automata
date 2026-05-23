#pragma once
#include <stream.hpp>

namespace automata {

inline float g_local_feedback = 0.f;

// LocalIn: reads the previous sample's output.
[[nodiscard]] inline Stream local_in() {
  return Stream([]() -> float { return g_local_feedback; });
}

// LocalOut: passes through while feeding each sample back to local_in().
[[nodiscard]] inline Stream local_out(Stream src) {
  return Stream([src = std::move(src)]() mutable -> float {
    float v = src.next();
    g_local_feedback = v;
    return v;
  });
}

}  // namespace automata
