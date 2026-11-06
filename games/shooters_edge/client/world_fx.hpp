// WHAT THE WINDOW THROWS INTO THE WORLD EACH FRAME: the game's effects (app/effects.hpp) read off
// what the listen server did, for the world pass (world_draw.hpp) to draw. Presentation only: it
// reads the server's state and the view's record of it (present.hpp's missiles and blasts) and
// writes nothing back.
//
// - A missile that flew since the last frame lays its trail over the stretch it flew (a rocket's
//   or a grenade's embers and smoke) and a rocket glows as it goes.
// - An explosion the mod announced bursts once and lights the walls while its light fades.
// - A campaign's temporary entities and particle bursts (Listen::effects): a shot's or a spike's
//   hit sparks back toward the eye, blood falls, a teleport flares, lava splashes, a beam lights
//   and sparks where it ends.
// - A PLAYER'S WARP (warps.hpp): one leaving (a teleport's near end) is drawn where he stood,
//   crumbling from his back to his face as the ash the wind takes is thrown off where the cut
//   passes (app/effects.hpp's dust); one arriving (a spawn, a respawn, a teleport's far end) is
//   blown in as ash from ahead of him, each flake on a way of its own landing on his skin as it
//   appears there (FxState::models), whole after kArriveS. The local player's own arrival blows
//   the ash past his eyes (never through them) and assembles the weapon in his hands
//   (`held_shown`). That is the enhanced warp (`FxSwitches::warps`: ce_warps in the modern
//   look); off, the original: the figure there or gone at once and the teleport's fog at each end
//   (in a campaign the program's own; with the dust on, the program's fog at a player's warp is
//   left out).
// - THE SCREEN'S BLEND: the liquid round the eye (the player's `waterlevel` and `watertype`), and
//   a hit's flash, fading: in a campaign off the blows the program reported (Listen::blows: the
//   blood red, what the armour took a duller amber), in a match off the health lost.
//
// - THE HELD WEAPON'S FIRE: each shot's flame, smoke and sparks from the barrel the hand puts in
//   the world (app/muzzle.hpp, each weapon's row at the size its gun is drawn), under ce_muzzles.
//
// THE ENHANCED WEAPON EFFECTS (app/weapon_fx.hpp), each under its classic-enhance switch in the
// modern look (`fx_switches`; off, or the classic look: the classic effects above):
// - ce_explosions: an explosion is a layered fireball of the engine's (its billows, its smoke, the
//   classic's ember ball, its light white-hot then orange), drawn by the explosions' pass.
// - ce_trails: a rocket burns a short flame out of its motor and lays fire into smoke that spreads;
//   a grenade trails pale smoke.
// - ce_beams: the arc gun's beam is the engine's bolt (render/beam.hpp) in r_beam_style's
//   style, held while the program
//   keeps sending it, lighting the walls along it, its root on the gun's own muzzle for the
//   player's own; blue-white sparks where it lands.
// - ce_hits: a pellet's or a nail's hit throws the burst of the face it hit (the map's triangle
//   the round's line crosses there: its normal and its texture's surface class, engine/detail).
//
// Every burst draws on its own seed from a counter, so the same run throws the same sparks.
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx effects trail rocket-trail explosion dynamic-light screen-blend hurt-flash
// ge:keywords underwater medium step_fx fx-state temp-entity gunshot spike teleport-splash beam
// ge:keywords held-fire muzzle-fire fire_held load_held_fire fx_switches held-beam
// ge:keywords ce_trails ce_hits ce_decals marks scorch bullet-hole burns mark_burns
// ge:keywords held-fire muzzle-fire fire_held load_held_fire warp spawn-effect teleport-effect
// ge:keywords respawn ghost materialize dematerialize player_dust dust ash held_shown
// ge:skill render shooter
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "brush/light_probe.hpp"
#include "camera/subject.hpp"
#include "detail/decals.hpp"
#include "render/beam.hpp"
#include "render/beams.hpp"
#include "render/explosions.hpp"
#include "render/haze.hpp"
#include "render/marks.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/app/muzzle.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/warps.hpp"
#include "shooters_edge/client/world_models.hpp"
#include "shooters_edge/view/hand.hpp"

