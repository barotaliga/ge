// CLASSIC REMIX'S ARSENALS: a game's rules with another game's things (`cr_items`,
// docs/goals/M18u.md). A weapon swapped in is the other game's weapon whole: Quake II's weapons in
// Quake's rules fire by Quake II's own hand and shots (`q2_worn`, classic/q2_arms.hpp: frames,
// refire, damage, spreads, missiles, blasts, hits, sounds), on the ammunition of Quake's that
// stands for theirs. The map keeps its class names, so its items stand where they always have; each
// weapon's view model and pickup, and each armour's model and pickup sound, are the other game's.
//
// - **By role** (`kRemixArsenals`): the weapon in a slot takes the other game's weapon of the
//   same place, as the item table does (profiles/remix.hpp: the nail weapons the bullet ones, the
//   beam the rail), the three armours the other game's three, weakest first, and the ammunition
//   and health boxes the other game's of the same kind and size (`ItemLook`).
// - **The held set** (`RemixArsenal::held`): the view models by slot, which the window draws in
//   the hand in place of the game's own.
// - **The frames** (`HeldFrames`, `held_frames`): a view model of Quake II's runs every
//   animation in one file (up, firing, idle, down); the hand takes the first idle frame as its
//   rest and the firing frames as its shot.
//
// ge:summary Classic remix's arsenals: a game's rules with another game's weapons, armour, sounds
// ge:keywords remix classic-remix cr_items remix-arsenal held-frames q2-weapons
// ge:keywords view-model pickup-model fire-sound by-role remix_arsenal held_frames
// ge:keywords cr_models remix_figure player-figure cr_physics remix_physics sv_physics
// ge:keywords movement-feel body-physics
// ge:skill shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "classic/q2_arms.hpp"
#include "classic/q2_body.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/profiles/arsenal.hpp"

