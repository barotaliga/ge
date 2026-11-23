// Quake II's main menu (`q2_menu`, main_menu.hpp): its own pages and rows in Quake II's words, laid
// over the pages Shooter's Edge shares with Quake (classic enhance, the held weapon, the ranked
// game, the questions). A page Quake II defines here replaces Quake's page of the same id.
//
// - The root holds Quake II's five rows (game, multiplayer, options, video, quit) after SELECT
// GAME;
//   its quit leaves at once.
// - The game page starts a new game at one of four skills; the deathmatch flags page toggles one
//   bit of `dmflags` a row (`Entry::bit`, some rows inverted: "allow health" is the bit cleared).
// - OPTIONS opens Quake II's setup pages: input, key bindings (the commands its game knows:
//   `weapnext`, `inven`, `invuse`, `help`, `score`), legacy keys, weapon bindings (`use <item>`),
//   video, sound, effects, the rail trail, screen, crosshair and downloads. A key row shows its
//   keys as "a or b", "???" with none (`Entry::bind`).
// - ESCAPE in a level opens the compact in-game page `game` (`MenuTable::in_level`).
// - Every row whose setting the game does not have yet is dimmed by its `needs`, so it lights up
// the
//   day the setting is registered.
//
// ge:summary Quake II's main menu table: its pages, rows, key rows, dmflags bits and in-game page
// ge:keywords q2-menu quake-ii menu pages dmflags gameflags key-bindings weapon-bindings
// ge:keywords legacy-keys
// ge:keywords input-setup sound-setup effects-setup railtrail screen-setup crosshair-setup
// ge:keywords downloads address-book adr0
// ge:keywords in-game-page skill easy medium hard nightmare
// ge:skill ui console
// ge:test tests/shooters_edge_menu_test.cpp
#include <string_view>
#include <vector>

#include "shooters_edge/menu/main_menu.hpp"

