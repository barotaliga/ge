// A BEAM OF LIGHTNING held on a target (docs/rendering.md § Beams): an electric arc a weapon, a
// machine or a spell keeps burning from a point to whatever it hits for as long as it is held,
// drawn as a channel of glowing segments, lighting the world round it and scorching where it
// lands. One code and a `BeamLook` of knobs; the presets are three ways an arc can read.
//
// - THE CHANNEL (`build_beam`): a walk from the source to the hit in `step_m` steps, each point
//   pushed sideways by a random kick (`wander`, a share of the step) and pulled back toward the
//   straight line (`pull`), pinned at both ends; so a beam stays as straight as its pull keeps it
//   and kinks as much as its wander lets it. `filaments` channels share the axis, each its own
//   walk wound round it on a helix (`braid_m` off the axis, `twist` turns a metre, spinning at
//   `spin_hz`), a plasma rope. A channel forks (`fork_chance` a step) into a short side branch
//   `fork_m` long, dimmer and thinner, that dies before the channel re-forms; never in the first
//   1.5 m off the source, where by the eye a fork would fill the view.
// - RE-FORMING: a real arc's channel does not move, it is struck again. The walk is redrawn
//   `strike_hz` times a second, each strike off its own hash of the beam's seed and its number,
//   so a beam held for a second shows that many channels one after the other, never a smooth
//   morph, and two frames inside one strike show the same channel.
// - THE LIGHT (`beam_level`): a strike flashes up and settles (its first `flash_s` brighter by
//   `flash`), each strike a little brighter or dimmer than the last (`flicker`), the whole beam
//   swelling in over `attack_s` when fired and dying over `release_s` when let go. The shader
//   adds a shimmer along its length (`shimmer`) on top: what moves inside one strike.
// - THE LOOK: a white-hot core `core_m` wide in `core_rgb`, a glowing sheath `glow_m` wide in
//   `glow_rgb` (the ionised air round it), both in cd/m^2 at full level (`core_cd`, `glow_cd`) so
//   the frame's exposure takes them as it takes the sun and the lamps. The core is whitened as it
//   brightens, so it burns white in the middle and holds its colour at the rim.
// - THE WORLD LIT (`beam_lights`): one light at the beam's root by its source (`root_cd`: what
//   lights the gun that fires it), a row along it (one each `light_every_m`, at most
//   kBeamLights - 2, its `light_cd` shared among them, each as soft as the spacing), and one at
//   the hit (`hit_cd`, a little off the surface): render/lights.hpp's `LocalLight`s, at the
//   beam's level of this frame.
// - THE BURN (`beam_burns`): where a held beam lands it leaves a scorch (`scorch_m` across) every
//   `scorch_every_s` while it lands, and at once wherever the hit has moved a third of a scorch
//   from the last one; a hit that jumped further between two frames gets its burns half a scorch
//   apart along the jump, one each time the caller asks, so a beam swept over a wall leaves an
//   unbroken black trail at any frame rate. The caller
//   presses each onto its world (engine/detail's `place_decal`, a scorch); a fresh burn glows
//   (`ember_s`, `ember_cd`) and cools to the soot it leaves, read by `ember_level`.
//
// Pure: the same look, shot and time give the same channel, levels, lights and burns (every
// random draw a hash of the shot's seed, the strike and the step; ge::core::libm for the sines).
//
// ge:summary Lightning beams: the channel, re-striking, flicker, the lights, scorches and embers
// ge:keywords render beam lightning arc bolt electric plasma channel strike flicker shimmer
// ge:keywords filament braid fork branch core glow sheath scorch burn ember beam_level
// ge:keywords build_beam beam_lights beam_burns beam_knobs thunderbolt weapon-effect
// ge:skill render
// ge:test tests/render_beam_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "render/lights.hpp"

