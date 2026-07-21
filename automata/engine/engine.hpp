#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "automata/config.hpp"
#include "automata/core/control_bus.hpp"
#include "automata/core/hash.hpp"
#include "automata/core/lockfree_queue.hpp"
#include "automata/engine/diff.hpp"
#include "automata/core/transport.hpp"

// The audio-thread half of the system (ADR 0003): render drains the inbox,
// runs at most two graphs (current + fading) under a linear crossfade, and
// pushes everything it retires to the outbox — it never allocates or frees.
// The control-side counterpart is Reconciler (engine/reconcile.hpp), which
// serializes swaps so at most one is ever pending here.

namespace automata {

class Graph;

struct InMsg {
  enum class Kind { ValuePatch, Swap, SetBpm };
  Kind kind = Kind::ValuePatch;
  Hash target_def = 0;      // ValuePatch: applied only if current matches
  float* values = nullptr;  // ValuePatch: control-owned table
  std::uint32_t value_count = 0;
  Graph* graph = nullptr;  // Swap
  TransferPlan* plan = nullptr;
  Quantize quantize = Quantize::Immediate;
  float fade_seconds = DefaultCrossfadeSeconds;
  float bpm = 0.f;  // SetBpm
};

struct OutMsg {
  enum class Kind { RetiredGraph, RetiredValues, SwapDone };
  Kind kind = Kind::RetiredGraph;
  Graph* graph = nullptr;
  float* values = nullptr;
  TransferPlan* plan = nullptr;  // SwapDone: doubles as the swap ack
};

class Engine {
public:
  Engine() = default;
  ~Engine();  // control context: frees whatever it still owns
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // The audio callback (or the offline loop). Interleaved stereo.
  void render(float* interleaved, std::size_t nframes) noexcept;

  [[nodiscard]] LockfreeQueue<InMsg, InboxCapacity>& inbox() noexcept {
    return inbox_;
  }
  [[nodiscard]] LockfreeQueue<OutMsg, OutboxCapacity>& outbox() noexcept {
    return outbox_;
  }
  [[nodiscard]] const Transport& transport() const noexcept {
    return transport_;
  }
  // External control: written by control-side threads, read by the params
  // of every graph built against this engine.
  [[nodiscard]] ControlBus& bus() noexcept { return bus_; }
  [[nodiscard]] std::uint64_t dropped_msgs() const noexcept {
    return dropped_msgs_;
  }

private:
  void render_block() noexcept;
  void drain_inbox() noexcept;
  void execute_swap() noexcept;
  void retire(const OutMsg& msg) noexcept;

  Graph* current_ = nullptr;
  Graph* fading_ = nullptr;
  std::uint32_t fade_total_ = 0;
  std::uint32_t fade_remaining_ = 0;

  bool has_pending_ = false;
  InMsg pending_{};
  std::uint64_t pending_at_ = 0;

  Transport transport_;
  std::array<float, BlockSize> mix_l_{};
  std::array<float, BlockSize> mix_r_{};
  std::size_t mix_pos_ = BlockSize;  // consumed; forces a first render_block

  std::uint64_t dropped_msgs_ = 0;

  ControlBus bus_;
  LockfreeQueue<InMsg, InboxCapacity> inbox_;
  LockfreeQueue<OutMsg, OutboxCapacity> outbox_;
};

}  // namespace automata
