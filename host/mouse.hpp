#pragma once

#include "automata/core/control_bus.hpp"

namespace automata::host {

// Writes the cursor position to mouse/x and mouse/y, 0..1 across the
// virtual desktop with y = 1 at the top. A no-op on platforms without a
// cursor API wired up.
void poll_mouse(ControlBus& bus);

}  // namespace automata::host
