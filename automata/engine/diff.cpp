#include "automata/engine/diff.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "automata/core/assert.hpp"
#include "automata/engine/graph.hpp"
#include "automata/graph/kernel_info.hpp"

namespace automata {

namespace {

struct Matcher {
  const GraphDef& from;
  const GraphDef& to;
  std::vector<std::uint32_t>& matches;  // from → to
  std::vector<std::uint8_t> to_taken;

  // Descends structurally: a pair is valid only when hashes agree, and its
  // inputs pair positionally — equal-hash siblings land by position, not by
  // flat ordinal, so reordering statements cannot cross-wire them.
  void descend(std::uint32_t i, std::uint32_t j) {
    if (from.nodes[i].structural_hash != to.nodes[j].structural_hash)
      return;
    if (matches[i] != NoMatch || to_taken[j] != 0)
      return;
    matches[i] = j;
    to_taken[j] = 1;
    const GraphDef::Node& a = from.nodes[i];
    const GraphDef::Node& b = to.nodes[j];
    atm_assert(a.input_count == b.input_count);  // hashes agreed
    for (std::uint32_t k = 0; k < a.input_count; ++k) {
      descend(from.inputs[a.input_begin + k], to.inputs[b.input_begin + k]);
    }
  }
};

// Sinks in deterministic order: by structural hash, then def order.
std::vector<std::uint32_t> sorted_sinks(const GraphDef& def) {
  std::vector<std::uint32_t> sinks;
  for (std::uint32_t i = 0; i < def.nodes.size(); ++i) {
    if (def.nodes[i].sink != 0)
      sinks.push_back(i);
  }
  std::stable_sort(
      sinks.begin(), sinks.end(), [&](std::uint32_t a, std::uint32_t b) {
        return def.nodes[a].structural_hash < def.nodes[b].structural_hash;
      });
  return sinks;
}

}  // namespace

std::vector<std::uint32_t> match_nodes(const GraphDef& from,
                                       const GraphDef& to) {
  std::vector<std::uint32_t> matches(from.nodes.size(), NoMatch);
  Matcher m{.from = from,
            .to = to,
            .matches = matches,
            .to_taken = std::vector<std::uint8_t>(to.nodes.size(), 0)};
  m.descend(from.outs[0], to.outs[0]);
  m.descend(from.outs[1], to.outs[1]);

  const auto from_sinks = sorted_sinks(from);
  const auto to_sinks = sorted_sinks(to);
  std::size_t cursor = 0;
  for (const std::uint32_t i : from_sinks) {
    while (cursor < to_sinks.size() &&
           to.nodes[to_sinks[cursor]].structural_hash <
               from.nodes[i].structural_hash) {
      ++cursor;
    }
    if (cursor < to_sinks.size() &&
        to.nodes[to_sinks[cursor]].structural_hash ==
            from.nodes[i].structural_hash) {
      m.descend(i, to_sinks[cursor]);
      ++cursor;
    }
  }

  // Second pass: subtrees that moved (an inserted wrapper changes every
  // ancestor's hash but not the subtree's own) pair globally by hash —
  // parents before leaves, so each descent claims a coherent subtree.
  std::unordered_map<Hash, std::vector<std::uint32_t>> to_by_hash;
  for (std::uint32_t j = 0; j < to.nodes.size(); ++j) {
    if (m.to_taken[j] == 0) {
      to_by_hash[to.nodes[j].structural_hash].push_back(j);
    }
  }
  for (std::uint32_t r = static_cast<std::uint32_t>(from.nodes.size()); r > 0;
       --r) {
    const std::uint32_t i = r - 1;
    if (matches[i] != NoMatch)
      continue;
    // Bare leaves (every Const hashes alike) carry no context of their own;
    // pairing them here would wire unrelated constants together. They still
    // match through a parent's descent.
    if (from.nodes[i].input_count == 0)
      continue;
    const auto bucket = to_by_hash.find(from.nodes[i].structural_hash);
    if (bucket == to_by_hash.end())
      continue;
    for (const std::uint32_t j : bucket->second) {
      if (m.to_taken[j] == 0) {
        m.descend(i, j);
        break;
      }
    }
  }
  return matches;
}

std::vector<float> remap_values(const GraphDef& running,
                                const GraphDef& incoming) {
  atm_assert(running.def_hash == incoming.def_hash);
  std::vector<float> values = running.values;
  const auto matches = match_nodes(running, incoming);
  for (std::uint32_t i = 0; i < running.nodes.size(); ++i) {
    if (matches[i] == NoMatch)
      continue;
    const GraphDef::Node& mine = running.nodes[i];
    const GraphDef::Node& theirs = incoming.nodes[matches[i]];
    atm_assert(mine.value_count == theirs.value_count);
    for (std::uint32_t v = 0; v < mine.value_count; ++v) {
      values[mine.value_begin + v] = incoming.values[theirs.value_begin + v];
    }
  }
  return values;
}

TransferPlan plan_transfer(const Graph& from, Graph& to) {
  TransferPlan plan;
  const auto matches = match_nodes(from.def(), to.def());
  for (std::uint32_t i = 0; i < from.def().nodes.size(); ++i) {
    if (matches[i] == NoMatch)
      continue;
    const KernelInfo& kernel = *from.def().nodes[i].kernel;
    atm_assert(kernel.type_hash ==
               to.def().nodes[matches[i]].kernel->type_hash);
    if (kernel.state_size == 0)
      continue;
    plan.copies.push_back({.src = from.state_ptr(i),
                           .dst = to.state_ptr(matches[i]),
                           .size = kernel.state_size});
    plan.total_bytes += kernel.state_size;
  }
  return plan;
}

}  // namespace automata
