// ge:summary Shooter's Edge's looks: the switch's options and the palette grade
// ge:keywords look classic modern palette grade quantize r_look r_fx_lights r_fx_glow r_bloom
// ge:keywords r_farland r_farland_warm far-land ce_bubbles_share bubbles ce_water_specks
// ge:keywords r_skyfog fog-command map-fog r_wateralpha r_slimealpha r_lavaalpha r_telealpha
// ge:keywords noskins baseskin allskins player-skins teamskin enemyskin teamcolor enemycolor
// ge:keywords see-through-liquids watervis liquid_alphas
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#include "shooters_edge/app/look.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "brush/map_fog.hpp"
#include "shooters_edge/app/classic_models.hpp"

namespace shooters_edge::look {

namespace ba = ge::brush::app;
namespace gr = ge::render;

void add_cvars(ge::console::Console& con, Picture& picture) {
  namespace gc = ge::console;
  auto& cv = con.cvars();
  cv.bind({.name = "r_look",
           .fallback = "modern",
           .help = "The picture's look.",
           .kind = gc::CvarKind::text,
           .values = "classic: crisp texels, the lightmaps, the palette; modern: filtered, "
                     "real-time lights, bloom (the default)"},
          &picture.look);
  // The modern look's real-time lights, exposure and bloom make a spark or a blast's light at the
  // classic's strength wash a room out, so its defaults turn them down.
  cv.bind({.name = "r_fx_lights",
           .fallback = "0.2",
           .help = "How much light explosions, rockets and shots throw in the modern look.",
           .min = 0,
           .max = 4,
           .values = "0: none; 0..4: that many times the classic's (0.2 the default)"},
          &picture.fx_lights);
  cv.bind({.name = "r_fx_glow",
           .fallback = "0.15",
           .help = "How bright sparks and embers glow in the modern look.",
           .min = 0,
           .max = 4,
           .values = "0: unlit; 0..4: that many times the classic's (0.15 the default)"},
          &picture.fx_glow);
  cv.bind({.name = "r_bloom",
           .fallback = "0.08",
           .help = "The modern look's bloom, read when the picture is built.",
           .min = 0,
           .max = 1,
           .values = "0: none; 0..1: that strength (0.08 the default)"},
          &picture.bloom);
  cv.bind({.name = "r_lava_glow",
           .fallback = "0.02",
           .help = "How much the lava lights the walls round it in the modern look.",
           .min = 0,
           .max = 4,
           .values = "0: none; 0..4: that strength (0.02 the default)"},
          &picture.lava_glow);
  cv.bind({.name = "r_lerpmodels",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "Whether the models' animation is drawn between their frames, so a figure moves "
                   "smoothly.",
           .kind = gc::CvarKind::boolean,
           .values = "0: each frame as the game steps it; 1: smoothly (the default)"},
          &picture.lerp_models);
  cv.bind(
      {.name = "r_lerpmove",
       .fallback = "1",
       .flags = gc::kArchive,
       .help = "Whether the models' places in your own game are drawn between the server's frames.",
       .kind = gc::CvarKind::boolean,
       .values = "0: where each frame puts them; 1: smoothly, a monster's strides over their time "
                 "(the default)"},
      &picture.lerp_move);
  cv.bind({.name = "r_arrival",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "How strongly your own arrival, at a spawn or a teleporter's far end, sweeps "
                   "the view in a flash and a warp, with ce_warps on in the modern look.",
           .min = 0,
           .max = 1,
           .values = "0: none; 0..1: that strength (1 the default)"},
          &picture.arrival);
  cv.bind({.name = "ce_bubbles_share",
           .fallback = "0.3",
           .flags = gc::kArchive,
           .help = "The share of the bubbles rising under water that is drawn, in either look.",
           .min = 0,
           .max = 1,
           .values = "0: none; 0..1: that share, 1 every one the game blows (0.3 the default)"},
          &picture.bubbles_share);
  cv.bind({.name = "ce_water_specks",
           .fallback = "0",
           .flags = gc::kArchive,
           .help = "How many specks float in the water round you while you swim under it in the "
                   "modern look.",
           .min = 0,
           .max = 1,
           .values = "0: none (the default); 0..1: that share of the water's own"},
          &picture.water_specks);
  cv.bind({.name = "r_slime_glow",
           .fallback = "1",
           .help = "How much the slime's green glow lights the walls round it in the modern look.",
           .min = 0,
           .max = 4,
           .values = "0: none; 0..4: that strength (1 the default)"},
          &picture.slime_glow);
  cv.bind({.name = "r_glow",
           .fallback = "0.6",
           .flags = gc::kArchive,
           .help = "How bright the glowing parts of the walls, windows and lamps' panels, are in "
                   "the modern look.",
           .min = 0,
           .max = 2,
           .values = "0: unlit; 0..2: that many times as the classic draws them (0.6 the default)"},
          &picture.glow);
  cv.bind(
      {.name = "r_fire_glow",
       .fallback = "1",
       .flags = gc::kArchive,
       .help = "How bright the torches', braziers' and bonfires' flames burn in the modern look.",
       .min = 0,
       .max = 4,
       .values = "0: unlit; 0..4: that many times as made (1 the default)"},
      &picture.fire_glow);
  cv.bind(
      {.name = "r_fill_shadows",
       .fallback = "0",
       .flags = gc::kArchive,
       .help = "Whether every map light throws a figure's shadow, the unseen fill lights too.",
       .min = 0,
       .max = 1,
       .values =
           "0: only lights whose source shows, flames, globes and lamps at their glowing fittings "
           "(the default); 1: every light, and a figure darkens the light bounced round him"},
      &picture.fill_shadows);
  cv.bind({.name = "r_fire_speed",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "How fast the torches', braziers' and bonfires' flames puff, churn and flicker "
                   "in the modern look.",
           .min = 0,
           .max = 2,
           .values = "0: frozen; 0..2: that many times a real fire's of their size, a torch "
                     "puffing about three times a second (1 the default)"},
          &picture.fire_pace);
  cv.bind(
      {.name = "r_water_speed",
       .fallback = "0.7",
       .flags = gc::kArchive,
       .help = "How fast the pools' water moves, its swell, chop and ripples, in the modern look.",
       .min = 0,
       .max = 2,
       .values = "0: still; 0..2: that many times a real water's (0.7 the default)"},
      &picture.water_pace);
  cv.bind({.name = "r_farland",
           .fallback = "auto",
           .help = "The land round the map seen over its walls from high up.",
           .kind = gc::CvarKind::text,
           .values = "auto: one to suit the map's sky (the default); off: none; alps: alpine "
                     "peaks; fjord: a fjord; highlands: highlands; winter: a winter land; "
                     "dolomites: dolomites; sierra: a sierra; volcano: volcanoes; arctic: arctic "
                     "ice; xeno: an alien land; ember: an ember land; rime: a rime land"},
          &picture.far_land);
  cv.bind({.name = "r_skyname",
           .fallback = "",
           .flags = gc::kArchive,
           .help = "A sky box drawn through every map's sky in place of the map's own.",
           .kind = gc::CvarKind::text,
           .values = "\"\": each map's own sky (the default); TEXT: the sky box of that name, "
                     "its six pictures under env/ or gfx/env/"},
          &picture.sky_name);
  cv.bind({.name = "r_loadlit",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "Coloured light from a map's .lit file, fetched from a server when you lack it.",
           .kind = gc::CvarKind::boolean,
           .values = "1: each map in its coloured light where it has one (the default); 0: every "
                     "map in its own grey light, and none fetched"},
          &picture.load_lit);
  cv.bind({.name = "r_drawflat",
           .fallback = "0",
           .flags = gc::kArchive,
           .help = "Walls and floors drawn in one colour each instead of their textures.",
           .kind = gc::CvarKind::integer,
           .min = 0,
           .max = 3,
           .values = "0: textured (the default); 1: walls and floors flat; 2: the walls alone; "
                     "3: the floors alone"},
          &picture.draw_flat);
  const struct {
    const char* name;
    const char* help;
    const char* fallback;
    std::string* value;
  } flats[] = {
      {"r_wallcolor", "The walls' colour when r_drawflat is on.", "150 140 125",
       &picture.wall_colour},
      {"r_floorcolor", "The floors' colour when r_drawflat is on.", "85 95 110",
       &picture.floor_colour},
      {"r_skycolor", "The sky's colour when r_fastsky is on.", "70 90 120", &picture.sky_colour},
  };
  for (const auto& f : flats)
    cv.bind({.name = f.name,
             .fallback = f.fallback,
             .flags = gc::kArchive,
             .help = f.help,
             .kind = gc::CvarKind::text,
             .values = "TEXT: red, green and blue, each 0 to 255 (one number: a grey)"},
            f.value);
  cv.bind({.name = "r_dynamic",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "Whether flashes, rockets and explosions light the walls and the models.",
           .kind = gc::CvarKind::boolean,
           .values = "1: they do (the default); 0: none do"},
          &picture.dynamic);
  cv.bind({.name = "r_fullbrightskins",
           .fallback = "0",
           .flags = gc::kArchive,
           .help = "How brightly players are lit wherever they stand.",
           .min = 0,
           .max = 1,
           .values = "0: lit as the floor under them (the default); 0..1: lifted that far toward "
                     "full light; 1: fully lit, unshaded"},
          &picture.fullbright_skins);
  const struct {
    const char* name;
    const char* help;
    double* value;
  } tints[] = {
      {"v_contentblend", "How strongly water, slime or lava round your eyes tints the view.",
       &picture.content_blend},
      {"v_damagecshift", "How strongly a hit flashes the view red.", &picture.damage_blend},
      {"v_quadcshift", "How strongly quad damage tints the view.", &picture.quad_blend},
      {"v_pentcshift", "How strongly invulnerability tints the view.", &picture.pent_blend},
      {"v_suitcshift", "How strongly the environment suit tints the view.", &picture.suit_blend},
      {"v_ringcshift", "How strongly invisibility tints the view.", &picture.ring_blend},
  };
  for (const auto& t : tints)
    cv.bind({.name = t.name,
             .fallback = "1",
             .flags = gc::kArchive,
             .help = t.help,
             .min = 0,
             .max = 1,
             .values = "0: none; 0..1: that share of it; 1: all of it (the default)"},
            t.value);
  cv.bind({.name = "gl_cshiftpercent",
           .fallback = "100",
           .flags = gc::kArchive,
           .help = "How strongly every tint of the view shows, in percent.",
           .min = 0,
           .max = 100,
           .values = "0: none; 0..100: that percent of each; 100: all of it (the default)"},
          &picture.cshift_percent);
  cv.bind({.name = "gl_polyblend",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "Whether the view is tinted at all, by water, hits or powers.",
           .kind = gc::CvarKind::boolean,
           .values = "1: it is (the default); 0: never"},
          &picture.poly_blend);
  cv.bind({.name = "r_lightmap",
           .fallback = "0",
           .flags = gc::kArchive,
           .help = "Every wall and floor drawn plain grey, so only the light on it shows.",
           .kind = gc::CvarKind::boolean,
           .values = "0: textured (the default); 1: plain grey under its light"},
          &picture.lightmap_only);
  cv.bind({.name = "r_fastsky",
           .fallback = "0",
           .flags = gc::kArchive,
           .help = "The sky drawn in one plain colour, r_skycolor.",
           .kind = gc::CvarKind::boolean,
           .values = "0: the map's sky (the default); 1: one plain colour"},
          &picture.fast_sky);
  cv.bind({.name = "r_skyfog",
           .fallback = "0.5",
           .flags = gc::kArchive,
           .help = "How much of a map's fog is laid over its sky.",
           .min = 0,
           .max = 1,
           .values = "0..1: that share of the fog's colour over the sky (0.5 the default), where "
                     "the map does not set its own"},
          &picture.sky_fog);
  cv.bind({.name = "r_wateralpha",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "How solid water looks: below 1 you see through it.",
           .min = 0,
           .max = 1,
           .values = "0..1: 1 solid (the default), 0.5 half see-through, 0 not drawn (a server "
                     "can keep water solid, and a map not made to be seen through shows less)"},
          &picture.water_alpha);
  const struct {
    const char* name;
    const char* help;
    std::string* value;
  } kinds[] = {
      {"r_slimealpha", "How solid slime looks.", &picture.slime_alpha},
      {"r_lavaalpha", "How solid lava looks.", &picture.lava_alpha},
      {"r_telealpha", "How solid a teleporter's face looks.", &picture.tele_alpha},
  };
  for (const auto& k : kinds)
    cv.bind({.name = k.name,
             .fallback = "",
             .flags = gc::kArchive,
             .help = k.help,
             .kind = gc::CvarKind::text,
             .values = "\"\": as r_wateralpha (the default); 0..1: 1 solid, 0 not drawn"},
            k.value);
  cv.bind({.name = "noskins",
           .fallback = "0",
           .flags = gc::kArchive,
           .help = "Whether players are drawn in their own skins.",
           .kind = gc::CvarKind::integer,
           .min = 0,
           .max = 2,
           .values = "0: each in their own skin (the default); 1: everyone in the plain skin, in "
                     "their colours; 2: their own skins, but none fetched from a server"},
          &picture.noskins);
  cv.bind({.name = "baseskin",
           .fallback = "base",
           .flags = gc::kArchive,
           .help = "The skin of a player who names none, or one you do not have.",
           .kind = gc::CvarKind::text,
           .values = "TEXT: the skin of that name (base, the default); \"\": the plain skin"},
          &picture.base_skin);
  con.add_command(
      "allskins", "allskins [skin]: one skin on every player; with none, each their own",
      [&picture](gc::Console& k, const gc::Args& a) {
        picture.all_skins = a.size() < 2 ? std::string{} : a[1];
        k.print(picture.all_skins.empty() ? "Every player wears their own skin."
                                          : "Every player wears " + picture.all_skins + ".");
      });
  cv.bind({.name = "teamskin",
           .fallback = "",
           .flags = gc::kArchive,
           .help = "The skin every teammate wears.",
           .kind = gc::CvarKind::text,
           .values = "TEXT: that skin on every teammate; \"\": each their own (the default)"},
          &picture.team_skin);
  cv.bind({.name = "enemyskin",
           .fallback = "",
           .flags = gc::kArchive,
           .help = "The skin every enemy wears.",
           .kind = gc::CvarKind::text,
           .values = "TEXT: that skin on every enemy; \"\": each their own (the default)"},
          &picture.enemy_skin);
  const auto colours = [&con](const char* name, const char* help, const char* who, int* top,
                              int* bottom) {
    con.add_command(name, help, [name, top, bottom, who](gc::Console& k, const gc::Args& a) {
      if (a.size() < 2) {
        k.print(*top < 0 ? std::string("Every ") + who + " wears their own colours."
                         : std::string("Every ") + who + " wears colours " + std::to_string(*top) +
                               " and " + std::to_string(*bottom) + ".");
        return;
      }
      if (a.size() > 3 ||
          !models::parse_forced_colours(a[1], a.size() > 2 ? a[2] : "", top, bottom)) {
        k.print(std::string(name) + " takes one or two colours from 0 to 13, or off.");
        return;
      }
      k.print(*top < 0 ? std::string("Every ") + who + " wears their own colours."
                       : std::string("Every ") + who + " wears colours " + std::to_string(*top) +
                             " and " + std::to_string(*bottom) + ".");
    });
  };
  colours("teamcolor", "teamcolor [off | colour | top bottom]: every teammate's colours",
          "teammate", &picture.team_top, &picture.team_bottom);
  colours("enemycolor", "enemycolor [off | colour | top bottom]: every enemy's colours", "enemy",
          &picture.enemy_top, &picture.enemy_bottom);
  con.add_command(
      "fog",
      "fog <density> <red> <green> <blue> [alpha] [bias]: the fog in the air, until the next map",
      [&picture](gc::Console& k, const gc::Args& a) {
        if (a.size() < 2) {
          k.print(picture.fog_serial == 0 ? "The fog is the map's own."
                                          : "The fog was last set to " + picture.fog + ".");
          return;
        }
        ge::brush::MapFog f;
        if (!ge::brush::parse_fog(a.rest(0), &f)) {
          k.print("fog: fog <density> <red> <green> <blue> [alpha] [bias], each a number");
          return;
        }
        picture.fog = ge::brush::fog_words(f);
        ++picture.fog_serial;
      });
  cv.bind({.name = "r_farland_warm",
           .fallback = "0",
           .help = "How much warmer the far land's air is than its own, in degrees C.",
           .min = -40,
           .max = 40,
           .values = "-40..40: that many degrees, below 0 colder, the snow coming down the "
                     "mountains and the lakes freezing (0 the default)"},
          &picture.far_warm);
  cv.bind({.name = "gamma",
           .fallback = "1",
           .flags = gc::kArchive,
           .help = "The picture's brightness, lower brighter.",
           .min = 0.3,
           .max = 1.5,
           .values = "0.3..1.5: that gamma, 1 as drawn and 0.5 the brightest the menu offers (1 "
                     "the default)"},
          &picture.gamma);
  cv.bind({.name = "r_quality",
           .fallback = "custom",
           .flags = gc::kArchive,
           .help = "The shadows, the water and the skies set together.",
           .kind = gc::CvarKind::text,
           .values = "custom: each ce_ switch and its quality as set (the default); low: the "
                     "cheapest; medium: between; high: the best but the top; top: everything; "
                     "auto: the most inside r_budget; 0..1: cheapest to the top, by measured cost"},
          &picture.quality);
  cv.bind({.name = "r_budget",
           .fallback = "8.3",
           .flags = gc::kArchive,
           .help = "The GPU time a frame may take with r_quality auto, in ms.",
           .min = 1,
           .max = 100,
           .values = "1..100: that many ms (8.3 the default, for 120 Hz)"},
          &picture.budget_ms);
  ge::classicenhance::bind_switches(cv, &picture.ce);
  // Each dear switch's level when it is on (Stops), beside its ce_ switch.
  const auto level = [&](const char* name, const char* help, const char* values,
                         std::string* field) {
    cv.bind({.name = name,
             .fallback = "high",
             .flags = gc::kArchive,
             .help = help,
             .kind = gc::CvarKind::text,
             .values = values},
            field);
  };
  level("ce_shadows_quality", "The live lights' shadows with ce_shadows on.",
        "low: the walls' alone, no figure's; medium: the figures' too, hard-edged; high: soft by "
        "each flame's size, from every light (the default)",
        &picture.shadows_quality);
  level("ce_water_quality", "The water with ce_water on.",
        "low: a still surface lit by the room, no reflection; medium: its chop and the room "
        "reflected off the screen; high: the room mirrored whole, the sun's caustics and shafts "
        "(the default)",
        &picture.water_quality);
  level("ce_skies_quality", "The sky with ce_skies on.",
        "low: our sky with its mean cloud, no cloud layer; medium: the cloud layer too; high: and "
        "the far land round the map seen from high up (the default)",
        &picture.skies_quality);
  ge::brush::app::add_weather_cvars(cv, picture.weather);
}

Look look_of(const Picture& picture) {
  Look look = Look::modern;
  parse_look(picture.look, &look);
  return look;
}

bool parse_look(std::string_view word, Look* out) {
  if (word == "classic") {
    *out = Look::classic;
    return true;
  }
  if (word == "modern") {
    *out = Look::modern;
    return true;
  }
  return false;
}

const char* look_name(Look look) { return look == Look::classic ? "classic" : "modern"; }

bool parse_level(std::string_view word, Stop* out) {
  if (word == "low")
    *out = Stop::low;
  else if (word == "medium")
    *out = Stop::medium;
  else if (word == "high")
    *out = Stop::high;
  else
    return false;
  return true;
}

const char* stop_name(Stop stop) {
  constexpr const char* kNames[] = {"off", "low", "medium", "high"};
  return kNames[static_cast<size_t>(stop)];
}

Stops custom_stops(const Picture& picture) {
  const auto stop = [](bool on, const std::string& level) {
    Stop s = Stop::high;
    if (!on) return Stop::off;
    parse_level(level, &s);
    return s;
  };
  return {stop(picture.ce.shadows, picture.shadows_quality),
          stop(picture.ce.water, picture.water_quality),
          stop(picture.ce.skies, picture.skies_quality)};
}

namespace {

// THE SHADOWS' STOPS on the renderer: the lights whose cubes the moving casters draw into, their
// edges and the capsules. Low keeps the walls' kept cubes alone (no figure casts); medium the
// figures' from the kCasterLights lights that light them most, hard-edged (one compare: their
// soft edges are nearly all of what the casters cost), with no capsules for the other lights; high
// soft by each light's size, and the capsules.
constexpr int kCasterLightsAt[] = {0, 0, ba::kCasterLights, ba::kCasterLights};
constexpr bool kSoftAt[] = {false, false, false, true};
constexpr bool kCapsulesAt[] = {false, false, false, true};

// The map's lights softened near them over this share of their half reach (bsp_draw.hpp's
// Options::near_soft): a torch's wall a few times its half-reach light, never a white disc.
constexpr double kNearSoft = 0.4;

}  // namespace

namespace {

// A liquid cvar's text as a number, or `fallback` when it is empty or not one.
double alpha_text(const std::string& text, double fallback) {
  double v = 0;
  return !text.empty() && ge::console::parse_number(text, v) ? v : fallback;
}

}  // namespace

void liquid_alphas(const Picture& picture, const double map[4], bool allowed, double out[4]) {
  const double water = map[0] >= 0 ? map[0] : picture.water_alpha;
  out[0] = water;
  out[1] = map[1] >= 0 ? map[1] : alpha_text(picture.slime_alpha, water);
  out[2] = map[2] >= 0 ? map[2] : alpha_text(picture.lava_alpha, water);
  out[3] = map[3] >= 0 ? map[3] : alpha_text(picture.tele_alpha, water);
  for (int i = 0; i < 4; ++i) out[i] = allowed ? std::clamp(out[i], 0.0, 1.0) : 1.0;
}

void map_liquid_alphas(const ge::brush::Map& map, double out[4]) {
  const char* keys[4] = {"wateralpha", "slimealpha", "lavaalpha", "telealpha"};
  for (int i = 0; i < 4; ++i) {
    out[i] = -1;
    if (map.entities.empty()) continue;
    const ge::brush::Entity& world = map.entities[0];
    std::string text = world.get(keys[i]);
    if (text.empty()) text = world.get(std::string("_") + keys[i]);
    double v = 0;
    if (!text.empty() && ge::console::parse_number(text, v)) out[i] = std::max(v, 0.0);
  }
}

bool watervis_allowed(const std::vector<std::pair<std::string, std::string>>& serverinfo) {
  for (const auto& [key, value] : serverinfo) {
    if (key != "watervis") continue;
    double v = 0;
    return !ge::console::parse_number(value, v) || v > 0;
  }
  return true;
}

void toned_blend(const Picture& picture, const effects::Screen& s, double rgba[4]) {
  const effects::BlendShares k{.content = picture.content_blend,
                               .damage = picture.damage_blend,
                               .quad = picture.quad_blend,
                               .pent = picture.pent_blend,
                               .suit = picture.suit_blend,
                               .ring = picture.ring_blend};
  effects::screen_blend(s, rgba, k);
  rgba[3] *= picture.poly_blend ? std::clamp(picture.cshift_percent, 0.0, 100.0) / 100.0 : 0;
}

bool rgb_words(std::string_view text, double out[3]) {
  double v[3];
  size_t n = 0, i = 0;
  while (i < text.size()) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i == text.size()) break;
    size_t end = i;
    while (end < text.size() && text[end] != ' ' && text[end] != '\t') ++end;
    if (n == 3) return false;
    const std::string word(text.substr(i, end - i));
    char* stop = nullptr;
    const double x = std::strtod(word.c_str(), &stop);
    if (stop == word.c_str() || *stop != '\0' || !(x >= 0)) return false;
    v[n++] = std::min(x, 255.0) / 255.0;
    i = end;
  }
  if (n == 1)
    v[1] = v[2] = v[0];
  else if (n != 3)
    return false;
  for (size_t k = 0; k < 3; ++k) out[k] = v[k];
  return true;
}

