// AN ITEM COMING BACK AS A HOLOGRAM (`ce_respawns`): for the last `kHologramLeadS` before a taken
// item shows again, it is drawn where it will lie as a blue see-through likeness of itself that
// fills in until it is solid, so it never pops into being. Off (or the classic look): the item
// appears at once, as the pack's game had it.
//
// `hologram_look` is the whole of the look as a pure function of the time left: how much of the
// model shows (`alpha`, the morph pass's ordered see-through), how far its light is the
// hologram's blue rather than the map's (`blue`), and the faint blue shell drawn over it
// (`shell_alpha`, flickering a little like a projection). At the moment it comes back it is the
// item itself, solid and in its own light, so the real one takes its place with no step.
//
// The window learns how long an item has left from the rules' world (shooter/rules_world.hpp's
// `WorldEntity::back_in`), which each server fills from its own clock; a game that cannot say
// leaves it unknown and draws no hologram.
//
// ge:summary An item about to respawn drawn as a blue hologram filling in to solid: the look
// ge:keywords hologram respawn item-respawn ce_respawns item-back fade-in see-through shell
// ge:keywords hologram_look hologram-lead returning-item
// ge:skill classicenhance
// ge:test tests/classicenhance_hologram_test.cpp
#pragma once

namespace ge::classicenhance {

// How long before it comes back an item shows as a hologram, s.
inline constexpr double kHologramLeadS = 0.5;

// The hologram's colour, linear 0..1 in the scene's white: the light on the model and its shell.
inline constexpr float kHologramBlue[3] = {0.30f, 0.62f, 1.25f};

// The shell's most see-through share (a faint glow over the model, never hiding its shape).
inline constexpr double kHologramShellAlpha = 0.35;

struct HologramLook {
  bool shown = false;     // false: not drawn (too long left, or already back)
  float alpha = 1;        // how much of the model shows, 0..1
  float blue = 0;         // the share of its light that is the hologram's blue, 0..1
  float shell_alpha = 0;  // the blue shell over it, 0 none
};

namespace hologram_detail {
inline double smoothstep(double a, double b, double x) {
  if (x <= a) return 0;
  if (x >= b) return 1;
  const double u = (x - a) / (b - a);
  return u * u * (3 - 2 * u);
}
}  // namespace hologram_detail

// The look `left_s` seconds before the item comes back, at the picture's clock `time_s` (the
// shell's flicker). Not shown when more than `lead_s` is left or none is.
inline HologramLook hologram_look(double left_s, double time_s, double lead_s = kHologramLeadS) {
  HologramLook h;
  if (!(lead_s > 0) || !(left_s >= 0) || left_s > lead_s) return h;
  const double r = 1 - left_s / lead_s;  // 0 just appearing .. 1 back
  h.shown = true;
  h.alpha = static_cast<float>(hologram_detail::smoothstep(0, 1, r));
  // Blue the whole way, turning to its own light over the last third.
  h.blue = static_cast<float>(1 - hologram_detail::smoothstep(0.65, 1, r));
  // A projection's flicker: a triangle wave of 11 a second between 0.75 and 1 (no transcendental:
  // the same on every machine).
  double ph = time_s * 11;
  ph -= static_cast<double>(static_cast<long long>(ph));
  if (ph < 0) ph += 1;
  const double flicker = 0.75 + 0.25 * (ph < 0.5 ? 2 * ph : 2 - 2 * ph);
  // A faint glow round it, never hiding the model's own shape.
  h.shell_alpha = static_cast<float>(kHologramShellAlpha * h.blue * h.alpha * flicker);
  return h;
}

}  // namespace ge::classicenhance
