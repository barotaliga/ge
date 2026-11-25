// ge:summary A shooter's looks: the picture's cvars with a game's defaults, the switch's options
// and the palette grade
// ge:keywords look classic modern palette grade quantize r_look r_fx_lights r_fx_glow r_bloom
// ge:keywords r_farland r_farland_warm far-land ce_bubbles_share bubbles ce_water_specks
// ge:keywords gl_damageblend_frac damage-frame
// ge:keywords r_skyfog fog-command map-fog r_wateralpha r_slimealpha r_lavaalpha r_telealpha
// ge:keywords noskins baseskin allskins player-skins teamskin enemyskin teamcolor enemycolor
// ge:keywords see-through-liquids watervis liquid_alphas cl_muzzleflash muzzle-flash r_drawflame
// ge:keywords r_particles scr_centertime centre-message r_powershells r_itempulse power-shells
// ge:skill classic shooter render
// ge:test tests/classic_look_test.cpp
#include "classic/app/look.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

#include "brush/map_fog.hpp"
#include "console/words.hpp"
#include "core/libm/libm.hpp"

namespace ge::classic::look {

namespace ba = ge::brush::app;
namespace gr = ge::render;
namespace gc = ge::console;

namespace {

std::string default_text(double v) { return gc::number_text(v); }
std::string default_text(int32_t v) { return std::to_string(v); }
std::string default_text(bool v) { return v ? "1" : "0"; }
std::string default_text(const std::string& v) { return v; }

// Binds `spec` over `field` with the value `field` holds now as its default, so each game that
// sets its own Picture before registering keeps its own (the values' "(the default)" marks
// following, console/words.hpp's `values_for_default`).
template <class T>
void bind(gc::Cvars& cv, gc::CvarSpec spec, T* field) {
  const std::string fallback = default_text(*field);  // the registry copies both
  const std::string values = gc::values_for_default(spec.values, fallback);
  spec.fallback = fallback;
  spec.values = values;
  cv.bind(spec, field);
}

}  // namespace

void add_cvars(ge::console::Console& con, Picture& picture) {
  auto& cv = con.cvars();
  bind(cv,
       {.name = "r_look",
        .help = "The picture's look.",
        .kind = gc::CvarKind::text,
        .values = "classic: crisp texels, the lightmaps, the palette; modern: filtered, "
                  "real-time lights, bloom (the default)"},
       &picture.look);
  // The modern look's real-time lights, exposure and bloom make a spark or a blast's light at the
  // classic's strength wash a room out, so its defaults turn them down.
  bind(cv,
       {.name = "r_fx_lights",
        .help = "How much light explosions, rockets and shots throw in the modern look.",
        .min = 0,
        .max = 4,
        .values = "0: none; 0..4: that many times the classic's (0.2 the default)"},
       &picture.fx_lights);
  bind(cv,
       {.name = "scr_centertime",
        .flags = gc::kArchive,
        .help = "How long a message in the middle of the screen stays, in seconds.",
        .min = 0,
        .max = 60,
        .values = "0..60: the seconds it stays (2 the default)"},
       &picture.centre_time);
  bind(cv,
       {.name = "r_fx_glow",
        .help = "How bright sparks and embers glow in the modern look.",
        .min = 0,
        .max = 4,
        .values = "0: unlit; 0..4: that many times the classic's (0.15 the default)"},
       &picture.fx_glow);
  // Every game's (docs/porting-games.md § One name, one meaning): the lightmaps' scale (Quake II's
  // gl_modulate and QuakeWorld's, 1 in both) and the textures' slant taps (QuakeWorld's and Quake
  // II's gl_anisotropy; Quake II's layer gives its 1).
  bind(cv,
       {.name = "gl_modulate",
        .flags = gc::kArchive,
        .help = "How strongly the maps' light shows: their light times this.",
        .min = 0,
        .max = 1000000,
        .values = "1: as the maps were lit; 2: twice as bright"},
       &picture.light_modulate);
  bind(cv,
       {.name = "gl_anisotropy",
        .flags = gc::kArchive,
        .help = "How sharp the textures stay on surfaces seen at a slant, when they are smoothed.",
        .min = 0,
        .max = 16,
        .values = "0: the renderer's own; 1: off; 2..16: that many samples, sharper"},
       &picture.anisotropy);
  bind(cv,
       {.name = "r_bloom",
        .help = "The modern look's bloom, read when the picture is built.",
        .min = 0,
        .max = 1,
        .values = "0: none; 0..1: that strength (0.08 the default)"},
       &picture.bloom);
  bind(cv,
       {.name = "r_lava_glow",
        .help = "How much the lava lights the walls round it in the modern look.",
        .min = 0,
        .max = 4,
        .values = "0: none; 0..4: that strength (0.02 the default)"},
       &picture.lava_glow);
  bind(cv,
       {.name = "r_lerpmodels",
        .flags = gc::kArchive,
        .help = "Whether the models' animation is drawn between their frames, so a figure moves "
                "smoothly.",
        .kind = gc::CvarKind::boolean,
        .values = "0: each frame as the game steps it; 1: smoothly (the default)"},
       &picture.lerp_models);
  bind(
      cv,
      {.name = "r_lerpmove",
       .flags = gc::kArchive,
       .help = "Whether the models' places in your own game are drawn between the server's frames.",
       .kind = gc::CvarKind::boolean,
       .values = "0: where each frame puts them; 1: smoothly, a monster's strides over their time "
                 "(the default)"},
      &picture.lerp_move);
  bind(cv,
       {.name = "r_arrival",
        .flags = gc::kArchive,
        .help = "How strongly your own arrival, at a spawn or a teleporter's far end, tints "
                "and streaks the edges of the view for a moment, with ce_warps on in the "
                "modern look.",
        .min = 0,
        .max = 1,
        .values = "0: none; 0..1: that strength (1 the default)"},
       &picture.arrival);
  bind(cv,
       {.name = "ce_bubbles_share",
        .flags = gc::kArchive,
        .help = "The share of the bubbles rising under water that is drawn, in either look.",
        .min = 0,
        .max = 1,
        .values = "0: none; 0..1: that share, 1 every one the game blows (0.3 the default)"},
       &picture.bubbles_share);
  bind(cv,
       {.name = "ce_blood_time",
        .flags = gc::kArchive,
        .help = "How long blood stays on the walls and the floors with ce_blood on, darkening "
                "as it dries.",
        .min = 0,
        .max = 3600,
        .values = "0: until newer marks need its place; 1..3600: that many seconds, drying in "
                  "the first half and fading at the end (120 the default)"},
       &picture.blood_time);
  bind(cv,
       {.name = "ce_blood_reflect",
        .flags = gc::kArchive,
        .help = "How strongly wet pools of blood mirror the room, with ce_blood and "
                "ce_reflections on.",
        .min = 0,
        .max = 2,
        .values = "0: no reflections; 0..2: that share of what the wet blood's gloss and the "
                  "angle give, a dried pool mirroring nothing at any strength (1 the default)"},
       &picture.blood_reflect);
  bind(cv,
       {.name = "ce_ao_radius",
        .flags = gc::kArchive,
        .help = "How far a wall, a step or a figure still darkens the surface beside it, with "
                "ce_ao on.",
        .min = 8,
        .max = 256,
        .values = "8..256: that many units (48 the default, about a crate's width)"},
       &picture.ao_radius);
  bind(cv,
       {.name = "ce_ao_strength",
        .flags = gc::kArchive,
        .help = "How dark the closed corners and the floor under figures grow, with ce_ao on.",
        .min = 0,
        .max = 1,
        .values = "0: no darkening; 0..1: that share of the light a closed corner loses (0.8 the "
                  "default)"},
       &picture.ao_strength);
  bind(cv,
       {.name = "ce_blood_amount",
        .flags = gc::kArchive,
        .help = "How much bodies bleed with ce_blood on: the spray, the splashes and pools, the "
                "stains on the body and its drips.",
        .min = 0,
        .max = 3,
        .values = "0: no blood; 0..3: that much of it, more drops, bigger splashes, pools and "
                  "stains (1 the default)"},
       &picture.blood_amount);
  bind(cv,
       {.name = "r_particle_scale",
        .flags = gc::kArchive,
        .help = "How large the specks the effects throw are: blood, sparks, where a shot or a "
                "nail hits, trails.",
        .min = 0.25,
        .max = 8,
        .values = "1: their own size (the default); 0.25..8: that many times it (2 or 3 shows "
                  "plainly where a shot lands)"},
       &picture.particle_scale);
  bind(cv,
       {.name = "ce_water_specks",
        .flags = gc::kArchive,
        .help = "How many specks float in the water round you while you swim under it in the "
                "modern look.",
        .min = 0,
        .max = 1,
        .values = "0: none (the default); 0..1: that share of the water's own"},
       &picture.water_specks);
  bind(cv,
       {.name = "r_slime_glow",
        .help = "How much the slime's green glow lights the walls round it in the modern look.",
        .min = 0,
        .max = 4,
        .values = "0: none; 0..4: that strength (1 the default)"},
       &picture.slime_glow);
  bind(cv,
       {.name = "r_glow",
        .flags = gc::kArchive,
        .help = "How bright the glowing parts of the walls, windows and lamps' panels, are in "
                "the modern look.",
        .min = 0,
        .max = 2,
        .values = "0: unlit; 0..2: that many times as the classic draws them (0.6 the default)"},
       &picture.glow);
  bind(cv,
       {.name = "r_fire_glow",
        .flags = gc::kArchive,
        .help = "How bright the torches', braziers' and bonfires' flames burn in the modern look.",
        .min = 0,
        .max = 4,
        .values = "0: unlit; 0..4: that many times as made (1 the default)"},
       &picture.fire_glow);
  bind(cv,
       {.name = "r_fill_shadows",
        .flags = gc::kArchive,
        .help = "Whether every map light throws a figure's shadow, the unseen fill lights too.",
        .min = 0,
        .max = 1,
        .values =
            "0: only lights whose source shows, flames, globes and lamps at their glowing fittings "
            "(the default); 1: every light, and a figure darkens the light bounced round him"},
       &picture.fill_shadows);
  bind(cv,
       {.name = "r_reflect",
        .flags = gc::kArchive,
        .help = "How strongly shiny floors mirror the room, with ce_reflections on.",
        .min = 0,
        .max = 2,
        .values = "0: no reflections; 0..2: that share of what the floor's gloss and the angle "
                  "give, rough stone and metal mirroring nothing at any strength (0.1 the "
                  "default, a hint of the room)"},
       &picture.reflect);
  bind(cv,
       {.name = "r_fire_speed",
        .flags = gc::kArchive,
        .help = "How fast the torches', braziers' and bonfires' flames puff, churn and flicker "
                "in the modern look.",
        .min = 0,
        .max = 2,
        .values = "0: frozen; 0..2: that many times a real fire's of their size, a torch "
                  "puffing about three times a second (1 the default)"},
       &picture.fire_pace);
  bind(cv,
       {.name = "r_water_speed",
        .flags = gc::kArchive,
        .help = "How fast the pools' water moves, its swell, chop and ripples, in the modern look.",
        .min = 0,
        .max = 2,
        .values = "0: still; 0..2: that many times a real water's (0.7 the default)"},
       &picture.water_pace);
  bind(cv,
       {.name = "r_farland",
        .help = "The land round the map seen over its walls from high up.",
        .kind = gc::CvarKind::text,
        .values = "auto: one to suit the map's sky (the default); off: none; alps: alpine "
                  "peaks; fjord: a fjord; highlands: highlands; winter: a winter land; "
                  "dolomites: dolomites; sierra: a sierra; volcano: volcanoes; arctic: arctic "
                  "ice; xeno: an alien land; ember: an ember land; rime: a rime land"},
       &picture.far_land);
  bind(cv,
       {.name = "r_skyname",
        .flags = gc::kArchive,
        .help = "A sky box drawn through every map's sky in place of the map's own.",
        .kind = gc::CvarKind::text,
        .values = "\"\": each map's own sky (the default); TEXT: the sky box of that name, "
                  "its six pictures under env/ or gfx/env/"},
       &picture.sky_name);
  bind(cv,
       {.name = "gl_texturemode",
        .flags = gc::kArchive,
        .help = "How the walls' textures are smoothed.",
        .kind = gc::CvarKind::text,
        .values = "\"\": as the look draws them (the default); TEXT: GL_NEAREST and the other "
                  "nearest modes for crisp square texels, GL_LINEAR_MIPMAP_LINEAR and the other "
                  "linear modes for smooth ones"},
       &picture.texture_mode);
  bind(cv,
       {.name = "r_loadlit",
        .flags = gc::kArchive,
        .help = "Coloured light from a map's .lit file, fetched from a server when you lack it.",
        .kind = gc::CvarKind::boolean,
        .values = "1: each map in its coloured light where it has one (the default); 0: every "
                  "map in its own grey light, and none fetched"},
       &picture.load_lit);
  bind(cv,
       {.name = "r_drawflat",
        .flags = gc::kArchive,
        .help = "Walls and floors drawn in one colour each instead of their textures.",
        .kind = gc::CvarKind::integer,
        .min = 0,
        .max = 3,
        .values = "0: textured (the default); 1: walls and floors flat; 2: the walls alone; "
                  "3: the floors alone"},
       &picture.draw_flat);
  bind(cv,
       {.name = "cl_muzzleflash",
        .flags = gc::kArchive,
        .help = "The light a gun throws around it as it fires.",
        .kind = gc::CvarKind::integer,
        .min = 0,
        .max = 2,
        .values = "1: every gun's (the default); 2: the others' alone, not your own; 0: none"},
       &picture.muzzle_flash);
  bind(cv,
       {.name = "cl_fakeshaft",
        .flags = gc::kArchive,
        .help = "How closely your own lightning beam follows your aim between the server's "
                "updates.",
        .min = 0,
        .max = 1,
        .values = "1: it follows your view every frame (the default); 0: the server's line; "
                  "between: part of the way"},
       &picture.fake_shaft);
  bind(cv,
       {.name = "gl_picmip",
        .flags = gc::kArchive,
        .help = "How much detail the walls' textures lose.",
        .kind = gc::CvarKind::integer,
        .min = 0,
        .max = 4,
        .values = "0: their full detail (the default); 1..4: each step halves it"},
       &picture.picmip);
  const struct {
    const char* name;
    const char* help;
    std::string* value;
  } flats[] = {
      {"r_wallcolor", "The walls' colour when r_drawflat is on.", &picture.wall_colour},
      {"r_floorcolor", "The floors' colour when r_drawflat is on.", &picture.floor_colour},
      {"r_skycolor", "The sky's colour when r_fastsky is on.", &picture.sky_colour},
  };
  for (const auto& f : flats)
    bind(cv,
         {.name = f.name,
          .flags = gc::kArchive,
          .help = f.help,
          .kind = gc::CvarKind::text,
          .values = "TEXT: red, green and blue, each 0 to 255 (one number: a grey)"},
         f.value);
  bind(cv,
       {.name = "r_dynamic",
        .flags = gc::kArchive,
        .help = "Whether flashes, rockets and explosions light the walls and the models.",
        .kind = gc::CvarKind::boolean,
        .values = "1: they do (the default); 0: none do"},
       &picture.dynamic);
  struct Kind {
    const char* name;
    const char* help;
    const char* values;
    bool* value;
  };
  for (const Kind& k :
       {Kind{"r_rocketlight", "Whether a rocket lights the walls as it flies.",
             "1: it does (the default); 0: it does not", &picture.rocket_light},
        Kind{"r_explosionlight", "Whether an explosion lights the walls round it.",
             "1: it does (the default); 0: it does not", &picture.explosion_light},
        Kind{"r_powerupglow", "Whether a player with a power glows.",
             "1: they do (the default); 0: they do not", &picture.power_glow},
        Kind{"r_waterwarp", "Whether the picture wobbles while you are under water.",
             "1: it does (the default); 0: it holds still", &picture.water_warp},
        Kind{"r_drawentities", "Whether players, monsters and items are drawn.",
             "1: they are (the default); 0: only the map is", &picture.draw_entities},
        Kind{"cl_gibfilter", "Whether the pieces a body bursts into are hidden.",
             "0: they fly and lie about (the default); 1: none is drawn", &picture.gib_filter},
        Kind{"cl_deadbodyfilter", "Whether the players' bodies are hidden once they lie dead.",
             "0: they lie where they fell (the default); 1: they are gone once they lie still",
             &picture.dead_body_filter},
        Kind{"r_drawflame", "Whether the torches' and braziers' flames are drawn.",
             "1: they burn (the default); 0: none is drawn, nor its fire", &picture.draw_flame},
        Kind{"r_drawvweps",
             "Whether players carry the weapon they hold, where the server shows it.",
             "1: they do (the default); 0: each figure holds its own plain gun",
             &picture.draw_vweps},
        Kind{"r_particles", "Whether the effects throw their specks: sparks, blood, smoke, trails.",
             "1: they do (the default); 0: none is thrown, the blasts and beams still show",
             &picture.particles},
        Kind{"r_powershells", "Whether a player with a power wears a coloured shell.",
             "0: none, the power's glow alone (the default); 1: quad damage blue, the pentagram "
             "or invulnerability red",
             &picture.power_shells},
        Kind{"r_itempulse", "Whether the items lying about pulse with light.",
             "0: they lie lit as the room is (the default); 1: their light pulses",
             &picture.item_pulse}})
    bind(cv,
         {.name = k.name,
          .flags = gc::kArchive,
          .help = k.help,
          .kind = gc::CvarKind::boolean,
          .values = k.values},
         k.value);
  bind(cv,
       {.name = "r_fullbrightskins",
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
    bind(cv,
         {.name = t.name,
          .flags = gc::kArchive,
          .help = t.help,
          .min = 0,
          .max = 1,
          .values = "0: none; 0..1: that share of it; 1: all of it (the default)"},
         t.value);
  bind(cv,
       {.name = "cl_bonusflash",
        .fallback = "1",
        .flags = gc::kArchive,
        .help = "Whether taking an item flashes the view gold.",
        .kind = gc::CvarKind::boolean,
        .values = "1: it does, as the game always did (the default); 0: never"},
       &picture.bonus_flash);
  bind(cv,
       {.name = "gl_cshiftpercent",
        .flags = gc::kArchive,
        .help = "How strongly every tint of the view shows, in percent.",
        .min = 0,
        .max = 100,
        .values = "0: none; 0..100: that percent of each; 100: all of it (the default)"},
       &picture.cshift_percent);
  bind(cv,
       {.name = "gl_polyblend",
        .flags = gc::kArchive,
        .help = "Whether the view is tinted at all, by water, hits or powers.",
        .kind = gc::CvarKind::boolean,
        .values = "1: it is (the default); 0: never"},
       &picture.poly_blend);
  bind(cv,
       {.name = "gl_damageblend_frac",
        .flags = gc::kArchive,
        .help = "How deep a hit's flash reaches in from the screen's edges, so the middle stays "
                "clear.",
        .kind = gc::CvarKind::number,
        .min = 0,
        .max = 0.5,
        .values = "0: over the whole view (the default); 0..0.5: a frame that share of the "
                  "screen's shorter side deep, fading inward"},
       &picture.damage_frac);
  bind(cv,
       {.name = "r_lightmap",
        .flags = gc::kArchive,
        .help = "Every wall and floor drawn plain grey, so only the light on it shows.",
        .kind = gc::CvarKind::boolean,
        .values = "0: textured (the default); 1: plain grey under its light"},
       &picture.lightmap_only);
  bind(cv,
       {.name = "r_fullbright",
        .flags = gc::kCheat,
        .help = "Every wall and floor drawn as it is painted, with no light or shade, while "
                "sv_cheats allows it.",
        .kind = gc::CvarKind::boolean,
        .values = "0: lit by the map's light (the default); 1: full light everywhere"},
       &picture.fullbright);
  bind(cv,
       {.name = "r_fastsky",
        .flags = gc::kArchive,
        .help = "The sky drawn in one plain colour, r_skycolor.",
        .kind = gc::CvarKind::boolean,
        .values = "0: the map's sky (the default); 1: one plain colour"},
       &picture.fast_sky);
  bind(cv,
       {.name = "r_skyfog",
        .flags = gc::kArchive,
        .help = "How much of a map's fog is laid over its sky.",
        .min = 0,
        .max = 1,
        .values = "0..1: that share of the fog's colour over the sky (0.5 the default), where "
                  "the map does not set its own"},
       &picture.sky_fog);
  bind(cv,
       {.name = "r_wateralpha",
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
    bind(cv,
         {.name = k.name,
          .flags = gc::kArchive,
          .help = k.help,
          .kind = gc::CvarKind::text,
          .values = "\"\": as r_wateralpha (the default); 0..1: 1 solid, 0 not drawn"},
         k.value);
  bind(cv,
       {.name = "noskins",
        .flags = gc::kArchive,
        .help = "Whether players are drawn in their own skins.",
        .kind = gc::CvarKind::integer,
        .min = 0,
        .max = 2,
        .values = "0: each in their own skin (the default); 1: everyone in the plain skin, in "
                  "their colours; 2: their own skins, but none fetched from a server"},
       &picture.noskins);
  bind(cv,
       {.name = "baseskin",
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
  bind(cv,
       {.name = "teamskin",
        .flags = gc::kArchive,
        .help = "The skin every teammate wears.",
        .kind = gc::CvarKind::text,
        .values = "TEXT: that skin on every teammate; \"\": each their own (the default)"},
       &picture.team_skin);
  bind(cv,
       {.name = "enemyskin",
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
      if (a.size() > 3 || !parse_forced_colours(a[1], a.size() > 2 ? a[2] : "", top, bottom)) {
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
  bind(cv,
       {.name = "r_farland_warm",
        .help = "How much warmer the far land's air is than its own, in degrees C.",
        .min = -40,
        .max = 40,
        .values = "-40..40: that many degrees, below 0 colder, the snow coming down the "
                  "mountains and the lakes freezing (0 the default)"},
       &picture.far_warm);
  bind(cv,
       {.name = "gamma",
        .flags = gc::kArchive,
        .help = "The picture's brightness, lower brighter.",
        .min = 0.3,
        .max = 1.5,
        .values = "0.3..1.5: that gamma, 1 as drawn and 0.5 the brightest the menu offers (1 "
                  "the default)"},
       &picture.gamma);
  bind(cv,
       {.name = "r_quality",
        .flags = gc::kArchive,
        .help = "The shadows, the water and the skies set together.",
        .kind = gc::CvarKind::text,
        .values = "custom: each ce_ switch and its quality as set (the default); low: the "
                  "cheapest; medium: between; high: the best but the top; top: everything; "
                  "auto: the most inside r_budget; 0..1: cheapest to the top, by measured cost"},
       &picture.quality);
  bind(cv,
       {.name = "r_budget",
        .flags = gc::kArchive,
        .help = "The GPU time a frame may take with r_quality auto, in ms.",
        .min = 1,
        .max = 100,
        .values = "1..100: that many ms (8.3 the default, for 120 Hz)"},
       &picture.budget_ms);
  ge::classicenhance::bind_switches(cv, &picture.ce);
  bind(cv,
       {.name = "ce_enabled",
        .fallback = "1",
        .flags = gc::kArchive,
        .help = "Classic enhance as a whole: off, the game as its packs have it, whatever each ce_ "
                "switch says.",
        .kind = gc::CvarKind::boolean,
        .min = 0,
        .max = 1,
        .values = "0: none of it, every ce_ switch kept as set for when it is on again; 1: each "
                  "ce_ switch as set (the default)"},
       &picture.ce_enabled);
  // Each dear switch's level when it is on (Stops), beside its ce_ switch.
  const auto level = [&](const char* name, const char* help, const char* values,
                         std::string* field) {
    bind(cv,
         {.name = name,
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

void apply_ce_enabled(Picture& picture, const gc::Cvars& cvars) {
  if (!picture.ce_enabled) {
    for (const ge::classicenhance::SwitchSpec& s : ge::classicenhance::switches())
      picture.ce.*s.field = false;
  } else if (!picture.ce_applied) {
    for (const ge::classicenhance::SwitchSpec& s : ge::classicenhance::switches())
      if (const gc::Cvar* c = cvars.find("ce_" + std::string(s.id)); c != nullptr)
        picture.ce.*s.field = c->on();
  }
  picture.ce_applied = picture.ce_enabled;
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
  return !text.empty() && console::parse_number(text, v) ? v : fallback;
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
    if (!text.empty() && console::parse_number(text, v)) out[i] = std::max(v, 0.0);
  }
}

bool watervis_allowed(const std::vector<std::pair<std::string, std::string>>& serverinfo) {
  for (const auto& [key, value] : serverinfo) {
    if (key != "watervis") continue;
    double v = 0;
    return !console::parse_number(value, v) || v > 0;
  }
  return true;
}

bool crisp_textures(const Picture& picture, Look look) {
  std::string mode;
  for (const char ch : picture.texture_mode)
    if (ch != ' ' && ch != '\t')
      mode += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  if (mode.starts_with("GL_")) mode.erase(0, 3);
  if (mode.starts_with("NEAREST")) return true;
  if (mode.starts_with("LINEAR")) return false;
  return look == Look::classic;
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

bool parse_forced_colours(std::string_view top, std::string_view bottom, int* out_top,
                          int* out_bottom) {
  if (top == "off" && bottom.empty()) {
    *out_top = *out_bottom = -1;
    return true;
  }
  const auto number = [](std::string_view v, int* out) {
    if (v.empty() || v.size() > 3) return false;
    int n = 0;
    for (char c : v) {
      if (c < '0' || c > '9') return false;
      n = n * 10 + (c - '0');
    }
    *out = std::min(n, kColourRows - 1);
    return true;
  };
  int t = 0, b = 0;
  if (!number(top, &t)) return false;
  if (bottom.empty()) {
    b = t;
  } else if (!number(bottom, &b)) {
    return false;
  }
  *out_top = t;
  *out_bottom = b;
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
  // Quake II's own (classic/q2_fx_switches.hpp; another game's defaults change nothing):
  // gl_drawsky 0 a black sky, gl_bilerp_skies 0 a sharp one, gl_saturation and gl_invert on the
  // walls' and floors' textures.
  const ge::classic::Q2FxSwitches& q2 = picture.q2fx;
  if (!q2.draw_sky) {
    base.fast_sky = true;
    base.sky_colour[0] = base.sky_colour[1] = base.sky_colour[2] = 0;
  }
  base.crisp_sky = !q2.smooth_sky;
  base.saturation = q2.saturation;
  base.invert = q2.invert;
  // gl_anisotropy (every game's) the filtered textures' taps.
  if (picture.anisotropy > 0) base.anisotropy = static_cast<int>(std::lround(picture.anisotropy));
  base.lightmap_saturation = q2.lightmap_saturation;  // gl_coloredlightmaps
  base.lerp_styles = q2.lerp_styles;                  // cl_lerp_lightstyles
  if (q2.dlight_falloff >= 0) base.dlight_falloff = q2.dlight_falloff == 0 ? 2 : 1;
  base.picmip = std::clamp(picture.picmip, 0, 4);  // gl_picmip
  base.fullbright = picture.fullbright;            // r_fullbright
  if (picture.lightmap_only) {                     // r_lightmap: grey faces, the light alone
    base.flat_walls = base.flat_floors = true;
    for (int k = 0; k < 3; ++k) base.wall_colour[k] = base.floor_colour[k] = kLightmapGrey;
  }
  if (look == Look::classic) {
    base.mode = ba::Mode::classic;
    base.classic_light = world_light_scale(picture);  // intensity, gl_modulate (_world)
    base.classic_lift = std::clamp(picture.light_lift, -1.0, 1.0);  // gl_brightness
    if (!picture.dynamic) base.dynamic_strength = 0;  // r_dynamic 0: no moment's light
    base.lighting = ba::Lighting::lightmaps;
    base.crisp = crisp_textures(picture, look);  // gl_texturemode, else crisp
    base.classic_filtered = !base.crisp;
    base.weather = false;
  } else {
    base.mode = ba::Mode::v2;
    base.lighting = ba::Lighting::lights;
    base.brightness *= world_light_scale(picture);  // intensity, gl_modulate: the map's light
    base.strength *= world_light_scale(picture);
    base.crisp = crisp_textures(picture, look);  // gl_texturemode, else filtered
    base.dynamic_strength = picture.dynamic ? picture.fx_lights : 0;
    base.lava_glow = picture.lava_glow;
    base.slime_glow = picture.slime_glow;
    base.fire_glow = picture.fire_glow;
    base.fire_pace = picture.fire_pace;
    base.water_pace = picture.water_pace;
    base.water_specks = picture.water_specks;
    base.water_warp = picture.water_warp;
    base.fill_shadows = picture.fill_shadows > 0.5;
    base.glow =
        picture.ce.glow ? picture.glow : 1;  // ce_glow off: the bright texels as the classic's
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
    base.flicker = picture.ce.flicker;  // ce_flicker: the flames' light and the styles waver
    // ce_reflections: the room mirrored in the faces by their gloss.
    base.reflections = picture.ce.reflections ? std::max(picture.reflect, 0.0) : 0;
    // ce_ao: the corners, creases and the floor under the figures darkened by how little they see.
    base.ao = picture.ce.ao ? std::clamp(picture.ao_strength, 0.0, 1.0) : 0;
    base.ao_radius_m = std::max(picture.ao_radius, 1.0) * ge::brush::kUnitM;
  }
  return base;
}

double effects_glow(Look look, const Picture& picture) {
  return look == Look::classic ? 1 : picture.fx_glow;
}

gr::PostSettings post_settings(Look look, const Picture& picture) {
  gr::PostSettings s;
  s.bloom = look == Look::classic ? 0 : picture.bloom;
  // The classic look is the painted colours times their light, worked out as the games did: shown
  // as it is, at the scale the world's passes write (a white face's luminance times the lightmaps'
  // brightness), never exposed or through the tone curve's toe, which sank the dark textures
  // toward black and took the blue out of their greys.
  if (look == Look::classic) s.display = ba::Options{}.brightness;
  // The modern look keeps the tone curve's shoulder but not its toe: the classic textures are
  // painted dark, and the toe took their blues and greys to brown.
  s.toe = false;
  return s;
}

double world_light_scale(const Picture& picture) {
  return light_scale(picture) * std::max(picture.q2fx.modulate_world, 0.0);
}

double model_light(const Picture& picture, Look look) {
  const ge::classic::Q2FxSwitches& q2 = picture.q2fx;
  const double mod = std::max(picture.light_modulate, 0.0);
  const double once = mod * std::max(q2.modulate_entities, 0.0);
  const double total = q2.double_light ? once * once : once;
  if (look == Look::classic) return total;  // the classic models take no other modulate
  // The modern look's models are lit at the world's brightness, its modulate in it already.
  const double world = mod * std::max(q2.modulate_world, 0.0);
  return world > 0 ? total / world : 0;
}

double light_scale(const Picture& picture) {
  return std::clamp(picture.light_intensity, 1.0, 5.0) * std::max(picture.light_modulate, 0.0) / 2;
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

}  // namespace ge::classic::look