ba::Options world_options(Look look, ba::Options base, const Picture& picture) {
  return world_options(look, base, picture, custom_stops(picture));
}

ba::Options world_options(Look look, ba::Options base, const Picture& picture, const Stops& stops) {
  // r_drawflat and r_fastsky, either look.
  base.flat_walls = picture.draw_flat == 1 || picture.draw_flat == 2;
  base.flat_floors = picture.draw_flat == 1 || picture.draw_flat == 3;
  base.fast_sky = picture.fast_sky;
  rgb_words(picture.wall_colour, base.wall_colour);
  rgb_words(picture.floor_colour, base.floor_colour);
  rgb_words(picture.sky_colour, base.sky_colour);
  if (picture.lightmap_only) {  // r_lightmap: grey faces, the light alone
    base.flat_walls = base.flat_floors = true;
    for (int k = 0; k < 3; ++k) base.wall_colour[k] = base.floor_colour[k] = kLightmapGrey;
  }
  if (look == Look::classic) {
    base.mode = ba::Mode::classic;
    if (!picture.dynamic) base.dynamic_strength = 0;  // r_dynamic 0: no moment's light
    base.lighting = ba::Lighting::lightmaps;
    base.crisp = true;
    base.weather = false;
  } else {
    base.mode = ba::Mode::v2;
    base.lighting = ba::Lighting::lights;
    base.crisp = false;
    base.dynamic_strength = picture.dynamic ? picture.fx_lights : 0;
    base.lava_glow = picture.lava_glow;
    base.slime_glow = picture.slime_glow;
    base.fire_glow = picture.fire_glow;
    base.fire_pace = picture.fire_pace;
    base.water_pace = picture.water_pace;
    base.water_specks = picture.water_specks;
    base.fill_shadows = picture.fill_shadows > 0.5;
    base.glow = picture.glow;
    base.near_soft = kNearSoft;
    base.pack_sky = stops.skies == Stop::off;  // ce_skies off: the pack's own scrolling sky
    const auto at = static_cast<size_t>(stops.shadows);
    if (stops.shadows == Stop::off) base.shadowed = 0;  // ce_shadows off: none cast
    base.caster_lights = kCasterLightsAt[at];
    base.soft_casters = kSoftAt[at];
    base.capsules = kCapsulesAt[at];
    // ce_relief and ce_parallax: the enhanced textures' normal, roughness and height maps.
    base.relief = picture.ce.relief;
    if (!picture.ce.parallax) base.parallax = 0;
    // ce_rebake: the light bounced round the rooms from the cache's rebake, under the live lights.
    base.rebake_bounce = picture.ce.rebake;
  }
  return base;
}

