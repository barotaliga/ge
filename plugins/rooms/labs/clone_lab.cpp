// LAB `clone`: a classic map CLONED into a room level and set beside the original. The map (the
// brush plugin's fixture, or `--map` paths read out of a `--pak`) goes through the cloner with a
// texture map (`--texmap`, an agent-approved `.texmap`; without one every texture is a placeholder
// drawn in its mean colour), the clone's brushes are built by CSG and drawn from the same eyes as
// the original's textured view, and a body is stood at every start in the clone's mover world.
// The numbers: cells, brushes, faces, unseen faces, groups, entities, the level's bytes, the clone
// and build times, and whether a body put down at each start stands on the original's floor there.
//
// Nothing a map holds is written into the tree: the pictures land in previews/, which is not
// committed, and the clone is written out only with --write (a path outside the tree).
//
// ge:summary Lab: a classic map cloned into a room level, beside the original, starts stood on
// ge:keywords lab clone bsp texmap side-by-side original brushes csg start stand walk compare
// ge:skill rooms brush labs
// ge:test tests/rooms_clone_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "brush/labs/textured_view.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "lab/lab.hpp"
#include "lab/model_view.hpp"
#include "lab/raster.hpp"
#include "move/spec.hpp"
#include "move/step.hpp"
#include "rooms/brush_build.hpp"
#include "rooms/clip.hpp"
#include "rooms/clone.hpp"
#include "rooms/codec.hpp"
#include "rooms/curves.hpp"
#include "rooms/movers.hpp"
#include "rooms/text.hpp"
#include "vfs/pak.hpp"

namespace {

namespace rm = ge::rooms;
namespace br = ge::brush;
namespace lab = ge::lab;
using lab::Det;
using lab::Picture;
using lab::Rgba;

constexpr Rgba kDark{22, 24, 30};
constexpr double kPi = 3.14159265358979323846;

std::string fixed(double v, int d) { return ge::core::num::to_fixed(v, d); }

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void label(Picture& p, int x, int y, const std::string& text) {
  lab::fill_rect(p, x, y, lab::text_width(text, 2) + 10, 22, Rgba{0, 0, 0, 160});
  lab::draw_text(p, x + 5, y + 4, text, Rgba{240, 240, 240}, 2);
}

ge::assets::Model model_of(const rm::Level& level) {
  ge::assets::Model m;
  for (const rm::Material& mat : level.materials) {
    ge::assets::Material out;
    out.name = mat.name;
    for (size_t k = 0; k < 3; ++k) {
      // The mean colour as it is (no squaring toward linear): a far face of a dark map reads.
      out.rgb[k] = float(mat.rgb[k]) / 255.0f;
    }
    m.materials.push_back(out);
  }
  return m;
}

// The brush mesh's drawn faces as a shape (sky and hidden faces left out; with `cut`, every face
// looking down too, so the rooms show from above).
lab::Shape shape_of(const rm::BrushMesh& m, bool cut = false) {
  lab::Shape s;
  for (size_t t = 0; t < m.triangles(); ++t) {
    if (m.hidden[t] || m.contents[t] == rm::Contents::sky) continue;
    if (cut && m.nrm[size_t(m.idx[t * 3]) * 3 + 1] < -0.3f) continue;
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = size_t(m.idx[t * 3 + k]) * 3;
      s.idx.push_back(uint32_t(s.part.size()));
      for (size_t a = 0; a < 3; ++a) s.pos.push_back(m.pos[v + a]), s.nrm.push_back(m.nrm[v + a]);
      s.part.push_back(m.material[t]);
      s.top.push_back(0);
    }
  }
  return s;
}

// The drawn triangles of every curve as a shape, each in the highlight material after the
// level's own (`base` convex, `base + 1` concave), lifted 2 cm off its face so it draws over it.
lab::Shape curves_shape(const rm::Level& level, const rm::BrushMesh& mesh,
                        const std::vector<rm::Curve>& curves, uint8_t base) {
  std::vector<int8_t> kind(level.faces.size(), -1);
  for (const rm::Curve& c : curves) {
    for (const uint32_t f : c.faces) kind[f] = c.concave ? 1 : 0;
  }
  lab::Shape s;
  for (size_t t = 0; t < mesh.triangles(); ++t) {
    const uint32_t face = mesh.face[t];
    if (kind[face] < 0 || mesh.hidden[t]) continue;
    const rm::Face& f = level.faces[face];
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = size_t(mesh.idx[t * 3 + k]) * 3;
      s.idx.push_back(uint32_t(s.part.size()));
      for (size_t a = 0; a < 3; ++a) {
        s.pos.push_back(float(double(mesh.pos[v + a]) + f.n[a] * 0.02));
        s.nrm.push_back(float(f.n[a]));
      }
      s.part.push_back(uint8_t(base + uint8_t(kind[face])));
      s.top.push_back(0);
    }
  }
  return s;
}

