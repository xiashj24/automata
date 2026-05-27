#pragma once

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include <note_number.hpp>
#include <v2/node.hpp>

namespace automata::v2 {

// ─── SeqNode
// ──────────────────────────────────────────────────────────────────
//
// Steps through a fixed value list one entry per rising edge of trigger.
// Holds the current value between triggers.

class SeqNode : public Node {
public:
  SeqNode(std::vector<float> vals, Signal trigger)
      : values(std::move(vals)), trigger(trigger) {}

  void reset() override {
    index = 0;
    current = 0.f;
    prev_trig = 0.f;
    trigger.reset();
  }

protected:
  float process() override {
    float t = trigger.next();
    if (t > 0.5f && prev_trig <= 0.5f) {
      current = values[index];
      index = (index + 1) % values.size();
    }
    prev_trig = t;
    return current;
  }

private:
  std::vector<float> values;
  Signal trigger;
  std::size_t index = 0;
  float current = 0.f;
  float prev_trig = 0.f;
};

inline Signal seq(std::vector<float> values, Signal trigger) {
  return Signal(std::make_shared<SeqNode>(std::move(values), trigger));
}

// ─── NoteToFreqNode
// ────────────────────────────────────────────────────────
//
// Converts a MIDI note number Signal to Hz (A4 = 440 Hz, note 69).

class NoteToFreqNode : public Node {
public:
  explicit NoteToFreqNode(Signal note) : note(note) {}

  void reset() override { note.reset(); }

protected:
  float process() override { return ::note_to_frequency(note.next()); }

private:
  Signal note;
};

inline Signal note_to_frequency(Signal note) {
  return Signal(std::make_shared<NoteToFreqNode>(note));
}

}  // namespace automata::v2
