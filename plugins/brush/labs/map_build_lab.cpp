// LAB `map-build`: THE MAP COMPILER (plugins/brush/compile) LOOKED AT. A map plan (by default the
// lab's own sample: two halls turned round the centre, a pool, stairs, a ramp, a sky court; or
// `--plan=<file>`, a plan or an SVG floor plan) compiled into a version 29 map and read back by
// our reader. Pictured: the compiled map from above (each spot's floor shaded by its height,
// water blue, sky open courts outlined, the entities as dots), eyes standing at the starts drawn
// with the map's own pictures and lightmaps, and a leak: the sample with a gap in a wall, refused,
// its way out drawn over the plan. The numbers: brushes, faces, leaves, nodes, clip nodes,
// luxels and bytes (held by the digest) and how long the compile takes (measured).
//
// `--textures=<folder>` takes the pictures from a game folder's maps (and the palette from its
// packs' gfx/palette.lmp) as `secli map build` does; without, every texture is the compiler's
// pattern under a palette of the lab's own, so CI needs no game data.
//
// ge:summary Lab: a map plan compiled to a BSP and read back: from above, eyes inside, a leak
// ge:keywords lab map-build map-compiler plan svg compile leak floor-plan above inside textured
// ge:skill brush labs
// ge:test tests/brush_compile_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/library.hpp"
#include "brush/compile/plan.hpp"
#include "brush/compile/svg.hpp"
#include "brush/labs/textured_view.hpp"
#include "brush/read.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace br = ge::brush;
namespace mc = ge::brush::mapc;
namespace lab = ge::lab;
using lab::Rgba;

// The lab's sample: a hall with a pool and a ledge, a run into a court under the sky, turned
// round the court's middle so the other hall is the same.
constexpr const char* kSample =
    R"(world message="Sample" light=14 _sunlight=160 "_sun_mangle=30 -50 0"
textures floor=floor wall=wall ceil=ceiling solid=wall
symmetry rotate=768,384
room hall z=0..192 box 0,64 384,704
solid ledge z=0..80 box 0,64 128,704 top=ledge
stairs z=0..80 box 128,576 224,704 up=-x top=ledge
room pool z=-80..0 box 224,128 352,256 floor=tile wall=tile
liquid z=-80..-16 box 224,128 352,256 kind=water tex=*water
room run z=0..128 box 384,320 576,448 floor=metal wall=metal
room court z=-32..256 box 576,128 960,640 floor=stone wall=stone ceil=sky1 once
stairs z=-32..0 box 576,320 640,448 up=-x top=metal
ramp z=-32..0 box 704,352 768,416 up=+x top=metal
solid plinth z=-32..0 box 768,320 832,448 top=metal
light 192,384,160 value=300
light 320,640,160 value=260
light 480,384,100 value=240
light 320,192,-40 value=180
spawn 64,384,104 angle=0
spawn 300,600,24 angle=270
entity weapon_rocketlauncher 800,384,8
entity item_health 300,128,24
)";

// The palette when no game hands one: rows of 16 shades, each row its own hue.
std::vector<uint8_t> lab_palette() {
  std::vector<uint8_t> p(768);
  const double hues[16][3] = {{1, 1, 1},       {0.8, 0.6, 0.4}, {0.6, 0.7, 0.9}, {0.7, 0.8, 0.6},
                              {0.9, 0.7, 0.6}, {0.6, 0.6, 0.7}, {0.8, 0.8, 0.6}, {0.7, 0.6, 0.8},
                              {0.6, 0.8, 0.8}, {0.9, 0.8, 0.7}, {0.7, 0.7, 0.7}, {0.8, 0.7, 0.8},
                              {0.6, 0.9, 0.7}, {0.9, 0.6, 0.6}, {0.7, 0.8, 0.9}, {1, 0.9, 0.6}};
  for (int i = 0; i < 256; ++i) {
    const int row = i / 16, shade = i % 16;
    for (int k = 0; k < 3; ++k) {
      p[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(255.0, (40 + shade * 12) * hues[row][k]));
    }
  }
  return p;
}

int32_t contents_at(const br::Map& m, double x, double y, double z) {
  int32_t n = m.models.empty() ? 0 : m.models[0].head_node;
  while (n >= 0) {
    const br::Node& node = m.nodes[size_t(n)];
    const br::Plane& p = m.planes[node.plane];
    const double d = p.n[0] * x + p.n[1] * y + p.n[2] * z - p.d;
    n = node.child[d >= 0 ? 0 : 1];
  }
  const int32_t leaf = -(n + 1);
  return leaf >= 0 && size_t(leaf) < m.leaves.size() ? m.leaves[size_t(leaf)].contents : -2;
}