lab::ViewStyle style() {
  lab::ViewStyle st;
  st.floor = false;
  st.background = kDark;
  return st;
}

struct Eye {
  std::string name;
  br::labs::TexturedEye eye;  // file units
};

std::vector<Eye> eyes_of(const br::Map& map) {
  std::vector<Eye> out;
  std::vector<const br::Entity*> others;
  const auto eye_at = [](const br::Entity& e) {
    br::labs::TexturedEye eye;
    double o[3] = {0, 0, 0};
    std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &o[0], &o[1], &o[2]);
    for (size_t k = 0; k < 3; ++k) eye.at[k] = o[k];
    eye.at[2] += 22;
    eye.yaw_deg = std::atof(e.get("angle").c_str());
    eye.fov_deg = 90;
    return eye;
  };
  for (const br::Entity& e : map.entities) {
    const std::string& c = e.get("classname");
    if (c == "info_player_start" && out.empty()) out.push_back({"THE START", eye_at(e)});
    if (c == "info_player_deathmatch" || c == "info_player_coop") others.push_back(&e);
  }
  for (size_t i = 0; i < 3 && i < others.size(); ++i) {
    const size_t at = others.size() <= 3 ? i : i * (others.size() - 1) / 2;
    out.push_back({"START " + std::to_string(at + 1), eye_at(*others[at])});
  }
  if (out.empty() && !map.models.empty()) {
    // A map with no starts (the fixture): its middle, looking along +x.
    Eye e{"THE MIDDLE", {}};
    for (size_t k = 0; k < 3; ++k) {
      e.eye.at[k] = (double(map.models[0].mins[k]) + double(map.models[0].maxs[k])) / 2;
    }
    out.push_back(e);
  }
  return out;
}

lab::ViewCam cam_of(const br::labs::TexturedEye& e) {
  const float at[3] = {float(e.at[0]), float(e.at[1]), float(e.at[2])};
  double p[3];
  br::to_engine(at, p);
  const double a = e.yaw_deg * kPi / 180;
  // Along the yaw in the map's x/y, which is the engine's x/z.
  const double dx = ge::core::libm::cos(a), dz = ge::core::libm::sin(a);
  lab::ViewCam cam;
  for (size_t k = 0; k < 3; ++k) cam.eye[k] = p[k];
  cam.target[0] = p[0] + dx, cam.target[1] = p[1], cam.target[2] = p[2] + dz;
  // The textured view's 90 degrees across a panel 8 wide and 5 high, as the height's angle.
  cam.fov_deg = 64.0;
  return cam;
}

// The height of the original's highest floor under (x, z) at or below `y` (engine metres): the
// near-level triangles of the world and its brush models (where the map put them) whose plan
// covers the point. False when there is none.
bool floor_below(const br::Map& map, double x, double y, double z, double* out) {
  bool found = false;
  for (const br::Surface& s : map.surfaces) {
    for (uint32_t t = 0; t < s.count / 3; ++t) {
      double p[3][3];
      for (uint32_t k = 0; k < 3; ++k) {
        br::to_engine(map.vertices[map.indices[s.first + t * 3 + k]].pos, p[k]);
      }
      const double ux = p[1][0] - p[0][0], uy = p[1][1] - p[0][1], uz = p[1][2] - p[0][2];
      const double vx = p[2][0] - p[0][0], vy = p[2][1] - p[0][1], vz = p[2][2] - p[0][2];
      const double ny = uz * vx - ux * vz;  // the normal's y, unnormalised
      const double nx = uy * vz - uz * vy, nz = ux * vy - uy * vx;
      const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
      if (len < 1e-12 || std::fabs(ny) < 0.7 * len) continue;
      // Barycentric in the plan.
      const double det = ux * vz - uz * vx;
      if (std::fabs(det) < 1e-12) continue;
      const double wx = x - p[0][0], wz = z - p[0][2];
      const double a = (wx * vz - wz * vx) / det, b = (ux * wz - uz * wx) / det;
      if (a < -1e-6 || b < -1e-6 || a + b > 1 + 1e-6) continue;
      const double h = p[0][1] + a * uy + b * vy;
      if (h > y + 1e-3) continue;  // the highest level face below is the floor, whichever way wound
      if (!found || h > *out) *out = h, found = true;
    }
  }
  return found;
}

