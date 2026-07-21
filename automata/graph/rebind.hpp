#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "automata/core/assert.hpp"
#include "automata/core/hash.hpp"
#include "automata/graph/def.hpp"
#include "automata/graph/fn.hpp"
#include "automata/graph/kernel_info.hpp"

// A def built inside a patch library points at that library's KernelInfo
// statics, and make_node's op bytes are setter member-pointers into its
// code. Rebinding replaces both with host-owned copies for every kernel the
// host knows — keyed by the info's own type_hash — which is what lets a
// vocabulary-only generation unload the moment describe returns (ADR 0001).
// Unknown kernels stay foreign and pin their generation.
//
// fn nodes key by their name-folded node hash instead (ADR 0009): a name
// the probe instantiated rebinds to the host's function pointer like any
// vocabulary kernel; a patch-local name misses and stays foreign.

namespace automata {

class KernelRegistry {
public:
  struct Entry {
    const KernelInfo* kernel = nullptr;
    std::vector<std::byte> op;  // canonical bytes; empty for data kernels
    bool rewrite_op = false;
  };

  // A kernel whose op bytes are per-node data (tap ids): pointer remap only.
  void add_data_kernel(const KernelInfo* info) {
    insert(info->type_hash, info, {}, false);
  }

  // A kernel whose op bytes are its factory's setter binding — identical for
  // every node of the type: rewritten to this canonical copy on rebind.
  void add_bound_kernel(const KernelInfo* info, std::span<const std::byte> op) {
    insert(info->type_hash, info, {op.begin(), op.end()}, true);
  }

  // Registers every kernel a host-built probe def instantiated, bound form.
  // Kernels already present (the data kernels, the probe's lifted Consts)
  // are skipped; a repeat with different op bytes is a hash collision.
  void add_probe(const GraphDef& def) {
    for (const GraphDef::Node& node : def.nodes) {
      const std::span<const std::byte> op{def.op_data.data() + node.op_begin,
                                          node.op_size};
      // fn nodes register per name; the family base stays unregistered so
      // unprobed names remain foreign.
      const Hash key = detail::is_fn_kernel(node.kernel)
                           ? node.type_hash
                           : node.kernel->type_hash;
      if (const Entry* existing = find(key)) {
        atm_assert(!existing->rewrite_op ||
                   std::ranges::equal(existing->op, op));
        continue;
      }
      insert(key, node.kernel, {op.begin(), op.end()}, true);
    }
  }

  [[nodiscard]] const Entry* find(Hash key) const {
    const auto it = entries_.find(key);
    return it != entries_.end() ? &it->second : nullptr;
  }

private:
  void insert(Hash key,
              const KernelInfo* info,
              std::vector<std::byte> op,
              bool rewrite) {
    atm_assert(info != nullptr);
    atm_assert(!entries_.contains(key));
    entries_.emplace(
        key, Entry{.kernel = info, .op = std::move(op), .rewrite_op = rewrite});
  }

  std::unordered_map<Hash, Entry> entries_;
};

// Rewrites every known node to the registry's host-owned kernel and
// canonical op bytes; returns how many nodes stayed foreign — custom
// kernels, whose generation the resulting Graph must keep alive (ADR 0001).
[[nodiscard]] inline std::uint32_t rebind_kernels(
    GraphDef& def,
    const KernelRegistry& registry) {
  std::uint32_t foreign = 0;
  for (GraphDef::Node& node : def.nodes) {
    // Name-folded first (fn vocabulary), then the kernel's own type.
    const KernelRegistry::Entry* entry = registry.find(node.type_hash);
    if (entry == nullptr) {
      entry = registry.find(node.kernel->type_hash);
    }
    if (entry == nullptr) {
      ++foreign;
      continue;
    }
    node.kernel = entry->kernel;
    if (entry->rewrite_op) {
      atm_assert(entry->op.size() == node.op_size);
      std::ranges::copy(entry->op, def.op_data.begin() + node.op_begin);
    }
  }
  return foreign;
}

}  // namespace automata
