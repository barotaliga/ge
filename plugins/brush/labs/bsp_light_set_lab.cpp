// LAB `bsp-light-set`: WHICH LIGHTS A FRAME DRAWS AS THE PLAYER WALKS (light_pick.hpp). A still
// picture cannot show a light that comes and goes between two steps, so this lab walks a path
// through a map, chooses the frame's lights at every step as the renderer does (choose_lights,
// deal_cubes) and counts the POPS: a light that could light what the eye sees at both steps (its
// reach inside the view's cone and in a cluster the eye's potentially visible set holds) whose
// share in the picture jumped from one step to the next: drawn or not, shadowed or shining
// through the walls unshadowed, faded in. A stable picture pops nothing; a fade moves a light's
// share by a step's worth.
//
// `--pak=<pack> --maps=<name>` walks a map of that pack (private numbers), `--path="x,y,z;..."` the
// path in the map's units (default: from start to start of the map's player spawns); without a
// pack it walks the sample fixture's halls. The chart draws the counts along the walk.
//
// ge:summary Lab: the frame's lights chosen along a walk: drawn, shadowed, leaking, waiting, popping
// ge:keywords lab bsp-light-set light-pick light-pop light-swap choose_lights deal_cubes walk
// ge:keywords shadowed-lights leak-through-walls stable-lights
// ge:skill brush render labs
// ge:test tests/brush_bsp_light_pick_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/draw_data.hpp"
#include "brush/app/light_pick.hpp"
#include "brush/map.hpp"
#include "brush/write.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "render/view.hpp"

namespace {

namespace gl = ge::lab;
namespace ba = ge::brush::app;
namespace gb = ge::brush;
namespace r = ge::render;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<std::string> split(const std::string& s, char c) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    size_t end = s.find(c, at);
    if (end == std::string::npos) end = s.size();
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

constexpr double kStepM = 0.125;        // a step of the walk: a run's frame at 60 Hz, near enough
constexpr double kDt = 1.0 / 60;        // s a step
constexpr double kEyeUnits = 22;        // the eye over the path, map units
constexpr double kPopShare = 0.25;      // a light's share jumping by this or more in one step pops
constexpr double kLeakShare = 1.0;      // an unshadowed light's share (it lights through walls)

// The path in the map's units: the given one, or the player spawns in the map's order.
std::vector<std::array<double, 3>> path_of(gl::Lab& lab, const gb::Map& map) {
  std::vector<std::array<double, 3>> out;
  for (const std::string& p : split(lab.arg("path"), ';')) {
    const std::vector<std::string> v = split(p, ',');
    if (v.size() != 3) continue;
    out.push_back({std::stod(v[0]), std::stod(v[1]), std::stod(v[2])});
  }
  if (!out.empty()) return out;
  for (const gb::Entity& e : map.entities) {
    const std::string& c = e.get("classname");
    if (c != "info_player_start" && c != "info_player_deathmatch") continue;
    double x = 0, y = 0, z = 0;
    if (std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &x, &y, &z) == 3)
      out.push_back({x, y, z});
  }
  return out;
}