namespace shooters_edge::client {

// The enhanced effects asked for (all off: the classic effects).
struct FxSwitches {
  bool muzzles = false, explosions = false, trails = false, beams = false, hits = false;
  bool warps = false;  // ce_warps: a player's teleport and spawn as dust (warps.hpp)
  bool decals =
      false;  // ce_decals: the marks the weapons leave (scorches, holes, the bolt's burns)
  // ce_flares, ce_bubbles: the pack's flare and bubble sprites drawn as the engine's (the models'
  // `prepare_models`, through classic-enhance's replacement table).
  bool flares = false, bubbles = false;
  const ge::render::BeamLook* bolt = nullptr;  // r_beam_style's (null: the default style)
};
// The switches `picture` asks for: each ce_ switch, in the modern look only, and its bolt's style.
// The explosion is the engine's where classic-enhance's replacement table names `explosion` for the
// pack's explosion sprite (classicenhance/replace.hpp), so the table, not this code, says so.
FxSwitches fx_switches(const look::Picture& picture);

// An arc gun's beam the program keeps sending: its shot, the beam's owner and when it last came.
struct HeldBeam {
  int entity = 0;
  ge::render::BeamShot shot;   // engine metres
  sh::Vec3f map_from, map_to;  // map units (the classic bolt's segments)
  EffectModel model = EffectModel::bolt2;
  const ge::render::BeamLook* look = nullptr;  // its enhanced bolt's (FxSwitches::bolt)
  ge::render::BurnTrail trail;                 // where it last burnt the wall it lands on
  double seen = 0;
};

// THE MARKS (ce_decals, the modern look): engine/detail's decals pressed onto the map's own faces
// (the world model's, sky and liquids left out, made into receivers once a map): a blast's scorch
// where it went off against a surface (render/explosion.hpp's `scorch_of`, its embers glowing for
// a second and a half), a bullet hole where a round hit a face, the bolt's burns where it lands
// (render/beam.hpp's `beam_burns`). Kept in a pool of kMarks, the oldest reused; drawn as the
// beams' pass's burns (soot and ember) through the decal masks' atlas (render/marks.hpp).
inline constexpr uint32_t kMarks = 128;
inline constexpr double kHoleM = 0.16, kNailHoleM = 0.2;
inline constexpr double kPelletSpread = 0.04;  // a shot's pellets' cone, rad: one message's holes
// THE WOUNDS (ce_decals): where a round struck a body, a hole with blood splattered round it,
// pinned to the body's own triangle there (body_triangles, the point's place on it), so it rides
// the figure's frames and turns; drawn each frame as specks lit by the room. A body keeps
// kWoundsEach, the oldest let go; all are gone when it comes back to life (a respawn).
inline constexpr size_t kWoundsEach = 24, kWoundsAll = 192;
inline constexpr float kWoundReachU = 64;  // a hit this near a body's origin is on it, units
struct Wound {
  int entity = 0;
  uint32_t tri = 0;    // its triangle of the body's skin
  float a = 0, b = 0;  // its place on it (barycentric, from its first corner)
  uint32_t seed = 0;   // its splatter
  float size = 1;      // the splatter's spread: a burst of drops, a share more
};  // boxes: the hole about half
// The faces the marks are pressed onto: the mesh and its receivers (which point into it), kept
// together.
struct MarkFaces {
  ge::detail::TriMesh mesh;
  ge::detail::Receivers receivers;
  ge::brush::LightProbe probe;  // the map's light under a point, for each mark's
};
inline constexpr double kBeamHoldS = 0.2;  // a beam not sent again for this long is let go

struct FxState {
  FxSwitches on;                 // set by the caller each frame
  std::vector<uint8_t> palette;  // the pack's (gfx/palette.lmp), set once: the classic specks'
  std::vector<ge::render::Blast> blasts;  // the enhanced explosions' and hits' puffs, live
  std::vector<ge::render::Blast> mains;   // the explosions' main balls, for their light
  std::vector<HeldBeam> beams;            // the arc guns' beams, held or fading
  // The marks: the map they were made for, its faces as receivers, the pool.
  const ge::brush::Map* marked = nullptr;
  std::shared_ptr<const MarkFaces> receivers;
  ge::detail::DecalPool marks;
  std::vector<std::array<float, 3>> mark_light;    // each slot's light, whites, as it was pressed
  std::unordered_map<int, ge::camera::Vec3> last;  // each missile's place a frame ago, m
  std::unordered_map<int, ge::camera::Vec3> next;  // this frame's, swapped into `last`
  std::vector<ge::render::HazeTrail> haze;         // the flights' hot air still warm, oldest first
  // Each rocket's launch as it looks (ce_trails; app/weapon_fx.hpp's launch_offset): when it was
  // first seen and the shooter's sway it carries; and the eye's aim a frame ago, for that sway.
  struct Launched {
    double born = 0;
    effects::RocketLaunch launch;
  };
  std::unordered_map<int, Launched> launched;
  double aim_yaw = 0, aim_pitch = 0, aim_rate[2] = {0, 0};
  bool aimed = false;
  // Each missile's place (map units) and the way it flew, a frame ago: the surface a blast went
  // off against lies along it.
  struct Flight {
    sh::Vec3f at, dir;
  };
  std::unordered_map<int, Flight> flights, flights_next;
  std::unordered_map<uint32_t, std::array<float, 3>> wall_colours;  // each texture's, as hit
  std::vector<Wound> wounds;
  std::unordered_map<int, float> wound_health;  // each wounded body's health a frame ago
  std::size_t explosions_seen = 0;              // of Listen::explosions
  std::vector<Blast> lit;                       // the explosions whose light still fades
  std::vector<Blast> sparked;                   // a nail's sparks off metal, their flick fading
  double hurt = 0;                              // the hit's flash now (effects::Screen::hurt)
  double armour = 0;                            // the armour's share of it (Screen::armour)
  std::size_t blows_seen = 0;                   // of Listen::blows
  std::size_t effects_seen = 0;                 // of Listen::effects
  float health = 0;                             // the last frame's
  uint32_t seed = 1;
  WarpWatch warps;                       // the players' arrivals and departures (warps.hpp)
  std::unordered_map<int, Placed> seen;  // each player as drawn last frame, by entity
  struct Leaving {
    bool drawn = false;       // the figure was drawn (not the local player's own)
    Placed figure;            // where it stood
    std::vector<float> skin;  // its triangles as it stood (body_triangles)
    effects::Dust dust;       // its dust and its warp's sweep
  };
  std::unordered_map<int, Leaving> leaving;  // each departure, by entity
  std::vector<Body> bodies;                  // scratch: this frame's players
  std::vector<float> skin;                   // scratch: an arriving body's triangles
  // The models the window draws (world_draw.hpp's world_models), for the ash to land on their
  // skins; null: on each body's box.
  const WorldModels* models = nullptr;
  std::vector<Warp> began;  // scratch: the warps that began this frame
};

// One frame's effects for the world pass.
struct FxFrame {
  effects::Particles births;
  std::vector<effects::Light> lights;
  double blend[4] = {0, 0, 0, 0};  // the screen's: sRGB colour and share
  // The same without a lava's or a slime's tint, for a look that draws the liquid round the eye.
  double blend_dry[4] = {0, 0, 0, 0};
  std::vector<Placed> models;  // the entities with a model (world_models.hpp)
  float held_shown = 1;        // the weapon in hand's warp: the local player's arrival
  // The weapon in hand's muzzle fire (app/muzzle.hpp): its shots' smoke left in the world and
  // their flames on this frame's muzzle, for the explosions' pass.
  std::vector<ge::render::Blast> puffs;
  std::vector<ge::render::MuzzleFire> flames;
  // The arc guns' beams to draw this frame (ce_beams), their look app/weapon_fx.hpp's bolt.
  std::vector<HeldBeam> beams;
  // The marks on the walls this frame (ce_decals), three vertices a triangle, for the beams' pass.
  std::vector<ge::render::BurnVertex> burns;
  // The hot air a nail's or a rocket's flight left (ce_trails), a stretch a frame, for the haze.
  std::vector<ge::render::HazeTrail> haze;
  // The local player as the world would place him (never drawn in his own eyes, still casting his
  // shadow, M54); `has_self` false when he is dead or there is no server.
  Placed self;
  bool has_self = false;
};

// A player's dust and the sweep of his warp from where he stands (`origin`, `angles`: map units
// and degrees) and his entity (the noise's seed): the wind blows into his face, so his back goes
// first and the ash streams behind him, and an arrival is blown in from ahead of him, his face and
// head first. The warp's share is the caller's.
effects::Dust player_dust(const sh::Vec3f& origin, const sh::Vec3f& angles, int entity);

// One frame of a warp's ash on `d`, `was` to `age` s into it (`was` < 0: its first frame): blown
// in to land on the body as it appears, or off it as the cut passes.
void warp_dust(const Warp& w, const effects::Dust& d, double was, double age, uint32_t seed,
               effects::Particles* out);

// The effects of the wall frame of `dt` s at time `t` s, after frame_view brought `v` up to date.
void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out);

// The held weapons' fire: every built weapon's look (by its index) and the shots still burning.
struct HeldFire {
  std::vector<ge::render::MuzzleLook> looks;
  effects::MuzzleShots shots;
};

// Each built weapon's fire from its row (view/weapons.hpp) at kBuiltScale; or, when `at_eye`
// names a classic game's own held weapons (its files, in the weapons' order), each one's fire from
// the classic table (classicenhance/weapons.hpp's `classic_fire_of`) at its true size. False with
// `error` naming the row when a row's knobs do not read.
bool load_held_fire(HeldFire& f, std::string* error, std::span<const std::string_view> at_eye = {});

// The hand's shot this frame (`hand.fired`) and the fire still burning, staged into `out` at time
// `now_s`; `on` false (ce_muzzles 0): a shot throws nothing, its light alone. The player's own
// beam (entity 1's) is rooted on the hand's muzzle.
void fire_held(HeldFire& f, const view::HandFrame& hand, bool on, double now_s, FxFrame& out);

}  // namespace shooters_edge::client
