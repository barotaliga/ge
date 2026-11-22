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
// ge:keywords gl_dynamic gl_lightmap cl_gun scr_centertime q2-crosshair
// ge:skill console shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "audio/music_console.hpp"
#include "classic/q2_fx_switches.hpp"
#include "console/console.hpp"
#include "hud/crosshair.hpp"
#include "shooter/mouse_feel.hpp"
#include "shooters_edge/client/chat_lines.hpp"
#include "shooters_edge/client/console_log.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/q2_session.hpp"
#include "shooters_edge/client/q2_view.hpp"
#include "shooters_edge/menu/classic_console.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/view/hand.hpp"

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
  // The game's files: `crosshairimage`'s picture is read from them (picture_files.hpp).
  const SearchPath* files = nullptr;
  ge::hud::CrosshairSettings* crosshair = nullptr;  // Quake II's own `crosshair`
  ge::shooter::MouseFeel* mouse = nullptr;          // Quake II's own `m_accel` (per frame)
  bool* predict = nullptr;      // Quake II's `cl_predict` (a server joined); true in Quake
  int* crouch = nullptr;        // Quake II's `cl_predict_crouch` (the eye's height predicted)
  bool* show_misses = nullptr;  // Quake II's `cl_showmiss` (each prediction miss printed)
  int* shownet = nullptr;       // Quake II's `cl_shownet` (each message's size printed)
  bool* showpackets = nullptr;  // Quake II's `cl_showpackets` (each packet sent shown)
  int* noskins = nullptr;       // Quake II's `cl_noskins` (the picture's stock_figures)
  bool* items_still = nullptr;  // Quake II's `cl_nobob` (the picture's items_still)
  // The held weapon's settings: Quake II's `hand` (2 draws no weapon), `cl_gun_x/y/z` and
  // `cl_gunfov`; Quake's `r_viewmodelsize` and `r_viewmodeloffset`.
  shooters_edge::view::ViewSettings* view = nullptr;
  int* moves = nullptr;         // Quake II's own `cl_maxfps`: its moves a second
  int* stuff_level = nullptr;   // Quake II's `cl_ignore_stufftext` (classic/protocol/stuff_filter)
  double* frame_cap = nullptr;  // the window's frame cap (our `cl_maxfps`), Quake II's `r_maxfps`
  // The maps' light: Quake II's `intensity`, `gl_modulate` and `gl_brightness` (the picture's
  // light_intensity, light_modulate and light_lift: classic/app/look.hpp); Quake keeps 2, 1, 0.
  double* light_intensity = nullptr;
  double* light_modulate = nullptr;
  double* light_lift = nullptr;
  bool* gibs = nullptr;     // Quake II's `cl_gibs` (the picture's gibs); true in Quake
  bool* no_glow = nullptr;  // Quake II's `cl_noglow` (the picture's no_glow); false in Quake
  // Quake II's effect switches (the picture's q2fx: classic/q2_fx_switches.hpp); off in Quake.
  ge::classic::Q2FxSwitches* q2fx = nullptr;
  // The chat's extras: Quake II's own `cl_chat_sound` (chat_lines.hpp), "" in every other game.
  ChatExtras* chat = nullptr;
  // The console's typed line: Quake II's `con_auto_chat` (console/view.hpp's game_chat), -1 in
  // every other game (our `con_chat` rules).
  int* auto_chat = nullptr;
  // Quake II's view and 2D settings (q2_view.hpp: scr_scale, scr_draw2d, scr_alpha, scr_lag_*,
  // cl_adjustfov, uf, cl_kickangles); their defaults in every other game. add_game_cvars binds its
  // own GameCvarValues::screen when this is null.
  Q2Screen* screen = nullptr;
  // The music (audio/music_console.hpp): Quake II's `ogg_enable` (its switch, `cd on|off`),
  // `ogg_shuffle` and `ogg_menu_track`; on, unshuffled and no menu track in every other game.
  ge::audio::MusicConsole* music = nullptr;
  // The console's look: Quake II's `con_background` (its picture under `pics/`, read from `files`);
  // the game's own `conback` in every other game.
  shooters_edge::menu::ClassicConsole* console = nullptr;
};

// Quake II's gender for a skin ("male/grunt"): its figure's folder, male for male and cyborg,
// female for female and crackhor, else none.
std::string auto_gender(std::string_view skin);

// The loaded game's layer.
struct GameCvarValues {
  std::string layer;  // "game:<id>", or "" before a game is loaded
  // Quake II's map commands, clock, cl_autopause and state cvars (q2_session.hpp); their defaults
  // in every other game.
  Q2Session q2;
  Q2Screen screen;  // Quake II's view and 2D settings (q2_view.hpp), bound as GameBindings::screen
  // Quake II's console log (console_log.hpp): written by the console's sink, wrapped once (when it
  // has one) by the first add_game_cvars, which shares it; off in every other game.
  std::shared_ptr<ConsoleLog> log;
  bool log_hooked = false;
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
