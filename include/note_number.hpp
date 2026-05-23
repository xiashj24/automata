#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <stream.hpp>
#include <numbers>


inline constexpr float A440Frequency = 440.0f;
inline constexpr int A440NoteNumber = 69;

// TODO: make these constexpr

inline float note_number_to_frequency(int note) {
  return A440Frequency *
         std::pow(2.0f, (static_cast<float>(note) - A440NoteNumber) / 12.0f);
}

inline float note_number_to_frequency(float note) {
  return A440Frequency *
         std::pow(2.0f, (note - static_cast<float>(A440NoteNumber)) / 12.0f);
}

inline float frequency_to_note_number(float frequency) {
  return static_cast<float>(A440NoteNumber) +
         (12.0f / std::numbers::ln2_v<float>)*std::log(frequency /
                                                       A440Frequency);
}

// A MIDI note number in the range 0–127.
struct NoteNumber {
  uint8_t note = 0;

  operator uint8_t() const { return note; }

  auto operator<=>(const NoteNumber&) const = default;

  // Position within the octave: 0 (C) – 11 (B).
  [[nodiscard]] int chromatic_index() const {
    return static_cast<int>(note % 12);
  }

  // Octave number.
  // octave_for_middle_c=3 gives Yamaha convention (middle C = C3).
  [[nodiscard]] int octave_number(int octave_for_middle_c = 4) const {
    return note / 12 + (octave_for_middle_c - 5);
  }

  [[nodiscard]] float frequency() const {
    return note_number_to_frequency(static_cast<int>(note));
  }

  // Note name with flats for accidentals (e.g. "Eb", "Bb").
  [[nodiscard]] std::string_view name() const {
    return std::addressof("C\0\0C#\0D\0\0Eb\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0Bb\0B"
                              [static_cast<size_t>(3 * chromatic_index())]);
  }

  [[nodiscard]] std::string_view name_with_sharps() const {
    return std::addressof("C\0\0C#\0D\0\0D#\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0A#\0B"
                              [static_cast<size_t>(3 * chromatic_index())]);
  }

  [[nodiscard]] std::string_view name_with_flats() const {
    return std::addressof("C\0\0Db\0D\0\0Eb\0E\0\0F\0\0Gb\0G\0\0Ab\0A\0\0Bb\0B"
                              [static_cast<size_t>(3 * chromatic_index())]);
  }

  [[nodiscard]] std::string name_with_octave() const {
    return std::string(name()) + std::to_string(octave_number());
  }

  // True for C D E F G A B; false for accidentals.
  [[nodiscard]] bool is_natural() const {
    return (0b101010110101 & (1 << chromatic_index())) != 0;
  }

  [[nodiscard]] bool is_accidental() const { return !is_natural(); }
};

[[nodiscard]] inline automata::Stream note_number_to_frequency(
    automata::Stream notes) {
  return automata::Stream([notes = std::move(notes)]() mutable -> float {
    return note_number_to_frequency(notes.next());
  });
}
