#include "host/mouse.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace automata::host {

#if defined(_WIN32)

void poll_mouse(ControlBus& bus) {
  POINT point;
  if (GetCursorPos(&point) == 0) {
    return;
  }
  const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (width <= 1 || height <= 1) {
    return;
  }
  const auto normalized = [](int value, int origin, int extent) {
    const float t =
        static_cast<float>(value - origin) / static_cast<float>(extent - 1);
    return t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
  };
  bus.set("mouse/x",
          normalized(point.x, GetSystemMetrics(SM_XVIRTUALSCREEN), width));
  bus.set("mouse/y",
          normalized(point.y, GetSystemMetrics(SM_YVIRTUALSCREEN), height));
}

#else

void poll_mouse(ControlBus&) {}

#endif

}  // namespace automata::host
