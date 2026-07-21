#include "automata/engine/reconcile.hpp"

#include <algorithm>
#include <utility>

#include "automata/engine/diff.hpp"
#include "automata/engine/graph.hpp"

namespace automata {

void Reconciler::update(GraphDef def,
                        Quantize quantize,
                        float fade_seconds,
                        std::shared_ptr<void> owner) {
  stashed_ = Pending{.def = std::move(def),
                     .quantize = quantize,
                     .fade_seconds = fade_seconds,
                     .owner = std::move(owner)};
  pump();
}

void Reconciler::drain() {
  OutMsg msg;
  while (engine_.outbox().try_pop(msg)) {
    switch (msg.kind) {
      case OutMsg::Kind::RetiredGraph:
        delete msg.graph;
        owners_.erase(msg.graph);  // after teardown: generation stays mapped
        break;
      case OutMsg::Kind::RetiredValues:
        delete[] msg.values;
        break;
      case OutMsg::Kind::SwapDone:
        delete msg.plan;
        in_flight_ = false;
        break;
    }
  }
  pump();
}

void Reconciler::pump() {
  if (in_flight_ || !stashed_.has_value())
    return;
  Pending p = std::move(*stashed_);
  stashed_.reset();

  if (has_def_ && p.def.def_hash == current_def_.def_hash) {
    auto remapped = remap_values(current_def_, p.def);
    auto* table = new float[remapped.size()];
    std::copy(remapped.begin(), remapped.end(), table);
    const InMsg msg{.kind = InMsg::Kind::ValuePatch,
                    .target_def = current_def_.def_hash,
                    .values = table,
                    .value_count = static_cast<std::uint32_t>(remapped.size())};
    if (!engine_.inbox().try_push(msg)) {
      delete[] table;
      stashed_.emplace(std::move(p));  // retried on the next drain
      return;
    }
    current_def_.values = std::move(remapped);
    return;
  }

  auto* graph = new Graph(p.def, &engine_.bus(), &engine_.transport());
  auto* plan = new TransferPlan(live_graph_ != nullptr
                                    ? plan_transfer(*live_graph_, *graph)
                                    : TransferPlan{});
  const InMsg msg{.kind = InMsg::Kind::Swap,
                  .graph = graph,
                  .plan = plan,
                  .quantize = p.quantize,
                  .fade_seconds = p.fade_seconds};
  if (!engine_.inbox().try_push(msg)) {
    delete graph;
    delete plan;
    stashed_.emplace(std::move(p));
    return;
  }
  in_flight_ = true;
  live_graph_ = graph;
  current_def_ = std::move(p.def);
  has_def_ = true;
  if (p.owner != nullptr) {
    owners_.emplace(graph, std::move(p.owner));
  }
}

}  // namespace automata
