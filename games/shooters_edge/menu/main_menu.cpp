// Shooter's Edge's main menu: the table of entries, the pages built from it, the values read from
// the console and the events run as console lines (main_menu.hpp).
//
// ge:summary Builds the main menu's pages from its table, reads the cvars, runs the events' lines
// ge:keywords menu main-menu table entries refresh apply_event console-line dimmed new-game start
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#include "shooters_edge/menu/main_menu.hpp"

#include <cmath>

#include "console/cvar.hpp"

namespace shooters_edge::menu {

namespace {

using ge::ui::ItemKind;

constexpr Choice kGameType[] = {{"COOPERATIVE", "0"}, {"DEATHMATCH", "1"}};
constexpr Choice kTeamplay[] = {{"OFF", "0"}, {"NO FRIENDLY FIRE", "1"}, {"FRIENDLY FIRE", "2"}};
constexpr Choice kSkill[] = {{"EASY", "0"}, {"NORMAL", "1"}, {"HARD", "2"}, {"NIGHTMARE", "3"}};
constexpr Choice kFragLimit[] = {{"NONE", "0"}, {"10", "10"}, {"20", "20"},  {"30", "30"},
                                 {"40", "40"},  {"50", "50"}, {"60", "60"},  {"70", "70"},
                                 {"80", "80"},  {"90", "90"}, {"100", "100"}};
constexpr Choice kTimeLimit[] = {{"NONE", "0"},    {"5 MIN", "5"},   {"10 MIN", "10"},
                                 {"15 MIN", "15"}, {"20 MIN", "20"}, {"30 MIN", "30"},
                                 {"45 MIN", "45"}, {"60 MIN", "60"}};
constexpr Choice kMaxPlayers[] = {{"2", "2"}, {"4", "4"}, {"8", "8"}, {"16", "16"}};
constexpr Choice kColours[] = {{"0", "0"},   {"1", "1"},   {"2", "2"},   {"3", "3"},  {"4", "4"},
                               {"5", "5"},   {"6", "6"},   {"7", "7"},   {"8", "8"},  {"9", "9"},
                               {"10", "10"}, {"11", "11"}, {"12", "12"}, {"13", "13"}};
constexpr Choice kHudStyle[] = {
    {"CLASSIC", "classic"}, {"MODERN", "modern"}, {"COMPACT", "compact"}};
constexpr Choice kWorldLook[] = {{"CLASSIC", "classic"}, {"MODERN", "modern"}};
constexpr Choice kMenuLook[] = {{"AUTO", "auto"}, {"CLASSIC", "classic"}, {"MODERN", "modern"}};
constexpr Choice kOnOff[] = {{"OFF", "0"}, {"ON", "1"}};
constexpr Choice kHudArt[] = {{"AUTO", "auto"}, {"CLASSIC", "classic"}, {"FREE", "free"}};
constexpr Choice kFall[] = {{"SNOW", "0"}, {"SLEET", "0.5"}, {"RAIN", "1"}};

// The menu, page by page. A row's `needs` dims it while the console knows no such name.
constexpr Entry kEntries[] = {
    // The root: the five rows of the classic main menu.
    {.page = "main",
     .id = "single",
     .text = "SINGLE PLAYER",
     .hint = "Play a map on your own.",
     .opens = "single"},
    {.page = "main",
     .id = "multi",
     .text = "MULTIPLAYER",
     .hint = "Start a match with its rules, or join one.",
     .opens = "multi"},
    {.page = "main",
     .id = "options",
     .text = "OPTIONS",
     .hint = "The mouse, the picture, the HUD and classic enhance.",
     .opens = "options"},
    {.page = "main", .id = "help", .text = "HELP", .hint = "The keys.", .opens = "help"},
    {.page = "main", .id = "quit", .text = "QUIT", .hint = "Leave the game.", .opens = "quit"},

    {.page = "single",
     .id = "new",
     .text = "NEW GAME",
     .hint = "The campaign from its first map, `start`. Needs a single-player program: the "
             "packs' own progs.dat.",
     .line = "newgame",
     .needs = "newgame",
     .closes = true},
    {.page = "single",
     .id = "load",
     .text = "LOAD",
     .hint = "Load a saved game: a slot of yours, or a classic .sav kept as s0 to s11.",
     .opens = "load",
     .needs = "load"},
    {.page = "single",
     .id = "save",
     .text = "SAVE",
     .hint = "Save the game into one of the slots.",
     .opens = "save",
     .needs = "save"},
    {.page = "single", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "multi",
     .id = "join",
     .text = "JOIN A GAME",
     .hint = "Find a server and connect to it. The client's network play comes with M18g.",
     .needs = "connect"},
    {.page = "multi",
     .id = "host",
     .text = "NEW GAME",
     .hint = "Start a match on this machine with its rules.",
     .opens = "host",
     .needs = "sv"},
    {.page = "multi",
     .id = "setup",
     .text = "SETUP",
     .hint = "Your name and colours.",
     .opens = "setup"},
    {.page = "multi", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "host",
     .id = "maxclients",
     .text = "MAX PLAYERS",
     .kind = ItemKind::choice,
     .hint = "How many may join. Others join once the client's network play is in.",
     .needs = "maxclients",
     .target = Target::server,
     .choices = kMaxPlayers},
    {.page = "host",
     .id = "deathmatch",
     .text = "GAME TYPE",
     .kind = ItemKind::choice,
     .hint = "Deathmatch, or the map played together.",
     .needs = "deathmatch",
     .target = Target::server,
     .choices = kGameType},
    {.page = "host",
     .id = "teamplay",
     .text = "TEAMPLAY",
     .kind = ItemKind::choice,
     .hint = "Teams by colour: a team's frags count together.",
     .needs = "teamplay",
     .target = Target::server,
     .choices = kTeamplay},
    {.page = "host",
     .id = "skill",
     .text = "SKILL",
     .kind = ItemKind::choice,
     .hint = "How hard the monsters fight in a cooperative game.",
     .needs = "skill",
     .target = Target::server,
     .choices = kSkill},
    {.page = "host",
     .id = "fraglimit",
     .text = "FRAG LIMIT",
     .kind = ItemKind::choice,
     .hint = "The match ends when a player reaches it.",
     .needs = "fraglimit",
     .target = Target::server,
     .choices = kFragLimit},
    {.page = "host",
     .id = "timelimit",
     .text = "TIME LIMIT",
     .kind = ItemKind::choice,
     .hint = "The match ends when the time is up.",
     .needs = "timelimit",
     .target = Target::server,
     .choices = kTimeLimit},
    {.page = "host",
     .id = "map",
     .text = "MAP",
     .kind = ItemKind::choice,
     .hint = "The map the match starts on.",
     .needs = "map"},
    {.page = "host",
     .id = "start",
     .text = "START",
     .hint = "Start the match with these rules.",
     .needs = "map",
     .closes = true},
    {.page = "host", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "setup",
     .id = "name",
     .text = "NAME",
     .hint = "Your name, from the next map. Type `name <yours>` in the console.",
     .line = "toggleconsole",
     .needs = "name",
     .closes = true},
    {.page = "setup",
     .id = "topcolor",
     .text = "SHIRT COLOUR",
     .kind = ItemKind::choice,
     .hint = "Your shirt's colour, as the others see you (a team game wears the team's).",
     .needs = "topcolor",
     .target = Target::client,
     .choices = kColours},
    {.page = "setup",
     .id = "bottomcolor",
     .text = "PANTS COLOUR",
     .kind = ItemKind::choice,
     .hint = "Your pants' colour, as the others see you (a team game wears the team's).",
     .needs = "bottomcolor",
     .target = Target::client,
     .choices = kColours},
    {.page = "setup", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "options",
     .id = "controls",
     .text = "CUSTOMIZE CONTROLS",
     .hint = "Bind a key to each move: ENTER, then the key.",
     .opens = "controls",
     .needs = "+forward"},
    {.page = "options",
     .id = "console",
     .text = "GO TO CONSOLE",
     .hint = "Drop the console.",
     .line = "toggleconsole",
     .needs = "toggleconsole",
     .closes = true},
    {.page = "options",
     .id = "reset",
     .text = "RESET TO DEFAULTS",
     .hint = "Every setting on these pages back to its default."},
    {.page = "options",
     .id = "viewsize",
     .text = "SCREEN SIZE",
     .kind = ItemKind::slider,
     .hint = "The status bars: 100 both, 110 the status bar alone, 120 none.",
     .needs = "viewsize",
     .target = Target::client,
     .min = 100,
     .max = 120,
     .step = 10},
    {.page = "options",
     .id = "gamma",
     .text = "BRIGHTNESS",
     .kind = ItemKind::slider,
     .hint = "The picture's brightness.",
     .needs = "gamma",
     .reversed = true,
     .target = Target::client,
     .min = 0.5,
     .max = 1,
     .step = 0.05},
    {.page = "options",
     .id = "sensitivity",
     .text = "MOUSE SPEED",
     .kind = ItemKind::slider,
     .hint = "Degrees of turn a mouse count.",
     .needs = "sensitivity",
     .target = Target::client,
     .min = 0.011,
     .max = 0.22,
     .step = 0.011},
    {.page = "options",
     .id = "bgmvolume",
     .text = "MUSIC VOLUME",
     .kind = ItemKind::slider,
     .hint = "The music's volume. The window plays no sound yet.",
     .needs = "bgmvolume",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "options",
     .id = "volume",
     .text = "SOUND VOLUME",
     .kind = ItemKind::slider,
     .hint = "The game's sound: a played demo's sounds and voices so far.",
     .needs = "volume",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "options",
     .id = "cl_run",
     .text = "ALWAYS RUN",
     .kind = ItemKind::toggle,
     .hint = "Run without holding the run key (SHIFT walks).",
     .needs = "cl_run",
     .target = Target::client},
    {.page = "options",
     .id = "m_invert",
     .text = "INVERT MOUSE",
     .kind = ItemKind::toggle,
     .hint = "The mouse's pitch upside down.",
     .needs = "m_invert",
     .target = Target::client},
    {.page = "options",
     .id = "lookspring",
     .text = "LOOKSPRING",
     .kind = ItemKind::toggle,
     .hint = "The view centres when you move.",
     .needs = "lookspring",
     .target = Target::client},
    {.page = "options",
     .id = "lookstrafe",
     .text = "LOOKSTRAFE",
     .kind = ItemKind::toggle,
     .hint = "The mouse's sideways moves sidestep instead of turning.",
     .needs = "lookstrafe",
     .target = Target::client},
    {.page = "options",
     .id = "video",
     .text = "VIDEO OPTIONS",
     .hint = "The world's look, the HUD and the menu's look.",
     .opens = "video"},
    {.page = "options",
     .id = "classic",
     .text = "CLASSIC ENHANCED",
     .hint = "Your classic packs enhanced, and the modern picture: light, water, sky, weather.",
     .opens = "enhanced",
     .needs = "ce_textures"},
    {.page = "options", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "video",
     .id = "r_look",
     .text = "WORLD LOOK",
     .kind = ItemKind::choice,
     .hint = "The world as the classic game drew it, or the modern picture.",
     .needs = "r_look",
     .target = Target::client,
     .choices = kWorldLook},
    {.page = "video",
     .id = "hud_style",
     .text = "HUD",
     .kind = ItemKind::choice,
     .hint = "The classic status bar, the modern corners or the compact rings.",
     .needs = "hud_style",
     .target = Target::client,
     .choices = kHudStyle},
    {.page = "video",
     .id = "menu_look",
     .text = "MENU LOOK",
     .kind = ItemKind::choice,
     .hint = "AUTO draws this menu in the pack's own pictures when the pack has them.",
     .needs = "menu_look",
     .target = Target::client,
     .choices = kMenuLook},
    {.page = "video",
     .id = "vid_fullscreen",
     .text = "FULLSCREEN",
     .kind = ItemKind::choice,
     .hint = "The window over the whole screen.",
     .needs = "vid_fullscreen",
     .target = Target::client,
     .choices = kOnOff},
    {.page = "video",
     .id = "hud_art",
     .text = "HUD PICTURES",
     .kind = ItemKind::choice,
     .hint =
         "The packs' own status bar pictures (AUTO when they have them) or the game's free set.",
     .needs = "hud_art",
     .target = Target::client,
     .choices = kHudArt},
    {.page = "video",
     .id = "weapon",
     .text = "HELD WEAPON",
     .hint = "The weapon in your hands: shown or not, its hands, where it is held, its sway.",
     .opens = "weapon",
     .needs = "r_drawviewmodel"},
    {.page = "video", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "weapon",
     .id = "r_drawviewmodel",
     .text = "SHOW WEAPON",
     .kind = ItemKind::toggle,
     .hint = "Draw the weapon in your hands.",
     .needs = "r_drawviewmodel",
     .target = Target::client},
    {.page = "weapon",
     .id = "r_viewmodel_hands",
     .text = "HANDS AND ARMS",
     .kind = ItemKind::toggle,
     .hint = "Draw the hands and arms that hold it; off, the weapon alone.",
     .needs = "r_viewmodel_hands",
     .target = Target::client},
    {.page = "weapon",
     .id = "r_viewmodel_center",
     .text = "CENTRED",
     .kind = ItemKind::toggle,
     .hint = "Held in the middle under the crosshair, the arena way; off, to the right.",
     .needs = "r_viewmodel_center",
     .target = Target::client},
    {.page = "weapon",
     .id = "cl_gunsway",
     .text = "SWAY",
     .kind = ItemKind::slider,
     .hint = "How much it sways, swings with your stride and breathes; 0 holds it still.",
     .needs = "cl_gunsway",
     .target = Target::client,
     .min = 0,
     .max = 2,
     .step = 0.25},
    {.page = "weapon",
     .id = "ce_muzzles",
     .text = "MUZZLE FIRE",
     .kind = ItemKind::toggle,
     .hint = "Each shot's flash and smoke from the barrel.",
     .needs = "ce_muzzles",
     .target = Target::client},
    {.page = "weapon", .id = "back", .text = "BACK", .kind = ItemKind::back},

    // CLASSIC ENHANCED: what the game adds to the classic picture, page by subject. Only what a
    // player will want to change is here; every other setting is the console's (`cvarlist ce_`).
    {.page = "enhanced",
     .id = "enhance",
     .text = "ENHANCE A PACK",
     .hint = "Pick one of your packs and enhance its textures, skins, light and sounds here.",
     .needs = "ce_textures"},
    {.page = "enhanced",
     .id = "art",
     .text = "TEXTURES AND MODELS",
     .hint = "The enhanced textures and skins, their relief and depth, the 2D art.",
     .opens = "ce_art",
     .needs = "ce_textures"},
    {.page = "enhanced",
     .id = "light",
     .text = "LIGHT AND SHADOW",
     .hint = "The rebaked light, shadows, the effects' light and the bloom.",
     .opens = "ce_light",
     .needs = "ce_rebake"},
    {.page = "enhanced",
     .id = "liquids",
     .text = "WATER, LAVA AND SKY",
     .hint = "Water that reflects, glowing lava and slime, teleporters, the sky and the clouds.",
     .opens = "ce_liquids",
     .needs = "ce_water"},
    {.page = "enhanced",
     .id = "fire",
     .text = "FIRE AND WIND",
     .hint = "Live torches, bonfires and the wind that moves them.",
     .opens = "ce_fire",
     .needs = "ce_flames"},
    {.page = "enhanced",
     .id = "weather",
     .text = "WEATHER",
     .hint = "Rain or snow over the open levels, the wind, storms and fog.",
     .opens = "ce_weather",
     .needs = "sv_weather"},
    {.page = "enhanced", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "ce_art",
     .id = "ce_textures",
     .text = "ENHANCED TEXTURES",
     .kind = ItemKind::toggle,
     .hint =
         "The walls and floors upscaled (once the pack is enhanced). From when the menu closes.",
     .needs = "ce_textures",
     .target = Target::client},
    {.page = "ce_art",
     .id = "ce_relief",
     .text = "SURFACE RELIEF",
     .kind = ItemKind::toggle,
     .hint = "The surfaces' bumps and shine lit; off, flat surfaces.",
     .needs = "ce_relief",
     .target = Target::client},
    {.page = "ce_art",
     .id = "ce_parallax",
     .text = "SURFACE DEPTH",
     .kind = ItemKind::toggle,
     .hint = "The mortar and the cracks sunk into the walls.",
     .needs = "ce_parallax",
     .target = Target::client},
    {.page = "ce_art",
     .id = "ce_models",
     .text = "ENHANCED SKINS",
     .kind = ItemKind::toggle,
     .hint = "The monsters' and items' skins upscaled. From when the menu closes.",
     .needs = "ce_models",
     .target = Target::client},
    {.page = "ce_art",
     .id = "ce_smooth",
     .text = "SMOOTH SHADING",
     .kind = ItemKind::toggle,
     .hint = "The models shaded smooth across their facets; off, flat facets.",
     .needs = "ce_smooth",
     .target = Target::client},
    {.page = "ce_art",
     .id = "ce_pictures",
     .text = "ENHANCED 2D ART",
     .kind = ItemKind::toggle,
     .hint = "The status bar, the menus and the font upscaled.",
     .needs = "ce_pictures",
     .target = Target::client},
    {.page = "ce_art", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "ce_light",
     .id = "ce_rebake",
     .text = "REBAKED LIGHT",
     .kind = ItemKind::toggle,
     .hint = "The light bounced round the rooms, coloured by the walls. From when the menu closes.",
     .needs = "ce_rebake",
     .target = Target::client},
    {.page = "ce_light",
     .id = "ce_shadows",
     .text = "SHADOWS",
     .kind = ItemKind::toggle,
     .hint = "Live lights and the monsters cast shadows.",
     .needs = "ce_shadows",
     .target = Target::client},
    {.page = "ce_light",
     .id = "r_fx_lights",
     .text = "EFFECT LIGHT",
     .kind = ItemKind::slider,
     .hint = "How much light the explosions, rockets and shots throw on the walls.",
     .needs = "r_fx_lights",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "ce_light",
     .id = "r_fx_glow",
     .text = "SPARK GLOW",
     .kind = ItemKind::slider,
     .hint = "How bright the sparks and embers glow.",
     .needs = "r_fx_glow",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.05},
    {.page = "ce_light",
     .id = "ce_bloom",
     .text = "BLOOM",
     .kind = ItemKind::toggle,
     .hint = "Bright lights bleed a soft glow. From when the menu closes.",
     .needs = "ce_bloom",
     .target = Target::client},
    {.page = "ce_light",
     .id = "r_bloom",
     .text = "BLOOM AMOUNT",
     .kind = ItemKind::slider,
     .hint = "How much glow the bloom mixes in. From when the menu closes.",
     .needs = "r_bloom",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "ce_light", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "ce_liquids",
     .id = "ce_water",
     .text = "WATER",
     .kind = ItemKind::toggle,
     .hint = "Water that reflects, refracts and ripples; off, the pack's warped picture.",
     .needs = "ce_water",
     .target = Target::client},
    {.page = "ce_liquids",
     .id = "ce_lava",
     .text = "LAVA",
     .kind = ItemKind::toggle,
     .hint = "Lava that glows, crusts and lights the room; off, the pack's warped picture.",
     .needs = "ce_lava",
     .target = Target::client},
    {.page = "ce_liquids",
     .id = "r_lava_glow",
     .text = "LAVA LIGHT",
     .kind = ItemKind::slider,
     .hint = "How much the lava lights the walls round it.",
     .needs = "r_lava_glow",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "ce_liquids",
     .id = "ce_slime",
     .text = "SLIME",
     .kind = ItemKind::toggle,
     .hint = "Slime that is thick and glows; off, the pack's warped picture.",
     .needs = "ce_slime",
     .target = Target::client},
    {.page = "ce_liquids",
     .id = "r_slime_glow",
     .text = "SLIME LIGHT",
     .kind = ItemKind::slider,
     .hint = "How much the slime's green glow lights the walls round it.",
     .needs = "r_slime_glow",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.1},
    {.page = "ce_liquids",
     .id = "ce_teleports",
     .text = "TELEPORTERS",
     .kind = ItemKind::toggle,
     .hint = "Teleporters drawn as a shimmering field that lights the floor.",
     .needs = "ce_teleports",
     .target = Target::client},
    {.page = "ce_liquids",
     .id = "ce_skies",
     .text = "SKIES",
     .kind = ItemKind::toggle,
     .hint = "A sky with depth, clouds and the evening light; off, the pack's scrolling sky.",
     .needs = "ce_skies",
     .target = Target::client},
    {.page = "ce_liquids",
     .id = "ce_clouds",
     .text = "LOW CLOUDS",
     .kind = ItemKind::toggle,
     .hint = "Low cloud over the open levels.",
     .needs = "ce_clouds",
     .target = Target::client},
    {.page = "ce_liquids", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "ce_fire",
     .id = "ce_flames",
     .text = "LIVE FIRE",
     .kind = ItemKind::toggle,
     .hint = "Torches and braziers burn as live fire; off, the pack's painted flames.",
     .needs = "ce_flames",
     .target = Target::client},
    {.page = "ce_fire",
     .id = "ce_bonfires",
     .text = "BONFIRES",
     .kind = ItemKind::toggle,
     .hint = "Braziers burn as bonfires throwing sparks into the wind.",
     .needs = "ce_bonfires",
     .target = Target::client},
    {.page = "ce_fire",
     .id = "ce_wind",
     .text = "WIND",
     .kind = ItemKind::toggle,
     .hint = "A wind that bends the flames and blows the sparks; off, still air.",
     .needs = "ce_wind",
     .target = Target::client},
    {.page = "ce_fire", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "ce_weather",
     .id = "sv_weather",
     .text = "WEATHER",
     .kind = ItemKind::toggle,
     .hint = "Rain or snow, wind and storms over the open levels.",
     .needs = "sv_weather",
     .target = Target::client},
    {.page = "ce_weather",
     .id = "sv_rain",
     .text = "RAIN OR SNOW",
     .kind = ItemKind::choice,
     .hint = "What falls: dry snow, sleet or rain.",
     .needs = "sv_rain",
     .target = Target::client,
     .choices = kFall},
    {.page = "ce_weather",
     .id = "sv_precip",
     .text = "HOW HARD",
     .kind = ItemKind::slider,
     .hint = "How hard it falls: light to heavy, a storm with lightning past 0.85.",
     .needs = "sv_precip",
     .target = Target::client,
     .min = 0,
     .max = 1,
     .step = 0.05},
    {.page = "ce_weather",
     .id = "sv_wind",
     .text = "WIND SPEED",
     .kind = ItemKind::slider,
     .hint = "The mean wind, metres a second.",
     .needs = "sv_wind",
     .target = Target::client,
     .min = 0,
     .max = 20,
     .step = 1,
     .unit = " M/S"},
    {.page = "ce_weather",
     .id = "r_lightning",
     .text = "LIGHTNING",
     .kind = ItemKind::toggle,
     .hint = "A storm's lightning flashes.",
     .needs = "r_lightning",
     .target = Target::client},
    {.page = "ce_weather",
     .id = "r_snowcover",
     .text = "SNOW COVER",
     .kind = ItemKind::toggle,
     .hint = "The snow settling on what faces up and is open to the sky.",
     .needs = "r_snowcover",
     .target = Target::client},
    {.page = "ce_weather",
     .id = "r_wet",
     .text = "WET SURFACES",
     .kind = ItemKind::toggle,
     .hint = "The wet faces and the puddles where the rain reaches.",
     .needs = "r_wet",
     .target = Target::client},
    {.page = "ce_weather",
     .id = "ce_fog",
     .text = "FOG",
     .kind = ItemKind::toggle,
     .hint = "The weather's fog and haze in the distance.",
     .needs = "ce_fog",
     .target = Target::client},
    {.page = "ce_weather", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "help",
     .id = "keys",
     .text = "W A S D  MOVE     SPACE  JUMP",
     .kind = ItemKind::label},
    {.page = "help",
     .id = "fire",
     .text = "MOUSE  LOOK     LEFT BUTTON  FIRE",
     .kind = ItemKind::label},
    {.page = "help",
     .id = "weapons",
     .text = "1 - 8  WEAPON     WHEEL  NEXT WEAPON",
     .kind = ItemKind::label},
    {.page = "help",
     .id = "console",
     .text = "THE KEY UNDER ESCAPE  CONSOLE",
     .kind = ItemKind::label},
    {.page = "help", .id = "menu", .text = "ESCAPE  THIS MENU", .kind = ItemKind::label},
    {.page = "help", .id = "back", .text = "BACK", .kind = ItemKind::back},

    {.page = "quit", .id = "ask", .text = "LEAVE SHOOTER'S EDGE?", .kind = ItemKind::label},
    {.page = "quit",
     .id = "yes",
     .text = "YES",
     .hint = "Quit the game.",
     .line = "quit",
     .needs = "quit",
     .closes = true},
    {.page = "quit", .id = "no", .text = "NO", .kind = ItemKind::back},
};

constexpr const char* kPages[][2] = {
    {"main", ""},
    {"single", "SINGLE PLAYER"},
    {"multi", "MULTIPLAYER"},
    {"host", "NEW GAME"},
    {"setup", "SETUP"},
    {"options", "OPTIONS"},
    {"video", "VIDEO"},
    {"weapon", "HELD WEAPON"},
    {"enhanced", "CLASSIC ENHANCED"},
    {"ce_art", "TEXTURES AND MODELS"},
    {"ce_light", "LIGHT AND SHADOW"},
    {"ce_liquids", "WATER, LAVA AND SKY"},
    {"ce_fire", "FIRE AND WIND"},
    {"ce_weather", "WEATHER"},
    {"help", "HELP"},
    {"quit", "QUIT"},
};

std::string control_id(size_t i) { return "c" + std::to_string(i); }

// The slot pages, built here rather than from the table: one row a slot, its words the save's
// line (`Host::saves`), and the slot's console line `load s<i>` or `save s<i>`.
constexpr const char* kSlotPages[][2] = {{"load", "LOAD"}, {"save", "SAVE"}};

bool slot_page(std::string_view page) { return page == "load" || page == "save"; }

std::string slot_id(int i) { return "s" + std::to_string(i); }

// A save's line cut to `chars`, the title shortened before the kills are: "the long
// corridor of doors, kills 4/31" reads "the long c, kills 4/31" in 22.
std::string slot_text(const std::string& line, size_t chars) {
  if (line.size() <= chars) return line;
  const size_t kills = line.rfind(" kills");
  if (kills == std::string::npos || line.size() - kills >= chars) return line.substr(0, chars);
  std::string head = line.substr(0, kills);
  const bool comma = !head.empty() && head.back() == ',';
  if (comma) head.pop_back();
  const std::string tail = (comma ? "," : "") + line.substr(kills);
  head.resize(chars - tail.size());
  while (!head.empty() && head.back() == ' ') head.pop_back();
  return head + tail;
}

bool same_value(std::string_view a, std::string_view b) {
  double x = 0, y = 0;
  if (ge::console::parse_number(a, x) && ge::console::parse_number(b, y)) return x == y;
  return ge::console::lower(a) == ge::console::lower(b);
}

bool shown(const Entry& e, const Host& host) {
  return e.built && (e.needs.empty() || host_knows(host, e.needs));
}

// The value a setting writes from its item now.
std::string item_text(const Entry& e, const ge::ui::Item& it) {
  switch (e.kind) {
    case ItemKind::toggle:
      return it.on ? "1" : "0";
    case ItemKind::choice:
      if (it.choice >= 0 && static_cast<size_t>(it.choice) < e.choices.size())
        return std::string(e.choices[static_cast<size_t>(it.choice)].value);
      return {};
    case ItemKind::slider:
      return ge::console::number_text(e.reversed ? e.max - it.value / 100 * (e.max - e.min)
                                                 : it.value);
    default:
      return {};
  }
}

}  // namespace

std::span<const Entry> entries() { return kEntries; }

const Entry* find_entry(std::string_view page, std::string_view id) {
  for (const Entry& e : kEntries)
    if (e.page == page && e.id == id) return &e;
  return nullptr;
}

bool host_knows(const Host& host, std::string_view name) {
  if (host.console != nullptr &&
      (host.console->cvars().find(name) != nullptr || host.console->has_command(name)))
    return true;
  return host.server != nullptr && host.server->find(name) != nullptr;
}

std::string host_value(const Host& host, std::string_view name) {
  if (host.console != nullptr)
    if (const ge::console::Cvar* c = host.console->cvars().find(name)) return c->value;
  if (host.server != nullptr)
    if (const ge::console::Cvar* c = host.server->find(name)) return c->value;
  return {};
}

ge::ui::Menu build_menu() {
  ge::ui::Menu m;
  for (const auto& [id, title] : kPages) {
    ge::ui::Page& page = m.add_page(id, title);
    for (const Entry& e : kEntries) {
      if (e.page != id) continue;
      ge::ui::Item it{.id = std::string(e.id), .text = std::string(e.text), .kind = e.kind};
      it.hint = std::string(e.hint);
      it.opens = std::string(e.opens);
      it.prominent = e.page == "host" && e.id == "start";
      for (const Choice& c : e.choices) it.choices.emplace_back(c.text);
      it.min = e.min, it.max = e.max, it.step = e.step, it.unit = std::string(e.unit);
      if (e.reversed) {  // shown 0 to 100 %, the cvar's max at 0 and its min at 100
        it.min = 0, it.max = 100, it.unit = "%";
        it.step = 100 * e.step / (e.max - e.min);
      }
      page.items.push_back(std::move(it));
    }
  }
  {
    ge::ui::Page& page = m.add_page("controls", "CUSTOMIZE CONTROLS");
    for (size_t i = 0; i < controls().size(); i++) {
      ge::ui::Item it{
          .id = control_id(i), .text = std::string(controls()[i].text), .kind = ItemKind::choice};
      it.hint = "ENTER, then the key for it. BACKSPACE clears it; a third key replaces both.";
      it.choices = {"---"};
      page.items.push_back(std::move(it));
    }
    page.items.push_back({.id = "back", .text = "BACK", .kind = ItemKind::back});
  }
  for (const auto& [id, title] : kSlotPages) {
    ge::ui::Page& page = m.add_page(id, title);
    for (int i = 0; i < kSaveSlots; i++) {
      ge::ui::Item it{.id = slot_id(i), .text = std::string(kEmptySlot)};
      it.hint = std::string(id) == "load" ? "Load this game." : "Save the game here.";
      page.items.push_back(std::move(it));
    }
    page.items.push_back({.id = "back", .text = "BACK", .kind = ItemKind::back});
  }
  return m;
}

void refresh(ge::ui::Menu& menu, const Host& host, bool hide_back) {
  for (const auto& [id, title] : kPages) {
    ge::ui::Page* page = menu.page(id);
    if (page == nullptr) continue;
    for (ge::ui::Item& it : page->items) {
      const Entry* e = find_entry(id, it.id);
      if (e == nullptr) continue;
      if (e->kind == ItemKind::back) {
        it.disabled = hide_back && e->page != "quit";
        continue;
      }
      if (e->kind == ItemKind::label) continue;
      it.disabled = !shown(*e, host);
      if (e->page == "host" && e->id == "map") {
        const std::string was =
            it.choices.empty() ? std::string() : it.choices[static_cast<size_t>(it.choice)];
        it.choices.clear();
        for (const std::string& map : host.maps) it.choices.push_back(map);
        if (it.choices.empty()) {
          it.choices.push_back("NONE");
          it.disabled = true;
        }
        it.choice = 0;
        for (size_t i = 0; i < it.choices.size(); i++)
          if (it.choices[i] == was) it.choice = static_cast<int>(i);
        continue;
      }
      if (e->id == "name") {
        const std::string name = host_value(host, "name");
        it.text = name.empty() ? std::string(e->text) : std::string(e->text) + "  " + name;
        continue;
      }
      if (e->target == Target::none || it.disabled) continue;
      const std::string v = host_value(host, e->id);
      if (e->kind == ItemKind::toggle) {
        double x = 0;
        it.on = ge::console::parse_number(v, x) && x != 0;
      } else if (e->kind == ItemKind::choice) {
        for (size_t i = 0; i < e->choices.size(); i++)
          if (same_value(v, e->choices[i].value)) it.choice = static_cast<int>(i);
      } else if (e->kind == ItemKind::slider) {
        double x = 0;
        if (ge::console::parse_number(v, x)) {
          x = std::fmin(e->max, std::fmax(e->min, x));
          it.value = e->reversed ? std::round((e->max - x) / (e->max - e->min) * 100) : x;
        }
      }
    }
    if (page->focus >= 0 && !page->items[static_cast<size_t>(page->focus)].stop())
      page->focus = -1;  // the menu moves it to the first stop
  }
  if (ge::ui::Page* page = menu.page("controls"); page != nullptr) {
    for (ge::ui::Item& it : page->items) {
      if (it.kind == ItemKind::back) {
        it.disabled = hide_back;
        continue;
      }
      const Control* c = control_of(it.id);
      if (c == nullptr) continue;
      // The command's first word (`impulse` of `impulse 10`) is what the console must know.
      it.disabled =
          host.console == nullptr || !host_knows(host, c->command.substr(0, c->command.find(' ')));
      it.choices = {host.console != nullptr
                        ? keys_text(bound_keys(host.console->binds(), c->command))
                        : std::string("---")};
      it.choice = 0;
    }
    if (page->focus >= 0 && !page->items[static_cast<size_t>(page->focus)].stop()) page->focus = -1;
  }
  for (const auto& [id, title] : kSlotPages) {
    ge::ui::Page* page = menu.page(id);
    if (page == nullptr) continue;
    const bool load = std::string_view(id) == "load";
    const bool known = host_knows(host, id);
    for (ge::ui::Item& it : page->items) {
      if (it.kind == ItemKind::back) {
        it.disabled = hide_back;
        continue;
      }
      const size_t i = std::stoul(it.id.substr(1));
      const std::string line = i < host.saves.size() ? host.saves[i] : std::string();
      it.text = line.empty() ? std::string(kEmptySlot)
                             : slot_text(line, hide_back ? kSlotChars : kSlotCharsModern);
      it.disabled = !known || (load && line.empty());
    }
    if (page->focus >= 0 && !page->items[static_cast<size_t>(page->focus)].stop()) page->focus = -1;
  }
}

Outcome apply_event(const ge::ui::MenuEvent& e, ge::ui::Menu& menu, Host& host) {
  if (e.kind == ge::ui::EventKind::back_at_root) return Outcome::back_at_root;
  if (host.console == nullptr) return Outcome::none;
  ge::console::Console& con = *host.console;
  if (slot_page(e.page)) {
    if (e.kind != ge::ui::EventKind::activated || e.item.empty() || e.item[0] != 's')
      return Outcome::none;
    con.execute(e.page + " " + e.item);
    return Outcome::close;
  }
  if (e.page == "controls") {
    return e.kind == ge::ui::EventKind::changed && control_of(e.item) != nullptr ? Outcome::bind_key
                                                                                 : Outcome::none;
  }
  const Entry* entry = find_entry(e.page, e.item);
  ge::ui::Page* page = menu.page(e.page);
  if (entry == nullptr || page == nullptr) return Outcome::none;
  const ge::ui::Item* it = page->find(e.item);
  if (it == nullptr) return Outcome::none;
  if (e.kind == ge::ui::EventKind::changed) {
    if (entry->target == Target::client)
      con.execute(std::string(entry->id) + " " + item_text(*entry, *it));
    return Outcome::none;
  }
  if (e.kind != ge::ui::EventKind::activated) return Outcome::none;
  if (e.page == "enhanced" && e.item == "enhance") return Outcome::classic_enhance;
  if (e.page == "options" && e.item == "reset") {
    reset_defaults(host);
    refresh(menu, host);
    return Outcome::none;
  }
  if (e.page == "host" && e.item == "start") {
    for (const ge::ui::Item& row : page->items) {
      const Entry* r = find_entry("host", row.id);
      if (r == nullptr || r->target != Target::server || row.disabled) continue;
      con.execute("sv " + std::string(r->id) + " " + item_text(*r, row));
    }
    const ge::ui::Item* map = page->find("map");
    if (map != nullptr && !map->disabled && !map->choices.empty())
      con.execute("map " + map->choices[static_cast<size_t>(map->choice)]);
    return Outcome::close;
  }
  if (!entry->line.empty()) con.execute(entry->line);
  return entry->closes ? Outcome::close : Outcome::none;
}

void reset_defaults(Host& host) {
  if (host.console == nullptr) return;
  for (const Entry& e : kEntries) {
    if (e.target != Target::client) continue;
    if (ge::console::Cvar* c = host.console->cvars().find(e.id))
      host.console->cvars().reset(*c, ge::console::Source::local);
  }
}

}  // namespace shooters_edge::menu
