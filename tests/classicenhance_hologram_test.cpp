// Tests: an item coming back drawn as a hologram (plugins/classicenhance/hologram.hpp). Nothing is
// drawn before the lead or once it is back; it starts fully see-through and blue, fills in, and
// ends solid in its own light with no shell, so the real item takes its place with no step.
//
// ge:summary Tests: the item-respawn hologram's look over its half second
// ge:keywords test hologram respawn ce_respawns hologram_look fade-in
// ge:skill classicenhance testing
#include "check.hpp"
#include "classicenhance/hologram.hpp"

namespace ce = ge::classicenhance;

TEST(nothing_is_drawn_before_the_lead_or_when_no_time_is_known) {
  CHECK(!ce::hologram_look(ce::kHologramLeadS + 0.01, 0).shown);
  CHECK(!ce::hologram_look(-1, 0).shown);
  CHECK(!ce::hologram_look(0.2, 0, 0).shown);
  CHECK(ce::hologram_look(ce::kHologramLeadS, 0).shown);
}

TEST(it_starts_see_through_and_blue_and_ends_solid_in_its_own_light) {
  const ce::HologramLook start = ce::hologram_look(ce::kHologramLeadS, 0.3);
  CHECK(start.alpha == 0.0f);
  CHECK(start.blue == 1.0f);
  CHECK(start.shell_alpha == 0.0f);
  const ce::HologramLook end = ce::hologram_look(0, 0.3);
  CHECK(end.alpha == 1.0f);
  CHECK(end.blue == 0.0f);
  CHECK(end.shell_alpha == 0.0f);
}

TEST(it_fills_in_steadily_and_glows_blue_on_the_way) {
  float was = -1;
  bool shelled = false;
  for (int i = 0; i <= 50; i++) {
    const double left = ce::kHologramLeadS * (1 - i / 50.0);
    const ce::HologramLook h = ce::hologram_look(left, i * 0.01);
    CHECK(h.shown);
    CHECK(h.alpha >= was);
    CHECK(h.alpha >= 0 && h.alpha <= 1 && h.blue >= 0 && h.blue <= 1);
    CHECK(h.shell_alpha >= 0 && h.shell_alpha <= ce::kHologramShellAlpha);
    shelled |= h.shell_alpha > 0.15f;
    was = h.alpha;
  }
  CHECK(shelled);
  const ce::HologramLook mid = ce::hologram_look(ce::kHologramLeadS / 2, 0);
  CHECK(mid.alpha > 0.4f && mid.alpha < 0.6f);
  CHECK(mid.blue == 1.0f);  // still all hologram half way
}
