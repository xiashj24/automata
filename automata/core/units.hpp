#pragma once

// Unit literals for the patch surface: annotated floats, not strong types —
// they document the unit a factory already expects. The namespace is inline,
// so a patch's file-scope `using namespace automata` brings them in.
//
// `_db` stays in decibels (for gain()'s db parameter) rather than converting
// to amplitude: a literal binds before unary minus, so -6_db could never
// convert correctly.

namespace automata {
inline namespace literals {

[[nodiscard]] constexpr float operator""_hz(long double v) noexcept {
  return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_hz(unsigned long long v) noexcept {
  return static_cast<float>(v);
}

[[nodiscard]] constexpr float operator""_bpm(long double v) noexcept {
  return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_bpm(unsigned long long v) noexcept {
  return static_cast<float>(v);
}

[[nodiscard]] constexpr float operator""_s(long double v) noexcept {
  return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_s(unsigned long long v) noexcept {
  return static_cast<float>(v);
}

// Milliseconds convert to seconds — the unit every time-taking factory
// expects. Divided in long double so the float result is correctly rounded
// (10_ms is exactly 0.01f).
[[nodiscard]] constexpr float operator""_ms(long double v) noexcept {
  return static_cast<float>(v / 1000.0L);
}
[[nodiscard]] constexpr float operator""_ms(unsigned long long v) noexcept {
  return static_cast<float>(static_cast<long double>(v) / 1000.0L);
}

[[nodiscard]] constexpr float operator""_db(long double v) noexcept {
  return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_db(unsigned long long v) noexcept {
  return static_cast<float>(v);
}

}  // namespace literals
}  // namespace automata
