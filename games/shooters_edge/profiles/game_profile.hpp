// A CLASSIC GAME'S GAME HALF: what Shooter's Edge does with a classic game's content, one row of
// data a game beside its content row (plugins/classicenhance/profile.hpp: where the files are
// and what kind each is). The window, the menu, the server and the labs read the row instead of
// writing a file name or a map into their code, so a second classic game is a second row here
// and a second there (docs/shooters-edge.md § Profiles).
//
// - **Programs**: each mode's program files, looked for in order through the game's search path
//   (client/files.hpp: a mod's folder over the base) by the game's adapter
//   (client/game_adapter.hpp). Each file names its FORM (what kind of program it is: QC
//   bytecode, a native game library, a sandboxed bytecode, the engine's own C++ mod) and its SIDE
//   (the server's rules, the client's half, the menus), because the classic games differ in both:
//   Quake's mod is one QC program for the server; Quake II's a native library for the
//   server; the third's up to three bytecode programs, one a side. A form this build has no runner
//   for is named when it is found and passed over, never read as another. For Quake the
//   multiplayer form's own name comes first, then the one name the single-player form also uses,
//   so a multiplayer mod that ships either name runs; a QC file of the other mode's layout (the
//   header's CRC, plugins/progs/layout.hpp) is passed over, never run in the wrong mode.
// - **Demos**: the extensions a demo of this game is listed by and tried with when named without
//   one, in order (a recording of another extension is still found by its bare name: the files'
//   top level is searched for that name with any extension, client/timedemo.hpp), and the folder
//   its packs keep them in (`demo_folder`: Quake II's `demos/`), where a bare name is looked for
//   too.
// - **The fallback map**: where the window starts when the content holds no start map (the
//   content row's `start_map`): the built-in arena.
// - **The full game** is the content row's `full_game` file being in the packs.
// - **The weapon groups**: what each number key picks with cl_weapongroups 1
//   (client/weapon_groups.hpp), one list of the game's weapon keys a group, best first.
// - **The episodes**: each one's first map, in the order the start map opens them.
// - **Items, their words and the sounds** a bot reads (items.hpp).
// - **Place names**: how far one reaches by default (`loc_dist`) and whether its client's place
//   macros (`$loc_here`, `$loc_there`) are there (client/place_names.hpp).
// - **The server's words** (words.hpp): the obituaries, the clock, the lobby and its countdown in
//   the game's own words; empty: the mod's and the lobby's own.
//
// ge:summary A classic game's game half as data: each mode's program files, the fallback map
// ge:keywords game-profile profile classic program progs qwprogs multiplayer single-player
// ge:keywords mod-program fallback-map game-half find_game program-form program-side
// ge:keywords demo-extensions weapon-groups kclassicgroups kq2groups
// ge:skill shooter
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "classicenhance/profile.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/profiles/items.hpp"
#include "shooters_edge/profiles/words.hpp"

