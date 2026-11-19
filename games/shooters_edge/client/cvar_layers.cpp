// The window's cvar layers and each classic game's own settings (cvar_layers.hpp).
//
// ge:summary The window's cvar layers: each game's own settings, the switch, the clashes printed
// ge:keywords cvar-layer game-cvars per-game-cvar same own clash game-switch crosshair
// ge:keywords vid_gamma s_volume gl_dynamic gl_lightmap cl_gun scr_centertime
// ge:skill console shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#include "shooters_edge/client/cvar_layers.hpp"

#include "classicenhance/profile.hpp"

#include <utility>
#include <vector>

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

// THE FIRST CLASSIC GAME: every name it shares with the layers under it means there what it means
// in the game (its crosshair: 0 none, 1 its own +, 2 a dot, as the engine's marks begin), so it has
// no settings of its own yet, and its crosshair numbers the engine's marks. Its spelling of the
// lens, `fov`, is our `cl_fov` (90 in both); `cvar_reset` is `reset`, `viewalias` is `alias`. Its
// servers' switch rules, `w_switch` and `b_switch`, are user info a server reads on a pickup (the
// sequel has none).
void add_first(con::Console& c, const GameBindings& b) {
  if (b.crosshair != nullptr) b.crosshair->pictures = false;
  if (b.mouse != nullptr) b.mouse->per_frame = false;  // m_accel: counts a millisecond
  if (b.predict != nullptr) *b.predict = true;  // only the sequel turns it off (its cl_predict)
  c.cvars().add_synonym("fov", "cl_fov");
  c.cvars().add_synonym("ignore_qtv", "ignore_tv");  // its spelling of the TV viewers' chat
  c.add_command_synonym("cvar_reset", "reset");      // its spellings of a setting's default and an
  c.add_command_synonym("viewalias", "alias");       // alias printed (`alias <name>` with no body)
  for (const auto& [name, help] :
       {std::pair{"w_switch", "The best weapon a server may switch you to on picking one up."},
        std::pair{"b_switch",
                  "The best weapon a server may switch you to on picking up a "
                  "backpack."}})
    c.cvars().add({.name = name,
                   .fallback = "",
                   .flags = con::kArchive | con::kUserInfo,
                   .help = help,
                   .kind = con::CvarKind::text,
                   .values = "\"\": the server's own rule (the default); 1..8: up to that weapon's "
                             "number"});
}