void run(gl::Lab& lab) {
  ba::Pack pack;
  std::vector<uint8_t> palette;
  std::string path = "maps/fixture.bsp", name = "fixture";
  if (!lab.arg("pak").empty()) {
    std::string err;
    if (!ba::read_pack(lab.arg("pak"), &pack, &err)) {
      lab.fail(err, "Pass --pak=<a pack file> holding maps/*.bsp. Skill: brush.");
      return;
    }
    const auto pal = ba::pack_file(pack, "gfx/palette.lmp");
    palette.assign(pal.begin(), pal.end());
    name = lab.arg("maps").empty() ? "dm2" : lab.arg("maps");
    path = "maps/" + name + ".bsp";
  } else {
    std::vector<uint8_t> bytes;
    std::string err;
    gb::write_bsp(gb::sample_fixture(), gb::Family::v29, &bytes, &err);
    pack.bytes = bytes;
    pack.files.push_back({path, 0, uint32_t(bytes.size())});
  }
  if (palette.size() < 768) palette.assign(768, 128);
  ba::Scene scene;
  {
    std::string err;
    if (!ba::load_scene(pack, path, palette, &scene, &err)) {
      lab.fail(path + ": " + err, "Check the map's name in the pack (--maps). Skill: brush.");
      return;
    }
  }
  ba::seen_lights(scene);
  const gb::Map& map = scene.map;
  const std::vector<std::array<double, 3>> route = path_of(lab, map);
  if (route.size() < 2) {
    lab.fail(name + " has no path to walk (fewer than two points)",
             "Pass --path=\"x,y,z;x,y,z\" in the map's units. Skill: brush.");
    return;
  }

  // The renderer's lights as a frame offers them (the map's, the lava's, the teleporters').
  const ba::Options o;
  std::vector<ba::PointLight> lava = ba::lava_lights(scene, 1150, 0.3);
  std::vector<ba::PointLight> portals = ba::portal_lights(scene);
  std::vector<ba::LightOffer> offers;
  for (const ba::PointLight& l : scene.lights) offers.push_back({&l, false});
  for (const ba::PointLight& l : lava) offers.push_back({&l, false});
  for (const ba::PointLight& l : portals) offers.push_back({&l, false});
  const ba::LightReach reach = ba::light_reach(map, scene.lights);
  ba::PickRules rules;
  rules.most = o.most;
  rules.shadowed = o.shadowed;
  rules.bonfires = o.bonfires;
  rules.widest = ba::kWidestAspect;
  ba::CubeSlots slots;
  slots.slots = std::min(ba::kMaxShadowed, ba::kMaxCubeSlots);
  ba::LightChoice choice;
  ba::CubeDeal deal;

  // Each light's share in the picture at the last step (by its offer), and whether it could show.
  const size_t n = offers.size();
  std::vector<double> share(n, 0), last(n, -1);
  std::vector<uint8_t> could(n, 0), could_last(n, 0);
  std::vector<uint8_t> seen;
  gl::Trace t_drawn{"drawn", {}, {}}, t_shadowed{"shadowed", {}, {}},
      t_leak{"unshadowed (through walls)", {}, {}}, t_could{"could show", {}, {}},
      t_pops{"pops (running total)", {}, {}};
  double walked = 0;
  int pops = 0, steps = 0, most_could = 0, most_leak = 0;
  for (size_t seg = 0; seg + 1 < route.size(); ++seg) {
    double a[3], b[3];
    {
      const float fa[3] = {float(route[seg][0]), float(route[seg][1]),
                           float(route[seg][2] + kEyeUnits)};
      const float fb[3] = {float(route[seg + 1][0]), float(route[seg + 1][1]),
                           float(route[seg + 1][2] + kEyeUnits)};
      gb::to_engine(fa, a);
      gb::to_engine(fb, b);
    }
    const double d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-6) continue;
    const int count = std::max(1, int(len / kStepM));
    for (int i = 0; i < count; ++i, ++steps) {
      const double k = double(i) / count;
      r::FrameView view;
      for (int c = 0; c < 3; ++c) view.eye[c] = a[c] + d[c] * k;
      view.heading = std::atan2(d[0], d[2]);
      view.pitch = 0;
      view.fov_y = 2 * std::atan(9.0 / 16.0);  // 90 degrees across a wide screen
      view.time_s = steps * kDt;
      ba::choose_lights(map, offers, &reach, view, rules, choice);
      std::vector<std::array<float, 4>> keys;
      for (const size_t c : choice.casters) {
        double s[4];
        ba::light_sphere(*choice.drawn[c].light, rules.bonfires, s);
        keys.push_back({float(s[0]), float(s[1]), float(s[2]), float(s[3])});
      }
      const size_t budget = steps == 0 ? size_t(slots.slots) : size_t(o.fresh_cubes);
      ba::deal_cubes(slots, keys, budget, kDt, 0.0, deal);
      // Each offer's share this step.
      std::fill(share.begin(), share.end(), 0.0);
      int drawn = 0, shadowed = 0, leak = 0;
      std::vector<int> slot_of(choice.drawn.size(), -2);  // -2 not a caster
      std::vector<float> fade_of(choice.drawn.size(), 1);
      for (size_t c = 0; c < choice.casters.size(); ++c) {
        slot_of[choice.casters[c]] = deal.slot[c];
        fade_of[choice.casters[c]] = deal.fade[c];
      }
      for (size_t j = 0; j < choice.drawn.size(); ++j) {
        size_t idx = n;
        for (size_t q = 0; q < n; ++q)
          if (offers[q].light == choice.drawn[j].light) idx = q;
        if (idx == n) continue;
        // A caster with its cube: its fade; one waiting for it: the share the deal gives it
        // unshadowed (light_pick.hpp); any other light of the map's: unshadowed.
        const int sl = slot_of[j];
        double v = 0;
        if (sl >= 0) {
          v = fade_of[j];
          ++shadowed;
        } else if (sl == -1) {
          v = fade_of[j] * kLeakShare;
          if (v > 0) ++leak;
        } else {
          v = kLeakShare;
          if (!choice.drawn[j].light->physical) ++leak;
        }
        if (v > 0) ++drawn;
        share[idx] = v;
      }
      // Which lights could show (policy-free): their reach in the view's cone and in a cluster
      // the eye's set holds.
      {
        const float fe[3] = {float(view.eye[0] / gb::kUnitM), float(view.eye[2] / gb::kUnitM),
                             float(view.eye[1] / gb::kUnitM)};
        gb::clusters_seen_from(map, fe, 1.0f, seen);
      }
      double ahead[3];
      r::look_dir(view.heading, view.pitch, ahead);
      const double cone = std::atan(std::tan(view.fov_y / 2) *
                                    std::sqrt(1 + rules.widest * rules.widest));
      int could_n = 0;
      for (size_t q = 0; q < n; ++q) {
        double s[4];
        ba::light_sphere(*offers[q].light, rules.bonfires, s);
        const double to[3] = {s[0] - view.eye[0], s[1] - view.eye[1], s[2] - view.eye[2]};
        const double dist = std::sqrt(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
        bool in = dist <= s[3];
        if (!in) {
          const double along = (to[0] * ahead[0] + to[1] * ahead[1] + to[2] * ahead[2]) / dist;
          in = std::acos(std::clamp(along, -1.0, 1.0)) - std::asin(s[3] / dist) <= cone;
        }
        bool vis = true;
        if (in && q < reach.clusters.size() && !reach.clusters[q].empty() && !seen.empty()) {
          vis = false;
          for (const int32_t cl : reach.clusters[q])
            vis = vis || cl < 0 || (seen[size_t(cl) >> 3] >> (cl & 7) & 1) != 0;
        }
        could[q] = in && vis;
        could_n += could[q];
        if (could[q] && could_last[q] && last[q] >= 0 && std::fabs(share[q] - last[q]) >= kPopShare)
          ++pops;
      }
      last = share;
      could_last = could;
      most_could = std::max(most_could, could_n);
      most_leak = std::max(most_leak, leak);
      t_drawn.x.push_back(walked);
      t_drawn.y.push_back(drawn);
      t_shadowed.x.push_back(walked);
      t_shadowed.y.push_back(shadowed);
      t_leak.x.push_back(walked);
      t_leak.y.push_back(leak);
      t_could.x.push_back(walked);
      t_could.y.push_back(could_n);
      t_pops.x.push_back(walked);
      t_pops.y.push_back(pops);
      walked += len / count;
    }
  }
  const double per100 = walked > 0 ? pops * 100.0 / walked : 0;
  lab.metric(name + ".lights", double(n), "count");
  lab.metric(name + ".walked", walked, "m");
  lab.metric(name + ".pops", pops, "count");
  lab.metric(name + ".pops-per-100m", per100, "count");
  lab.metric(name + ".most-could-show", most_could, "count");
  lab.metric(name + ".most-unshadowed", most_leak, "count");

  gl::LineChart line;
  line.frame = {"THE FRAME'S LIGHTS ALONG A WALK THROUGH " + name,
                "every " + fmt(kStepM, 3) + " m, as the renderer chooses them", "walked (m)",
                "lights"};
  line.traces = {t_could, t_drawn, t_shadowed, t_leak, t_pops};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({name + "-walk",
            chart.title,
            "The lights that could light what the eye sees (in view and in its visible set), those "
            "drawn, those shadowed, those drawn unshadowed (they shine through the walls), and the "
            "pops so far: a light that could show whose share jumped between two steps.",
            "the pops' line flat; no light drawn unshadowed",
            {{"map", name}}},
           gl::to_picture(chart.plot));
  lab.table({"THE WALK",
             {"measure", "value"},
             {{"lights offered", std::to_string(n)},
              {"walked, m", fmt(walked, 1)},
              {"pops", std::to_string(pops)},
              {"pops a 100 m", fmt(per100, 2)},
              {"most lights that could show at once", std::to_string(most_could)},
              {"most lights drawn unshadowed at once", std::to_string(most_leak)}}});
}

const gl::LabSpec kSpec{
    "bsp-light-set",
    gl::LabKind::sim,
    "plugin:brush",
    "The frame's lights chosen along a walk (plugins/brush/app/light_pick)",
    "render",
    "As the player walks through a map, does any light that could show come and go, lose its "
    "shadow or shine through a wall from one step to the next?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