namespace shooters_edge::profiles {

// How far above the floor Quake II's items stand: its items are measured from their middle,
// 15 units over the floor (profiles/remix.hpp's `kItemFeet`).
inline constexpr int32_t kQ2Lift = 15;

// A slot of the classic rules holding Quake II's weapon `id` whole: the rules fire it by Quake II's
// own hand and shots (classic/q2_arms.hpp: its frames, refire, damage, spreads, missiles, blasts,
// hits and sounds), spending `ammo` (Quake's kind that stands for Quake II's) a shot as Quake II
// does. The row's refire is the weapon's period for a reader (the hand keeps its own time); its
// pellets, damage, spread, reach, life and splash are the hand's, so the row leaves them 0.
constexpr WeaponRow q2_worn(int32_t slot, ge::classic::Q2ArmId id, Ammo ammo, std::string_view view,
                            std::string_view fire_sound, std::string_view missile,
                            std::string_view land_sound, std::string_view blast_sound,
                            std::string_view pickup, int32_t pickup_ammo) {
  const ge::classic::Q2Arm& a = ge::classic::q2_arm(id);
  const bool held = a.held != ge::classic::Q2Held::once;
  const float period =
      held ? 0.1f : static_cast<float>(a.frames.last_fire - a.frames.last_activate + 1) / 10.0f;
  // Quake II's own carry limit of the ammunition the slot's stands for (nails the bullets, cells
  // the slugs).
  const int32_t most = ammo == Ammo::shells    ? ge::classic::kQ2MostShells
                       : ammo == Ammo::nails   ? ge::classic::kQ2MostBullets
                       : ammo == Ammo::rockets ? ge::classic::kQ2MostRockets
                       : ammo == Ammo::cells   ? ge::classic::kQ2MostSlugs
                                               : 0;
  return {slot,       static_cast<Fire>(static_cast<int>(kFireQ2First) + static_cast<int>(id)),
          ammo,       a.per_shot,
          period,     0,
          0,          0,
          0,          0,
          0,          view,
          fire_sound, missile,
          land_sound, blast_sound,
          pickup,     pickup_ammo,
          most,       kQ2Lift};
}

// Quake's eight slots as Quake II's weapons, whole: the axe's slot the blaster, the nailguns the
// machinegun and chaingun (on nails, standing for bullets), the lightning gun the railgun (on
// cells, standing for slugs).
inline constexpr WeaponRow kQ1AsQ2Weapons[] = {
    q2_worn(1, ge::classic::Q2ArmId::blaster, Ammo::none, "models/weapons/v_blast/tris.md2",
            "weapons/blastf1a.wav", "models/objects/laser/tris.md2", "-", "-", "-", 0),
    q2_worn(2, ge::classic::Q2ArmId::shotgun, Ammo::shells, "models/weapons/v_shotg/tris.md2",
            "weapons/shotgf1b.wav", "-", "-", "-", "-", 0),
    q2_worn(3, ge::classic::Q2ArmId::super_shotgun, Ammo::shells,
            "models/weapons/v_shotg2/tris.md2", "weapons/sshotf1b.wav", "-", "-", "-",
            "models/weapons/g_shotg2/tris.md2", 5),
    q2_worn(4, ge::classic::Q2ArmId::machinegun, Ammo::nails, "models/weapons/v_machn/tris.md2",
            "weapons/machgf1b.wav", "-", "-", "-", "models/weapons/g_machn/tris.md2", 30),
    q2_worn(5, ge::classic::Q2ArmId::chaingun, Ammo::nails, "models/weapons/v_chain/tris.md2",
            "weapons/machgf1b.wav", "-", "-", "-", "models/weapons/g_chain/tris.md2", 30),
    q2_worn(6, ge::classic::Q2ArmId::grenade_launcher, Ammo::rockets,
            "models/weapons/v_launch/tris.md2", "weapons/grenlf1a.wav",
            "models/objects/grenade/tris.md2", ge::classic::kQ2GrenadeBounce,
            ge::classic::kQ2GrenadeBoom, "models/weapons/g_launch/tris.md2", 5),
    q2_worn(7, ge::classic::Q2ArmId::rocket_launcher, Ammo::rockets,
            "models/weapons/v_rocket/tris.md2", "weapons/rocklf1a.wav",
            "models/objects/rocket/tris.md2", "-", ge::classic::kQ2RocketBoom,
            "models/weapons/g_rocket/tris.md2", 5),
    q2_worn(8, ge::classic::Q2ArmId::railgun, Ammo::cells, "models/weapons/v_rail/tris.md2",
            "weapons/railgf1a.wav", "-", "-", "-", "models/weapons/g_rail/tris.md2", 15),
};
// Its three armours as Quake II's jacket, combat and body armour, taken by Quake II's rule: each
// its own count, adding onto the one worn up to its most.
inline constexpr ArmourRow kQ1AsQ2Armour[] = {
    {1, ge::classic::kQ2Armours[1].normal, ge::classic::kQ2Armours[1].base, 0,
     "models/items/armor/jacket/tris.md2", "misc/ar1_pkup.wav", ge::classic::kQ2Armours[1].maximum,
     kQ2Lift},
    {2, ge::classic::kQ2Armours[2].normal, ge::classic::kQ2Armours[2].base, 0,
     "models/items/armor/combat/tris.md2", "misc/ar1_pkup.wav", ge::classic::kQ2Armours[2].maximum,
     kQ2Lift},
    {3, ge::classic::kQ2Armours[3].normal, ge::classic::kQ2Armours[3].base, 0,
     "models/items/armor/body/tris.md2", "misc/ar1_pkup.wav", ge::classic::kQ2Armours[3].maximum,
     kQ2Lift},
};
// Its ammunition and health as Quake II's: the nails the bullets, the cells the slugs, the
// small health the medium, the plain one the large, the mega health Quake II's own, each giving
// Quake II's own points.
inline constexpr ItemLook kQ1AsQ2Looks[] = {
    {"arsenal_ammo1", "models/items/ammo/shells/medium/tris.md2", "misc/am_pkup.wav", kQ2Lift,
     ge::classic::kQ2AmmoShells},
    {"arsenal_ammo2", "models/items/ammo/bullets/medium/tris.md2", "misc/am_pkup.wav", kQ2Lift,
     ge::classic::kQ2AmmoBullets},
    {"arsenal_ammo3", "models/items/ammo/rockets/medium/tris.md2", "misc/am_pkup.wav", kQ2Lift,
     ge::classic::kQ2AmmoRockets},
    {"arsenal_ammo4", "models/items/ammo/slugs/medium/tris.md2", "misc/am_pkup.wav", kQ2Lift,
     ge::classic::kQ2AmmoSlugs},
    {"arsenal_health1", "models/items/healing/medium/tris.md2", "items/n_health.wav", kQ2Lift,
     ge::classic::kQ2HealthMedium},
    {"arsenal_health2", "models/items/healing/large/tris.md2", "items/l_health.wav", kQ2Lift,
     ge::classic::kQ2HealthLarge},
    {"arsenal_health3", "models/items/mega_h/tris.md2", "items/m_health.wav", kQ2Lift,
     ge::classic::kQ2HealthMega},
};
// The view models by slot, 1..8.
inline constexpr std::string_view kQ1AsQ2Held[] = {
    "models/weapons/v_blast/tris.md2",  "models/weapons/v_shotg/tris.md2",
    "models/weapons/v_shotg2/tris.md2", "models/weapons/v_machn/tris.md2",
    "models/weapons/v_chain/tris.md2",  "models/weapons/v_launch/tris.md2",
    "models/weapons/v_rocket/tris.md2", "models/weapons/v_rail/tris.md2"};

struct RemixArsenal {
  std::string_view played;  // the content profile id whose rules play
  std::string_view items;   // the content profile id whose things they wear
  Arsenal arsenal;
  std::span<const std::string_view> held;  // the view models by slot, 1 first
};

inline constexpr RemixArsenal kRemixArsenals[] = {
    {"q1", "q2", {kQ1AsQ2Weapons, kQ1AsQ2Armour, kQ1AsQ2Looks, "misc/w_pkup.wav"}, kQ1AsQ2Held},
};

// The arsenal `played`'s rules wear with `items`'s things, or null (the same game, or no row).
constexpr const RemixArsenal* remix_arsenal(std::string_view played, std::string_view items) {
  if (played == items) return nullptr;
  for (const RemixArsenal& r : kRemixArsenals)
    if (r.played == played && r.items == items) return &r;
  return nullptr;
}

// A THING RENAMED (`Look`): a rules engine of our own that names its models and sounds in code
// (Quake II's) wears another game's by name: each name the rules register is sent to the
// players as `to`, an entity drawn with that model takes `skin`, and is drawn `offset` units from
// where the rules hold it (the other game measures its items from another point of their box).
struct Look {
  std::string_view from;
  std::string_view to;
  int32_t skin = 0;
  float offset[3] = {0, 0, 0};
};

// Where Quake's items stand against Quake II's: Quake II's lie 15 units over the
// floor, the classic weapons and armour on it, its boxes from their corner, its artifacts 24 over.
inline constexpr float kOnFloor = -kQ2Lift;
inline constexpr float kArtifact = 24 - kQ2Lift;

// Quake II's rules in Quake's things, by role as the item table maps the items
// (profiles/remix.hpp): the weapons on the floor (the shotgun the double-barrelled one, the
// machinegun and chaingun the nailguns, the hyperblaster, railgun and BFG the lightning gun), the
// armour, the boxes and packs by kind and size, the artifacts, what the monsters' missiles look
// and sound like, the pickups' sounds. The weapons in the hand are not renamed: Quake's own eight
// are fired whole (server/q2_quake_arms.hpp), their models and sounds their own.
inline constexpr Look kQ2AsQ1[] = {
    // On the floor.
    {"models/weapons/g_shotg/tris.md2", "progs/g_shot.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_shotg2/tris.md2", "progs/g_shot.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_machn/tris.md2", "progs/g_nail.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_chain/tris.md2", "progs/g_nail2.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_launch/tris.md2", "progs/g_rock.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_rocket/tris.md2", "progs/g_rock2.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_hyperb/tris.md2", "progs/g_light.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_rail/tris.md2", "progs/g_light.mdl", 0, {0, 0, kOnFloor}},
    {"models/weapons/g_bfg/tris.md2", "progs/g_light.mdl", 0, {0, 0, kOnFloor}},
    {"models/items/armor/jacket/tris.md2", "progs/armor.mdl", 0, {0, 0, kOnFloor}},
    {"models/items/armor/combat/tris.md2", "progs/armor.mdl", 1, {0, 0, kOnFloor}},
    {"models/items/armor/body/tris.md2", "progs/armor.mdl", 2, {0, 0, kOnFloor}},
    {"models/items/ammo/shells/medium/tris.md2", "maps/b_shell0.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/ammo/bullets/medium/tris.md2", "maps/b_nail0.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/ammo/grenades/medium/tris.md2", "maps/b_rock0.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/ammo/rockets/medium/tris.md2", "maps/b_rock0.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/ammo/cells/medium/tris.md2", "maps/b_batt0.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/ammo/slugs/medium/tris.md2", "maps/b_batt0.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/healing/stimpack/tris.md2", "maps/b_bh10.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/healing/medium/tris.md2", "maps/b_bh10.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/healing/large/tris.md2", "maps/b_bh25.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/mega_h/tris.md2", "maps/b_bh100.bsp", 0, {-16, -16, kOnFloor}},
    {"models/items/quaddama/tris.md2", "progs/quaddama.mdl", 0, {0, 0, kArtifact}},
    {"models/items/invulner/tris.md2", "progs/invulner.mdl", 0, {0, 0, kArtifact}},
    {"models/items/enviro/tris.md2", "progs/suit.mdl", 0, {0, 0, kArtifact}},
    // In flight.
    {"models/objects/rocket/tris.md2", "progs/missile.mdl"},
    {"models/objects/grenade/tris.md2", "progs/grenade.mdl"},
    {"models/objects/laser/tris.md2", "progs/laser.mdl"},
    // Heard.
    {"weapons/rocklx1a.wav", "weapons/r_exp3.wav"},
    {"weapons/grenlx1a.wav", "weapons/r_exp3.wav"},
    {"weapons/grenlb1b.wav", "weapons/bounce.wav"},
    {"misc/w_pkup.wav", "weapons/pkup.wav"},
    {"misc/am_pkup.wav", "weapons/lock4.wav"},
    {"misc/ar1_pkup.wav", "items/armor1.wav"},
    {"misc/ar2_pkup.wav", "items/armor1.wav"},
    {"items/s_health.wav", "items/r_item1.wav"},
    {"items/n_health.wav", "items/r_item1.wav"},
    {"items/l_health.wav", "items/health1.wav"},
    {"items/m_health.wav", "items/r_item2.wav"},
};

struct RemixLooks {
  std::string_view played;  // the content profile id whose rules play
  std::string_view items;   // the content profile id whose things they wear
  std::span<const Look> looks;
};

inline constexpr RemixLooks kRemixLooks[] = {{"q2", "q1", kQ2AsQ1}};

// The renames `played`'s rules wear with `items`'s things, or null (the same game, or no row).
constexpr const RemixLooks* remix_looks(std::string_view played, std::string_view items) {
  if (played == items) return nullptr;
  for (const RemixLooks& r : kRemixLooks)
    if (r.played == played && r.items == items) return &r;
  return nullptr;
}

// The look `name` takes in `looks` (any case), or null when it keeps its own.
constexpr const Look* look_of(std::span<const Look> looks, std::string_view name) {
  for (const Look& l : looks) {
    if (l.from.size() != name.size()) continue;
    bool same = true;
    for (size_t i = 0; i < name.size() && same; i++) {
      const char a = l.from[i], b = name[i];
      same = (a >= 'A' && a <= 'Z' ? char(a + 32) : a) == (b >= 'A' && b <= 'Z' ? char(b + 32) : b);
    }
    if (same) return &l;
  }
  return nullptr;
}

// THE FIGURES (`cr_models`): the one figure every player is drawn as while the played game's
// players wear another game's. The window reads it as the folder `folder` under players/; `file`
// names where the worn game keeps it when that is not players/<folder>/ (Quake's own
// figure, which stands alone), its frames mapped from the played game's figure's by kind
// (app/figure_frames.hpp).
struct RemixFigure {
  std::string_view played;  // the content profile id whose players are drawn
  std::string_view models;  // the content profile id whose figure they wear
  std::string_view folder;
  std::string_view file;  // "": players/<folder>/tris.*
};

inline constexpr RemixFigure kRemixFigures[] = {
    {"q1", "q2", "male", ""},
    {"q2", "q1", "q1", "progs/player.mdl"},
};

// The figure `played`'s players wear as `models`'s, or null (the same game, or no row).
constexpr const RemixFigure* remix_figure(std::string_view played, std::string_view models) {
  if (played == models) return nullptr;
  for (const RemixFigure& f : kRemixFigures)
    if (f.played == played && f.models == models) return &f;
  return nullptr;
}

// THE MOVES (`cr_physics`): the played game's players move as another game's do. Each game's move
// keeps its own code (the step, the duck, the slide); what differs is set on it: the running speed,
// the ground's friction and how the air steers. `lines` are the played game's server lines from
// the next map: the classic rules' move told Quake II's (300 and 6, the air steered at 1 by the
// whole wish, the landing pause), Quake II's told Quake's by `sv_physics`
// (`kQ1Feel`).
struct RemixPhysics {
  std::string_view played;   // the content profile id whose rules play
  std::string_view physics;  // the content profile id whose physics they take
  std::span<const std::string_view> lines;
};

inline constexpr std::string_view kQ1PhysicsAsQ2[] = {
    "sv_maxspeed 300", "sv_friction 6",      "pm_airaccel 1",    "pm_airwish 0",
    "pm_landpause 1",  "sv_waterfriction 1", "pm_swimscale 0.5", "pm_waterjump 350",
    "pm_airclimb 1",   "set sv_falldamage 1"};
inline constexpr std::string_view kQ2PhysicsAsQ1[] = {"set sv_physics \"q1\""};

inline constexpr RemixPhysics kRemixPhysics[] = {
    {"q1", "q2", kQ1PhysicsAsQ2},
    {"q2", "q1", kQ2PhysicsAsQ1},
};

// The physics `played`'s rules take as `physics`'s, or null (the same game, or no row).
constexpr const RemixPhysics* remix_physics(std::string_view played, std::string_view physics) {
  if (played == physics) return nullptr;
  for (const RemixPhysics& m : kRemixPhysics)
    if (m.played == played && m.physics == physics) return &m;
  return nullptr;
}

// Quake's ground and air as Quake II's move takes them (`sv_physics q1`): its
// running speed, its friction, its air steered by the strafing rule in every mode, and no landing
// pause, so a hop chains on the landing.
using MoveFeel = ge::classic::Q2Feel;
inline constexpr MoveFeel kQ1Feel{320, 4, 10, false, 0.7f, 4, 310, false};

// A view model whose file holds every animation: the frame it rests on and its firing frames,
// and the weapon of Quake II's it is (its hand's runs and its kick, plugins/classic/q2_arms.hpp).
struct HeldFrames {
  std::string_view model;
  int rest = 0;        // the first idle frame
  int fire_first = 0;  // the firing frames, both ends included
  int fire_last = 0;
  ge::classic::Q2ArmId arm = ge::classic::Q2ArmId::blaster;
};

// Quake II's view models: up 0..a, firing a+1..f, idle from f+1.
inline constexpr HeldFrames kHeldFrames[] = {
    {"models/weapons/v_blast/tris.md2", 9, 5, 8, ge::classic::Q2ArmId::blaster},
    {"models/weapons/v_shotg/tris.md2", 19, 8, 18, ge::classic::Q2ArmId::shotgun},
    {"models/weapons/v_shotg2/tris.md2", 18, 7, 17, ge::classic::Q2ArmId::super_shotgun},
    {"models/weapons/v_machn/tris.md2", 6, 4, 5, ge::classic::Q2ArmId::machinegun},
    {"models/weapons/v_chain/tris.md2", 32, 5, 31, ge::classic::Q2ArmId::chaingun},
    {"models/weapons/v_launch/tris.md2", 17, 6, 16, ge::classic::Q2ArmId::grenade_launcher},
    {"models/weapons/v_rocket/tris.md2", 13, 5, 12, ge::classic::Q2ArmId::rocket_launcher},
    {"models/weapons/v_rail/tris.md2", 19, 4, 18, ge::classic::Q2ArmId::railgun},
};

// The frames of the view model `model`, or null when its first frame is its rest and the rest its
// shot (Quake's own).
constexpr const HeldFrames* held_frames(std::string_view model) {
  for (const HeldFrames& f : kHeldFrames)
    if (f.model == model) return &f;
  return nullptr;
}

}  // namespace shooters_edge::profiles
