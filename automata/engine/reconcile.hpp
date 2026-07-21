#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "automata/config.hpp"
#include "automata/engine/engine.hpp"
#include "automata/graph/def.hpp"

// The control-thread half of two-tier reconciliation (ADR 0002/0003).
// update() takes any new def: an identical-hash def becomes a value patch —
// unless it carries an owner, whose code pointers must land, so it swaps —
// anything else a structural swap. Swaps are serialized — a newer def
// arriving while one is in flight replaces the stashed one (last writer
// wins), and is issued once the engine acks via SwapDone. drain() must be
// called periodically: it frees everything the engine retires.
//
// An optional owner rides along with the def and is released when the Graph
// built from it retires — how a custom-kernel patch's generation stays
// mapped exactly as long as code from it can run (ADR 0001).

namespace automata {

class Reconciler {
public:
  explicit Reconciler(Engine& engine) : engine_(engine) {}

  void update(GraphDef def,
              Quantize quantize = Quantize::Immediate,
              float fade_seconds = DefaultCrossfadeSeconds,
              std::shared_ptr<void> owner = {});

  void drain();

  // True when nothing is stashed or awaiting an ack.
  [[nodiscard]] bool idle() const {
    return !in_flight_ && !stashed_.has_value();
  }

private:
  struct Pending {
    GraphDef def;
    Quantize quantize;
    float fade_seconds;
    std::shared_ptr<void> owner;
  };

  void pump();

  Engine& engine_;
  GraphDef current_def_{};
  bool has_def_ = false;
  Graph* live_graph_ = nullptr;  // engine-owned; plan source only
  bool in_flight_ = false;
  std::optional<Pending> stashed_;
  std::unordered_map<Graph*, std::shared_ptr<void>> owners_;
};

}  // namespace automata
