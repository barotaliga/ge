// The window's cvar layers and each classic game's own settings (cvar_layers.hpp).
//
// ge:summary The window's cvar layers: each game's own settings, the switch, the clashes printed
// ge:keywords cvar-layer game-cvars per-game-cvar same own clash game-switch crosshair
// ge:keywords vid_gamma s_volume gl_dynamic gl_lightmap cl_gun scr_centertime
// ge:skill console shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#include "shooters_edge/client/cvar_layers.hpp"

#include <utility>
#include <vector>

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

// THE FIRST CLASSIC GAME: every name it shares with the layers under it means there what it means
// in the game (its crosshair: 0 none, 1 its own +, 2 a dot, as the engine's marks begin), so it has
// no settings of its own yet, and its crosshair numbers the engine's marks.
void add_first(con::Console&, const GameBindings& b) {
  if (b.crosshair != nullptr) b.crosshair->pictures = false;
}

// THE SEQUEL (porting/shooters-edge/cvar-meanings.md):
// - `crosshair` is its own: 0 none, 1 to 3 its own pictures (ch1, ch2, ch3), never the engine's
//   marks, so a sequel player's config keeps its crosshair;
// - `scr_centertime` is the same setting, 2.5 seconds by default where the first game's is 2;
// - its own spellings of settings we have under the first game's names, the same thing in each:
//   `vid_gamma` (`gamma`: higher darker in both), `s_volume` (`volume`), `gl_dynamic`
//   (`r_dynamic`), `gl_lightmap` (`r_lightmap`), `cl_gun` (`r_drawviewmodel`).
void add_sequel(con::Console& c, const GameBindings& b) {
  con::Cvars& v = c.cvars();
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
        std::pair{"cl_gun", "r_drawviewmodel"}})
    v.add_synonym(name, ours);
}

struct GameRows {
  std::string_view game;  // the game profile's id
  void (*add)(con::Console&, const GameBindings&);
};
constexpr GameRows kGameCvars[] = {{"classic", add_first}, {"sequel", add_sequel}};

}  // namespace

std::string game_layer(std::string_view game_id) { return "game:" + std::string(game_id); }

bool is_game_layer(std::string_view layer) { return layer.starts_with("game:"); }

void without_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& game,
                        const GameBindings& b, const std::function<void()>& run) {
  // The game's own settings' values (not a `same` one's: that is the series' setting).
  std::vector<std::pair<std::string, std::string>> kept;
  for (const con::Cvar* cv : c.cvars().sorted())
    if (cv->layer == v.layer &&
        (cv->earlier.empty() || cv->earlier.back().how != con::Overlap::same))
      kept.emplace_back(cv->name, cv->value);
  if (!v.layer.empty()) c.cvars().drop_layer(v.layer);
  run();
  add_game_cvars(c, v, game, b);
  for (const auto& [name, value] : kept) c.cvars().set(name, value, con::Source::local);
}

void add_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& game,
                    const GameBindings& b) {
  v.layer = game_layer(game.id);
  con::CvarLayer layer(c.cvars(), v.layer);
  for (const GameRows& row : kGameCvars)
    if (row.game == game.id) row.add(c, b);
}

int switch_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& now,
                      const GameBindings& b) {
  const int moved = v.layer.empty() ? 0 : c.cvars().drop_layer(v.layer);
  const size_t before = c.cvars().size() + c.cvars().synonyms().size();
  add_game_cvars(c, v, now, b);
  return moved + static_cast<int>(c.cvars().size() + c.cvars().synonyms().size() - before);
}

}  // namespace shooters_edge::client
