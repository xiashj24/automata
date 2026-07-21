#include "automata/patch.hpp"

using namespace automata;

AUTOMATA_PATCH(g) {
  // Kick on the euclidean grid: a pitch-dropping sine per trigger.
  auto kick_trig = euclid(bar(), 3.f, 8.f);
  auto kick_env = ar(kick_trig, 0.001f, 0.25f);
  auto kick = sine(50.f + kick_env * 80.f) * kick_env;

  // Swung offbeat hats: filtered clicks from the doubled beat.
  auto hat_trig = (beat() * 2.f).swing(param("swing", 0.55f)).trig();
  auto hat = svf_hp(hat_trig, 6000.f, 0.5f) * 0.6f;

  // A bass line stepping over the bar, gated to leave air.
  auto note = seq(bar(), {55.f, 55.f, 65.41f, 49.f});
  auto bass = saw(note) * beat().gate(0.6f) * 0.35f;
  auto bass_voice = svf_lp(bass, 400.f + mouse_y() * 2000.f, 0.8f);

  auto mix = kick * 0.9f + hat + bass_voice;
  auto out = fn(
      "drive", [](float x) { return x / (1.f + (x < 0.f ? -x : x)); },
      mix * 1.5f);
  g.out(out, out);
}