// An eye facing curve `c`: off the middle of its sweep on its open side (outside a pillar, under
// an arch), looking at that middle.
lab::ViewCam curve_cam(const rm::Level& level, const rm::Curve& c) {
  double mean[3] = {0, 0, 0};
  for (const uint32_t f : c.faces) {
    for (size_t k = 0; k < 3; ++k) mean[k] += level.faces[f].n[k];
  }
  double l = std::sqrt(mean[0] * mean[0] + mean[1] * mean[1] + mean[2] * mean[2]);
  if (l < 1e-6 * double(c.faces.size())) {
    for (size_t k = 0; k < 3; ++k) mean[k] = level.faces[c.faces.front()].n[k];
    l = 1;
  }
  double n[3] = {mean[0] / l, mean[1] / l, mean[2] / l};
  if (c.sweep_deg >= 359.9) {
    // All the way round: the facet facing most nearly level.
    for (const uint32_t f : c.faces) {
      if (std::fabs(level.faces[f].n[1]) < std::fabs(n[1])) {
        for (size_t k = 0; k < 3; ++k) n[k] = level.faces[f].n[k];
      }
    }
  }
  lab::ViewCam cam;
  const double side = c.concave ? -1 : 1;
  const double back = c.concave ? c.radius + 2 : 3;
  for (size_t k = 0; k < 3; ++k) {
    cam.target[k] = c.centre[k] + side * n[k] * c.radius;
    cam.eye[k] = cam.target[k] + n[k] * back;
  }
  if (std::fabs(n[1]) > 0.85) {
    // Under an arch or over a beam: stand off along the axis too, so the view is not straight up
    // or down.
    for (size_t k = 0; k < 3; ++k) cam.eye[k] -= c.axis[k] * back;
  }
  return cam;
}

// The map's eye for an engine camera (file units, z up; yaw and pitch toward the target).
br::labs::TexturedEye eye_of(const lab::ViewCam& cam) {
  br::labs::TexturedEye e;
  e.at[0] = cam.eye[0] / br::kUnitM, e.at[1] = cam.eye[2] / br::kUnitM;
  e.at[2] = cam.eye[1] / br::kUnitM;
  const double dx = cam.target[0] - cam.eye[0], dy = cam.target[1] - cam.eye[1];
  const double dz = cam.target[2] - cam.eye[2];
  e.yaw_deg = ge::core::libm::atan2(dz, dx) * 180 / kPi;
  e.pitch_deg = ge::core::libm::atan2(dy, std::sqrt(dx * dx + dz * dz)) * 180 / kPi;
  e.fov_deg = 90;
  return e;
}

struct Loaded {
  std::string name;
  std::vector<uint8_t> bytes;
  br::Map map;
};