double effects_glow(Look look, const Picture& picture) {
  return look == Look::classic ? 1 : picture.fx_glow;
}

gr::PostSettings post_settings(Look look, const Picture& picture) {
  gr::PostSettings s;
  s.bloom = look == Look::classic ? 0 : picture.bloom;
  return s;
}

std::vector<float> palette_grade(std::span<const uint8_t> palette) {
  std::vector<float> table = gr::identity_grade();
  const size_t colours = palette.size() / 3;
  if (colours == 0) return table;
  for (size_t e = 0; e + 2 < table.size(); e += 3) {
    double best = 1e30;
    size_t pick = 0;
    for (size_t c = 0; c < colours; ++c) {
      double d = 0;
      for (size_t k = 0; k < 3; ++k) {
        const double x = double(table[e + k]) - double(palette[c * 3 + k]) / 255.0;
        d += x * x;
      }
      if (d < best) best = d, pick = c;
    }
    for (size_t k = 0; k < 3; ++k) table[e + k] = float(palette[pick * 3 + k]) / 255.0f;
  }
  return table;
}

std::vector<float> gamma_grade(std::vector<float> table, double gamma) {
  if (gamma == 1) return table;
  for (float& v : table) v = static_cast<float>(std::pow(std::fmax(0.0, double(v)), gamma));
  return table;
}

}  // namespace shooters_edge::look
