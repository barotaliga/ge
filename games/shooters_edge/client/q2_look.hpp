// QUAKE II'S LOOK BITS: what an entity's effect bits and render bits (Quake II protocol's two
// fields) ask of the picture, read once into a `Q2Look` the draw applies.
//
// - SEE-THROUGH: the render bits exactly "translucent" draw it at 0.7; a beam at 0.3; the BFG's
//   effect bit (its ball in flight and its burst) at 0.3. A sprite is solid (its clear texels cut
//   out) unless it is see-through.
// - SPIN: the rotate effect bit turns it about its yaw on the picture's clock, its own angles
//   ignored (an item on the floor).
// - GLOW (render bit 512, every item): its light pulses, gaining 0.1 * sin(7 t) a channel, never
//   under 80 % of the light before the pulse (`q2_pulse`).
// - THE SHELL: quad damage (an effect bit) is a blue shell, invulnerability a red one, a power
//   shield's hit a green one, god mode all three (white); a shell is a second copy of the model,
//   untextured, its points pushed 4 units out along their normals, a flat colour that ignores the
//   room's light, at 0.3. The copy under it is drawn plain: no glow, no see-through.
// - TRAILS: a rocket's smoke and fire, a grenade's smoke, a blaster's and a hyperblaster's
//   bolt, the BFG's ball (`Q2Look::missile`).
//
// ge:summary Quake II's effect and render bits read into what the picture draws
// ge:keywords q2 render-fx effects shell quad invulnerability translucent glow pulse rotate
// ge:keywords colour-shell power-shield god-mode see-through items-glow trails power_look
// ge:keywords r_powershells r_itempulse q1-shells
// ge:skill render shooter
// ge:test tests/shooters_edge_q2_look_test.cpp
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

struct Q2Look {
  float alpha = 1;                 // the model's (or sprite's) share that shows; 1 solid
  bool turns = false;              // about its yaw on the picture's clock
  bool pulse = false;              // its light pulses (`q2_pulse`)
  bool shell = false;              // a shell copy over it
  bool bleeds = false;             // a body's piece: a blood trail behind it (the gib bit)
  float shell_rgb[3] = {0, 0, 0};  // the shell's flat colour, 0..1
  // Its trail and light: a rocket (smoke and fire), a grenade (smoke), a bolt (a blaster's or a
  // hyperblaster's), the BFG's ball; none: a plain entity.
  enum class Missile : uint8_t { none, rocket, grenade, bolt, bfg } missile = Missile::none;
};

// THE POWERS' SHELLS AND THE ITEMS' PULSE IN EITHER GAME (r_powershells, r_itempulse). Quake II
// draws both from its own bits above; Quake had neither, its powers shown by their glow
// on the walls alone, so it takes them from its own bits when the player asks: quad damage (its
// blue glow bit) a blue shell on its carrier, the pentagram (the red) a red one, both magenta, as
// Quake II colours them (the pickups glow by the same bits and wear none); an item (a model that
// turns as it lies, its model file's rotate flag) pulses as Quake II's items do. Each is drawn
// only while its switch is on, in either game.
inline constexpr uint32_t kFirstQuad = 64u, kFirstPentagram = 128u;  // Quake's glow bits
inline constexpr uint32_t kFirstRotates = 8u;                        // its model flag
struct PowerIn {
  bool q2 = false;                    // Quake II's entity: its look read already (below)
  bool player = false;                // a player (never an item)
  uint32_t effects = 0;               // Quake's effect bits
  uint32_t model_flags = 0;           // Quake's model file flags
  bool shell = false, pulse = false;  // Quake II's look (Q2Look)
  float shell_rgb[3] = {0, 0, 0};
};
struct PowerLook {
  bool shell = false, pulse = false;
  float shell_rgb[3] = {0, 0, 0};
};
PowerLook power_look(const PowerIn& in, bool power_shells, bool item_pulse);

// The look of an entity with these effect and render bits.
Q2Look q2_look(uint32_t effects, uint32_t render_fx);

// The frame a Quake II brush model's animated textures show at the picture's clock `t`, by its
// effect bits (the first set of: ANIM01 0x400, frames 0 and 1 by the half second; ANIM23 0x800,
// 2 and 3; ANIM_ALL 0x1000, one more each half second; ANIM_ALLFAST 0x2000, ten a second), else
// its own `frame`.
int q2_texture_frame(uint32_t effects, int frame, double t);

// The glow's pulse on a light (each channel, 0..1 a lightmap's white) at the picture's clock `t`.
void q2_pulse(float rgb[3], double t);

}  // namespace shooters_edge::client
