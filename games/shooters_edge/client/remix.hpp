// CLASSIC REMIX: one classic game played with another's parts, a feature of classic-enhance
// (docs/goals/M18u.md). Off unless the player turns it on, and then only with two classic games
// loaded as FULL games (a shareware or demo install never counts), so a game loaded on its own
// keeps its own assets alone.
//
// - **The settings** (`RemixSettings`, `bind_remix`): `cr_enabled` (0 off, the default; 1 on)
//   and `cr_items` (0 the played game's own items; N the Nth classic game's: 1 the classic game's,
//   2 the sequel's), both kept in the player's config.
// - **The check** (`remix_check`): the installs the window knows, each with its edition; the
//   remix is on when it is enabled and a second full game is there, else the refusal says what
//   is missing in the game's words.
// - **Another game's map** (`map_game`, `remix_entities`): a map found only in the other game's
//   packs is played in the played game, its items made the played game's (or `cr_items`'s) by
//   the remix table (profiles/remix.hpp).
// - **Another game's items** (`items_game`): with `cr_items` naming the other game, a game's own
//   maps are played with that game's items, and the HUD that shows them is that game's. The
//   played game's rules keep their numbers and wear the other game's things (`worn_arsenal`):
//   its weapons in the hand and on the floor, their missiles and sounds, its armour. Rules that
//   name their things in code (the sequel's) wear the other game's by name instead
//   (`worn_looks`): the server's `sv_wear` renames each model and sound as the players are told it.
// - **The players** (`worn_figure`, `worn_physics`): `cr_models` draws every player as that game's
//   figure, read from the remix's files; `cr_physics` gives the played game's move that game's
//   running speed, friction and air by server lines (profiles/remix_arsenal.hpp).
// - **Over a mod** (`cr_over_mods`): a mod's own rules set their weapons and physics, so the remix
//   leaves them be; with `cr_over_mods 1` the physics `cr_physics` names are set over the mod's
//   (`wear_lines`). What a mod's weapons do is its own code: they are never swapped.
// - **The monsters** (`remix_monsters`): `cr_monsters` names the game whose monsters a map's are
//   made (profiles/remix_monsters.hpp), whole, where the played game's rules run them; with it 0
//   the played game's, so another game's map plays with the played game's monsters.
// - **The sounds** (`heard_game`): `cr_sounds` names the game every multiplayer sound is heard as
//   (profiles/remix_sounds.hpp), on the client, whoever's things the rules wear: with it 0 the
//   played game's, so `cr_items` alone swaps what is seen and never what is heard.
// - **The menu and the letters** (client/remix_pictures.hpp): `cr_menu` names the game whose menu
//   pictures are drawn, `cr_charset` the game whose glyph sheet writes every letter.
//
// ge:summary Classic remix: cr_enabled and cr_items, the two-full-games check, items swapped
// ge:keywords remix classic-remix cr_enabled cr_items remix_check remix_entities items_game
// ge:keywords cr_models cr_physics worn_figure worn_physics cr_sounds heard_game cr_over_mods
// ge:keywords cr_monsters remix_monsters cr_menu cr_charset
// ge:keywords other-game-map item-swap full-game entity-text map-items
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/cvar.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/installs.hpp"
#include "shooters_edge/profiles/remix.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "viewmodel/held.hpp"