namespace shooters_edge::profiles {

// What kind of program a file is: which runner it needs.
enum class ProgramForm : uint8_t {
  qc,                  // QC bytecode, run by plugins/progs
  game_library,        // a native library exporting a game's rules (no runner yet)
  sandboxed_bytecode,  // a bytecode for a sandboxed machine of its own (no runner yet)
  cpp_mod,             // the engine's own C++ mod package (engine/mod)
};
// Which half of the game a program is.
enum class ProgramSide : uint8_t { server, client, menu };

struct ProgramFile {
  std::string_view name;  // its path in the game's files
  ProgramForm form = ProgramForm::qc;
  ProgramSide side = ProgramSide::server;
};

// The form's name for messages ("QC bytecode").
constexpr std::string_view form_name(ProgramForm f) {
  switch (f) {
    case ProgramForm::qc:
      return "QC bytecode";
    case ProgramForm::game_library:
      return "native game library";
    case ProgramForm::sandboxed_bytecode:
      return "sandboxed bytecode";
    case ProgramForm::cpp_mod:
      return "C++ mod";
  }
  return "unknown";
}

struct GameProfile {
  std::string_view id;  // the content row's id (classicenhance/profile.hpp)
  std::span<const ProgramFile> multiplayer_programs;    // looked for in order
  std::span<const ProgramFile> single_player_programs;  // looked for in order
  std::string_view fallback_map;                        // with no start map in the content
  std::span<const std::string_view> demo_extensions;    // tried in order on a bare demo name
  // The folder its packs keep their recordings in ("demos/"), where a bare name is also looked
  // for; "": the files' top level only.
  std::string_view demo_folder;
  ItemTables tables;  // its items, their words and the sounds a bot reads (items.hpp)
  // Its first-person weapons' model files, in the order of the number keys (view::Weapon's).
  std::span<const std::string_view> held_weapons;
  Arsenal arsenal;  // its weapons and armour as data, which our programs read (arsenal.hpp)
  std::span<const WordRow> words;  // what its server prints (words.hpp)
  // Each episode's first map, in order: the start map opens them one at a time, each once the
  // one before is done (server/campaign_episodes.hpp).
  std::span<const std::string_view> episode_maps;
  // The number keys' weapon groups with cl_weapongroups 1 (client/weapon_groups.hpp): key 1's
  // first, each the game's weapon keys best first ("7 6").
  std::span<const std::string_view> weapon_groups;
  // Its maps carry their own single-player rules (Quake II's runner keeps the campaign: the
  // body carried, the help computer): NEW GAME and `coopgame` start them with `deathmatch` 0, and
  // the runner is kept from level to level, when no single-player program is found.
  bool rules_campaign = false;
  // The recordings played in turn behind its main menu (client/main.cpp's `menu_attract` auto),
  // by name as `playdemo` takes them; "" none (the title's night stays).
  std::string_view attract;
  // Its own deathmatch rules on our arena (rules/common_quake.cpp), named to the arena by the
  // local info `rules` (`rules_line`) when the baseline mod plays its packs; "" the arena's own.
  std::string_view arena_rules;
  // How far a place name reaches by default, units (client/place_names.hpp's `loc_dist`): a point
  // farther from the player than this is no place they are in (500 in every classic game); 0, no
  // limit.
  std::string_view place_range = "500";
  // Its client's place macros, `$loc_here` and `$loc_there` (client/place_names.hpp).
  bool place_macros = false;
  // Its classic look picks every colour of the picture from its palette's 256, as its own drawing
  // did (client/world_draw.cpp's grade); false, full colour (Quake II's renderer drew the
  // textures' colours times their light with nothing picked).
  bool palette_look = true;
  // How fast its console slides by default (`scr_conspeed`, client/game_start.hpp), and how much
  // of that is a whole screen: lines a second of a 200-line screen, or screens a second.
  std::string_view conspeed = "300";
  double conspeed_screen = 200;
};

inline constexpr ProgramFile kQ1Multiplayer[] = {{.name = "qwprogs.dat"}, {.name = "progs.dat"}};
inline constexpr ProgramFile kQ1SinglePlayer[] = {{.name = "progs.dat"}};
inline constexpr std::string_view kQ1Demos[] = {".dem",  ".mvd",   ".qwdv",  ".mvdv", ".mvdz",
                                                ".qwdz", ".mvdvz", ".qwdvz", ".ged",  ".dz"};

inline constexpr std::string_view kQ1Held[] = {
    "progs/v_axe.mdl",   "progs/v_shot.mdl", "progs/v_shot2.mdl", "progs/v_nail.mdl",
    "progs/v_nail2.mdl", "progs/v_rock.mdl", "progs/v_rock2.mdl", "progs/v_light.mdl"};

// 1 the launchers (rockets, then grenades), 2 the beam gun, 3 the shotguns (double first, by the
// movement keys: the shotguns are used more), 4 the nailguns (heavy first), 5 the melee weapon.
inline constexpr std::string_view kQ1Groups[] = {"7 6", "8", "3 2", "5 4", "1"};

inline constexpr std::string_view kQ1Episodes[] = {"e1m1", "e2m1", "e3m1", "e4m1"};

// Quake II's rules are a native library for one platform, which this build never loads: each
// is named when found and passed over (our own runner plays its maps: server/q2_server.hpp).
inline constexpr ProgramFile kQ2Programs[] = {
    {.name = "gamex86.dll", .form = ProgramForm::game_library},
    {.name = "gamex86_64.so", .form = ProgramForm::game_library},
    {.name = "gamei386.so", .form = ProgramForm::game_library}};
inline constexpr std::string_view kQ2Demos[] = {".dm2", ".mvd2"};
// By Quake II's weapon keys (1 blaster, 2 shotgun, 3 super shotgun, 4 machinegun, 5 chaingun,
// 6 hand grenades, 7 grenade launcher, 8 rocket launcher, 9 hyperblaster, 10 railgun, 11 BFG):
// 1 the explosives (rockets, the grenade launcher, hand grenades), 2 the railgun, 3 the shotguns,
// 4 the rapid fire guns (hyperblaster, chaingun, machinegun), 5 the blaster, 6 the BFG.
// Quake II's held weapons' models, in its weapon order (its rules' table): each fires as its
// model's folder names it (classicenhance's `classic_fire_of`).
inline constexpr std::string_view kQ2Held[] = {
    "models/weapons/v_blast/tris.md2",  "models/weapons/v_shotg/tris.md2",
    "models/weapons/v_shotg2/tris.md2", "models/weapons/v_machn/tris.md2",
    "models/weapons/v_chain/tris.md2",  "models/weapons/v_launch/tris.md2",
    "models/weapons/v_rocket/tris.md2", "models/weapons/v_hyperb/tris.md2",
    "models/weapons/v_rail/tris.md2",   "models/weapons/v_bfg/tris.md2",
    "models/weapons/v_handgr/tris.md2"};
inline constexpr std::string_view kQ2Groups[] = {"8 7 6", "10", "3 2", "9 5 4", "1", "11"};

inline constexpr GameProfile kGameProfiles[] = {
    {.id = "q1",
     .multiplayer_programs = kQ1Multiplayer,
     .single_player_programs = kQ1SinglePlayer,
     .fallback_map = "arena",
     .demo_extensions = kQ1Demos,
     .demo_folder = "",
     .tables = kQ1Tables,
     .held_weapons = kQ1Held,
     .arsenal = kQ1Arsenal,
     .words = kQ1ServerWords,
     .episode_maps = kQ1Episodes,
     .weapon_groups = kQ1Groups,
     .rules_campaign = false,
     .attract = "demo1 demo2 demo3",
     .arena_rules = "q1"},
    // Quake II: the demo's packs have no start map of the full game, so its first map.
    {.id = "q2",
     .multiplayer_programs = kQ2Programs,
     .single_player_programs = kQ2Programs,
     .fallback_map = "demo1",
     .demo_extensions = kQ2Demos,
     .demo_folder = "demos/",
     .tables = {},
     .held_weapons = kQ2Held,
     .arsenal = {},
     .words = {},
     .episode_maps = {},
     .weapon_groups = kQ2Groups,
     .rules_campaign = true,
     .attract = "q2demo1",
     .arena_rules = "",
     .place_macros = true,
     .palette_look = false,
     .conspeed = "3",
     .conspeed_screen = 1},
};

// The first row: Quake, the one every reader uses until a choice of game is made.
inline const GameProfile& q1_game() { return kGameProfiles[0]; }

// The row of `id`, or nullptr.
inline const GameProfile* find_game(std::string_view id) {
  for (const GameProfile& g : kGameProfiles) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

// The row of `id`, or Quake's when there is none (a profile named on the command line).
inline const GameProfile& game_or_quake(std::string_view id) {
  const GameProfile* g = find_game(id);
  return g != nullptr ? *g : q1_game();
}

// The game row's content row (every game row has one: the test holds it).
inline const ge::classicenhance::Profile& content_of(const GameProfile& g) {
  const ge::classicenhance::Profile* p = ge::classicenhance::find_profile(g.id);
  return p != nullptr ? *p : ge::classicenhance::q1_profile();
}

}  // namespace shooters_edge::profiles
