#pragma once

#include <cstdint>
#include <span>

#include "automata/core/control_bus.hpp"

// OSC 1.0 decode straight into the ControlBus — the receive half is all a
// live-control surface needs. An address minus its leading '/' names the
// slot: the first numeric argument (i f d h, or T/F as 1/0) writes `name`,
// later numeric ones write `name/1`, `name/2`, …. Bundles unpack
// recursively with timetags ignored — external control is *now*. Every
// read is bounds-checked; a message with an unknown type tag, a truncated
// payload, or any malformed framing writes nothing at all, and non-finite
// floats are dropped before they can poison a glide. Returns the number of
// slot writes.

namespace automata {

[[nodiscard]] std::uint32_t apply_osc_packet(std::span<const std::byte> packet,
                                             ControlBus& bus);

}  // namespace automata
