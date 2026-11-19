// THE WINDOW'S CVAR LAYERS (docs/porting-games.md § One name, one meaning): every cvar the window
// registers belongs to a layer, so a name two layers both register is never merged silently
// (console/cvar.hpp § LAYERS):
//
//   engine   any game's: the window, the pacing, the camera, the console, screenshots, the HUD kit
//   shooter  every shooter's: the move buttons and mouse look, a demo's transport
//   classic  the classic plugin's: the picture over classic maps (r_look and kin)
//   shell    Shooter's Edge's own: its HUD, menus, coop, the held weapon
//   game:<id>  the LOADED classic game's own settings (`kGameCvars`): taken away when another
//              game is loaded, and the new game's put in their place
//
// Each game's settings say how each of its names meets the layers under it: `same` (one setting:
// the game's own default and words), `own` (the game's meaning of the name: the layers' setting
// waits while the game is loaded), a synonym (the game's own spelling of one of ours) or a name of
// its own. The loaded (primary) game decides what a shared name means; what no classic game had
// (an improvement every game gets) stays in the lower layers. A name that meets a lower layer's
// without saying so is a clash: nothing is printed to the player, and
// tests/shooters_edge_cvar_layers_test.cpp fails on it. What each name means in each classic game
// is porting/shooters-edge/cvar-meanings.md (a reader's facts); the census is docs/porting-games.md
// § One name, one meaning.
//
// ge:summary The window's cvar layers: engine, shooter, classic, shell and the loaded game's own
// ge:keywords cvar-layer layer game-cvars per-game-cvar one-name-one-meaning same own clash
// ge:keywords game-switch drop_layer game_layer crosshair vid_gamma s_volume
// ge:keywords gl_dynamic gl_lightmap cl_gun scr_centertime sequel-crosshair
// ge:skill console shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "console/console.hpp"
#include "hud/crosshair.hpp"
#include "shooter/mouse_feel.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace shooters_edge::client {

inline constexpr std::string_view kLayerEngine = "engine";
inline constexpr std::string_view kLayerShooter = "shooter";
inline constexpr std::string_view kLayerClassic = "classic";
inline constexpr std::string_view kLayerShell = "shell";

// "game:<id>": the layer of a game's own settings.
std::string game_layer(std::string_view game_id);
// Whether `layer` is a game's own ("game:<id>").
bool is_game_layer(std::string_view layer);
// The game's scope for its own values of common settings (console/cvar.hpp § SCOPES): its base
// folder's name, as a player types it before a colon (`<folder>:gamma 0`; `game:` the loaded).
std::string game_scope(const profiles::GameProfile& game);

// What a game's own settings write: the window's structs they bind to.
struct GameBindings {
  ge::hud::CrosshairSettings* crosshair = nullptr;  // the sequel's own `crosshair`
  ge::shooter::MouseFeel* mouse = nullptr;          // the sequel's own `m_accel` (per frame)
  bool* predict = nullptr;  // the sequel's `cl_predict` (a server joined); true in the first game
  int* crouch = nullptr;    // the sequel's `cl_predict_crouch` (the eye's height predicted)
  int* noskins = nullptr;   // the sequel's `cl_noskins` (the picture's stock_figures)
  int* hand = nullptr;      // the sequel's `hand` (the view's: 2 draws no weapon)
};

// The sequel's gender for a skin ("male/grunt"): its figure's folder, male for male and cyborg,
// female for female and crackhor, else none.
std::string auto_gender(std::string_view skin);

// The loaded game's layer.
struct GameCvarValues {
  std::string layer;  // "game:<id>", or "" before a game is loaded
};
// The loaded game's own settings on `con`, in its layer (`kGameCvars` in cvar_layers.cpp says
// what each game gives each name).
void add_game_cvars(ge::console::Console& con, GameCvarValues& v, const profiles::GameProfile& game,
                    const GameBindings& b);
// `run` with the loaded game's layer away (the series config read in our meanings), then the layer
// back with the values its own settings had before (a config read earlier set them).
void without_game_cvars(ge::console::Console& con, GameCvarValues& v,
                        const profiles::GameProfile& game, const GameBindings& b,
                        const std::function<void()>& run);
// Another game loaded: the old game's layer taken away, `now`'s registered. Returns how many
// cvars and names moved.
int switch_game_cvars(ge::console::Console& con, GameCvarValues& v,
                      const profiles::GameProfile& now, const GameBindings& b);

}  // namespace shooters_edge::client
