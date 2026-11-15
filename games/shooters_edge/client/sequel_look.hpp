// THE SEQUEL'S LOOK BITS: what an entity's effect bits and render bits (the sequel protocol's two
// fields) ask of the picture, read once into a `SequelLook` the draw applies.
//
// - SEE-THROUGH: the render bits exactly "translucent" draw it at 0.7; a beam at 0.3; the BFG's
//   effect bit (its ball in flight and its burst) at 0.3. A sprite is solid (its clear texels cut
//   out) unless it is see-through.
// - SPIN: the rotate effect bit turns it about its yaw on the picture's clock, its own angles
//   ignored (an item on the floor).
// - GLOW (render bit 512, every item): its light pulses, gaining 0.1 * sin(7 t) a channel, never
//   under 80 % of the light before the pulse (`sequel_pulse`).
// - THE SHELL: quad damage (an effect bit) is a blue shell, invulnerability a red one, a power
//   shield's hit a green one, god mode all three (white); a shell is a second copy of the model,
//   untextured, its points pushed 4 units out along their normals, a flat colour that ignores the
//   room's light, at 0.3. The copy under it is drawn plain: no glow, no see-through.
// - TRAILS: a rocket's smoke and fire, a grenade's smoke, a blaster's and a hyperblaster's
//   bolt, the BFG's ball (`SequelLook::missile`).
//
// ge:summary The sequel's effect and render bits read into what the picture draws
// ge:keywords sequel render-fx effects shell quad invulnerability translucent glow pulse rotate
// ge:keywords colour-shell power-shield god-mode see-through items-glow trails
// ge:skill render shooter
// ge:test tests/shooters_edge_sequel_look_test.cpp
#pragma once

#include <cstdint>

namespace shooters_edge::client {

// The effect bits (the entity's `effects`) this client reads.
inline constexpr uint32_t kSqRotate = 1u, kSqGib = 2u, kSqBlaster = 8u, kSqRocket = 16u,
                          kSqGrenade = 32u, kSqHyperblaster = 64u, kSqBfg = 128u,
                          kSqColourShell = 256u, kSqQuad = 32768u, kSqInvulnerable = 65536u,
                          kSqDoubleDamage = 1u << 27, kSqHalfDamage = 1u << 30;
// The render bits (the entity's `renderfx`).
inline constexpr uint32_t kRfTranslucent = 32u, kRfBeam = 128u, kRfGlow = 512u, kRfShellRed = 1024u,
                          kRfShellGreen = 2048u, kRfShellBlue = 4096u, kRfShellDouble = 65536u,
                          kRfShellHalf = 131072u;

inline constexpr float kSqShellPushUnits = 4;  // a shell's points out along their normals
inline constexpr float kSqShellAlpha = 0.3f;

struct SequelLook {
  float alpha = 1;                 // the model's (or sprite's) share that shows; 1 solid
  bool turns = false;              // about its yaw on the picture's clock
  bool pulse = false;              // its light pulses (`sequel_pulse`)
  bool shell = false;              // a shell copy over it
  float shell_rgb[3] = {0, 0, 0};  // the shell's flat colour, 0..1
  // Its trail and light: a rocket (smoke and fire), a grenade (smoke), a bolt (a blaster's or a
  // hyperblaster's), the BFG's ball; none: a plain entity.
  enum class Missile : uint8_t { none, rocket, grenade, bolt, bfg } missile = Missile::none;
};

// The look of an entity with these effect and render bits.
SequelLook sequel_look(uint32_t effects, uint32_t render_fx);

// The glow's pulse on a light (each channel, 0..1 a lightmap's white) at the picture's clock `t`.
void sequel_pulse(float rgb[3], double t);

}  // namespace shooters_edge::client
