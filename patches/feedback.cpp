#include "automata/patch.hpp"

using namespace automata;

// Sample-serial feedback (ADR 0011): both loops here are tighter than a
// block, so the engine runs each cycle one sample at a time.
AUTOMATA_PATCH(g) {
  // Feedback FM: the sine modulates its own phase through a z^-1 tap.
  auto gate = pulsen(25_ms, 1.5_s);
  auto env = are(gate, 3_ms, 0.9_s);
  auto fm = g.tap("fm");
  auto voice =
      sine(55_hz + mouse_x() * 200.f, fm.read() * env * param("fm", 0.9f)) *
      env;
  fm.write(voice);

  // Flanger on a saw pad: a swept sub-block delay with regeneration.
  auto pad = saw(110_hz) * 0.2f;
  auto fl = g.tap("flange");
  auto swept = fl.read(1_ms + (sine(0.2_hz) * 0.5f + 0.5f) * 4_ms);
  fl.write(pad + swept * param("regen", 0.6f));

  auto mix = voice * 0.6f + (pad + swept) * 0.8f;
  g.out(soft_clip(mix), soft_clip(mix));
}