struct Frame {
  double lo[3] = {}, hi[3] = {};
  double scale = 1;  // px a unit
  int pad = 24;
  int px(double x) const { return pad + int((x - lo[0]) * scale); }
  int py(double y) const { return pad + int((hi[1] - y) * scale); }
};

Rgba entity_colour(const std::string& cls) {
  if (cls.rfind("info_player", 0) == 0) return {80, 220, 110};
  if (cls.rfind("light", 0) == 0) return {255, 250, 200};
  if (cls.rfind("weapon_", 0) == 0) return {255, 150, 60};
  if (cls.rfind("item_", 0) == 0) return {250, 220, 70};
  return {180, 180, 180};
}

bool origin_of(const br::Entity& e, double o[3]) {
  std::istringstream in(e.get("origin"));
  return bool(in >> o[0] >> o[1] >> o[2]);
}

// The map from above: each column's highest floor shaded by height, water blue, open sky where a
// column reaches the sky; the entities as dots.
lab::Picture above(const br::Map& m, const Frame& f) {
  const int w = f.px(f.hi[0]) + f.pad, h = f.py(f.lo[1]) + f.pad;
  lab::Picture p = lab::create_picture(w, h, {22, 24, 30});
  const double step = 1 / f.scale;
  for (int y = f.pad; y < h - f.pad; ++y) {
    for (int x = f.pad; x < w - f.pad; ++x) {
      const double wx = f.lo[0] + (x - f.pad + 0.5) * step, wy = f.hi[1] - (y - f.pad + 0.5) * step;
      // Down from the top: the first open space, then the floor under it.
      bool open = false, sky = false, water = false;
      double floor_z = 0;
      for (double z = f.hi[2]; z >= f.lo[2]; z -= 4) {
        const int32_t c = contents_at(m, wx, wy, z);
        if (c == -6) sky = true;
        if (!open && (c == -1 || c == -3 || c == -4 || c == -5)) open = true;
        if (open && (c == -3 || c == -4 || c == -5)) water = true;
        if (open && (c == -2 || c == -6)) {
          floor_z = z;
          break;
        }
      }
      if (!open) continue;
      const double t = (floor_z - f.lo[2]) / std::max(1.0, f.hi[2] - f.lo[2]);
      const auto g = uint8_t(70 + 160 * std::clamp(t, 0.0, 1.0));
      Rgba c = water ? Rgba{60, 110, uint8_t(std::min(255, g + 40))} : Rgba{g, g, uint8_t(g + 8)};
      if (sky) c = {uint8_t(c.r * 0.85), uint8_t(c.g * 0.92), uint8_t(std::min(255, c.b + 30))};
      lab::set_pixel(p, x, y, c);
    }
  }
  for (const br::Entity& e : m.entities) {
    double o[3];
    if (!origin_of(e, o)) continue;
    const std::string& cls = e.get("classname");
    const Rgba c = entity_colour(cls);
    if (cls.rfind("info_player", 0) == 0) {
      lab::draw_ring(p, f.px(o[0]), f.py(o[1]), 5, c);
      lab::draw_ring(p, f.px(o[0]), f.py(o[1]), 4, c);
    } else {
      lab::fill_disc(p, f.px(o[0]), f.py(o[1]), cls.rfind("light", 0) == 0 ? 2 : 3, c);
    }
  }
  return p;
}

// A plan's source brushes sliced at height `z`: solid grey, liquid blue, open dark.
lab::Picture slice(const mc::Source& src, const Frame& f, double z) {
  const int w = f.px(f.hi[0]) + f.pad, h = f.py(f.lo[1]) + f.pad;
  lab::Picture p = lab::create_picture(w, h, {22, 24, 30});
  for (int y = f.pad; y < h - f.pad; ++y) {
    for (int x = f.pad; x < w - f.pad; ++x) {
      const mc::V3 at = {f.lo[0] + (x - f.pad + 0.5) / f.scale,
                         f.hi[1] - (y - f.pad + 0.5) / f.scale, z};
      uint32_t c = 0;
      for (const mc::Brush& b : src.entities[0].brushes) {
        bool in = true;
        for (const mc::Side& s : b.sides) in = in && mc::dist(s.plane, at) < 0;
        if (in) c = std::max(c, b.contents);
      }
      if (c == 0) continue;
      lab::set_pixel(p, x, y, mc::liquid(c) ? Rgba{60, 100, 200} : Rgba{110, 110, 118});
    }
  }
  return p;
}