// THE SEQUEL (porting/shooters-edge/cvar-meanings.md):
// - `crosshair` is its own: 0 none, 1 to 3 its own pictures (ch1, ch2, ch3), never the engine's
//   marks, so a sequel player's config keeps its crosshair;
// - `scr_centertime` is the same setting, 2.5 seconds by default where the first game's is 2;
// - its own spellings of settings we have under the first game's names, the same thing in each:
//   `vid_gamma` (`gamma`: higher darker in both), `s_volume` (`volume`), `gl_dynamic`
//   (`r_dynamic`), `gl_lightmap` (`r_lightmap`), `cl_gun` (`r_drawviewmodel`), `fov` (`cl_fov`);
// - the move keys' speeds are the same settings, 200 walking by default where the first game's
//   are 400 (the sequel has no back speed of its own: `cl_backspeed` keeps the forward's 200);
// - its player's user info: `hand`, `gender` and `gender_auto`; its own `m_accel` (per frame);
//   `cl_predict` (our `cl_nopred` the other way round).
void add_sequel(con::Console& c, const GameBindings& b) {
  con::Cvars& v = c.cvars();
  // Its `m_accel` adds the frame's counts times the setting (held within 0..1), no time in it.
  if (b.mouse != nullptr) b.mouse->per_frame = true;
  if (b.crosshair != nullptr) {
    b.crosshair->pictures = true;
    v.bind({.name = "crosshair",
            .fallback = "1",
            .flags = con::kArchive,
            .help = "The crosshair: one of the game's own pictures.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 3,
            .values = "0: none; 1, 2, 3: the game's crosshair pictures (1 the default)",
            .overlap = con::Overlap::own},
           &b.crosshair->mark);
  }
  v.add({.name = "scr_centertime",
         .fallback = "2.5",
         .flags = con::kArchive,
         .help = "How long a message in the middle of the screen stays, in seconds.",
         .min = 0,
         .max = 30,
         .values = "0..30: that many seconds (2.5 the default)",
         .overlap = con::Overlap::same});
  for (const auto& [name, ours] :
       {std::pair{"vid_gamma", "gamma"}, std::pair{"s_volume", "volume"},
        std::pair{"gl_dynamic", "r_dynamic"}, std::pair{"gl_lightmap", "r_lightmap"},
        std::pair{"cl_gun", "r_drawviewmodel"}, std::pair{"fov", "cl_fov"}})
    v.add_synonym(name, ours);
  // Its own spelling of our `cl_nopred` the other way round: 0 draws the server's place.
  if (b.predict != nullptr)
    v.bind({.name = "cl_predict",
            .fallback = "1",
            .help = "Whether your own moves are predicted on a server joined.",
            .kind = con::CvarKind::boolean,
            .values = "1: predicted, drawn where you will be (the default); 0: drawn where the "
                      "server last put you, a round trip late"},
           b.predict);
  // Its powers' shells and its items' pulse are its own look, on in it (the first game's default
  // is off, the shells and the pulse never in it).
  for (const auto& [name, help] :
       {std::pair{"r_powershells", "Whether a player with a power wears a coloured shell."},
        std::pair{"r_itempulse", "Whether the items lying about pulse with light."}})
    v.add({.name = name,
           .fallback = "1",
           .flags = con::kArchive,
           .help = help,
           .kind = con::CvarKind::boolean,
           .values = "0: none; 1: on, as the game draws them (the default)",
           .overlap = con::Overlap::same});
  // The player as its servers read him: the hand his weapon is held in (the shot leaves from that
  // side), and the words his own deaths are told in (gender_auto: picked from his skin's figure).
  v.add({.name = "hand",
         .fallback = "0",
         .flags = con::kArchive | con::kUserInfo,
         .help = "The hand you hold your weapon in; the server fires from that side.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .max = 2,
         .values = "0: right (the default); 1: left; 2: in the middle"});
  v.add({.name = "gender",
         .fallback = "male",
         .flags = con::kArchive | con::kUserInfo,
         .help = "How the server words your own deaths: his, her or its.",
         .kind = con::CvarKind::text,
         .values = "male (the default), female or none"});
  v.add({.name = "gender_auto",
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether your gender follows your skin's figure when you join.",
         .kind = con::CvarKind::boolean,
         .values = "1: male for the male and cyborg figures, female for the female ones, else "
                   "none (the default); 0: as you set it"});
  for (const char* name : {"cl_forwardspeed", "cl_backspeed", "cl_sidespeed", "cl_upspeed"})
    v.add({.name = name,
           .fallback = "200",
           .flags = con::kArchive,
           .help = "The speed a move key asks for, walking; running asks cl_movespeedkey times it.",
           .kind = con::CvarKind::integer,
           .min = 0,
           .max = 2000,
           .values = "0..2000: units a second, the server capping it (200 the default)",
           .overlap = con::Overlap::same});
}

struct GameRows {
  std::string_view game;  // the game profile's id
  void (*add)(con::Console&, const GameBindings&);
};
constexpr GameRows kGameCvars[] = {{"classic", add_first}, {"sequel", add_sequel}};

}  // namespace

std::string auto_gender(std::string_view skin) {
  std::string figure(skin.substr(0, skin.find('/')));
  for (char& ch : figure)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  if (figure == "male" || figure == "cyborg") return "male";
  if (figure == "female" || figure == "crackhor") return "female";
  return "none";
}

std::string game_layer(std::string_view game_id) { return "game:" + std::string(game_id); }

bool is_game_layer(std::string_view layer) { return layer.starts_with("game:"); }

std::string game_scope(const profiles::GameProfile& game) {
  const ge::classicenhance::Profile* row = ge::classicenhance::find_profile(game.id);
  return con::lower(row != nullptr ? row->base_dir : game.id);
}

void without_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& game,
                        const GameBindings& b, const std::function<void()>& run) {
  // The game's own settings' values (not a `same` one's: that is the series' setting).
  std::vector<std::pair<std::string, std::string>> kept;
  for (const con::Cvar* cv : c.cvars().sorted())
    if (cv->layer == v.layer &&
        (cv->earlier.empty() || cv->earlier.back().how != con::Overlap::same))
      kept.emplace_back(cv->name, cv->value);
  if (!v.layer.empty()) c.drop_layer(v.layer);
  run();
  add_game_cvars(c, v, game, b);
  for (const auto& [name, value] : kept) c.cvars().set(name, value, con::Source::local);
}

void add_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& game,
                    const GameBindings& b) {
  v.layer = game_layer(game.id);
  c.cvars().set_scope(game_scope(game));  // its own values over the common ones (cvar.hpp)
  con::CvarLayer layer(c.cvars(), v.layer);
  for (const GameRows& row : kGameCvars)
    if (row.game == game.id) row.add(c, b);
}

int switch_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& now,
                      const GameBindings& b) {
  const int moved = v.layer.empty() ? 0 : c.drop_layer(v.layer);
  const size_t before = c.cvars().size() + c.cvars().synonyms().size();
  add_game_cvars(c, v, now, b);
  return moved + static_cast<int>(c.cvars().size() + c.cvars().synonyms().size() - before);
}

}  // namespace shooters_edge::client
