#include "automata/patch.hpp"

// External control (ADR 0007): OSC writes land on params — try
//   /cutoff 1200.0   /amp 0.5   (host --osc 9000)
// — and the mouse's vertical position feeds the delay back.
AUTOMATA_PATCH(g) {
  using namespace automata;
  auto env = ar(metro(2.0f), 0.005f, 0.12f);
  auto voice = svf_lp(saw(110.0f), param("cutoff", 800.0f), 0.7f) * env;
  auto fb = g.tap("fb");
  auto wet = voice + fb.read(0.375f) * (mouse_y() * 0.6f);
  fb.write(wet);
  auto out = soft_clip(wet * param("amp", 0.8f));
  g.out(out, out);
}