namespace shooters_edge::menu {

namespace {

using ge::ui::ItemKind;

constexpr Choice kWindowed[] = {{"windowed", "0"}};  // then each mode of vid_modelist
constexpr Choice kFovScale[] = {{"vert-", "0"}, {"hor+", "1"}};
constexpr Choice kMsaa[] = {{"no", "0"}, {"2x MSAA", "2"}, {"4x MSAA", "4"}, {"8x MSAA", "8"}};
constexpr Choice kFilter[] = {{"nearest", "GL_NEAREST"},
                              {"linear", "GL_LINEAR"},
                              {"bilinear", "GL_LINEAR_MIPMAP_NEAREST"},
                              {"trilinear", "GL_LINEAR_MIPMAP_LINEAR"}};
constexpr Choice kAniso[] = {{"no", "1"}, {"2x", "2"}, {"4x", "4"}, {"8x", "8"}, {"16x", "16"}};
constexpr Choice kIntensity[] = {{"1x", "1"}, {"2x", "2"}, {"3x", "3"}};
constexpr Choice kSoundEngine[] = {{"no sound", "0"}, {"software", "1"}, {"OpenAL", "2"}};
constexpr Choice kAmbient[] = {{"no", "0"}, {"yes", "1"}, {"player's own", "2"}};
constexpr Choice kChatBeep[] = {{"disabled", "0"}, {"default", "1"}, {"alternative", "2"}};
constexpr Choice kDynamic[] = {{"no", "0"}, {"yes", "1"}, {"switchable", "2"}};
constexpr Choice kCel[] = {{"no", "0"}, {"1x", "1"}, {"2x", "2"}, {"3x", "3"}};
constexpr Choice kShadows[] = {{"no", "0"}, {"yes", "1"}, {"fade", "2"}};
constexpr Choice kRailType[] = {{"default", "0"}, {"core only", "1"}, {"core+spiral", "2"}};
constexpr Choice kColours[] = {{"black", "black"},     {"red", "red"},    {"green", "green"},
                               {"yellow", "yellow"},   {"blue", "blue"},  {"cyan", "cyan"},
                               {"magenta", "magenta"}, {"white", "white"}};
constexpr Choice kDemoBar[] = {{"no", "0"}, {"yes", "1"}, {"verbose", "2"}};
constexpr Choice kScale[] = {{"auto", "0"}, {"1x", "1"}, {"2x", "2"}, {"4x", "4"}};
constexpr Choice kCrosshair[] = {{"none", "0"}, {"cross", "1"}, {"dot", "2"}, {"angle", "3"}};
constexpr Choice kChScale[] = {{"0.5x", "0.5"}, {"1x", "1"}, {"2x", "2"}};
constexpr Choice kHand[] = {{"right", "0"}, {"left", "1"}, {"center", "2"}};

// A toggle of client cvar `id`.
constexpr Entry toggle(std::string_view page, std::string_view id, std::string_view text,
                       bool inverted = false) {
  return {.page = page,
          .id = id,
          .text = text,
          .kind = ItemKind::toggle,
          .needs = id,
          .target = Target::client,
          .inverted = inverted};
}
// A spin of client cvar `id` over `choices`.
constexpr Entry spin(std::string_view page, std::string_view id, std::string_view text,
                     std::span<const Choice> choices) {
  return {.page = page,
          .id = id,
          .text = text,
          .kind = ItemKind::choice,
          .needs = id,
          .target = Target::client,
          .choices = choices};
}
// A slider of client cvar `id`; `reversed` when the number falls to the right.
constexpr Entry slide(std::string_view page, std::string_view id, std::string_view text, double min,
                      double max, double step, bool reversed = false) {
  return {.page = page,
          .id = id,
          .text = text,
          .kind = ItemKind::slider,
          .needs = id,
          .reversed = reversed,
          .target = Target::client,
          .min = min,
          .max = max,
          .step = step};
}
// A key row binding `command`.
constexpr Entry key(std::string_view page, std::string_view id, std::string_view text,
                    std::string_view command) {
  return {.page = page, .id = id, .text = text, .kind = ItemKind::choice, .bind = command};
}
// A deathmatch flag: one bit of the server's `dmflags`.
constexpr Entry flag(std::string_view id, std::string_view text, int bit, bool inverted = false) {
  return {.page = "gameflags",
          .id = id,
          .text = text,
          .kind = ItemKind::toggle,
          .needs = "dmflags",
          .target = Target::server,
          .bit = bit,
          .inverted = inverted};
}
// A row opening `page`.
constexpr Entry opens(std::string_view page, std::string_view id, std::string_view text,
                      std::string_view to, std::string_view needs = {}) {
  return {.page = page, .id = id, .text = text, .opens = to, .needs = needs};
}
// A new single-player game at `skill`.
constexpr Entry skill(std::string_view id, std::string_view text, std::string_view line) {
  return {
      .page = "single", .id = id, .text = text, .line = line, .needs = "newgame", .closes = true};
}
// A row of the address book: server `id` (`adr0`..`adr15`), its address shown beside it
// (main_menu.cpp), typed in the console.
constexpr Entry book(std::string_view id) {
  return {.page = "addresses",
          .id = id,
          .text = id,
          .hint = "A server JOIN A GAME always asks. Type it in the console: adr0 host:port.",
          .line = "toggleconsole",
          .needs = id,
          .closes = true};
}
constexpr Entry back(std::string_view page) {
  return {.page = page, .id = "back", .text = "back", .kind = ItemKind::back};
}

constexpr Entry kQ2[] = {
    // The root.
    {.page = "main",
     .id = "games",
     .text = "SELECT GAME",
     .hint = "Pick the game you play and find it on this computer."},
    opens("main", "single", "game", "single"),
    opens("main", "multi", "multiplayer", "multi"),
    opens("main", "options", "options", "options"),
    opens("main", "video", "video", "video"),
    {.page = "main", .id = "quit", .text = "quit", .line = "quit", .needs = "quit"},

    // game: a new game at a skill, or a save.
    skill("easy", "easy", "sv skill 0; newgame"),
    skill("medium", "medium", "sv skill 1; newgame"),
    skill("hard", "hard", "sv skill 2; newgame"),
    skill("nightmare", "nightmare", "sv skill 3; newgame"),
    {.page = "single", .id = "gap", .kind = ItemKind::label},
    opens("single", "load", "load game...", "load", "load"),
    opens("single", "save", "save game...", "save", "save"),
    back("single"),

    // multiplayer.
    {.page = "multi",
     .id = "join",
     .text = "browse servers",
     .hint = "Servers on the LAN, the masters and the hub.",
     .needs = "connect"},
    {.page = "multi",
     .id = "q2servers",
     .text = "browse q2servers.com",
     .hint = "The servers q2servers.com lists.",
     .line = "pushmenu servers \"+http://q2servers.com/?raw=2\"",
     .needs = "connect"},
    {.page = "multi",
     .id = "book",
     .text = "browse address book",
     .hint = "The address book, servers.lst and the LAN.",
     .line = "pushmenu servers \"favorites://\" \"file:///servers.lst\" \"broadcast://\"",
     .needs = "connect"},
    {.page = "multi", .id = "demos", .text = "browse demos", .needs = "playdemo"},
    opens("multi", "addresses", "address book", "addresses", "adr0"),
    opens("multi", "host", "start server", "host", "sv"),
    {.page = "multi",
     .id = "ranked",
     .text = "ranked game",
     .hint = "Be matched with players of your rating. Sign in to your account first.",
     .opens = "ranked",
     .needs = "ranked"},
    back("multi"),

    // The address book: the servers JOIN A GAME asks besides the LAN and the masters.
    book("adr0"),
    book("adr1"),
    book("adr2"),
    book("adr3"),
    book("adr4"),
    book("adr5"),
    book("adr6"),
    book("adr7"),
    book("adr8"),
    book("adr9"),
    book("adr10"),
    book("adr11"),
    book("adr12"),
    book("adr13"),
    book("adr14"),
    book("adr15"),
    back("addresses"),

    // The deathmatch flags.
    flag("falling", "falling damage", 3, true),
    flag("weaponsstay", "weapons stay", 2),
    flag("instant", "instant powerups", 4),
    flag("powerups", "allow powerups", 1, true),
    flag("health", "allow health", 0, true),
    flag("armor", "allow armor", 11, true),
    flag("farthest", "spawn farthest", 9),
    flag("samemap", "same map", 5),
    flag("respawn", "force respawn", 10),
    flag("infinite", "infinite ammo", 13),
    flag("fixedfov", "fixed FOV", 15),
    flag("quaddrop", "quad drop", 14),
    back("gameflags"),

    // player setup.
    {.page = "setup", .id = "name", .text = "name", .needs = "name"},
    // The figure and its skin (menu/figures.hpp), read off the game's files, written to `skin`.
    {.page = "setup", .id = "model", .text = "model", .kind = ItemKind::choice, .needs = "skin"},
    {.page = "setup", .id = "figskin", .text = "skin", .kind = ItemKind::choice, .needs = "skin"},
    spin("setup", "hand", "handedness", kHand),
    back("setup"),

    // options.
    opens("options", "setup", "player setup", "setup"),
    opens("options", "input", "input setup", "input"),
    opens("options", "keys", "key bindings", "keys", "+forward"),
    opens("options", "weapons", "weapon bindings", "weapons", "use"),
    opens("options", "video", "video setup", "video"),
    opens("options", "sound", "sound setup", "sound"),
    opens("options", "effects", "effects setup", "effects"),
    opens("options", "screen", "screen setup", "screen"),
    opens("options", "downloads", "download options", "downloads"),
    {.page = "options", .id = "screenshots", .text = "screenshots", .needs = "screenshot"},
    opens("options", "classic", "CLASSIC ENHANCED", "enhanced", "ce_textures"),
    opens("options", "server", "server settings", "server"),  // server_page.hpp
    {.page = "options",
     .id = "console",
     .text = "go to console",
     .line = "toggleconsole",
     .needs = "toggleconsole",
     .closes = true},
    {.page = "options", .id = "reset", .text = "reset to defaults"},
    back("options"),

    // input setup.
    slide("input", "sensitivity", "mouse sens", 0.5, 20, 0.5),
    toggle("input", "m_autosens", "auto sens"),
    toggle("input", "m_filter", "mouse filter"),
    toggle("input", "freelook", "free look"),
    toggle("input", "cl_run", "always run"),
    back("input"),

    // key bindings.
    key("keys", "attack", "attack", "+attack"),
    key("keys", "weapnext", "next weapon", "weapnext"),
    key("keys", "weapprev", "previous weapon", "weapprev"),
    key("keys", "forward", "walk forward", "+forward"),
    key("keys", "back_", "backpedal", "+back"),
    key("keys", "speed", "run", "+speed"),
    key("keys", "left", "step left", "+moveleft"),
    key("keys", "right", "step right", "+moveright"),
    key("keys", "up", "up / jump", "+moveup"),
    key("keys", "down", "down / crouch", "+movedown"),
    key("keys", "inven", "inventory", "inven"),
    key("keys", "invuse", "use item", "invuse"),
    key("keys", "invdrop", "drop item", "invdrop"),
    key("keys", "invprev", "prev item", "invprev"),
    key("keys", "invnext", "next item", "invnext"),
    key("keys", "help", "help computer", "help"),
    key("keys", "pause", "pause game", "pause"),
    key("keys", "score", "scoreboard", "score"),
    key("keys", "chat", "chat", "messagemode"),
    opens("keys", "legacy", "legacy keys...", "legacykeys"),
    back("keys"),

    key("legacykeys", "turnleft", "turn left", "+left"),
    key("legacykeys", "turnright", "turn right", "+right"),
    key("legacykeys", "strafe", "sidestep", "+strafe"),
    key("legacykeys", "lookup", "look up", "+lookup"),
    key("legacykeys", "lookdown", "look down", "+lookdown"),
    key("legacykeys", "center", "center view", "centerview"),
    key("legacykeys", "mlook", "mouse look", "+mlook"),
    key("legacykeys", "klook", "keyboard look", "+klook"),
    back("legacykeys"),

    // weapon bindings: `use` and the item's name as the game spells it.
    key("weapons", "blaster", "blaster", "use Blaster"),
    key("weapons", "shotgun", "shotgun", "use Shotgun"),
    key("weapons", "sshotgun", "super shotgun", "use Super Shotgun"),
    key("weapons", "machinegun", "machinegun", "use Machinegun"),
    key("weapons", "chaingun", "chaingun", "use Chaingun"),
    key("weapons", "glauncher", "grenade launcher", "use Grenade Launcher"),
    key("weapons", "rlauncher", "rocket launcher", "use Rocket Launcher"),
    key("weapons", "hyperblaster", "hyperblaster", "use HyperBlaster"),
    key("weapons", "railgun", "railgun", "use Railgun"),
    key("weapons", "bfg", "bfg10k", "use BFG10K"),
    key("weapons", "grenades", "grenades", "use Grenades"),
    back("weapons"),

    // video setup, then Shooter's Edge's own looks.
    {.page = "video",
     .id = "vid_fullscreen",
     .text = "video mode",
     .kind = ItemKind::choice,
     .needs = "vid_fullscreen",
     .target = Target::client,
     .choices = kWindowed,
     .modes_from = "vid_modelist"},
    spin("video", "cl_adjustfov", "fov scaling", kFovScale),
    toggle("video", "gl_swapinterval", "vertical sync"),
    spin("video", "gl_multisamples", "anti-aliasing", kMsaa),
    slide("video", "vid_gamma", "texture gamma", 0.3, 1.3, 0.1, true),
    toggle("video", "vid_hwgamma", "hardware gamma"),
    slide("video", "gl_picmip", "texture quality", 0, 3, 1, true),
    spin("video", "gl_texturemode", "texture filter", kFilter),
    spin("video", "gl_anisotropy", "anisotropic filter", kAniso),
    slide("video", "gl_saturation", "texture saturation", 0, 1, 0.1),
    spin("video", "intensity", "texture intensity", kIntensity),
    slide("video", "gl_coloredlightmaps", "lightmap saturation", 0, 1, 0.1),
    slide("video", "gl_brightness", "lightmap brightness", 0, 0.3, 0.03),
    opens("video", "weapon", "HELD WEAPON", "weapon", "r_drawviewmodel"),
    back("video"),

    // sound setup.
    spin("sound", "s_enable", "sound engine", kSoundEngine),
    slide("sound", "s_mixahead", "sound latency", 0.05, 0.2, 0.015),
    slide("sound", "s_volume", "sound volume", 0, 1, 0.1),
    slide("sound", "ogg_volume", "music volume", 0, 1, 0.1),
    toggle("sound", "ogg_enable", "enable music"),
    toggle("sound", "ogg_shuffle", "shuffle tracks"),
    toggle("sound", "s_underwater", "underwater effect"),
    spin("sound", "s_ambient", "ambient sounds", kAmbient),
    spin("sound", "cl_chat_sound", "chat beep", kChatBeep),
    back("sound"),

    // effects setup.
    spin("effects", "gl_dynamic", "dynamic lighting", kDynamic),
    spin("effects", "gl_celshading", "entity cel-shading", kCel),
    toggle("effects", "cl_noglow", "entity glowing", true),
    spin("effects", "gl_shadows", "ground shadows", kShadows),
    toggle("effects", "gl_polyblend", "screen blending"),
    toggle("effects", "gl_waterwarp", "screen warping"),
    {.page = "effects",
     .id = "grenades",
     .text = "grenade explosions",
     .kind = ItemKind::toggle,
     .needs = "cl_disable_explosions",
     .target = Target::client,
     .bit = 0,
     .inverted = true},
    {.page = "effects",
     .id = "rockets",
     .text = "rocket explosions",
     .kind = ItemKind::toggle,
     .needs = "cl_disable_explosions",
     .target = Target::client,
     .bit = 1,
     .inverted = true},
    opens("effects", "rail", "railgun trail setup...", "railtrail"),
    back("effects"),

    spin("railtrail", "cl_railtrail_type", "rail trail type", kRailType),
    slide("railtrail", "cl_railtrail_time", "rail trail duration", 0.1, 3, 0.1),
    slide("railtrail", "cl_railcore_width", "core width", 1, 6, 1),
    slide("railtrail", "cl_railspiral_radius", "spiral radius", 1, 6, 1),
    spin("railtrail", "cl_railcore_color", "core color", kColours),
    spin("railtrail", "cl_railspiral_color", "spiral color", kColours),
    back("railtrail"),

    // screen setup.
    slide("screen", "viewsize", "screen size", 40, 100, 10),
    toggle("screen", "scr_lag_draw", "ping graph"),
    spin("screen", "scr_demobar", "demo bar", kDemoBar),
    slide("screen", "scr_alpha", "HUD opacity", 0, 1, 0.1),
    slide("screen", "con_alpha", "console opacity", 0, 1, 0.1),
    spin("screen", "scr_scale", "HUD scale", kScale),
    spin("screen", "con_scale", "console scale", kScale),
    spin("screen", "ui_scale", "menu scale", kScale),
    opens("screen", "crosshair", "crosshair setup...", "crosshair"),
    back("screen"),

    spin("crosshair", "crosshair", "crosshair type", kCrosshair),
    spin("crosshair", "ch_scale", "crosshair scale", kChScale),
    toggle("crosshair", "ch_health", "color by health"),
    slide("crosshair", "ch_red", "red channel", 0, 1, 0.1),
    slide("crosshair", "ch_green", "green channel", 0, 1, 0.1),
    slide("crosshair", "ch_blue", "blue channel", 0, 1, 0.1),
    slide("crosshair", "ch_alpha", "alpha channel", 0, 1, 0.1),
    back("crosshair"),

    // download options.
    toggle("downloads", "allow_download", "allow downloading"),
    toggle("downloads", "allow_download_maps", "maps"),
    toggle("downloads", "allow_download_players", "player models/skins"),
    toggle("downloads", "allow_download_models", "models"),
    toggle("downloads", "allow_download_sounds", "sounds"),
    toggle("downloads", "allow_download_textures", "textures"),
    toggle("downloads", "allow_download_pics", "pics"),
    toggle("downloads", "cl_http_downloads", "HTTP downloads"),
    back("downloads"),

    // The in-game page ESCAPE opens in a level.
    {.page = "game",
     .id = "q2servers",
     .text = "browse q2servers.com",
     .hint = "The servers q2servers.com lists.",
     .line = "pushmenu servers \"+http://q2servers.com/?raw=2\"",
     .needs = "connect"},
    {.page = "game",
     .id = "book",
     .text = "browse address book",
     .hint = "The address book, servers.lst and the LAN.",
     .line = "pushmenu servers \"favorites://\" \"file:///servers.lst\" \"broadcast://\"",
     .needs = "connect"},
    {.page = "game", .id = "join", .text = "browse servers", .needs = "connect"},
    {.page = "game", .id = "demos", .text = "browse demos", .needs = "playdemo"},
    opens("game", "single", "single player", "single"),
    opens("game", "options", "options", "options"),
    {.page = "game",
     .id = "disconnect",
     .text = "disconnect",
     .line = "disconnect",
     .needs = "disconnect",
     .closes = true},
    {.page = "game", .id = "quit", .text = "quit", .line = "quit", .needs = "quit"},
};

constexpr PageTitle kQ2Pages[] = {
    {"main", ""},
    {"single", "Game"},
    {"multi", "Multiplayer"},
    {"addresses", "Address Book"},
    {"host", "Start Server"},
    {"gameflags", "Deathmatch Flags"},
    {"setup", "Player Setup"},
    {"ranked", "RANKED GAME"},
    {"options", "Options"},
    {"input", "Input Setup"},
    {"keys", "Key Bindings"},
    {"legacykeys", "Legacy Key Bindings"},
    {"weapons", "Weapon Bindings"},
    {"video", "Video"},
    {"weapon", "HELD WEAPON"},
    {"sound", "Sound Setup"},
    {"effects", "Effects Setup"},
    {"railtrail", "Railgun Trail Setup"},
    {"screen", "Screen Setup"},
    {"crosshair", "Crosshair Setup"},
    {"downloads", "Download Options"},
    {"enhanced", "CLASSIC ENHANCED"},
    {"ce_art", "TEXTURES, MODELS"},
    {"ce_light", "LIGHT AND SHADOW"},
    {"ce_liquids", "LIQUIDS AND SKY"},
    {"ce_fire", "FIRE AND WIND"},
    {"ce_weather", "WEATHER"},
    {"server", "Server Settings"},
    {"help", "HELP"},
    {"quit", "QUIT"},
    {"newgame", "NEW GAME"},
    {"game", ""},
};

// The pages Quake II's rows above replace whole; every other page keeps Quake's rows.
bool own_page(std::string_view page) {
  for (const Entry& e : kQ2)
    if (e.page == page) return page != "host";  // START SERVER keeps Quake's rows, flags added
  return false;
}

// LOAD: the autosave a level start writes, a gap, then the fifteen a player saves to; SAVE: those
// fifteen.
constexpr std::string_view kQ2Load[] = {"save0",  "",       "save1",  "save2",  "save3", "save4",
                                        "save5",  "save6",  "save7",  "save8",  "save9", "save10",
                                        "save11", "save12", "save13", "save14", "save15"};
constexpr std::span<const std::string_view> kQ2Save = std::span(kQ2Load).subspan(2);

}  // namespace

const MenuTable& q2_menu() {
  static const std::vector<Entry> rows = [] {
    std::vector<Entry> out(std::begin(kQ2), std::end(kQ2));
    for (const Entry& e : entries()) {
      if (own_page(e.page)) continue;
      if (e.page == "host" && e.id == "start")  // the flags page before START
        out.push_back(opens("host", "dmflags", "deathmatch flags...", "gameflags", "dmflags"));
      out.push_back(e);
    }
    return out;
  }();
  static const MenuTable table{.pages = kQ2Pages,
                               .entries = rows,
                               .empty_slot = kEmptySlot,
                               .in_level = "game",
                               .script = "q2pro.menu",
                               .load_slots = kQ2Load,
                               .save_slots = kQ2Save};
  return table;
}

}  // namespace shooters_edge::menu