namespace shooters_edge::client {

struct GameAdapter;
struct ListenOptions;
struct LoadedMap;

struct RemixSettings {
  bool enabled = false;            // cr_enabled
  int32_t items = 0;               // cr_items: 0 the played game's; N the Nth classic game's
  int32_t models = 0;              // cr_models: the same for the players' figure
  int32_t physics = 0;             // cr_physics: the same for the players' bodies' physics
  int32_t sounds = 0;              // cr_sounds: the same for the sounds heard
  bool over_mods = false;          // cr_over_mods: the swaps above set over a mod's own too
  int32_t monsters = 0;            // cr_monsters: the same for a campaign map's monsters
  int32_t menu = 0;                // cr_menu: the same for the menu's pictures
  int32_t charset = 0;             // cr_charset: the same for the glyph sheet every letter is
  profiles::RemixBalance balance;  // cr_strong_min, cr_strong_share, cr_pool_radius
};

// The parts the remix swaps, as the window compares them: cr_items, cr_models, cr_physics,
// cr_sounds, cr_over_mods, cr_monsters, cr_menu, cr_charset.
using RemixParts = std::array<int32_t, 8>;
inline RemixParts remix_parts(const RemixSettings& s) {
  return {s.items,    s.models, s.physics, s.sounds, s.over_mods ? 1 : 0,
          s.monsters, s.menu,   s.charset};
}

// `cr_enabled`, `cr_items`, `cr_models`, `cr_physics`, `cr_sounds`, `cr_over_mods`, `cr_monsters`,
// `cr_menu`, `cr_charset` and the balance's, writing `out`.
void bind_remix(ge::console::Cvars& cvars, RemixSettings* out);

// A classic game the window knows: its content profile id, its install and its edition.
struct KnownGame {
  std::string id;
  std::string basedir;
  Edition edition = Edition::unknown;
};

struct RemixState {
  bool on = false;
  std::string other;          // the other full game's profile id ("" when off)
  std::string other_basedir;  // its install
  std::string why;            // when off but enabled: what is missing, in the game's words
};

// The classic games this computer knows: the played one at `played_basedir`, the others from the
// installs kept in `prefs` (installs.hpp), each with its edition.
std::vector<KnownGame> known_games(const std::string& prefs, std::string_view played,
                                   const std::string& played_basedir);

// Whether the remix is on while `played` is played, from the games the window knows.
RemixState remix_check(const RemixSettings& s, std::string_view played,
                       std::span<const KnownGame> games);

// The profile id of the game whose items a map gets while the remix is on: `cr_items`'s game
// when it names a content profile, else `played`'s.
std::string_view items_game(const RemixSettings& s, std::string_view played);

// What a rewrite did, for the console and the lab.
struct RemixSwap {
  std::string from, to;  // class names; `to` empty: dropped
  int count = 0;
};
struct RemixReport {
  std::vector<RemixSwap> swaps;    // in the order first met
  int items = 0;                   // items met
  int swapped = 0;                 // items given another class name
  int dropped = 0;                 // items left out (no match, or a pool too small)
  int weapons = 0;                 // weapons laid down
  int strong_by_role = 0;          // strong weapons the role table alone gave
  int strong = 0;                  // strong weapons after the balance
  int pooled = 0;                  // small pickups added into pools
  int laid = 0;                    // the pools' pickups laid down again
  int to_ammo = 0;                 // weak weapons past `weak_same` laid down as their ammo
  std::vector<std::string> notes;  // each balance step, for the lab and the console's log
};

// The entity text `text` of a map whose items are `from`'s, with every item made `to`'s by the
// remix table and the map then balanced (profiles/remix.hpp: the weapons' tiers, the pools), into
// `out`. The same text when the games are the same. False with `error` when the text does not
// parse.
bool remix_entities(std::string_view text, std::string_view from, std::string_view to,
                    std::string& out, RemixReport* report, std::string& error,
                    const profiles::RemixBalance& balance = {});

// The line the console prints for a rewrite, in the game's words.
std::string remix_line(const RemixReport& r, std::string_view to);

// THE OTHER GAME'S FILES while the remix is on: its search path, opened from its install, with
// the played game's own mounted over it, so another game's map reads its own wall textures and
// sky while every model, sound and picture the played game's rules name is the played game's.
// The window keeps one and opens it again when the settings or the game change (`open_remix`).
struct RemixFiles {
  RemixState state;
  std::string played;              // the played game's profile id
  SearchPath files;                // the other game's, the played game's over it (empty while off)
  profiles::RemixBalance balance;  // the settings' (the window copies them before a load)
  // The other game's weapons and armour the played game's rules wear (`cr_items`,
  // profiles/remix_arsenal.hpp), or null: its models and sounds are read from `files`.
  const profiles::RemixArsenal* worn = nullptr;
  // The other game's things the played game's rules wear by name (`sv_wear`), or null.
  const profiles::RemixLooks* looks = nullptr;
  // The figure every player is drawn as (`cr_models`), or null: each their own.
  const profiles::RemixFigure* figure = nullptr;
  // How the players move (`cr_physics`): the server lines that set the other game's, or null.
  const profiles::RemixPhysics* physics = nullptr;
  // The game every sound is heard as (`cr_sounds`, `heard_game`): "" while off.
  std::string_view heard;
  // The game whose monsters a map is played with (`cr_monsters`, `heard_game`'s rule): "" while
  // off.
  std::string_view monsters;
  // The games whose menu pictures (`cr_menu`) and glyph sheet (`cr_charset`) the window draws
  // (client/remix_pictures.hpp, `heard_game`'s rule): "" while off.
  std::string_view menu, charset;
  // A mod's own module plays the rules (its qwprogs.qvm): it sets the weapons, the HUD's numbers
  // and the physics, so `cr_items` and `cr_physics` take nothing; the maps and figures still swap.
  bool mod_rules = false;
  // True while the rules or the players wear the other game's things: read from `files`.
  bool wearing() const { return worn != nullptr || looks != nullptr || figure != nullptr; }
  // True while the server reads the other game's files: wearing, or `cr_monsters` naming the other
  // game (its monsters' models and sounds).
  bool from_files() const { return wearing() || (!monsters.empty() && monsters != played); }
  // True while a sound may be heard from the other game's files: wearing, or `cr_sounds` naming it.
  bool heard_from_files() const { return wearing() || (!heard.empty() && heard != played); }
  // Where the sounds are read: the remix's `files` while `heard_from_files`, else the game's `own`.
  const SearchPath& heard_from(const SearchPath& own) const {
    return heard_from_files() ? files : own;
  }
  void undress() {
    worn = nullptr, looks = nullptr, figure = nullptr, physics = nullptr, heard = {};
    menu = {}, charset = {};
  }
};

// What the played game's rules wear while `r` is on (`worn_arsenal`, `worn_looks`) and the
// players' figure (`worn_figure`) and physics (`worn_physics`) set in `r`; true when it changed.
bool remix_dress(RemixFiles& r, const RemixSettings& s, std::string_view played);

// The server's lines while `r` wears the other game's things: the worn arsenal's, or the game's
// `own` with `sv_wear` naming the game worn; then the lines of the physics taken (`cr_physics`).
std::vector<std::string> worn_lines(const RemixFiles& r, const std::vector<std::string>& own);

// The window's server lines while `r` is dressed: `opt.arsenal` (`worn_lines`) and, while a mod's
// own rules play and `cr_over_mods` is on, `opt.over_mod`: the physics taken (`cr_physics`).
void wear_lines(ListenOptions& opt, const RemixFiles& r, const std::vector<std::string>& own);

// The entity text `text` of a map played on `played`'s rules with every monster made game `to`'s
// by the monster table (profiles/remix_monsters.hpp), where `played`'s rules run it, else the
// played game's own partner, into `out`; each change counted in `report`'s swaps. False with
// `error` when the text does not parse.
bool remix_monsters(std::string_view text, std::string_view played, std::string_view to,
                    std::string& out, RemixReport* report, std::string& error);

// The console's line when what the rules wear changes to `r`'s, in the game's words.
std::string worn_line(const RemixFiles& r);

// The held weapons the window draws while `remix` wears the other game's things: their view
// models through the remix's files, else the game's own from `own` (held.hpp's `load_held_set`).
// True when they are authored at the eye.
bool load_worn_held(const GameAdapter& game, const RemixFiles& remix, const SearchPath& own,
                    std::span<const std::string> dirs, std::vector<ge::viewmodel::HeldModel>& out);

// The arsenal the played game's rules wear while `state` is on: `cr_items`'s game's things on the
// played game's numbers, or null (off, the played game's own, or no row for the pair).
const profiles::RemixArsenal* worn_arsenal(const RemixSettings& s, const RemixState& state,
                                           std::string_view played);

// The renames the played game's rules wear while `state` is on (`cr_items`'s game's things by
// name), or null: the rules that name their things in code (profiles/remix_arsenal.hpp's
// `kRemixLooks`).
const profiles::RemixLooks* worn_looks(const RemixSettings& s, const RemixState& state,
                                       std::string_view played);

// The figure every player is drawn as while `state` is on: `cr_models`'s game's, or null (off,
// the played game's own, or no row for the pair).
const profiles::RemixFigure* worn_figure(const RemixSettings& s, const RemixState& state,
                                         std::string_view played);

// The physics the players take while `state` is on: `cr_physics`'s game's, or null (off, the played
// game's own, or no row for the pair).
const profiles::RemixPhysics* worn_physics(const RemixSettings& s, const RemixState& state,
                                           std::string_view played);

// The game every sound is heard as while `state` is on: `cr_sounds`'s game, else the played
// game's own (so the other game's weapons worn by `cr_items` fire with the played game's shots);
// "" while off, each sound heard as the server names it.
std::string_view heard_game(const RemixSettings& s, const RemixState& state,
                            std::string_view played);

// Opens the other game's files for `state` under the played game's install `basedir` (its game
// folder `game`); closes them when it is off. False with the reason in the game's words when an
// install does not open.
bool open_remix(RemixFiles& r, const RemixState& state, std::string_view played,
                const std::string& basedir, const std::string& game, std::string& error);

// The map `name`: the played game's own from `files`, else (the remix on) the other game's from
// `remix->files`, its items made the played game's (`said`: the line saying so). A map of brushes
// the classic game plays is played on its programs' rules, traced through its brushes; a map of
// clip hulls the sequel would play is refused for now in the game's words.
std::unique_ptr<LoadedMap> load_remixed_map(const SearchPath& files, const std::string& name,
                                            const RemixFiles* remix, std::string& error,
                                            std::string* said = nullptr);

}  // namespace shooters_edge::client
