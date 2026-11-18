// LAB `se-live-map`: THE LIVE MAP (games/shooters_edge/livemap/live_map.hpp, docs/goals/M62.md): a
// recording played and pictured from above at the times `--at` names, the whole screen (the map
// fitted over the HUD's band, the player followed glowing blue and named) and the corner of a
// player's view (the map small in the top right, as `demo_map 2` draws it over the world). Every
// figure is the pack's own model at the entity's own frame and angles, seen from three quarters
// above.
//
// With `--game` (`<basedir>/<game>`: the packs) and `--demo` (any recording the window plays) it is
// that match on its own map; without, a small map compiled from a plan and a made-up moment on it
// (three players with no models, so dots; a rocket's trail, an explosion, an item taken and its
// timer), so it runs on any machine and its pictures feed the digest.
//
// ge:summary Lab: the live map of a recording, whole screen and in the corner, at chosen times
// ge:keywords lab se-live-map live-map overview tv spectate demo figures follow corner-map
// ge:skill map-design labs replay
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/livemap/live_map.hpp"
#include "ui/draw.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace mc = ge::brush::mapc;
namespace cl = shooters_edge::client;
namespace lv = shooters_edge::livemap;
namespace ui = ge::ui;

constexpr const char* kSample = R"(# se-live-map's sample
room west z=0..384 box 0,0 512,768
room east z=0..384 box 896,0 1408,768
room bridge z=96..224 box 512,320 896,448
room pit z=-128..0 box 512,0 896,768
room pitup z=0..96 box 512,0 896,768
liquid z=-128..-48 box 512,0 896,768 kind=water tex=*water1
stairs z=0..96 box 384,288 512,480 up=+x
stairs z=0..96 box 896,288 1024,480 up=-x
light 256,384,300 300
light 1152,384,300 300
light 704,384,200 200
entity info_player_deathmatch 128,128,24 angle=45
entity item_armorInv 1280,640,24
entity weapon_rocketlauncher 256,640,24
entity item_artifact_super_damage 704,384,120
)";

// A made-up moment on the sample at `t` s: three players walking, a rocket in flight, the red
// armour taken at 1 s, an explosion at 2 s.
void sample_state(cl::DemoState& s, double t) {
  s.map = "sample";
  s.time = t;
  s.protocol = 28;
  s.models = {"", "progs/player.mdl", "progs/missile.mdl", "progs/armor.mdl"};
  s.players.assign(32, cl::DemoEntity{});
  const char* names[3] = {"Ranger", "Lynx", "Moss"};
  for (int i = 0; i < 3; ++i) {
    cl::DemoEntity& p = s.players[size_t(i)];
    p.model = 1;
    p.origin = {float(200 + 300 * i + 40 * t), float(160 + 120 * i), 24};
    p.angles = {0, float(30 * i + 20 * t), 0};
    s.names[size_t(i)] = names[i];
    s.top[size_t(i)] = i * 4;
    s.bottom[size_t(i)] = i == 1 ? 4 : 13;
  }
  s.players[2].effects = 64;  // the quad's glow
  s.entities.assign(8, cl::DemoEntity{});
  if (t < 1) {
    s.entities[3].model = 3;  // the red armour, until taken
    s.entities[3].present = true;
    s.entities[3].origin = {1280, 640, 24};
  }
  s.entities[4].model = 2;  // a rocket, west to east
  s.entities[4].present = t < 2;
  s.entities[4].origin = {float(300 + 500 * t), 600, 60};
  s.effects.clear();
  if (t >= 2 && t < 2.05) {
    cl::DemoEffect fx;
    fx.kind = cl::DemoEffectKind::explosion;
    fx.at = {1300, 600, 60};
    s.effects.push_back(fx);
  }
  ++s.frames;
}

gl::Picture picture(const ui::Image& img) { return {img.width, img.height, img.rgba}; }