void run(lab::Lab& lab) {
  const int px = int(lab.arg_num("px"));
  const std::string maps_arg = lab.arg("map");
  const std::string pak_path = lab.arg("pak");
  const std::string texmap_path = lab.arg("texmap");
  const bool smooth = lab.arg("smooth") == "1";
  const bool merge = lab.arg("merge") != "0";
  std::vector<uint8_t> pak_bytes, palette;
  std::vector<Loaded> maps;
  if (pak_path.empty()) {
    Loaded l;
    l.name = "fixture";
    std::string error;
    if (!br::write_bsp(br::sample_fixture(), br::Family::v29, &l.bytes, &error)) {
      lab.fail("clone: writing the fixture: " + error, "next: fix plugins/brush; skill: brush");
      return;
    }
    maps.push_back(std::move(l));
  } else {
    std::ifstream in(pak_path, std::ios::binary);
    pak_bytes.assign(std::istreambuf_iterator<char>(in), {});
    ge::vfs::Pak pak;
    std::string error;
    if (pak_bytes.empty() || !ge::vfs::read_pak(pak_bytes, &pak, &error)) {
      lab.fail("clone: cannot read the pack " + pak_path + " " + error,
               "next: pass --pak=<file.pak> (outside the tree) and --map=maps/<name>.bsp");
      return;
    }
    if (const auto* e = ge::vfs::pak_find(pak, "gfx/palette.lmp"); e != nullptr && e->size == 768) {
      const auto b = ge::vfs::pak_file(pak_bytes, *e);
      palette.assign(b.begin(), b.end());
    }
    size_t from = 0;
    while (from <= maps_arg.size()) {
      const size_t to = std::min(maps_arg.find(',', from), maps_arg.size());
      const std::string name = maps_arg.substr(from, to - from);
      from = to + 1;
      const auto* e = ge::vfs::pak_find(pak, name);
      if (e == nullptr) {
        std::string what = "clone: ";
        what += pak_path;
        what += " holds no ";
        what += name;
        lab.fail(what, "next: pass --map=maps/<name>.bsp, comma separated");
        return;
      }
      const auto b = ge::vfs::pak_file(pak_bytes, *e);
      Loaded l;
      l.name = name;
      l.bytes.assign(b.begin(), b.end());
      maps.push_back(std::move(l));
    }
  }

  lab::LabTable table{
      "CLONED MAPS (cells met, merges, brushes kept, faces, unseen: no surface on "
      "them)",
      {"map", "cells", "merged", "brushes", "faces", "unseen", "open", "groups", "entities",
       "bytes", "placeholders", "starts stood"},
      {}};
  lab::LabTable times{
      "CLONE AND BUILD TIMES, ms (measured)", {"map", "clone", "build", "curves"}, {}};
  lab::LabTable curve_table{
      "CURVES FOUND (runs of faces faceting a round surface: convex a pillar, concave an arch "
      "or vault; full round: all the way round)",
      {"map", "convex", "concave", "full round"},
      {}};
  const size_t workers = ge::core::default_workers();
  for (Loaded& l : maps) {
    std::string error;
    if (!br::read_bsp(l.bytes, &l.map, &error)) {
      lab.fail("clone: reading " + l.name + ": " + error, "skill: brush");
      return;
    }
    std::string stem = l.name.substr(l.name.rfind('/') + 1);
    stem = stem.substr(0, stem.rfind('.'));
    rm::TextureMap tm;
    rm::CloneOptions opt;
    opt.palette = palette;
    opt.smooth = smooth;
    opt.merge = merge;
    opt.workers = workers;
    if (!texmap_path.empty()) {
      std::string file = texmap_path;
      std::string in_dir = texmap_path;
      in_dir += '/';
      in_dir += stem;
      in_dir += "/textures.texmap";
      if (std::ifstream(in_dir)) file = in_dir;
      std::ifstream in(file);
      std::stringstream text;
      text << in.rdbuf();
      if (!in || !rm::parse_texture_map(text.str(), &tm, &error)) {
        std::string what = "clone: the texture map ";
        what += file;
        what += ' ';
        what += error;
        lab.fail(what, "next: pass --texmap=<file.texmap or the matcher's directory>");
        return;
      }
      opt.placeholders = lab.arg("placeholders") == "1";
    } else {
      opt.placeholders = true;
    }
    rm::Level level;
    rm::CloneReport rep;
    auto t0 = std::chrono::steady_clock::now();
    if (!rm::clone_map(l.map, tm, opt, &level, &rep, &error)) {
      lab.fail(l.name + ": " + error, "skill: rooms");
      return;
    }
    const double clone_ms = ms_since(t0);
    t0 = std::chrono::steady_clock::now();
    const rm::BrushMesh mesh = rm::build_brushes(level, workers);
    const double build_ms = ms_since(t0);
    const std::vector<uint8_t> bytes = rm::encode(level);
    if (const std::string out = lab.arg("write"); !out.empty()) {
      std::string base = out;
      base += '/';
      base += stem;
      std::ofstream(base + ".gelv", std::ios::binary)
          .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
      std::ofstream(base + ".gelv.txt") << rm::print(level);
    }

    // The clone's moving parts, as movers.hpp reads them.
    rm::Movers movers;
    std::string merr;
    if (!rm::build_movers(level, workers, &movers, &merr)) {
      std::printf("  %s: movers: %s\n", stem.c_str(), merr.c_str());
    }
    size_t kinds[4] = {0, 0, 0, 0};
    for (const rm::Mover& mv : movers.movers) ++kinds[size_t(mv.kind)];
    lab.metric(stem + "-doors", double(kinds[0]), "count");
    lab.metric(stem + "-plats", double(kinds[1]), "count");
    lab.metric(stem + "-buttons", double(kinds[2]), "count");
    lab.metric(stem + "-trains", double(kinds[3]), "count");
    lab.metric(stem + "-triggers", double(movers.triggers.size()), "count");
    lab.metric(stem + "-groups-not-moved", double(movers.skipped), "count");

    // The runs of faces that facet an arch or a pillar (curves.hpp).
    t0 = std::chrono::steady_clock::now();
    const std::vector<rm::Curve> curves = rm::find_curves(level, mesh);
    const double curves_ms = ms_since(t0);
    size_t pillars = 0, arches = 0, round = 0;
    for (const rm::Curve& c : curves) {
      ++(c.concave ? arches : pillars);
      if (c.sweep_deg >= 359.9) ++round;
    }
    lab.metric(stem + "-curves-convex", double(pillars), "count");
    lab.metric(stem + "-curves-concave", double(arches), "count");
    lab.metric(stem + "-curves-full-round", double(round), "count");
    curve_table.rows.push_back(
        {l.name, std::to_string(pillars), std::to_string(arches), std::to_string(round)});
    for (size_t i = 0; i < curves.size() && i < size_t(lab.arg_num("list")); ++i) {
      const rm::Curve& c = curves[i];
      std::printf(
          "  %s curve %zu: %s, %zu facets, %zu faces, r %.3f m, sweep %.1f, length %.2f m, "
          "axis %.2f %.2f %.2f, centre %.2f %.2f %.2f\n",
          stem.c_str(), i, c.concave ? "concave" : "convex", c.facets, c.faces.size(), c.radius,
          c.sweep_deg, c.length, c.axis[0], c.axis[1], c.axis[2], c.centre[0], c.centre[1],
          c.centre[2]);
    }

    // A body stood at every start in the clone's mover world.
    const ge::move::MeshWorld w = rm::clip_world(level, workers);
    const ge::move::World world = ge::move::as_world(w);
    const ge::move::MoveSpec spec = *ge::move::find_preset("walker");
    size_t starts = 0, stood = 0;
    double worst = 0;
    for (const rm::Entity& e : level.entities) {
      if (e.classname.rfind("info_player_", 0) != 0) continue;
      double o[3] = {0, 0, 0};
      if (std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &o[0], &o[1], &o[2]) != 3) continue;
      ++starts;
      // The original's floor under the start (a start is placed at any height over it), and a body
      // put down on it in the clone: it stands on the same floor, or the clone lost it.
      // The floor is the highest under the body's footprint (its centre and its four corners), so a
      // start at a step's edge stands on the step.
      const ge::move::Box box = ge::move::hull(spec, false);
      double floor = 0;
      bool any = false;
      const double at[5][2] = {{o[0], o[2]},
                               {o[0] + box.mins.x, o[2] + box.mins.z},
                               {o[0] + box.mins.x, o[2] + box.maxs.z},
                               {o[0] + box.maxs.x, o[2] + box.mins.z},
                               {o[0] + box.maxs.x, o[2] + box.maxs.z}};
      for (const auto& xz : at) {
        double h = 0;
        if (floor_below(l.map, xz[0], o[1], xz[1], &h) && (!any || h > floor))
          floor = h, any = true;
      }
      if (!any) {
        std::printf("  %s %s at %s: no floor under it in the original\n", stem.c_str(),
                    e.classname.c_str(), e.get("origin").c_str());
        continue;
      }
      const ge::move::MoveState s = ge::move::stand_at(spec, world, {o[0], floor + 0.3, o[2]});
      const double off = s.pos.y - floor;
      if (s.grounded && std::fabs(off) < 0.05) {
        ++stood;
      } else {
        std::printf("  %s %s at %s: floor %.3f, stood %s at %.3f\n", stem.c_str(),
                    e.classname.c_str(), e.get("origin").c_str(), floor,
                    s.grounded ? "grounded" : "in the air", s.pos.y);
      }
      worst = std::max(worst, std::fabs(off));
    }
    lab.metric(stem + "-brushes", double(rep.brushes), "count");
    lab.metric(stem + "-merged", double(rep.merged), "count");
    lab.metric(stem + "-faces", double(rep.faces), "count");
    lab.metric(stem + "-unseen", double(rep.unseen), "count");
    lab.metric(stem + "-open", double(rep.open), "count");
    lab.metric(stem + "-triangles", double(mesh.triangles()), "count");
    lab.metric(stem + "-bytes", double(bytes.size()), "count");
    lab.metric(stem + "-starts-stood", double(stood), "count");
    lab.metric(stem + "-start-worst-m", worst, "m");
    table.rows.push_back({l.name, std::to_string(rep.cells), std::to_string(rep.merged),
                          std::to_string(rep.brushes), std::to_string(rep.faces),
                          std::to_string(rep.unseen), std::to_string(rep.open),
                          std::to_string(rep.groups), std::to_string(rep.entities),
                          std::to_string(bytes.size()), std::to_string(rep.placeholders.size()),
                          std::to_string(stood) + " of " + std::to_string(starts)});
    times.rows.push_back({l.name, fixed(clone_ms, 1), fixed(build_ms, 1), fixed(curves_ms, 1)});

    // The original and the clone from the same eyes.
    const auto eyes = eyes_of(l.map);
    const auto model = model_of(level);
    const lab::Shape shape = shape_of(mesh);
    const int cw = px, ch = px * 5 / 8;
    const bool textured = !palette.empty();
    Picture p = lab::create_picture(cw * 2, ch * int(eyes.size()), kDark);
    for (size_t i = 0; i < eyes.size(); ++i) {
      const int y = int(i) * ch;
      if (textured) {
        br::labs::draw_textured(p, 0, y, cw, ch, l.map, palette, eyes[i].eye);
      }
      label(p, 4, y + 4, eyes[i].name + (textured ? ": THE MAP" : ""));
      lab::draw_shape(p, cw, y, cw, ch, cam_of(eyes[i].eye), model, shape, style());
      label(p, cw + 4, y + 4, "THE CLONE");
    }
    std::string id = "clone-" + stem;
    for (char& c : id) c = char(std::tolower(uint8_t(c)));
    lab.shot({id,
              "CLONED: " + l.name,
              "Left the map as it is drawn (when the pack's palette is there), right its clone: "
              "the cells of its tree as brushes, built by CSG, each face in its texture's mean "
              "colour (the procedural texture comes later), from the same eyes.",
              "the same walls, floors, steps and openings on both sides; nothing missing or "
              "doubled; no face showing through a wall",
              {{"pak", pak_path}, {"map", l.name}, {"texmap", texmap_path}}},
             p);
    {
      const lab::Shape cut = shape_of(mesh, true);
      Picture o = lab::create_picture(px * 2, px * 4 / 3, kDark);
      lab::draw_shape(o, 0, 0, px * 2, px * 4 / 3,
                      lab::orbit_cam(lab::shape_box(cut), 30, 60, 30, 0.95, 1.5), model, cut,
                      style());
      lab.shot({id + "-whole",
                "THE CLONE CUT AWAY: " + l.name,
                "The whole clone from above with every ceiling, roof and sky left off, so the "
                "rooms, corridors and liquids show from inside.",
                "the map's plan reads; no stray slabs far from it",
                {{"map", l.name}}},
               o);
    }
    if (!curves.empty()) {
      // Close-ups of the curves found, the most facets first: the map from an eye facing each one
      // on the left, the clone with every curve painted on the right (convex orange, concave
      // cyan).
      ge::assets::Model hm = model;
      const uint8_t base = uint8_t(hm.materials.size());
      ge::assets::Material convex, concave;
      convex.name = "curve-convex";
      convex.rgb[0] = 1.0f, convex.rgb[1] = 0.55f, convex.rgb[2] = 0.1f;
      concave.name = "curve-concave";
      concave.rgb[0] = 0.15f, concave.rgb[1] = 0.85f, concave.rgb[2] = 1.0f;
      hm.materials.push_back(convex);
      hm.materials.push_back(concave);
      lab::Shape painted = shape;
      const lab::Shape hl = curves_shape(level, mesh, curves, base);
      const uint32_t at = uint32_t(painted.part.size());
      painted.pos.insert(painted.pos.end(), hl.pos.begin(), hl.pos.end());
      painted.nrm.insert(painted.nrm.end(), hl.nrm.begin(), hl.nrm.end());
      painted.part.insert(painted.part.end(), hl.part.begin(), hl.part.end());
      painted.top.insert(painted.top.end(), hl.top.begin(), hl.top.end());
      for (const uint32_t i : hl.idx) painted.idx.push_back(at + i);
      std::vector<size_t> order(curves.size());
      for (size_t i = 0; i < order.size(); ++i) order[i] = i;
      std::stable_sort(order.begin(), order.end(), [&curves](size_t x, size_t y) {
        return curves[x].facets > curves[y].facets;
      });
      const size_t shown = std::min<size_t>(order.size(), size_t(lab.arg_num("closeups")));
      const rm::BrushMesh rounded = rm::round_curves(level, mesh, curves, lab.arg_num("segment"));
      const lab::Shape round_shape = shape_of(rounded);
      lab.metric(stem + "-rounded-triangles", double(rounded.triangles()), "count");
      Picture o = lab::create_picture(cw * 3, ch * int(shown), kDark);
      for (size_t r = 0; r < shown; ++r) {
        const rm::Curve& c = curves[order[r]];
        lab::ViewCam cam = curve_cam(level, c);
        cam.fov_deg = 64.0;
        const int y = int(r) * ch;
        if (textured) {
          br::labs::draw_textured(o, 0, y, cw, ch, l.map, palette, eye_of(cam));
        }
        lab::draw_shape(o, cw, y, cw, ch, cam, hm, painted, style());
        lab::draw_shape(o, cw * 2, y, cw, ch, cam, model, round_shape, style());
        label(o, cw * 2 + 4, y + 4, "DRAWN ROUND");
        label(o, 4, y + 4,
              std::string(c.concave ? "CONCAVE" : "CONVEX") + ", " + std::to_string(c.facets) +
                  " FACETS, R " + fixed(c.radius, 2) + " M, " + fixed(c.sweep_deg, 0) + " DEG");
      }
      lab.shot({id + "-curves",
                "CURVES FOUND: " + l.name,
                std::to_string(pillars) +
                    " convex runs (orange: the solid inside the bend, a "
                    "pillar or a rounded corner) and " +
                    std::to_string(arches) +
                    " concave (cyan: the open side inside, an arch or a vault); the ones with the "
                    "most facets close up: the map, the clone painted, the clone drawn round "
                    "(round_curves: cut every `segment` degrees onto the facets' circle).",
                "the painted faces facet one round surface each; no flat wall, stair or chamfer "
                "painted",
                {{"map", l.name}}},
               o);
    }
  }
  lab.table(table);
  lab.table(curve_table);
  lab.table(times, Det::measured);
}

