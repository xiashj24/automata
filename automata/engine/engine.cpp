#include "automata/engine/engine.hpp"

#include <algorithm>
#include <cstring>

#include "automata/core/assert.hpp"
#include "automata/engine/graph.hpp"

namespace automata {

Engine::~Engine() {
  delete current_;
  delete fading_;
  if (has_pending_) {
    delete pending_.graph;
    delete pending_.plan;
  }
  InMsg in;
  while (inbox_.try_pop(in)) {
    delete[] in.values;
    delete in.graph;
    delete in.plan;
  }
  OutMsg out;
  while (outbox_.try_pop(out)) {
    delete out.graph;
    delete[] out.values;
    delete out.plan;
  }
}

void Engine::render(float* interleaved, std::size_t nframes) noexcept {
  for (std::size_t i = 0; i < nframes; ++i) {
    if (mix_pos_ == BlockSize) {
      render_block();
      mix_pos_ = 0;
    }
    interleaved[2 * i] = mix_l_[mix_pos_];
    interleaved[2 * i + 1] = mix_r_[mix_pos_];
    ++mix_pos_;
  }
}

void Engine::drain_inbox() noexcept {
  InMsg msg;
  while (inbox_.try_pop(msg)) {
    switch (msg.kind) {
      case InMsg::Kind::ValuePatch: {
        if (current_ != nullptr && msg.target_def == current_->def().def_hash &&
            msg.value_count == current_->def().values.size()) {
          current_->apply_values(msg.values, msg.value_count);
        }
        retire({.kind = OutMsg::Kind::RetiredValues, .values = msg.values});
        break;
      }
      case InMsg::Kind::Swap: {
        // Reconciler serializes swaps; a second pending one is a bug.
        atm_assert(!has_pending_);
        pending_ = msg;
        has_pending_ = true;
        pending_at_ = transport_.next_boundary(msg.quantize);
        break;
      }
      case InMsg::Kind::SetBpm: {
        transport_.bpm = msg.bpm;
        break;
      }
    }
  }
}

void Engine::execute_swap() noexcept {
  for (const TransferPlan::Copy& copy : pending_.plan->copies) {
    std::memcpy(copy.dst, copy.src, copy.size);
  }
  if (fading_ != nullptr) {  // a swap mid-fade snaps the older fade
    retire({.kind = OutMsg::Kind::RetiredGraph, .graph = fading_});
  }
  fading_ = current_;
  current_ = pending_.graph;

  fade_total_ = fading_ != nullptr
                    ? static_cast<std::uint32_t>(pending_.fade_seconds *
                                                 static_cast<float>(SampleRate))
                    : 0;
  fade_remaining_ = fade_total_;
  if (fade_total_ == 0 && fading_ != nullptr) {
    retire({.kind = OutMsg::Kind::RetiredGraph, .graph = fading_});
    fading_ = nullptr;
  }
  retire({.kind = OutMsg::Kind::SwapDone, .plan = pending_.plan});
  has_pending_ = false;
}

void Engine::render_block() noexcept {
  drain_inbox();
  if (has_pending_ && transport_.sample_pos >= pending_at_) {
    execute_swap();
  }

  if (current_ != nullptr) {
    current_->process_block();
  }
  if (fading_ != nullptr) {
    fading_->process_block();
  }

  if (current_ != nullptr && fading_ != nullptr) {
    const float inv = 1.f / static_cast<float>(fade_total_);
    const std::uint32_t done = fade_total_ - fade_remaining_;
    const auto cl = current_->left();
    const auto cr = current_->right();
    const auto fl = fading_->left();
    const auto fr = fading_->right();
    for (std::uint32_t i = 0; i < BlockSize; ++i) {
      float gain = static_cast<float>(done + i) * inv;
      gain = gain > 1.f ? 1.f : gain;
      mix_l_[i] = cl[i] * gain + fl[i] * (1.f - gain);
      mix_r_[i] = cr[i] * gain + fr[i] * (1.f - gain);
    }
    fade_remaining_ -=
        std::min(fade_remaining_, static_cast<std::uint32_t>(BlockSize));
    if (fade_remaining_ == 0) {
      retire({.kind = OutMsg::Kind::RetiredGraph, .graph = fading_});
      fading_ = nullptr;
    }
  } else if (current_ != nullptr) {
    const auto cl = current_->left();
    const auto cr = current_->right();
    std::copy(cl.begin(), cl.end(), mix_l_.begin());
    std::copy(cr.begin(), cr.end(), mix_r_.begin());
  } else {
    mix_l_.fill(0.f);
    mix_r_.fill(0.f);
  }

  transport_.advance(BlockSize);
}

void Engine::retire(const OutMsg& msg) noexcept {
  if (!outbox_.try_push(msg)) {
    ++dropped_msgs_;  // leaks under catastrophic backpressure — counted
  }
}

}  // namespace automata
