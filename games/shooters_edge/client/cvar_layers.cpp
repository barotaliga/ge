// The window's cvar layers and each classic game's own settings (cvar_layers.hpp).
//
// ge:summary The window's cvar layers: each game's own settings, the switch, the clashes printed
// ge:keywords cvar-layer game-cvars per-game-cvar same own clash game-switch crosshair
// ge:keywords vid_gamma s_volume gl_dynamic gl_lightmap cl_gun scr_centertime
// ge:keywords intensity gl_modulate gl_brightness
// ge:keywords sshot_format sshot_dir image_jpeg_quality_level
// ge:skill console shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#include "shooters_edge/client/cvar_layers.hpp"

#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "shooters_edge/client/picture_files.hpp"

#include <utility>
#include <vector>

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

// THE FIRST CLASSIC GAME: every name it shares with the layers under it means there what it means
// in the game (its crosshair: 0 none, 1 its own +, 2 a dot, as the engine's marks begin), so it has
// no settings of its own yet, and its crosshair numbers the engine's marks. Its spelling of the
// lens, `fov`, is our `cl_fov` (90 in both); `cvar_reset` is `reset`, `viewalias` is `alias`.
// `cl_novweps` keeps its servers from showing the weapons players carry (the connect leaves the
// visible weapons' bit out: net/qw_remote.cpp). Its
// servers' switch rules, `w_switch` and `b_switch`, are user info a server reads on a pickup (the
// Quake II has none). Its weapon's `r_viewmodelsize` and `r_viewmodeloffset` (units, added to
// ours), and `r_viewmodellastfired` (a recording's weapon the one that last fired).
void add_first(con::Console& c, const GameBindings& b) {
  if (b.crosshair != nullptr) {  // Quake II's own shares and health colour set aside
    b.crosshair->pictures = false;
    b.crosshair->shade[0] = b.crosshair->shade[1] = b.crosshair->shade[2] = 1;
    b.crosshair->by_health = false;
  }
  if (b.mouse != nullptr) b.mouse->per_frame = false;  // m_accel: counts a millisecond
  if (b.predict != nullptr) *b.predict = true;  // only Quake II turns it off (its cl_predict)
  if (b.light_intensity != nullptr) *b.light_intensity = 2;  // Quake II's light settings set aside
  if (b.light_modulate != nullptr) *b.light_modulate = 1;
  if (b.light_lift != nullptr) *b.light_lift = 0;
  if (b.view != nullptr) {  // Quake's weapon is always drawn, at its own place and lens
    b.view->hand = 0;
    b.view->gun = 1;
    b.view->gun_units[0] = b.view->gun_units[1] = b.view->gun_units[2] = 0;
    b.view->gun_fov_h = 0;
  }
  // Its weapon's depth and offset, in its units; the offset adds to our cl_gunx/y/z.
  const con::CvarSpec size{
      .name = "r_viewmodelsize",
      .fallback = "1",
      .flags = con::kArchive,
      .help =
          "How long the held weapon looks: under 1 it is squashed toward "
          "you.",
      .min = 0,
      .max = 1,
      .values = "1: its full length (the default); 0..1: down to half as long"};
  const con::CvarSpec offset{
      .name = "r_viewmodeloffset",
      .fallback = "",
      .flags = con::kArchive,
      .help =
          "Moves the held weapon by up to three numbers in game units: right, down and "
          "forward; added to cl_gunx, cl_guny and cl_gunz.",
      .kind = con::CvarKind::text,
      .values =
          "\"\": where the game holds it (the default); \"x y z\": that far right, down and "
          "forward, missing numbers 0"};
  // A recording's weapon: the one that last fired, kept while the player flicks through others.
  const con::CvarSpec last_fired{
      .name = "r_viewmodellastfired",
      .fallback = "0",
      .flags = con::kArchive,
      .help =
          "Whether a recording played back shows the weapon last fired while the player "
          "looks through others.",
      .kind = con::CvarKind::boolean,
      .values =
          "0: the weapon held (the default); 1: the weapon that last fired, until another "
          "one fires"};
  if (b.view != nullptr) {
    c.cvars().bind(size, &b.view->size);
    c.cvars().bind(offset, &b.view->offset_text);
    c.cvars().bind(last_fired, &b.view->last_fired);
  } else {
    c.cvars().add(size);
    c.cvars().add(offset);
    c.cvars().add(last_fired);
  }
  if (b.noskins != nullptr) *b.noskins = 0;  // its figures are its own (choose_figure)
  c.cvars().add_synonym("fov", "cl_fov");
  c.cvars().add_synonym("ignore_qtv", "ignore_tv");     // its spelling of the TV viewers' chat
  c.cvars().add_synonym("cl_chatmode", "con_chat");     // what a typed line is: command or chat
  c.cvars().add_synonym("sshot_format", "scr_format");  // its spellings of the screenshot's
  c.cvars().add_synonym("sshot_dir", "scr_folder");     // format, folder and JPEG quality
  c.cvars().add_synonym("image_jpeg_quality_level", "scr_quality");
  c.add_command_synonym("cvar_reset", "reset");  // its spellings of a setting's default and an
  c.add_command_synonym("viewalias", "alias");   // alias printed (`alias <name>` with no body)
  c.cvars().add({.name = "cl_novweps",
                 .fallback = "0",
                 .flags = con::kArchive,
                 .help = "Whether you tell servers not to show the weapons players carry.",
                 .kind = con::CvarKind::boolean,
                 .values = "0: they may (the default); 1: they show none, from the next connect"});
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

// QUAKE II (porting/shooters-edge/cvar-meanings.md):
// - `crosshair` is its own: 0 none, 1 to 3 its own pictures (ch1, ch2, ch3), never the engine's
//   marks, so a Quake II player's config keeps its crosshair;
// - `scr_centertime` is the same setting, 2.5 seconds by default where Quake's is 2, and
//   `cl_timeout` 120 where Quake's is 60;
// - its own spellings of settings we have under Quake's names, the same thing in each:
//   `vid_gamma` (`gamma`: higher darker in both), `s_volume` (`volume`), `con_alpha`
//   (`con_bgalpha`), `gl_dynamic` (`r_dynamic`), `gl_lightmap` (`r_lightmap`), `fov` (`cl_fov`),
//   `cl_vwep` (`r_drawvweps`: the weapon a figure carries, 1 in both), and its crosshair's `ch_x`,
//   `ch_y` (`cl_crossx`, `cl_crossy`), `ch_scale` (`crosshairsize`) and `ch_alpha`
//   (`crosshairalpha`); its own `ch_red`, `ch_green`, `ch_blue` and `ch_health` colour it;
// - the move keys' speeds are the same settings, 200 walking by default where Quake's
//   are 400 (Quake II has no back speed of its own: `cl_backspeed` keeps the forward's 200);
// - its player's user info: `hand`, `gender` and `gender_auto`; its own `m_accel` (per frame);
//   `cl_predict` (our `cl_nopred` the other way round) and `cl_predict_crouch` (the eye's height
//   predicted with the moves; Quake never ducks);
// - its weapon's place and lens in its units: `cl_gun_x/y/z` (added to ours) and `cl_gunfov`
//   (across at 4:3; our `r_viewmodel_fov` wins when set); its own `cl_gun`, the weapon drawn and
//   its side (`hand` 1 or `cl_gun` 3 draw it mirrored on the left);
// - its own `cl_maxfps`, the moves a second (ours, the frame cap, set aside while it is loaded),
//   and `r_maxfps`, the frame cap, kept with its own settings;
// - the maps' light: `intensity`, `gl_modulate` and `gl_brightness` (Quake's stay 2, 1 and 0);
// - its chat: its own `cl_chat_sound` (one sound by number or name) and `con_auto_chat` (2 says a
//   typed line to the team, where our `con_chat` 2 tells a command by its first word).
void add_q2(con::Console& c, const GameBindings& b) {
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
  // The maps' light, as its own renderer works it out: a wall shows its painted colour times its
  // light / 255 plus gl_brightness, times gl_modulate, times intensity (2: a light of 128 shows the
  // colour as painted).
  if (b.light_intensity != nullptr)
    v.bind({.name = "intensity",
            .fallback = "2",
            .flags = con::kArchive,
            .help = "How bright the walls are lit: their light times this.",
            .min = 1,
            .max = 5,
            .values = "1..5: 2 the default, as the maps were lit; 3 a third brighter again"},
           b.light_intensity);
  if (b.light_modulate != nullptr)
    v.bind({.name = "gl_modulate",
            .fallback = "1",
            .flags = con::kArchive,
            .help = "How strongly the maps' light shows: their light times this.",
            .min = 0,
            .max = 1000000,
            .values = "0 and up: 1 the default, as the maps were lit; 2 twice as bright"},
           b.light_modulate);
  if (b.light_lift != nullptr)
    v.bind({.name = "gl_brightness",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "Light added to every wall, so the darkest corners show.",
            .min = -1,
            .max = 1,
            .values = "-1..1: 0 the default; 0.1 to 0.3 lifts the dark corners"},
           b.light_lift);
  v.add({.name = "cl_timeout",  // Quake II's servers are waited for longer
         .fallback = "120",
         .flags = con::kArchive,
         .help = "How long a joined server may send nothing before you leave it, in seconds.",
         .min = 0,
         .max = 86400,
         .values = "0: never; 1..86400: that many seconds (120 the default)",
         .overlap = con::Overlap::same});
  c.add_command_synonym("seek", "demo_jump");  // its spelling: a time, or + or - and one
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
        std::pair{"fov", "cl_fov"}, std::pair{"ch_x", "cl_crossx"}, std::pair{"ch_y", "cl_crossy"},
        std::pair{"ch_scale", "crosshairsize"}, std::pair{"ch_alpha", "crosshairalpha"},
        std::pair{"cl_vwep", "r_drawvweps"}, std::pair{"con_alpha", "con_bgalpha"}})
    v.add_synonym(name, ours);
  // Its chat sound (0 none, 1 talk, 2 talk1, or a name) over s_chat_custom (chat_lines.hpp), and
  // what a line typed in the console is (0 a command, 1 said, 2 said to the team) over con_chat.
  if (b.chat != nullptr) add_q2_chat_sound(c, *b.chat);
  if (b.auto_chat != nullptr)
    v.bind({.name = "con_auto_chat",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "What a line typed in the console without a slash is.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: a command (the default); 1: said to everyone; 2: said to your team"},
           b.auto_chat);
  // Its own spelling of our `cl_nopred` the other way round: 0 draws the server's place.
  if (b.predict != nullptr)
    v.bind({.name = "cl_predict",
            .fallback = "1",
            .help = "Whether your own moves are predicted on a server joined.",
            .kind = con::CvarKind::boolean,
            .values = "1: predicted, drawn where you will be (the default); 0: drawn where the "
                      "server last put you, a round trip late"},
           b.predict);
  if (b.crouch != nullptr)
    v.bind({.name = "cl_predict_crouch",
            .fallback = "1",
            .help = "Whether the eye's height is predicted with your moves on a server joined, so "
                    "ducking lowers the view at once.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: the view follows the server; 1: on our servers, which say how they move "
                      "you (the default); 2: on every server"},
           b.crouch);
  // Its own `cl_noskins`, beside the shared `noskins` (whose 2 keeps skins but fetches none).
  if (b.noskins != nullptr)
    v.bind({.name = "cl_noskins",
            .fallback = "0",
            .help = "Whether players are drawn in their own figures and skins.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: each in their own (the default); 1: everyone as the stock male; 2: "
                      "women as the stock female, everyone else as the stock male"},
           b.noskins);
  // Its powers' shells and its items' pulse are its own look, on in it (Quake's default
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
  const con::CvarSpec hand{
      .name = "hand",
      .fallback = "0",
      .flags = con::kArchive | con::kUserInfo,
      .help = "The hand you hold your weapon in; the server fires from that side.",
      .kind = con::CvarKind::integer,
      .min = 0,
      .max = 2,
      .values =
          "0: right (the default); 1: left, the weapon drawn on the left; 2: in the middle, "
          "no weapon drawn"};
  if (b.view != nullptr)
    v.bind(hand, &b.view->hand);
  else
    v.add(hand);
  // Its crosshair's own colour: shares of red, green and blue over the picture, or the player's
  // health (white at full, yellow at two thirds, red at a third).
  if (b.crosshair != nullptr) {
    const char* names[3] = {"ch_red", "ch_green", "ch_blue"};
    const char* words[3] = {"red", "green", "blue"};
    for (int k = 0; k < 3; ++k)
      v.bind({.name = names[k],
              .fallback = "1",
              .flags = con::kArchive,
              .help = std::string("How much ") + words[k] + " the crosshair shows.",
              .min = 0,
              .max = 1,
              .values = "0..1: that share (1 the default)"},
             &b.crosshair->shade[k]);
    v.bind({.name = "ch_health",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "Whether the crosshair takes the colour of your health.",
            .kind = con::CvarKind::boolean,
            .values = "0: its own colour (the default); 1: white at full health, yellow at two "
                      "thirds, red at a third and below"},
           &b.crosshair->by_health);
  }
  // Its own `cl_gun`, not our `r_drawviewmodel`: its 2 and 3 force the side, where Quake's 2 hides
  // the weapon at a wide lens. Both have to allow the weapon for it to be drawn.
  const con::CvarSpec gun_drawn{
      .name = "cl_gun",
      .fallback = "1",
      .flags = con::kArchive,
      .help = "Whether the held weapon is drawn, and on which side.",
      .kind = con::CvarKind::integer,
      .min = -1,
      .max = 3,
      .values =
          "1: on the side of your hand (the default); 2: always on the right; 3: always on "
          "the left; 0 or -1: not drawn"};
  if (b.view != nullptr)
    v.bind(gun_drawn, &b.view->gun);
  else
    v.add(gun_drawn);
  if (b.view != nullptr) {  // Quake's place and size for the weapon set aside
    b.view->size = 1;
    b.view->offset_text.clear();
    b.view->last_fired = false;
  }
  // Its own place and lens for the weapon, in its units.
  const char* gun_names[3] = {"cl_gun_x", "cl_gun_y", "cl_gun_z"};
  const char* gun_helps[3] = {"Moves the held weapon right, in game units; added to cl_gunx.",
                              "Moves the held weapon forward, in game units; added to cl_gunz.",
                              "Moves the held weapon up, in game units; added to cl_guny."};
  for (int k = 0; k < 3; ++k) {
    const con::CvarSpec gun{
        .name = gun_names[k],
        .fallback = "0",
        .flags = con::kArchive,
        .help = gun_helps[k],
        .min = -20,
        .max = 20,
        .values = "-20..20: that many units, negative the other way (0 the default)"};
    if (b.view != nullptr)
      v.bind(gun, &b.view->gun_units[k]);
    else
      v.add(gun);
  }
  const con::CvarSpec gun_fov{
      .name = "cl_gunfov",
      .fallback = "90",
      .flags = con::kArchive,
      .help = "The held weapon's own lens, in degrees across; r_viewmodel_fov wins when set.",
      .min = 0,
      .max = 160,
      .values =
          "0: the view's; 30..160: that many degrees across at 4:3 (90 the default), less "
          "held at 30"};
  if (b.view != nullptr)
    v.bind(gun_fov, &b.view->gun_fov_h);
  else
    v.add(gun_fov);
  // Its move rate and its frame cap: its `cl_maxfps` is the moves a second (ours, the frame cap, is
  // set aside while it is loaded and comes back with its value after), its `r_maxfps` the frame
  // cap, starting at the cap in force when the game loads and kept with the game's own settings.
  const con::CvarSpec moves{
      .name = "cl_maxfps",
      .fallback = "62",
      .flags = con::kArchive,
      .help = "How many moves a second you send the server.",
      .kind = con::CvarKind::integer,
      .min = 0,
      .max = 125,
      .values = "62: the default; 10..125: that many, whole milliseconds apart; 0: 125",
      .overlap = con::Overlap::own};
  if (b.moves != nullptr)
    v.bind(moves, b.moves);
  else
    v.add(moves);
  const std::string cap = b.frame_cap != nullptr ? ge::core::num::to_string(*b.frame_cap) : "0";
  const con::CvarSpec frames{
      .name = "r_maxfps",
      .fallback = cap,
      .flags = con::kArchive,
      .help =
          "The most frames a second drawn; kept with this game's own settings (cl_maxfps in "
          "the other games).",
      .min = 0,
      .max = 10000,
      .values =
          "0: no cap; 0..10000: that many (the cap in force when the game loaded, the "
          "default)"};
  if (b.frame_cap != nullptr)
    v.bind(frames, b.frame_cap);
  else
    v.add(frames);
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
constexpr GameRows kGameCvars[] = {{"q1", add_first}, {"q2", add_q2}};

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
  if (b.chat != nullptr) b.chat->named_sound.clear();  // only Quake II's cl_chat_sound sets it
  if (b.auto_chat != nullptr) *b.auto_chat = -1;       // only Quake II's con_auto_chat sets it
  for (const GameRows& row : kGameCvars)
    if (row.game == game.id) row.add(c, b);
  if (b.crosshair != nullptr) {  // crosshairimage's picture read again from this game's files
    if (b.files != nullptr) add_crosshair_reader(*b.crosshair, *b.files);
    b.crosshair->picture_of.clear();
  }
}

int switch_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& now,
                      const GameBindings& b) {
  const int moved = v.layer.empty() ? 0 : c.drop_layer(v.layer);
  const size_t before = c.cvars().size() + c.cvars().synonyms().size();
  add_game_cvars(c, v, now, b);
  return moved + static_cast<int>(c.cvars().size() + c.cvars().synonyms().size() - before);
}

}  // namespace shooters_edge::client