const lab::LabSpec kSpec{
    "clone",
    lab::LabKind::picture,
    "plugin:rooms",
    "plugins/rooms",
    "rooms",
    "Does a classic map cloned into a room level keep its structure, and does a body stand in it?",
    {
        {"pak", "", "a pack archive (outside the tree) --map's paths are read from"},
        {"map", "", "maps inside --pak, comma separated (default: the brush plugin's fixture)"},
        {"texmap", "",
         "an approved texture map, or the matcher's directory holding <map>/textures.texmap "
         "(default: every texture a placeholder)"},
        {"placeholders", "0", "1: a texture the map does not name is drawn plain, not refused"},
        {"smooth", "0", "1: every face shaded round with its smooth neighbours"},
        {"merge", "1", "0: every leaf its own brush (1: a subtree of one contents as one brush)"},
        {"write", "", "a directory OUTSIDE the tree the clones are written to (.gelv, .gelv.txt)"},
        {"list", "0", "print the first N curves found on each map"},
        {"segment", "7.5", "degrees a curve turns between the cuts it is drawn round with"},
        {"closeups", "6", "the curves drawn close up on each map, the most facets first"},
        {"px", "480", "a panel's width, px"},
    },
    "--px=160",
    run,
};
GE_LAB(kSpec);

}  // namespace
