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
// - FLIES round a corpse (the flies bit); a POWER SCREEN's plate (the screen bit, while it takes
//   a blow): its own model (`kSqPowerScreenModel`, frame 0) at the bearer's place and angles,
//   drawn as a green shell alone.
// - THE DISGUISE (render bit 1 << 18, the mission pack's): a player's figure in its folder's
//   `disguise` skin (`q2_disguised`); a figure without one wears its own.
//
// ge:summary Quake II's effect and render bits read into what the picture draws
// ge:keywords q2 render-fx effects shell quad invulnerability translucent glow pulse rotate
// ge:keywords colour-shell power-shield god-mode see-through items-glow trails power_look
// ge:keywords r_powershells r_itempulse q1-shells disguise use-disguise q2_disguised
// ge:skill render shooter
// ge:test tests/shooters_edge_q2_look_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::client {

// The effect bits (the entity's `effects`) this client reads.
inline constexpr uint32_t kSqRotate = 1u, kSqGib = 2u, kSqBob = 4u, kSqBlaster = 8u,
                          kSqRocket = 16u, kSqGrenade = 32u, kSqHyperblaster = 64u, kSqBfg = 128u,
                          kSqColourShell = 256u, kSqPowerScreen = 512u, kSqFlies = 16384u,
                          kSqQuad = 32768u, kSqInvulnerable = 65536u, kSqFlag1 = 1u << 18,
                          kSqFlag2 = 1u << 19, kSqIonRipper = 1u << 20, kSqGreenGib = 1u << 21,
                          kSqBlueHyperblaster = 1u << 22, kSqSpinningLights = 1u << 23,
                          kSqPlasma = 1u << 24, kSqTrap = 1u << 25, kSqTracker = 1u << 26,
                          kSqDoubleDamage = 1u << 27, kSqSphereTrans = 1u << 28,
                          kSqTagTrail = 1u << 29, kSqHalfDamage = 1u << 30,
                          kSqTrackerTrail = 1u << 31;
// The render bits (the entity's `renderfx`).
inline constexpr uint32_t kRfMinLight = 1u, kRfFullBright = 8u, kRfTranslucent = 32u,
                          kRfBeam = 128u, kRfGlow = 512u, kRfShellRed = 1024u,
                          kRfShellGreen = 2048u, kRfShellBlue = 4096u, kRfShellDouble = 65536u,
                          kRfShellHalf = 131072u, kRfShellLiteGreen = 1u << 19,
                          kRfIrVisible = 32768u, kRfNoShadow = 8192u, kRfUseDisguise = 1u << 18,
                          kRfCustomSkin = 256u;
// The view's bit for infra-red goggles: an `kRfIrVisible` model drawn red, lit by nothing.
inline constexpr int kQ2RdIrGoggles = 4;

inline constexpr const char* kSqPowerScreenModel = "models/items/armor/effect/tris.md2";
inline constexpr float kSqShellPushUnits = 4;  // a shell's points out along their normals
inline constexpr float kSqShellAlpha = 0.3f;
inline constexpr double kSqSpinDegPerS = 500;  // spinning lights' turn
// Seen through infra-red goggles (the view's `kQ2RdIrGoggles`): a model marked `kRfIrVisible`
// is drawn as its skin times pure red, lit by nothing, unless a shell or full light holds it.
bool q2_infra_red(int rdflags, uint32_t render_fx, bool shell, bool full_bright);
// The bob bit's lift at `t` s, units.
double q2_bob_units(double t);

struct Q2Look {
  float alpha = 1;     // the model's (or sprite's) share that shows; 1 solid
  bool turns = false;  // about its yaw on the picture's clock
  // Upside down, spinning 500 degrees a second, a red light 64 units ahead of it (spinning lights;
  // the rotate bit wins).
  bool spinning = false;
  // Its light: full whatever the room (fullbright), or never darker than a tenth (minlight).
  bool full_bright = false, min_light = false;
  bool no_shadow = false;  // it casts no ground shadow (model_shadow.hpp)
  bool disguise = false;   // a player's figure in its folder's disguise skin (`q2_disguised`)
  bool bobs = false;    // rises and sinks 5 units, a cycle every 2.5 s, all in step (the bob bit)
  bool pulse = false;   // its light pulses (`q2_pulse`)
  bool shell = false;   // a shell copy over it
  bool bleeds = false;  // a body's piece: a blood trail behind it (the gib bit)
  bool green_blood = false;        // that trail green (the green gib bit alone)
  bool flies = false;              // a corpse: flies swarm round it (effects::q2_flies)
  bool power_screen = false;       // a power screen's plate before it, a green shell (it was hit)
  float shell_rgb[3] = {0, 0, 0};  // the shell's flat colour, 0..1
  // Its trail and light: a rocket (smoke and fire), a grenade (smoke), a bolt (a blaster's or a
  // hyperblaster's), the BFG's ball; a flag carried (red or blue: a fuzzy column of specks and a
  // light), a tag token (the same in yellow), an ion ripper's bolt (red specks shaken across,
  // a pink light), a blue bolt (a blue light), a plasma bolt (a pink light, see-through at 0.6);
  // none: a plain entity.
  enum class Missile : uint8_t {
    none,
    rocket,
    grenade,
    bolt,
    bfg,
    flag1,
    flag2,
    tag,
    ion,
    blue,
    plasma,
    trap,
    tracker,
    tracker_dark,
    tracker_shell
  } missile = Missile::none;
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

// The disguise's look for a player's look ("male/grunt"): its folder's `disguise` skin
// ("male/disguise"); a look with no folder, "male/disguise".
std::string q2_disguised(std::string_view look);

// A picture the server names by its list's name: a name after a slash is a path in the game's
// files ("/models/x/skin.pcx"), any other is under `pics/` as a .pcx ("i_health").
std::string q2_picture_path(std::string_view name);
// The custom skin bit (`kRfCustomSkin`, read only from an extended server or recording): the skin
// number names a picture of the server's list (`images`) the model wears in place of its own, by
// its path (`q2_picture_path`); "" when the bit is clear or the number names none.
std::string q2_custom_skin(uint32_t render_fx, int skin, std::span<const std::string> images);

// The glow's pulse on a light (each channel, 0..1 a lightmap's white) at the picture's clock `t`.
void q2_pulse(float rgb[3], double t);

}  // namespace shooters_edge::client
