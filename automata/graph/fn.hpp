#pragma once

#include <cstring>
#include <span>
#include <string_view>

#include "automata/graph/builder.hpp"

// fn: a named stateless node from a captureless function (ADR 0009). The
// name is the identity — the tap/param rule — so editing the body never
// disturbs downstream state. The pointer rides the op bytes and targets
// patch code, which pins the defining generation like any custom kernel;
// the reconciler always swaps a pinned def so body edits land.

namespace automata {

namespace detail {

template <typename F>
[[nodiscard]] F fn_from_op(const std::byte* op) {
  F f = nullptr;
  std::memcpy(&f, op, sizeof(f));
  return f;
}

inline void fn_process1(void*,
                        const float* const* inputs,
                        const std::byte* op,
                        const float*,
                        float* out,
                        std::uint32_t nframes) {
  const auto f = fn_from_op<float (*)(float)>(op);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    out[i] = f(inputs[0][i]);
  }
}

inline void fn_process2(void*,
                        const float* const* inputs,
                        const std::byte* op,
                        const float*,
                        float* out,
                        std::uint32_t nframes) {
  const auto f = fn_from_op<float (*)(float, float)>(op);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    out[i] = f(inputs[0][i], inputs[1][i]);
  }
}

inline void fn_process3(void*,
                        const float* const* inputs,
                        const std::byte* op,
                        const float*,
                        float* out,
                        std::uint32_t nframes) {
  const auto f = fn_from_op<float (*)(float, float, float)>(op);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    out[i] = f(inputs[0][i], inputs[1][i], inputs[2][i]);
  }
}

// Arity is part of the base hash, so fn("x", unary) and fn("x", binary)
// are distinct identities.
inline const KernelInfo FnInfo1{
    .type_hash = hash_string("automata.Fn1"),
    .state_size = 0,
    .state_align = 1,
    .output_count = 1,
    .construct = +[](void*) {},
    .reset = +[](void*) {},
    .process = &fn_process1,
};

inline const KernelInfo FnInfo2{
    .type_hash = hash_string("automata.Fn2"),
    .state_size = 0,
    .state_align = 1,
    .output_count = 1,
    .construct = +[](void*) {},
    .reset = +[](void*) {},
    .process = &fn_process2,
};

inline const KernelInfo FnInfo3{
    .type_hash = hash_string("automata.Fn3"),
    .state_size = 0,
    .state_align = 1,
    .output_count = 1,
    .construct = +[](void*) {},
    .reset = +[](void*) {},
    .process = &fn_process3,
};

// True for this binary's fn-family infos — how the registry recognizes a
// probe's fn nodes to key them by name (rebind.hpp).
[[nodiscard]] inline bool is_fn_kernel(const KernelInfo* info) {
  return info == &FnInfo1 || info == &FnInfo2 || info == &FnInfo3;
}

template <typename F>
[[nodiscard]] Signal fn_node(const KernelInfo* info,
                             std::string_view name,
                             F f,
                             std::span<const Signal> inputs) {
  std::byte op[sizeof(F)];
  std::memcpy(op, &f, sizeof(f));
  return ActiveGraph::current().add_node(
      info, hash_combine(info->type_hash, hash_string(name)), inputs, {}, {},
      op);
}

}  // namespace detail

// A captureless lambda or free function as a one-off stateless node. The
// pointer parameter is the capture firewall: a capturing lambda fails to
// convert, so state can't sneak past the hashing and transfer machinery.
[[nodiscard]] inline Signal fn(std::string_view name,
                               float (*f)(float),
                               Signal a) {
  const Signal inputs[1] = {a};
  return detail::fn_node(&detail::FnInfo1, name, f, inputs);
}

[[nodiscard]] inline Signal fn(std::string_view name,
                               float (*f)(float, float),
                               Signal a,
                               Signal b) {
  const Signal inputs[2] = {a, b};
  return detail::fn_node(&detail::FnInfo2, name, f, inputs);
}

[[nodiscard]] inline Signal fn(std::string_view name,
                               float (*f)(float, float, float),
                               Signal a,
                               Signal b,
                               Signal c) {
  const Signal inputs[3] = {a, b, c};
  return detail::fn_node(&detail::FnInfo3, name, f, inputs);
}

}  // namespace automata
