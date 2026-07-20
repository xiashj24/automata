#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

// SPSC lock-free queue for sharing data between exactly two threads without
// locks (ADR 0003) — the audio/control channel automata::RingBuffer does not
// cover (that type is single-context only; see its own header comment). One
// producer context and one consumer context; never more than one of each.
//
// Ported from xhal's LockfreeQueue with one deliberate adaptation: head_ and
// tail_ are cache-line aligned, because automata's producer and consumer are
// separate cores hitting the same line on every push/pop — false sharing
// xhal's single-core target had no reason to pay against. Still the simplest
// member of its family — acquire/release on two atomic indices — not the
// cached-index or fenced variants built for contention that a few messages
// per audio block never produces.
//
// Caller's contract:
// - Exactly one producer context calls try_push(); exactly one consumer
//   context calls try_pop(). A second producer, a second consumer, or the
//   same context racing itself is a contract violation, not a checked error.
// - clear() is not safe to call concurrently with the other context's
//   try_push()/try_pop() — call it only while both sides are quiesced.
// - No iterators, no front()/back()/operator[]: a slot visible to one
//   context can be concurrently overwritten or consumed by the other, so
//   only the try_push()/try_pop() handoff is safe to expose.
// - No silent-overwrite push(): discarding the oldest element to make room
//   would need the producer to also advance the consumer's index, breaking
//   the single-writer-per-index invariant this type relies on for
//   correctness without locks. Callers that want overwrite-on-full
//   semantics should size the queue to never fill, or drain more often.

namespace automata {

namespace detail {
#ifdef __cpp_lib_hardware_interference_size
inline constexpr std::size_t DestructiveInterferenceSize =
    std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t DestructiveInterferenceSize = 64;
#endif
}  // namespace detail

template <typename T, size_t N>
class LockfreeQueue {
  static_assert((N > 1) && ((N & (N - 1)) == 0), "N must be power of 2");

public:
  [[nodiscard]] bool try_push(const T& value) noexcept(
      std::is_nothrow_copy_assignable_v<T>) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    if (tail - head >= Capacity)
      return false;

    data_[tail & Capacity] = value;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool try_push(T&& value) noexcept(
      std::is_nothrow_move_assignable_v<T>) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);
    if (tail - head >= Capacity)
      return false;

    data_[tail & Capacity] = std::move(value);
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool try_pop(T& value) noexcept(
      std::is_nothrow_move_assignable_v<T>) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail)
      return false;

    value = std::move(data_[head & Capacity]);
    head_.store(head + 1, std::memory_order_release);
    return true;
  }

  // A snapshot, not a synchronization point — racy against a concurrent
  // push/pop from the other context by design. Fine for diagnostics.
  [[nodiscard]] size_t size() const noexcept {
    return tail_.load(std::memory_order_acquire) -
           head_.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool empty() const noexcept {
    return head_.load(std::memory_order_relaxed) ==
           tail_.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool full() const noexcept {
    return (tail_.load(std::memory_order_relaxed) -
            head_.load(std::memory_order_acquire)) >= Capacity;
  }

  [[nodiscard]] size_t capacity() const noexcept { return Capacity; }

  // Not safe concurrently with the other context's try_push()/try_pop().
  void clear() noexcept {
    head_.store(tail_.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
  }

private:
  static constexpr size_t Capacity = N - 1;  // also used as bitmask
  std::array<T, N> data_{};

  alignas(detail::DestructiveInterferenceSize) std::atomic<size_t> head_{0};
  alignas(detail::DestructiveInterferenceSize) std::atomic<size_t> tail_{0};
};

}  // namespace automata
