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
//   the first game's mod is one QC program for the server; its sequel's a native library for the
//   server; the third's up to three bytecode programs, one a side. A form this build has no runner
//   for is named when it is found and passed over, never read as another. For the first game the
//   multiplayer form's own name comes first, then the one name the single-player form also uses,
//   so a multiplayer mod that ships either name runs; a QC file of the other mode's layout (the
//   header's CRC, plugins/progs/layout.hpp) is passed over, never run in the wrong mode.
// - **Demos**: the extensions a demo of this game is listed by and tried with when named without
//   one, in order (a recording of another extension is still found by its bare name: the files'
//   top level is searched for that name with any extension, client/timedemo.hpp).
// - **The fallback map**: where the window starts when the content holds no start map (the
//   content row's `start_map`): the built-in arena.
// - **The full game** is the content row's `full_game` file being in the packs.
// - **Items, their words and the sounds** a bot reads (items.hpp).
//
// ge:summary A classic game's game half as data: each mode's program files, the fallback map
// ge:keywords game-profile profile classic program progs qwprogs multiplayer single-player
// ge:keywords mod-program fallback-map game-half find_game program-form program-side
// ge:keywords demo-extensions
// ge:skill shooter
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "classicenhance/profile.hpp"
#include "shooters_edge/profiles/items.hpp"

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
  ItemTables tables;  // its items, their words and the sounds a bot reads (items.hpp)
};

inline constexpr ProgramFile kClassicMultiplayer[] = {{.name = "qwprogs.dat"},
                                                      {.name = "progs.dat"}};
inline constexpr ProgramFile kClassicSinglePlayer[] = {{.name = "progs.dat"}};
inline constexpr std::string_view kClassicDemos[] = {".dem",  ".mvd",  ".qwdv",  ".mvdv",
                                                     ".mvdz", ".qwdz", ".mvdvz", ".qwdvz"};

inline constexpr GameProfile kGameProfiles[] = {
    {.id = "classic",
     .multiplayer_programs = kClassicMultiplayer,
     .single_player_programs = kClassicSinglePlayer,
     .fallback_map = "arena",
     .demo_extensions = kClassicDemos,
     .tables = kClassicTables},
};

// The first row: the classic game, the one every reader uses until a choice of game is made.
inline const GameProfile& classic_game() { return kGameProfiles[0]; }

// The row of `id`, or nullptr.
inline const GameProfile* find_game(std::string_view id) {
  for (const GameProfile& g : kGameProfiles) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

// The game row's content row (every game row has one: the test holds it).
inline const ge::classicenhance::Profile& content_of(const GameProfile& g) {
  const ge::classicenhance::Profile* p = ge::classicenhance::find_profile(g.id);
  return p != nullptr ? *p : ge::classicenhance::classic_profile();
}

}  // namespace shooters_edge::profiles