namespace ge::render {

// Every knob of a beam's look. The units are in each comment; `beam_knobs` names them for a
// console or a lab's `--set=knob=value`.
struct BeamLook {
  // The channel.
  float step_m = 0.6f;    // the walk's step along the beam
  float wander = 0.35f;   // the sideways kick a step, a share of the step
  float pull = 0.35f;     // how much of its offset a point gives back toward the axis a step
  float filaments = 1;    // channels round the axis (1..kBeamFilaments)
  float braid_m = 0;      // a filament's helix off the axis
  float twist = 0;        // the helix's turns a metre
  float spin_hz = 0;      // turns a second the helix spins
  float fork_chance = 0;  // a fork's chance a step
  float fork_m = 0.8f;    // a fork's length
  float strike_hz = 24;   // the channel redrawn this many times a second
  // The light.
  float core_m = 0.035f;  // the white-hot core's width
  float glow_m = 0.35f;   // the glowing sheath's width
  float core_cd = 6e4f;   // the core's luminance at full level, cd/m^2
  float glow_cd = 120;    // the sheath's at its axis, cd/m^2
  float core_rgb[3] = {0.85f, 0.9f, 1.0f};
  float glow_rgb[3] = {0.45f, 0.55f, 1.0f};
  float flash = 0.6f;       // a strike's first moments brighter by this share
  float flash_s = 0.012f;   // and for this long
  float flicker = 0.3f;     // a strike's level off the last by up to this share
  float shimmer = 0.35f;    // the shader's ripple along the beam inside a strike, 0..1
  float attack_s = 0.03f;   // the swell when fired
  float release_s = 0.08f;  // the fade when let go
  // The world.
  float light_cd = 2500;    // the beam's own light, shared along it, cd
  float light_every_m = 3;  // a light each this far along the beam
  float hit_cd = 1800;      // the light where it lands, cd
  float root_cd = 200;      // the light at its root, by the source (a gun's muzzle), cd
  float light_rgb[3] = {0.55f, 0.65f, 1.0f};
  float scorch_m = 0.55f;        // a burn's size across
  float scorch_every_s = 0.35f;  // a new burn while it lands on one spot
  float ember_s = 1.2f;          // how long a fresh burn glows
  float ember_cd = 400;          // its glow at first, cd/m^2
};

inline constexpr int kBeamFilaments = 3;
inline constexpr int kBeamLights = 6;  // along the beam and at the hit
inline constexpr int kMaxBeamSegments = 512;

// The presets: "arc" (one thick, nearly straight channel: a held plasma arc), "bolt" (a jagged
// stepped channel that forks and strikes in pulses: lightning's own shape), "braid" (three
// filaments wound round each other and spinning, a crackling rope).
inline constexpr int kBeamLooks = 3;
std::string_view beam_look_name(int index);
// The preset named `name`; false (and `out` untouched) for a name no preset has.
bool beam_look(std::string_view name, BeamLook& out);

// THE KNOBS by name, for a console or a lab: the field, its low and high ends.
struct BeamKnob {
  std::string_view name;
  float BeamLook::*field;
  float lo, hi;
};
inline constexpr int kBeamKnobs = 28;
const BeamKnob* beam_knobs();
// Set knob `name` (clamped to its ends); false for an unknown name.
bool set_beam_knob(BeamLook& look, std::string_view name, float value);

// ONE SHOT: a beam held from `from` to `to` (world m) from `born_s` until `end_s` (the frame's
// clock; end_s < 0: still held). `normal` is the hit surface's, out of it (unit), for the hit's
// light and the burn; `seed` its own stream so two beams never strike as one.
struct BeamShot {
  double from[3] = {0, 0, 0};
  double to[3] = {0, 0, 1};
  double normal[3] = {0, 0, -1};
  double born_s = 0;
  double end_s = -1;
  uint32_t seed = 1;
};

// The beam's level at `t_s` (0 before it is fired and once it has faded; about 1 held, up to
// 1 + flash in a strike's first moments).
float beam_level(const BeamLook& look, const BeamShot& shot, double t_s);
// The strike `t_s` falls in (its number since the shot was fired).
int64_t beam_strike(const BeamLook& look, const BeamShot& shot, double t_s);

// A SEGMENT of the channel: world-space ends (relative to `origin`, the shot's source, so a
// renderer can make them camera-relative in floats), the width share at each end (1 the main
// channel's), its glow share (1 the main channel, less on a fork and along it) and the distance
// along the beam of each end (for the shader's shimmer).
struct BeamSegment {
  float a[3], b[3];
  float width_a, width_b;
  float glow;
  float s_a, s_b;
};
struct BeamShape {
  double origin[3] = {0, 0, 0};
  float level = 0;     // beam_level at the time it was built
  int64_t strike = 0;  // the strike it shows
  int count = 0;
  BeamSegment seg[kMaxBeamSegments];
};
// The channel of `shot` at `t_s` (count 0 when its level is 0). Allocates nothing.
void build_beam(const BeamLook& look, const BeamShot& shot, double t_s, BeamShape& out);

// THE LIGHTS of `shot` at `t_s` into `out` (at most kBeamLights: the root, the row along it, the
// hit): steady lights at this frame's level (their flicker is the beam's own, already in their
// candela). Returns how many.
int beam_lights(const BeamLook& look, const BeamShot& shot, double t_s, LocalLight* out);

// THE BURNS: one scorch the beam leaves.
struct BeamBurn {
  double at[3] = {0, 0, 0};
  double normal[3] = {0, 0, -1};
  double size_m = 0;
  double spin_rad = 0;  // its turn about its normal
  double born_s = 0;
  uint32_t variant = 0;
};
// The memory a held beam's burns keep between frames (the last one's place and time).
struct BurnTrail {
  bool any = false;
  double last_at[3] = {0, 0, 0};
  double last_s = 0;
  uint32_t count = 0;
};
// The burn `shot` leaves this frame, if it leaves one: true and `out` filled when one is due at
// `t_s` (held, landing, and either `scorch_every_s` since the last or moved a third of a scorch
// from it; past two thirds, half a scorch on toward the hit). `trail` is the shot's own and is
// advanced; ask again until false (a few times at most) to fill a sweep's gap.
bool beam_burns(const BeamLook& look, const BeamShot& shot, double t_s, BurnTrail& trail,
                BeamBurn& out);
// A burn's glow at `age_s`, cd/m^2: `ember_cd` at once, cooling to nothing over `ember_s`.
float ember_level(const BeamLook& look, double age_s);

}  // namespace ge::render