void run(gl::Lab& lab) {
  std::string error;
  const int w = std::max(320, int(lab.arg_num("width")));
  const int h = std::max(200, int(lab.arg_num("height")));
  cl::SearchPath files;
  std::vector<uint8_t> palette_store;
  std::span<const uint8_t> palette;
  const std::string folder = lab.arg("game");
  const bool real = !folder.empty();
  cl::Demo demo;
  cl::DemoState sample;
  std::vector<uint8_t> map_bytes;
  std::string map_name = "sample";
  if (real) {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("se-live-map: " + error, "Pass --game=<basedir>/<game folder>. Skill: map-design.");
      return;
    }
    palette = cl::game_palette(files, palette_store);
    std::ifstream in(lab.arg("demo"), std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes.empty() || !cl::open_demo(std::move(bytes), demo, error)) {
      lab.fail("se-live-map: the recording " + lab.arg("demo") + " does not open: " + error,
               "Pass --demo=<a recording the window plays>. Skill: replay.");
      return;
    }
    while (demo.state.map.empty() && cl::next_frame(demo, error) == cl::DemoRead::frame) {
    }
    map_name = demo.state.map;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + map_name + ".bsp", store);
    map_bytes.assign(f.begin(), f.end());
  } else {
    mc::Source src;
    if (!mc::read_plan(kSample, &src, &error)) {
      lab.fail("se-live-map: the sample plan: " + error, "Fix the lab's sample. Skill: brush.");
      return;
    }
    mc::CompileReport rep;
    if (!mc::compile_map(src, {}, &map_bytes, &rep, &error)) {
      lab.fail("se-live-map: the sample: " + error, "Fix the lab's sample. Skill: brush.");
      return;
    }
    palette_store.resize(768);
    for (int i = 0; i < 256; ++i) {
      palette_store[size_t(i) * 3] = uint8_t(std::min(255, i + 30));
      palette_store[size_t(i) * 3 + 1] = uint8_t(i * 7 / 8 + 20);
      palette_store[size_t(i) * 3 + 2] = uint8_t(i * 3 / 4 + 14);
    }
    palette = palette_store;
  }
  lv::LiveMap lm;
  const auto t0 = std::chrono::steady_clock::now();
  if (!lv::live_map_level(lm, files, palette, map_bytes, map_name, int(lab.arg_num("plate")),
                          &error)) {
    lab.fail("se-live-map: the map " + map_name + ": " + error,
             "Pass a version 29 map's recording. Skill: map-design.");
    return;
  }
  lab.metric(
      "plate-build",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(),
      "ms", gl::Det::measured);
  lab.metric("spots", double(lm.plate->spots.size()), "count");
  lab.metric("teleporter-links", double(lm.plate->links.size()), "count");
  lab.metric("water-pixels", double(lm.plate->water.size()), "count");
  // The times.
  std::vector<double> times;
  {
    std::stringstream ss(lab.arg("at"));
    std::string item;
    while (std::getline(ss, item, ',')) times.push_back(std::atof(item.c_str()));
  }
  const double step = lab.arg_num("step");
  const int frames = std::max(1, int(lab.arg_num("frames")));
  int shot_n = 0;
  double draw_ms = 0;
  int draws = 0;
  for (const double at : times) {
    for (int k = 0; k < frames; ++k) {
      const double t = at + step * k;
      const cl::DemoState* s = &sample;
      if (real) {
        // On to `t`, every frame read (the effects of each taken by the map as it goes).
        while (demo.state.time < t) {
          if (cl::next_frame(demo, error) != cl::DemoRead::frame) break;
          if (demo.state.time > t - 0.5) {  // the last half second drawn, so trails and timers run
            ui::DrawList scratch;
            scratch.begin(w, h);
            lv::draw_live_map(lm, demo.state,
                              {.area = {0, 0, float(w), float(h)}, .now_s = demo.state.time},
                              scratch);
          }
        }
        s = &demo.state;
      } else {
        for (int j = 12; j >= 1; --j) {
          const double u = t - 0.05 * j;
          if (u < 0) continue;
          sample_state(sample, u);
          ui::DrawList scratch;
          scratch.begin(w, h);
          lv::draw_live_map(lm, sample, {.area = {0, 0, float(w), float(h)}, .now_s = u}, scratch);
        }
        sample_state(sample, t);
      }
      int follow = s->player;
      if (follow < 0 || follow >= 32 || s->names[size_t(follow)].empty())
        follow = lv::next_followed(*s, -1, 1);
      if (lab.arg("follow") != "") {
        const std::string& want = lab.arg("follow");
        for (int i = 0; i < 32; ++i)
          if (s->names[size_t(i)] == want) follow = i;
      }
      for (int i = 0; i < 32 && k == 0; ++i) {
        if (s->names[size_t(i)].empty() || s->players.size() <= size_t(i)) continue;
        const cl::DemoEntity& e = s->players[size_t(i)];
        char line[200];
        std::snprintf(line, sizeof line, "t %.1f: %s model %s frame %d at %.0f,%.0f,%.0f%s%s",
                      s->time, s->names[size_t(i)].c_str(),
                      size_t(e.model) < s->models.size() ? s->models[size_t(e.model)].c_str() : "?",
                      e.frame, e.origin.x, e.origin.y, e.origin.z, e.dead ? " dead" : "",
                      s->spectator[size_t(i)] ? " spectator" : "");
        lab.note(line);
      }
      // The whole screen: the map over a band the HUD keeps.
      const float hud_band = float(h) * 0.12f;
      ui::DrawList dl;
      dl.begin(w, h);
      lv::LiveView v;
      v.area = {0, 0, float(w), float(h) - hud_band};
      v.fill = true;
      v.names = true;
      v.follow = follow;
      v.now_s = s->time;
      const auto d0 = std::chrono::steady_clock::now();
      lv::draw_live_map(lm, *s, v, dl);
      draw_ms +=
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - d0).count();
      ++draws;
      dl.rect({0, float(h) - hud_band, float(w), hud_band}, {0.08f, 0.07f, 0.06f, 1});
      ui::Image img = ui::make_image(w, h, {0, 0, 0, 1});
      ui::render(dl, img);
      char id[64], tt[64];
      std::snprintf(id, sizeof id, "full-%02d", shot_n);
      std::snprintf(tt, sizeof tt, "%s at %.2f s, the whole screen", map_name.c_str(), s->time);
      lab.shot(
          {id,
           tt,
           "The match from above: the map's floors, every player the pack's own model at his "
           "own frame and angles in his colours, the items turning, the rockets and grenades "
           "with their smoke, taken items counting down; the one followed glowing blue and named. "
           "The dark band is where the HUD stays.",
           "figures stand where the players are, turned their way; nothing flies off an "
           "explosion; the followed player's blue glow and name",
           {{"map", map_name}, {"t", std::to_string(s->time)}}},
          picture(img), real ? gl::Det::measured : gl::Det::deterministic);
      if (k == 0) {
        // The corner: the map small in the top right over the view (a plain stand-in here).
        ui::DrawList cd;
        cd.begin(w, h);
        cd.rect({0, 0, float(w), float(h)}, {0.22f, 0.2f, 0.18f, 1});
        const float side = float(h) * float(lab.arg_num("scale"));
        lv::LiveView c;
        c.area = {float(w) - side * 1.5f - 12, 12, side * 1.5f, side};
        c.follow = follow;
        c.min_figure_px = 10;
        c.now_s = s->time;
        lv::draw_live_map(lm, *s, c, cd);
        ui::Image ci = ui::make_image(w, h, {0, 0, 0, 1});
        ui::render(cd, ci);
        std::snprintf(id, sizeof id, "corner-%02d", shot_n);
        std::snprintf(tt, sizeof tt, "%s at %.2f s, in the corner", map_name.c_str(), s->time);
        lab.shot({id,
                  tt,
                  "The same moment as the corner map draws it over a player's view (demo_map 2), "
                  "its height the demo_map_scale share of the screen's.",
                  "the map whole in the corner, the followed player still glowing",
                  {{"map", map_name}, {"scale", lab.arg("scale")}}},
                 picture(ci), real ? gl::Det::measured : gl::Det::deterministic);
      }
      ++shot_n;
    }
  }
  for (const auto& [name, id] : lm.model_of) {
    const auto& a = *lm.models[size_t(id)];
    char line[200];
    std::snprintf(line, sizeof line, "model %s: %s, radius %.0f units", name.c_str(),
                  a.ok ? "drawn" : "not found", double(a.radius));
    lab.note(line);
  }
  lab.metric("figures-drawn", double(lm.figures_drawn), "count", gl::Det::measured);
  lab.metric("draw-ms", draws > 0 ? draw_ms / draws : 0, "ms", gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-live-map",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the live map of a recording from above (whole screen and corner) at chosen times",
    "map-design",
    "Does the live map show the match as it is: every player and item the game's own model at its "
    "own frame and angles, projectiles with their trails, timers on taken items, the followed "
    "player plain to see?",
    {
        {"game", "", "a game folder, <basedir>/<game>: the packs (empty: the sample)"},
        {"demo", "", "with --game: a recording to play"},
        {"at", "0.5,1.5,2.0", "the times to picture, s on the recording's clock, comma separated"},
        {"frames", "1", "pictures at each time, `step` apart (a filmstrip)"},
        {"step", "0.1", "s between a time's pictures"},
        {"follow", "", "a player's name to follow (empty: the recording's own)"},
        {"width", "1280", "px"},
        {"height", "720", "px"},
        {"plate", "1600", "the map's picture, px on its long side"},
        {"scale", "0.34", "the corner map's height, a share of the screen's (demo_map_scale)"},
    },
    "--width=640 --height=360 --plate=640",
    run,
};
GE_LAB(kSpec);

}  // namespace
