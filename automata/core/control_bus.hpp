#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string_view>

#include "automata/config.hpp"
#include "automata/core/hash.hpp"

// The external-control surface (ADR 0003/0007): a fixed table of named
// atomic floats. OSC, mouse, and host code write slots by name; param nodes
// read them on the audio thread through pointers resolved at Graph build. A
// name binds its slot for the bus's lifetime, so those pointers stay valid
// across every hot-swap. Registration takes a lock — control-side threads
// only, never the audio thread; writes to a full bus are dropped and
// counted.

namespace automata {

class ControlBus {
public:
  class Slot {
  public:
    // Value before flag: a reader that observes written sees the value.
    void write(float value) noexcept {
      value_.store(value, std::memory_order_relaxed);
      written_.store(1, std::memory_order_release);
    }

    // Audio thread: the live value, or fallback while nothing has written.
    [[nodiscard]] float read_or(float fallback) const noexcept {
      if (written_.load(std::memory_order_acquire) == 0) {
        return fallback;
      }
      return value_.load(std::memory_order_relaxed);
    }

  private:
    std::atomic<float> value_{0.f};
    std::atomic<std::uint32_t> written_{0};
  };

  // Finds or registers the slot for a name; the pointer is stable for the
  // bus's lifetime. Returns nullptr when the bus is full.
  [[nodiscard]] Slot* channel(Hash name) {
    const std::scoped_lock lock(mutex_);
    for (std::size_t i = 0; i < count_; ++i) {
      if (names_[i] == name) {
        return &slots_[i];
      }
    }
    if (count_ == ControlBusCapacity) {
      return nullptr;
    }
    names_[count_] = name;
    return &slots_[count_++];
  }
  [[nodiscard]] Slot* channel(std::string_view name) {
    return channel(hash_string(name));
  }

  void set(Hash name, float value) {
    Slot* slot = channel(name);
    if (slot == nullptr) {
      dropped_.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    slot->write(value);
  }
  void set(std::string_view name, float value) {
    set(hash_string(name), value);
  }

  // Writes lost to a full bus.
  [[nodiscard]] std::uint64_t dropped() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
  }

private:
  std::mutex mutex_;
  std::array<Hash, ControlBusCapacity> names_{};
  std::size_t count_ = 0;
  std::array<Slot, ControlBusCapacity> slots_;
  std::atomic<std::uint64_t> dropped_{0};
};

}  // namespace automata
