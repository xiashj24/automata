#include "automata/patch.hpp"

using namespace automata;

AUTOMATA_PATCH(g) {
  auto lfo = sine(0.25f);
  auto env = ar(metro(2.0f), 0.005f, 0.12f);
  auto voice = svf_lp(saw(110.0f), 800.0f + lfo * 600.0f, 0.7f) * env;
  auto fb = g.tap("fb");
  auto wet = voice + fb.read(0.375f) * 0.35f;
  fb.write(wet);
  g.out(soft_clip(wet), soft_clip(wet));
}
