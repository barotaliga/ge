// The map compiler's driver (compile.hpp): texinfos, the world and each brush entity compiled,
// lit and laid out.
//
// ge:summary Defines compile_map: texinfos, models' trees, the flood, hulls, light and the layout
// ge:keywords map-compiler compile_map driver texinfo brush-model leak entities lump
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/compile.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <tuple>

#include "brush/compile/parts.hpp"
#include "core/num_text.hpp"

namespace ge::brush::mapc {

namespace {

std::string num(double v) {
  if (v == std::nearbyint(v)) return std::to_string(int64_t(v));
  return core::num::to_fixed(v, 2);
}

std::string where(const V3& p) { return "(" + num(p[0]) + " " + num(p[1]) + " " + num(p[2]) + ")"; }

bool special(const std::string& name) {
  return name.rfind('*', 0) == 0 || name.rfind("sky", 0) == 0;
}

struct Texinfos {
  std::vector<std::string> names;
  std::vector<TexInfo> infos;
  std::map<std::string, uint32_t> name_of;
  std::map<std::tuple<std::array<uint32_t, 8>, uint32_t, int32_t>, int> info_of;

  int add(const Side& s) {
    auto it = name_of.find(s.texture);
    uint32_t t = 0;
    if (it == name_of.end()) {
      t = uint32_t(names.size());
      names.push_back(s.texture);
      name_of.emplace(s.texture, t);
    } else {
      t = it->second;
    }
    TexInfo ti;
    std::array<uint32_t, 8> key{};
    for (int a = 0; a < 2; ++a) {
      for (int k = 0; k < 4; ++k) {
        ti.vecs[a][k] = float(s.vecs[a][k]);
        std::memcpy(&key[(size_t(a) * 4) + size_t(k)], &ti.vecs[a][k], 4);
      }
    }
    ti.texture = t;
    ti.flags = special(s.texture) ? 1 : 0;
    const auto k = std::make_tuple(key, t, ti.flags);
    auto f = info_of.find(k);
    if (f != info_of.end()) return f->second;
    const int id = int(infos.size());
    infos.push_back(ti);
    info_of.emplace(k, id);
    return id;
  }
};

bool origin_of(const Entity& e, V3* out) {
  const std::string* o = e.get("origin");
  if (o == nullptr) return false;
  std::istringstream in(*o);
  return bool(in >> (*out)[0] >> (*out)[1] >> (*out)[2]);
}

// A model's brushes compiled: tree, portals, (flood), faces, hulls.
struct Built {
  std::unique_ptr<Tree> tree;
  Hull hulls[2];
  V3 lo = {1e30, 1e30, 1e30}, hi = {-1e30, -1e30, -1e30};
};

bool build_model(const std::vector<Brush>& brushes, bool world, const std::vector<Seed>& seeds,
                 Texinfos* tx, Planes* planes, Built* out, CompileReport* report,
                 std::string* error) {
  std::vector<Brush> drawn;  // in the point tree
  std::vector<std::vector<int>> infos;
  for (const Brush& b : brushes) {
    Brush c = b;
    if (!world && (c.contents == kTrigger || c.contents == kClip)) c.contents = kSolid;
    if (c.contents == kClip || c.contents == kTrigger) continue;
    std::vector<int> ti;
    ti.reserve(c.sides.size());
    for (const Side& s : c.sides) ti.push_back(tx->add(s));
    infos.push_back(std::move(ti));
    drawn.push_back(std::move(c));
  }
  std::vector<BrushSides> sides;
  std::vector<Piece> pieces = make_pieces(drawn, infos, planes, &sides, &report->warnings);
  for (const Piece& p : pieces) {
    for (size_t k = 0; k < 3; ++k) {
      out->lo[k] = std::min(out->lo[k], p.lo[k]);
      out->hi[k] = std::max(out->hi[k], p.hi[k]);
    }
  }
  if (pieces.empty()) {
    if (world) {
      *error = "the world has no brushes that close; next: give the map at least one room";
      return false;
    }
    out->lo = out->hi = {0, 0, 0};
  }
  report->brushes += pieces.size();
  V3 lo, hi;
  for (size_t k = 0; k < 3; ++k) {
    lo[k] = std::floor(out->lo[k]) - 16;
    hi[k] = std::ceil(out->hi[k]) + 16;
  }
  out->tree = std::make_unique<Tree>();
  build_tree(std::move(pieces), lo, hi, planes, out->tree.get());
  make_portals(planes, out->tree.get());
  report->portals += out->tree->portals.size();
  if (world) {
    Leak leak;
    if (!flood_fill(seeds, *planes, out->tree.get(), &leak)) {
      if (leak.entity < 0) {
        *error =
            "no entity stands inside the map to find its inside from; next: place a player "
            "start (info_player_start or info_player_deathmatch) in a room";
        return false;
      }
      report->leaked = true;
      report->leak_path = leak.path;
      *error = "the map leaks";
      return false;
    }
  }
  size_t missing = 0;
  make_faces(sides, tx->infos, out->tree.get(), &missing);
  report->faces_without_side += missing;
  merge_faces(out->tree.get(), *planes);
  subdivide_faces(out->tree.get(), *planes, tx->infos);
  finish_tree(out->tree.get());
  const V3 boxes[2][2] = {{{-16, -16, -24}, {16, 16, 32}}, {{-32, -32, -24}, {32, 32, 64}}};
  std::vector<Brush> stops;
  for (const Brush& b : brushes) {
    Brush c = b;
    if (!world) c.contents = kSolid;
    if (opaque(c.contents) || c.contents == kClip) stops.push_back(std::move(c));
  }
  for (int h = 0; h < 2; ++h) {
    out->hulls[h] = build_hull(stops, boxes[h][0], boxes[h][1], lo, hi, planes);
  }
  return true;
}

}  // namespace

bool compile_map(const Source& source, const CompileOptions& options, std::vector<uint8_t>* out,
                 CompileReport* report, std::string* error) {
  *report = CompileReport{};
  if (source.entities.empty()) {
    *error = "the source has no entities; next: start it with the world (worldspawn)";
    return false;
  }
  Source src = source;
  set_contents(&src);
  Planes planes;
  Texinfos tx;

  // Seeds: every point entity's origin.
  std::vector<Seed> seeds;
  for (size_t i = 1; i < src.entities.size(); ++i) {
    V3 o;
    if (src.entities[i].brushes.empty() && origin_of(src.entities[i], &o)) {
      seeds.push_back({o, int(i)});
    }
  }

  std::vector<Built> built(1);
  if (!build_model(src.entities[0].brushes, true, seeds, &tx, &planes, &built[0], report, error)) {
    if (report->leaked) {
      const Entity* e = nullptr;
      for (const Seed& s : seeds) {
        const int l = leaf_at(*built[0].tree, planes, s.at);
        if (built[0].tree->nodes[size_t(l)].reached >= 0) {
          e = &src.entities[size_t(built[0].tree->nodes[size_t(l)].reached)];
          break;
        }
      }
      V3 o = {0, 0, 0};
      std::string cls = "an entity";
      if (e != nullptr) {
        origin_of(*e, &o);
        if (const std::string* c = e->get("classname")) cls = *c;
      }
      report->leak_entity = cls + " at " + where(o);
      const size_t n = report->leak_path.size();
      const V3 last = n ? report->leak_path.back() : o;
      const V3 gap = n >= 2 ? report->leak_path[n - 2] : last;
      report->leak_path.insert(report->leak_path.begin(), o);  // the way starts at the entity
      *error = "the map leaks: the open space around " + report->leak_entity +
               " reaches the void, last through " + where(gap) +
               "; next: close the gap there (a wall, floor or ceiling with a hole or a missing "
               "brush)";
    }
    return false;
  }
  for (size_t i = 1; i < src.entities.size(); ++i) {
    Entity& e = src.entities[i];
    if (e.brushes.empty()) continue;
    Built b;
    if (!build_model(e.brushes, false, {}, &tx, &planes, &b, report, error)) return false;
    e.set("model", "*" + std::to_string(built.size()));
    built.push_back(std::move(b));
  }

  // Light.
  const LightSet lights = lights_of(src);
  report->lights = lights.lights.size();
  LightSet set = lights;
  if (!options.light) {
    set.lights.clear();
    set.sun.value = 0;
  }
  for (Built& b : built) {
    light_faces(set, *built[0].tree, planes, tx.infos, b.tree.get(), &report->luxels);
  }

  // Layout.
  Layout L;
  L.planes = &planes;
  L.textures = tx.names;
  L.texinfos = tx.infos;
  for (const std::string& name : tx.names) {
    TextureImage img;
    if (options.textures && options.textures(name, &img) && img.width > 0 && img.height > 0) {
      ++report->textures_found;
      L.images.push_back(std::move(img));
    } else {
      L.images.push_back(pattern_image(name));
    }
  }
  report->textures = tx.names.size();
  for (Built& b : built) {
    ModelOut m;
    m.tree = b.tree.get();
    m.hulls[0] = b.hulls[0];
    m.hulls[1] = b.hulls[1];
    m.lo = b.lo;
    m.hi = b.hi;
    L.models.push_back(m);
  }
  std::string text;
  for (const Entity& e : src.entities) {
    text += "{\n";
    for (const auto& [k, v] : e.keys) {
      text += '"';
      text += k;
      text += "\" \"";
      text += v;
      text += "\"\n";
    }
    text += "}\n";
  }
  L.entities = text;
  report->models = built.size();
  if (!write_v29_map(L, out, error, report)) {
    *error += "; next: split the map or make it simpler";
    return false;
  }
  report->planes = planes.size();
  return true;
}

}  // namespace ge::brush::mapc