bool read_file(const std::string& path, std::string* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), {});
  return true;
}

void run(lab::Lab& lab) {
  std::string plan_text = kSample, name = "sample";
  const std::string plan_path = lab.arg("plan");
  if (!plan_path.empty()) {
    std::string text, err;
    if (!read_file(plan_path, &text)) {
      lab.fail("map-build: cannot read " + plan_path, "next: pass --plan=<file>; skill: brush");
      return;
    }
    name = std::filesystem::path(plan_path).stem().string();
    if (std::filesystem::path(plan_path).extension() == ".svg") {
      if (!mc::svg_to_plan(text, &plan_text, &err)) {
        lab.fail("map-build: " + err, "next: fix the drawing; skill: brush");
        return;
      }
    } else {
      plan_text = text;
    }
  }
  mc::Source src;
  std::string err;
  if (!mc::read_plan(plan_text, &src, &err)) {
    lab.fail("map-build: " + err, "next: fix the plan; skill: brush");
    return;
  }
  mc::CompileOptions opt;
  mc::TextureLibrary lib;
  std::vector<uint8_t> palette = lab_palette();
  const std::string tex = lab.arg("textures");
  if (!tex.empty()) {
    if (!mc::load_texture_library(tex, &lib, &err)) {
      lab.fail("map-build: " + err, "next: pass --textures=<a game folder of packs>; skill: brush");
      return;
    }
    opt.textures = mc::finder_of(lib);
    for (const auto& entry : std::filesystem::directory_iterator(tex)) {
      if (entry.path().extension() != ".pak") continue;
      std::ifstream in(entry.path(), std::ios::binary);
      const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(in), {});
      ge::vfs::Pak pak;
      if (!ge::vfs::read_pak(bytes, &pak, &err)) continue;
      if (const ge::vfs::PakEntry* e = ge::vfs::pak_find(pak, "gfx/palette.lmp");
          e != nullptr && e->size == 768) {
        const auto b = ge::vfs::pak_file(bytes, *e);
        palette.assign(b.begin(), b.end());
        break;
      }
    }
  }
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  const auto t0 = std::chrono::steady_clock::now();
  const bool ok = mc::compile_map(src, opt, &bytes, &rep, &err);
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (!ok) {
    lab.fail("map-build: " + name + ": " + err, "next: fix the plan; skill: brush");
    return;
  }
  br::Map m;
  if (!br::read_bsp(bytes, &m, &err)) {
    lab.fail("map-build: the reader refuses the compiled " + name + ": " + err,
             "next: fix plugins/brush/compile/write29.cpp; skill: brush");
    return;
  }

  // The frame: the model's bounds.
  Frame f;
  for (int k = 0; k < 3; ++k) {
    f.lo[k] = m.models[0].mins[k];
    f.hi[k] = m.models[0].maxs[k];
  }
  f.scale = std::min(900.0 / (f.hi[0] - f.lo[0]), 700.0 / (f.hi[1] - f.lo[1]));
  const std::vector<std::pair<std::string, std::string>> in = {{"plan", name}};
  lab.shot({name + "-above", name + " compiled, from above",
            "The compiled map read back and sampled from above: each spot's highest floor shaded "
            "by its height (lighter higher), water blue, courts open to the sky tinted blue; "
            "starts green rings, lights pale dots, weapons orange, items yellow.",
            "every room the plan carves is open and joined to the rest; the pools blue; nothing "
            "open outside the walls (the outside is filled)",
            in},
           above(m, f));

  // Eyes at the starts.
  std::vector<br::labs::TexturedEye> eyes;
  for (const br::Entity& e : m.entities) {
    double o[3];
    if (e.get("classname").rfind("info_player", 0) != 0 || !origin_of(e, o)) continue;
    br::labs::TexturedEye eye;
    eye.at[0] = o[0];
    eye.at[1] = o[1];
    eye.at[2] = o[2] + 22;
    std::istringstream a(e.get("angle"));
    a >> eye.yaw_deg;
    eye.pitch_deg = -6;
    eyes.push_back(eye);
    if (eyes.size() == 4) break;
  }
  if (!eyes.empty()) {
    const int cw = 480, ch = 270, cols = 2;
    const int rows = int((eyes.size() + 1) / 2);
    lab::Picture p = lab::create_picture(cols * cw + 8, rows * ch + 8, {16, 16, 20});
    for (size_t i = 0; i < eyes.size(); ++i) {
      br::labs::draw_textured(p, 4 + int(i % 2) * cw, 4 + int(i / 2) * ch, cw - 4, ch - 4, m,
                              palette, eyes[i]);
    }
    lab.shot({name + "-inside", name + ": standing at the starts",
              "Eyes at head height at the map's first starts, looking the way each start faces, "
              "every surface drawn with the map's own pictures and lit only by the lightmaps the "
              "compiler baked (" +
                  std::string(tex.empty() ? "the compiler's patterns under the lab's palette"
                                          : "the game folder's pictures") +
                  ").",
              "walls, floors and ceilings meet with no gaps; light pools under the lamps and "
              "shadows fall behind solids; water is a flat sheet",
              in},
             p);
  }

  // The leak: the sample with the hall's west wall opened to nothing.
  if (plan_path.empty()) {
    // A plan cannot leak (its block encloses every room), so the gap is cut by hand: the solid
    // pieces west of the hall, where its outer wall stands, are taken away.
    mc::Source ls;
    std::string lerr;
    mc::read_plan(kSample, &ls, &lerr);
    if (!ls.entities.empty()) {
      std::vector<mc::Brush>& bs = ls.entities[0].brushes;
      const mc::V3 wall = {-16, 384, 48};
      std::erase_if(bs, [&](const mc::Brush& b) {
        for (const mc::Side& s : b.sides) {
          if (mc::dot(s.plane.n, wall) > s.plane.d) {
            return false;
          }
        }
        return true;
      });
    }
    std::vector<uint8_t> lb;
    mc::CompileReport lrep;
    const bool refused = !mc::compile_map(ls, opt, &lb, &lrep, &lerr) && lrep.leaked;
    lab.metric("leak-refused", refused ? 1 : 0, "count");
    Frame lf = f;
    lab::Picture p = slice(ls, lf, 120);
    for (size_t i = 0; i + 1 < lrep.leak_path.size(); ++i) {
      const mc::V3& a = lrep.leak_path[i];
      const mc::V3& b = lrep.leak_path[i + 1];
      lab::draw_line(p, lf.px(a[0]), lf.py(a[1]), lf.px(b[0]), lf.py(b[1]), {255, 70, 60}, 2);
    }
    if (!lrep.leak_path.empty()) {
      const mc::V3& e = lrep.leak_path.back();
      lab::draw_ring(p, lf.px(e[0]), lf.py(e[1]), 6, {255, 70, 60});
    }
    lab.note("leak: " + lerr);
    lab.shot({"leak",
              "a leak found and refused",
              "The sample with the west hall's outer wall taken away to nothing, sliced "
              "at standing height (solid grey, water blue): the compiler refuses it, and the red "
              "line is the way the open space reaches the void, from the start to the gap.",
              "the red line leaves the hall through the gap; the compiler said where to close it",
              {{"plan", "sample, west wall removed"}}},
             p);
  }

  lab::LabTable t{name + ": the compiled map",
                  {"brushes", "faces", "leaves", "nodes", "clip nodes", "luxels", "lights",
                   "textures", "bytes"},
                  {}};
  t.rows.push_back(
      {std::to_string(rep.brushes), std::to_string(rep.faces), std::to_string(rep.leaves),
       std::to_string(rep.nodes), std::to_string(rep.clip_nodes), std::to_string(rep.luxels),
       std::to_string(rep.lights), std::to_string(rep.textures), std::to_string(bytes.size())});
  lab.table(t);
  lab.metric("faces", double(rep.faces), "count");
  lab.metric("leaves", double(rep.leaves), "count");
  lab.metric("clip-nodes", double(rep.clip_nodes), "count");
  lab.metric("bytes", double(bytes.size()), "count");
  lab.metric("warnings", double(rep.warnings.size()), "count");
  lab.metric("faces-without-side", double(rep.faces_without_side), "count");
  lab.metric("compile-ms", ms, "ms", lab::Det::measured);
  for (const std::string& w : rep.warnings) lab.note("warning: " + w);
  if (lab.arg_bool("keep")) lab.attach(name + ".bsp", bytes);
}

const lab::LabSpec kSpec{
    "map-build",
    lab::LabKind::picture,
    "plugin:brush",
    "plugins/brush/compile (the map compiler: plans, SVG floor plans, the tree, hulls, light)",
    "brush",
    "Does a map plan compile into a sealed, lit version 29 map our reader takes, does it look "
    "like the plan from above and from inside, and is a leak found and refused with its way out?",
    {
        {"plan", "", "a map plan or an SVG floor plan to compile (empty: the lab's sample)"},
        {"textures", "",
         "a game folder whose packs' maps give the pictures and the palette (empty: patterns)"},
        {"keep", "0", "1: attach the compiled map (<name>.bsp)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
