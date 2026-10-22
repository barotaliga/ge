// LAB `real-hints`: THE REAL FACES' HINTS (R25), read and baked. Reading (the default) decodes
// every face's hints from the tree (real_hints/) and counts them: the real lifts by kind, the
// real pistes by grade and their length, and the woods (the tree line in real metres and the
// cover of each band of the face's height). Each face named alone is drawn with its hints over
// its shaded relief. A claim that "the real ski area on alpine-7 has nine chairs" is a claim
// about a row here.
//
// BAKING (`--bake`) is the offline half that writes the tree's hint files: each face's window is
// read off the open map data (osm_data.hpp; `--fetch` downloads the boxes and relations it needs
// into `--osm`, once, from the map's public API, which needs no credentials) and only what a
// generator can lean on is kept: every lift's kind and two ends (the bottom the lower on the
// face), every downhill piste's grade and its line top first, thinned to the bends that keep it
// within `kStray` m, with its width at each bend (straight across to the edges of the narrowest
// mapped piste area it lies in), and the woods: the forest the map draws rasterised onto the
// face's height grid and binned by height into the tree line (the top of the highest of
// `kLineBins` bins still `kLineCover` wooded) and `kWoodsBands` bands' cover. `--write` writes
// the faces baked and the index (plugins/mountain/mapgen/real_hints/), run from the repository's
// root. A hint file is GENERATED: never edited by hand. Nothing is kept by name.
//
// The map data: © OpenStreetMap contributors, under the Open Database Licence (ODbL 1.0); the
// baked hints are a derived database under the same licence.
//
// ge:summary Lab: the real faces' lifts, pistes and woods, read, drawn and baked off the map data
// ge:keywords lab real-hints bake openstreetmap lifts pistes woods tree-line r25 picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/osm_data.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/mapgen/regions.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace fs = std::filesystem;
using ge::lab::Rgba;
using ge::mountain::labs::OsmData;
using ge::mountain::labs::OsmWay;

constexpr const char* kTree = "plugins/mountain/mapgen/real_hints";
constexpr int kN = mg::kFaceN;
constexpr double kCell = mg::kFaceCell;
constexpr double kSize = mg::kFaceSize;

// How far a piste's kept line may stray from its real one, m; the shortest piste kept, m; a
// piste area wider than this across its line is a bowl mapped whole and says nothing of the
// piste's own width, m.
constexpr double kStray = 6;
constexpr double kShortest = 60;
constexpr double kWidest = 160;
// The woods: the fine bins the tree line is read in, the fewest cells a bin is read on, the cover
// a bin must keep to stand under the line, and the least share of the window wooded for a face to
// have woods of its own.
constexpr int kLineBins = 48;
constexpr int kLineCells = 12;
constexpr double kLineCover = 0.05;
constexpr double kLeast = 0.01;

using Pt = mg::PlanPoint;
using Ring = std::vector<Pt>;

std::string f0(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", v);
  return b;
}
std::string f2(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}

// The faces asked for: "all", or ids comma-separated.
std::vector<const mg::FaceCrop*> faces_of(ge::lab::Lab& lab) {
  std::vector<const mg::FaceCrop*> out;
  const std::string& asked = lab.arg("faces");
  if (asked == "all") {
    for (const mg::FaceCrop& c : mg::kFaceCrops) out.push_back(&c);
    return out;
  }
  std::stringstream in(asked);
  std::string id;
  while (std::getline(in, id, ',')) {
    if (const mg::FaceCrop* c = mg::face_crop(id)) {
      out.push_back(c);
    } else {
      lab.fail("--faces names " + id + ", which no face is",
               "next: name faces by their ids in plugins/mountain/mapgen/real_face_crops.hpp.");
    }
  }
  return out;
}

bool on_map(const Pt& p) { return p.x >= 0 && p.x <= kSize && p.z >= 0 && p.z <= kSize; }

double length_of(const std::vector<Pt>& pts) {
  double s = 0;
  for (size_t i = 1; i < pts.size(); i++) {
    s += ge::core::hypot(pts[i].x - pts[i - 1].x, pts[i].z - pts[i - 1].z);
  }
  return s;
}

// A line thinned to the bends that keep it within `tol` m (Douglas–Peucker).
std::vector<Pt> thin(const std::vector<Pt>& pts, double tol) {
  if (pts.size() < 3) return pts;
  const Pt a = pts.front();
  const Pt b = pts.back();
  const double dx = b.x - a.x;
  const double dz = b.z - a.z;
  const double len = std::max(1e-9, ge::core::hypot(dx, dz));
  double worst = 0;
  size_t at = 0;
  for (size_t i = 1; i + 1 < pts.size(); i++) {
    const double d = std::abs((pts[i].x - a.x) * dz - (pts[i].z - a.z) * dx) / len;
    if (d > worst) {
      worst = d;
      at = i;
    }
  }
  if (worst <= tol) return {a, b};
  std::vector<Pt> left = thin({pts.begin(), pts.begin() + long(at) + 1}, tol);
  const std::vector<Pt> right = thin({pts.begin() + long(at), pts.end()}, tol);
  left.pop_back();
  left.insert(left.end(), right.begin(), right.end());
  return left;
}

// The longest run of a line's points that stay on the map.
std::vector<Pt> longest_on_map(const std::vector<Pt>& pts) {
  std::vector<Pt> best;
  std::vector<Pt> cur;
  for (const Pt& p : pts) {
    if (on_map(p)) {
      cur.push_back(p);
      continue;
    }
    if (cur.size() > best.size()) best = cur;
    cur.clear();
  }
  return cur.size() > best.size() ? cur : best;
}

bool inside(const Ring& ring, double x, double z) {
  bool odd = false;
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const Pt a = ring[i];
    const Pt b = ring[j];
    if ((a.z > z) != (b.z > z) && x < (b.x - a.x) * (z - a.z) / (b.z - a.z) + a.x) odd = !odd;
  }
  return odd;
}

// How far from (x, z) along (dx, dz) the ring's edge is, m (infinity where it is never met).
double reach_to(const Ring& ring, double x, double z, double dx, double dz) {
  double best = std::numeric_limits<double>::infinity();
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const double ax = ring[j].x;
    const double az = ring[j].z;
    const double ex = ring[i].x - ax;
    const double ez = ring[i].z - az;
    const double den = dx * ez - dz * ex;
    if (std::abs(den) < 1e-9) continue;
    const double t = ((ax - x) * ez - (az - z) * ex) / den;
    const double u = ((ax - x) * dz - (az - z) * dx) / den;
    if (t > 0 && u >= 0 && u <= 1) best = std::min(best, t);
  }
  return best;
}

struct Area {
  Ring ring;
  double x0, x1, z0, z1;
};

// A piste's width at each bend: straight across its line to the edges of the narrowest piste
// area it lies in, 0 where it lies in none.
std::vector<double> widths_of(const std::vector<Pt>& pts, const std::vector<Area>& areas) {
  std::vector<double> out;
  for (size_t i = 0; i < pts.size(); i++) {
    const Pt a = pts[i > 0 ? i - 1 : 0];
    const Pt b = pts[std::min(pts.size() - 1, i + 1)];
    const double l = std::max(1e-9, ge::core::hypot(b.x - a.x, b.z - a.z));
    const double nx = -(b.z - a.z) / l;
    const double nz = (b.x - a.x) / l;
    double best = 0;
    for (const Area& ar : areas) {
      const Pt p = pts[i];
      if (p.x < ar.x0 || p.x > ar.x1 || p.z < ar.z0 || p.z > ar.z1 || !inside(ar.ring, p.x, p.z))
        continue;
      const double w = reach_to(ar.ring, p.x, p.z, nx, nz) + reach_to(ar.ring, p.x, p.z, -nx, -nz);
      if (w < kWidest && (best == 0 || w < best)) best = w;
    }
    out.push_back(best);
  }
  return out;
}

// A lift's kind off its `aerialway` tag, or none for anything not ridden uphill on snow.
std::optional<mg::HintLiftKind> lift_kind(const std::string& v) {
  if (v == "chair_lift" || v == "mixed_lift") return mg::HintLiftKind::chair;
  if (v == "gondola" || v == "cable_car") return mg::HintLiftKind::gondola;
  if (v == "drag_lift" || v == "t-bar" || v == "j-bar" || v == "platter" || v == "rope_tow" ||
      v == "magic_carpet")
    return mg::HintLiftKind::drag;
  return std::nullopt;
}

// A piste's grade off its tagged difficulty, read as the country it lies in reads it: Europe's
// easy is a blue, the Americas' and the far east's a green.
mg::HintGrade grade_of(const mg::FaceCrop& f, const std::string& d) {
  using G = mg::HintGrade;
  const bool west = f.lon < -30 || f.lon > 100;
  if (d == "novice") return G::green;
  if (d == "easy") return west ? G::green : G::blue;
  if (d == "intermediate") return west ? G::blue : G::red;
  if (d == "advanced" || d == "expert") return G::black;
  if (d == "freeride" || d == "extreme") return G::route;
  return west ? G::blue : G::red;
}

// The way's points on the map, the nodes the data has.
std::vector<Pt> points_of(const mg::FaceCrop& f, const OsmData& d, const OsmWay& w) {
  std::vector<Pt> out;
  for (const int64_t r : w.refs) {
    const auto it = d.nodes.find(r);
    if (it == d.nodes.end()) continue;
    const auto [x, z] = mg::map_at(f, it->second.first, it->second.second);
    out.push_back({x, z});
  }
  return out;
}

// The lifts and pistes of a face's map data.
void lifts_and_pistes(const mg::FaceCrop& f, const OsmData& d, const mg::RealFace& face,
                      mg::RealHints& h) {
  std::vector<Area> areas;
  for (const auto& [id, w] : d.ways) {
    if (w.tag("piste:type") != "downhill" || !w.closed()) continue;
    std::vector<Pt> ring = points_of(f, d, w);
    if (ring.size() < 4) continue;
    ring.pop_back();
    Area a{ring, 1e18, -1e18, 1e18, -1e18};
    for (const Pt& p : ring) {
      a.x0 = std::min(a.x0, p.x);
      a.x1 = std::max(a.x1, p.x);
      a.z0 = std::min(a.z0, p.z);
      a.z1 = std::max(a.z1, p.z);
    }
    areas.push_back(std::move(a));
  }
  const auto height = [&](const Pt& p) { return mg::face_height(face, p.x, p.z); };
  for (const auto& [id, w] : d.ways) {
    const std::vector<Pt> pts = points_of(f, d, w);
    if (pts.size() < 2) continue;
    if (const auto kind = lift_kind(w.tag("aerialway"))) {
      Pt a = pts.front();
      Pt b = pts.back();
      if (!on_map(a) || !on_map(b)) continue;
      if (height(a) > height(b)) std::swap(a, b);
      h.lifts.push_back({*kind, a, b});
      continue;
    }
    if (w.tag("piste:type") == "downhill" && !w.closed() && w.tag("area") != "yes") {
      std::vector<Pt> line = longest_on_map(pts);
      if (line.size() < 2 || length_of(line) < kShortest) continue;
      if (height(line.front()) < height(line.back())) std::reverse(line.begin(), line.end());
      std::vector<Pt> bends = thin(line, kStray);
      std::vector<double> widths = widths_of(bends, areas);
      h.pistes.push_back(
          {grade_of(f, w.tag("piste:difficulty")), std::move(bends), std::move(widths)});
    }
  }
}

bool is_forest(const std::map<std::string, std::string>& tags) {
  const auto at = [&](const char* k) {
    const auto it = tags.find(k);
    return it == tags.end() ? std::string() : it->second;
  };
  return at("landuse") == "forest" || at("natural") == "wood";
}

// An edge of a forest polygon on the map.
struct Edge {
  Pt a, b;
};

// Every forest polygon of a face (the edges of all its rings together; even-odd fills them right
// whatever order they come in). A multipolygon whose members the boxes left out is read whole
// (`relation_full`); `missing` counts those that could not be.
std::vector<std::vector<Edge>> forests(const mg::FaceCrop& f, const OsmData& d,
                                       ge::mountain::labs::OsmCache& osm, bool fetch, int& missing,
                                       std::string& error) {
  std::vector<std::vector<Edge>> polys;
  const auto edges_of = [&](const OsmData& src, const std::vector<int64_t>& refs) {
    std::vector<Edge> out;
    for (size_t i = 1; i < refs.size(); i++) {
      const auto a = src.nodes.find(refs[i - 1]);
      const auto b = src.nodes.find(refs[i]);
      if (a == src.nodes.end() || b == src.nodes.end()) continue;
      const auto [ax, az] = mg::map_at(f, a->second.first, a->second.second);
      const auto [bx, bz] = mg::map_at(f, b->second.first, b->second.second);
      out.push_back({{ax, az}, {bx, bz}});
    }
    return out;
  };
  for (const auto& [id, w] : d.ways) {
    if (is_forest(w.tags) && w.closed()) polys.push_back(edges_of(d, w.refs));
  }
  for (const auto& [id, r] : d.relations) {
    if (r.tag("type") != "multipolygon" || !is_forest(r.tags)) continue;
    const auto whole = [&](const OsmData& src) {
      return std::all_of(r.members.begin(), r.members.end(),
                         [&](int64_t m) { return src.ways.count(m) > 0; });
    };
    const OsmData* src = &d;
    OsmData full;
    if (!whole(d)) {
      if (!osm.relation_full(id, fetch, full, error) || !whole(full)) {
        if (!error.empty()) return polys;
        missing++;
        continue;
      }
      src = &full;
    }
    std::vector<Edge> all;
    for (const int64_t m : r.members) {
      const std::vector<Edge> e = edges_of(*src, src->ways.at(m).refs);
      all.insert(all.end(), e.begin(), e.end());
    }
    polys.push_back(std::move(all));
  }
  return polys;
}

// Which cells of the face's grid stand in a forest, each cell's middle filled even-odd.
std::vector<uint8_t> rasterise(const std::vector<std::vector<Edge>>& polys) {
  std::vector<uint8_t> wooded(size_t(kN) * size_t(kN));
  for (const std::vector<Edge>& edges : polys) {
    double z0 = 1e18, z1 = -1e18, x0 = 1e18, x1 = -1e18;
    for (const Edge& e : edges) {
      z0 = std::min({z0, e.a.z, e.b.z});
      z1 = std::max({z1, e.a.z, e.b.z});
      x0 = std::min({x0, e.a.x, e.b.x});
      x1 = std::max({x1, e.a.x, e.b.x});
    }
    if (z1 < 0 || z0 > kSize || x1 < 0 || x0 > kSize) continue;
    const int r0 = std::max(0, int(std::ceil(z0 / kCell)));
    const int r1 = std::min(kN - 1, int(std::floor(z1 / kCell)));
    std::map<int, std::vector<double>> rows;
    for (const Edge& e : edges) {
      if (e.a.z == e.b.z) continue;
      const double lo_z = std::min(e.a.z, e.b.z);
      const double hi_z = std::max(e.a.z, e.b.z);
      const int lo = std::max(r0, int(std::ceil(lo_z / kCell)));
      const int hi = std::min(r1, int(std::ceil(hi_z / kCell)) - 1);
      for (int r = lo; r <= hi; r++) {
        const double z = r * kCell;
        // Half-open: an edge counts at a row it starts on, never one it ends on.
        if (z < lo_z || z >= hi_z) continue;
        rows[r].push_back(e.a.x + (e.b.x - e.a.x) * (z - e.a.z) / (e.b.z - e.a.z));
      }
    }
    for (auto& [r, xs] : rows) {
      std::sort(xs.begin(), xs.end());
      for (size_t k = 0; k + 1 < xs.size(); k += 2) {
        const int c0 = std::max(0, int(std::ceil(xs[k] / kCell)));
        const int c1 = std::min(kN - 1, int(std::floor(xs[k + 1] / kCell)));
        for (int c = c0; c <= c1; c++) wooded[size_t(r) * size_t(kN) + size_t(c)] = 1;
      }
    }
  }
  return wooded;
}

// The face's woods by height off its wooded cells, and the share of its window wooded; none for
// a face hardly wooded at all.
std::optional<mg::HintWoods> woods_by_height(const std::vector<uint8_t>& wooded,
                                             const std::vector<float>& heights, double& share) {
  const auto [lo_it, hi_it] = std::minmax_element(heights.begin(), heights.end());
  const double lo = *lo_it;
  const double span = std::max(1.0, double(*hi_it) - lo);
  size_t total = 0;
  for (const uint8_t w : wooded) total += w;
  share = double(total) / double(wooded.size());
  if (share < kLeast) return std::nullopt;
  const auto count = [&](int bins, std::vector<int>& all, std::vector<int>& woods) {
    all.assign(size_t(bins), 0);
    woods.assign(size_t(bins), 0);
    for (size_t i = 0; i < heights.size(); i++) {
      const double t = std::min(0.999999, (heights[i] - lo) / span);
      const size_t b = size_t(t * bins);
      all[b]++;
      woods[b] += wooded[i];
    }
  };
  std::vector<int> all;
  std::vector<int> woods;
  mg::HintWoods out{};
  count(mg::kWoodsBands, all, woods);
  for (size_t b = 0; b < out.bands.size(); b++) {
    out.bands[b] = all[b] > 0 ? double(woods[b]) / all[b] : 0;
  }
  count(kLineBins, all, woods);
  int top = 0;
  for (int b = 0; b < kLineBins; b++) {
    if (all[size_t(b)] >= kLineCells && double(woods[size_t(b)]) / all[size_t(b)] >= kLineCover)
      top = b + 1;
  }
  out.line = double(top) / kLineBins;
  return out;
}

// A face baked: its hints, its data as kept, and what the bake could not read.
struct Baked {
  mg::RealHints hints;
  std::string data;
  double wooded = 0;  // the share of the window the map draws as forest
  int missing = 0;    // forest relations left out for want of members
};

bool bake(const mg::FaceCrop& f, const mg::RealFace& face, ge::mountain::labs::OsmCache& osm,
          bool fetch, Baked& out, std::string& error) {
  const OsmData d = osm.read_face(f);
  lifts_and_pistes(f, d, face, out.hints);
  const auto polys = forests(f, d, osm, fetch, out.missing, error);
  if (!error.empty()) return false;
  out.hints.woods = woods_by_height(rasterise(polys), face.heights, out.wooded);
  // A face its crop marks BARE stands wholly above its trees: no forest on its map is no forest on
  // the mountain, not an unmapped one.
  if (!out.hints.woods && f.bare) out.hints.woods = mg::HintWoods{0, {}};
  out.data = mg::encode_hints(out.hints);
  // What a reader gets back: the kept data decoded, on the grain.
  std::optional<mg::RealHints> back = mg::decode_hints(out.data);
  if (!back) return false;
  out.hints = std::move(*back);
  return true;
}

// The C++ name of a face's hints: "alpine-12" -> "kAlpine12".
std::string symbol_of(const std::string& id) {
  std::string s = "k";
  bool up = true;
  for (const char ch : id) {
    if (ch == '-') {
      up = true;
      continue;
    }
    s += up ? char(std::toupper(static_cast<unsigned char>(ch))) : ch;
    up = false;
  }
  return s;
}

std::string file_of(const std::string& id) {
  std::string s = id;
  std::replace(s.begin(), s.end(), '-', '_');
  return "hints_" + s + ".cpp";
}

constexpr const char* kNotice =
    "// The map data: © OpenStreetMap contributors, under the Open Database Licence (ODbL 1.0);\n"
    "// the baked hints are a derived database under the same licence.\n";

std::string hint_file(const mg::FaceCrop& f, const Baked& b) {
  std::string s =
      "// GENERATED by the `real-hints` lab (`make lab LAB=real-hints ARGS=\"--bake --write\"`):\n"
      "// never edit by hand. The real ski area's hints on the real face " +
      std::string(f.id) + " (real_hints.hpp).\n";
  s += kNotice;
  s += "//\n// ge:summary R25 the real face " + std::string(f.id) + "'s hints (generated)\n";
  s += "// ge:keywords real-hints generated\n// ge:skill mountain\n";
  s += "// ge:test tests/mountain_real_hints_test.cpp\n";
  s += "#include \"mountain/mapgen/real_hints.hpp\"\n\nnamespace ge::mountain::mapgen::hints_baked "
       "{\n\n";
  const std::string sym = symbol_of(f.id);
  s += "extern const HintData " + sym + ";\n";
  constexpr size_t kLine = 90;
  if (b.data.size() <= kLine - 10) {
    s += "const HintData " + sym + "{\"" + f.id + "\", \"" + b.data + "\"};\n";
  } else {
    s += "const HintData " + sym + "{\n    \"" + f.id + "\",\n";
    for (size_t i = 0; i < b.data.size(); i += kLine) {
      s += "    \"" + b.data.substr(i, kLine) + "\"";
      s += i + kLine >= b.data.size() ? "};\n" : "\n";
    }
  }
  s += "\n}  // namespace ge::mountain::mapgen::hints_baked\n";
  return s;
}

std::string index_file() {
  std::string s =
      "// GENERATED by the `real-hints` lab (`make lab LAB=real-hints ARGS=\"--bake --write\"`):\n"
      "// never edit by hand. Every real face's hints baked, in the crops' order "
      "(real_hints.hpp).\n";
  s += kNotice;
  s += "//\n// ge:summary R25 the real faces' hints baked, listed (generated)\n";
  s += "// ge:keywords real-hints generated baked-hints index\n// ge:skill mountain\n";
  s += "// ge:test tests/mountain_real_hints_test.cpp\n";
  s += "#include <array>\n\n#include \"mountain/mapgen/real_hints.hpp\"\n\n";
  s += "namespace ge::mountain::mapgen {\n\nnamespace hints_baked {\n";
  std::vector<std::string> syms;
  for (const mg::FaceCrop& f : mg::kFaceCrops) {
    if (!fs::exists(fs::path(kTree) / file_of(f.id))) continue;
    syms.push_back(symbol_of(f.id));
    s += "extern const HintData " + syms.back() + ";\n";
  }
  s += "}  // namespace hints_baked\n\nstd::span<const HintData* const> baked_hints() {\n";
  s += "  static constexpr std::array<const HintData*, " + std::to_string(syms.size()) +
       "> kAll{{\n      // clang-format off\n";
  for (const std::string& sym : syms) s += "      &hints_baked::" + sym + ",\n";
  s += "      // clang-format on\n  }};\n  return kAll;\n}\n\n}  // namespace "
       "ge::mountain::mapgen\n";
  return s;
}

Rgba grade_colour(mg::HintGrade g) {
  switch (g) {
    case mg::HintGrade::green:
      return {40, 170, 70};
    case mg::HintGrade::blue:
      return {40, 90, 220};
    case mg::HintGrade::red:
      return {220, 40, 40};
    case mg::HintGrade::black:
      return {10, 10, 10};
    case mg::HintGrade::route:
      return {240, 140, 20};
  }
  return {0, 0, 0};
}

// The face shaded by height and lit from the north-west, its woods tinted green by the face's
// cover at each height, its pistes in their colours and its lifts in white over black.
ge::lab::Picture draw_hints(const mg::RealFace& face, const mg::RealHints& h, int px) {
  auto pic = ge::lab::create_picture(px, px, {0, 0, 0});
  const auto [lo_it, hi_it] = std::minmax_element(face.heights.begin(), face.heights.end());
  const double lo = *lo_it;
  const double span = std::max(1.0, double(*hi_it) - lo);
  const auto woods = mg::face_woods(face, h);
  const double k = kSize / px;
  for (int py = 0; py < px; py++) {
    for (int pxx = 0; pxx < px; pxx++) {
      const double x = (pxx + 0.5) * k;
      const double z = (py + 0.5) * k;
      const double e = std::max(2.0, k);
      const double gx =
          (mg::face_height(face, x + e, z) - mg::face_height(face, x - e, z)) / (2 * e);
      const double gz =
          (mg::face_height(face, x, z + e) - mg::face_height(face, x, z - e)) / (2 * e);
      const double nl = std::sqrt(gx * gx + 1 + gz * gz);
      const double lit = std::clamp((gx + 1.41421356 + gz) / (2 * nl), 0.0, 1.0);
      const double y = mg::face_height(face, x, z);
      const double t = std::clamp((y - lo) / span, 0.0, 1.0);
      const double shade = 0.35 + 0.65 * lit;
      double cover = 0;
      if (woods && y < woods->line) cover = mg::face_cover_at(*woods, y);
      const double base = 150 + 105 * t;
      const double r = (base * (1 - cover) + 40 * cover) * shade;
      const double g = (base * (1 - cover) + 110 * cover) * shade;
      const double b = (base * (1 - cover) + 50 * cover) * shade;
      ge::lab::set_pixel(
          pic, pxx, py,
          {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)});
    }
  }
  const double s = px / kSize;
  for (const mg::HintPiste& p : h.pistes) {
    for (size_t i = 1; i < p.points.size(); i++) {
      ge::lab::draw_line(pic, p.points[i - 1].x * s, p.points[i - 1].z * s, p.points[i].x * s,
                         p.points[i].z * s, grade_colour(p.grade), 2);
    }
  }
  for (const mg::HintLift& l : h.lifts) {
    ge::lab::draw_line(pic, l.bottom.x * s, l.bottom.z * s, l.top.x * s, l.top.z * s, {0, 0, 0}, 3);
    ge::lab::draw_line(pic, l.bottom.x * s, l.bottom.z * s, l.top.x * s, l.top.z * s,
                       {255, 255, 255}, 1);
  }
  return pic;
}

void run(ge::lab::Lab& lab) {
  const auto faces = faces_of(lab);
  const int px = static_cast<int>(lab.arg_num("size"));
  const bool baking = lab.arg_bool("bake");
  const bool fetch = lab.arg_bool("fetch");
  ge::mountain::labs::OsmCache osm(lab.arg("osm"));
  std::vector<std::vector<std::string>> rows;
  std::vector<std::pair<mg::RealFace, mg::RealHints>> drawn;
  int written = 0;
  int lifts = 0;
  int pistes = 0;
  int wooded = 0;
  for (const mg::FaceCrop* f : faces) {
    const std::string id = f->id;
    const auto face = mg::real_face(id);
    if (!face) {
      lab.fail(id + " has no heights in the tree",
               "next: bake them first: make lab LAB=real-faces ARGS=\"--bake --fetch --write "
               "--faces=" +
                   id + "\" (skill: mountain).");
      continue;
    }
    std::optional<mg::RealHints> h = mg::real_hints(id);
    std::vector<std::string> extra;
    if (baking) {
      std::string error;
      if (fetch && !osm.fetch_face(*f, error)) {
        lab.fail(std::string(id).append(": fetching its map data: ").append(error),
                 "next: check the network reaches api.openstreetmap.org, then run again.");
        continue;
      }
      if (!osm.kept(*f)) {
        lab.fail(id + " has no map data kept in " + lab.arg("osm"),
                 "next: add --fetch to download it (skill: mountain).");
        continue;
      }
      Baked b;
      if (!bake(*f, *face, osm, fetch, b, error)) {
        lab.fail(std::string(id).append(": ").append(error),
                 "next: check the network, then run again with --fetch.");
        continue;
      }
      const bool same = h && mg::encode_hints(*h) == b.data;
      extra = {f2(b.wooded), std::to_string(b.missing), std::to_string(b.data.size()),
               h ? (same ? "same" : "moved") : "new"};
      if (lab.arg_bool("write")) {
        if (!fs::exists("plugins/mountain/mapgen/real_hints.hpp")) {
          lab.fail("--write must run from the repository's root",
                   "next: cd to the checkout's root and run the lab again.");
          return;
        }
        fs::create_directories(kTree);
        std::ofstream(fs::path(kTree) / file_of(id)) << hint_file(*f, b);
        written++;
      }
      h = std::move(b.hints);
    }
    if (!h) {
      lab.fail(id + " has no hints in the tree",
               "next: bake them: make lab LAB=real-hints ARGS=\"--bake --fetch --write --faces=" +
                   id + "\" (skill: mountain).");
      continue;
    }
    std::array<int, 3> kinds{};
    for (const mg::HintLift& l : h->lifts) kinds[size_t(l.kind)]++;
    std::array<int, 5> grades{};
    double metres = 0;
    for (const mg::HintPiste& p : h->pistes) {
      grades[size_t(p.grade)]++;
      metres += length_of(p.points);
    }
    const auto woods = mg::face_woods(*face, *h);
    lifts += int(h->lifts.size());
    pistes += int(h->pistes.size());
    wooded += woods ? 1 : 0;
    std::string bands;
    if (woods) {
      for (const double b : woods->bands) bands += (bands.empty() ? "" : " ") + f2(b);
    }
    std::vector<std::string> row{id,
                                 mg::region_name(f->region),
                                 std::to_string(kinds[0]),
                                 std::to_string(kinds[1]),
                                 std::to_string(kinds[2]),
                                 std::to_string(grades[0]) + "/" + std::to_string(grades[1]) + "/" +
                                     std::to_string(grades[2]) + "/" + std::to_string(grades[3]) +
                                     "/" + std::to_string(grades[4]),
                                 f0(metres),
                                 woods ? f0(woods->line) : "-",
                                 bands.empty() ? "-" : bands};
    row.insert(row.end(), extra.begin(), extra.end());
    rows.push_back(std::move(row));
    lab.metric(id + ".lifts", double(h->lifts.size()), "count");
    lab.metric(id + ".pistes", double(h->pistes.size()), "count");
    if (woods) lab.metric(id + ".tree-line", woods->line, "m");
    if (faces.size() <= 4) drawn.emplace_back(*face, std::move(*h));
  }
  if (written > 0) {
    std::ofstream(fs::path(kTree) / "index.cpp") << index_file();
    lab.note("wrote " + std::to_string(written) + " faces' hints and the index into " + kTree +
             "; run make fmt-check and commit them");
  }
  std::vector<std::string> head{"face",     "region",      "chairs",
                                "gondolas", "drags",       "pistes g/b/r/k/route",
                                "piste m",  "tree line m", "cover by band, bottom first"};
  if (baking) head.insert(head.end(), {"wooded", "missing", "base64 bytes", "vs the tree"});
  lab.table({"the real faces' hints (the tree's; with --bake, the bake's)", head, rows});
  lab.metric("lifts", lifts, "count");
  lab.metric("pistes", pistes, "count");
  lab.metric("wooded", wooded, "count");
  for (const auto& [face, h] : drawn) {
    const std::string id = face.crop->id;
    lab.shot({id,
              "REAL FACE " + id + " AND ITS SKI AREA",
              "The real face " + id +
                  " shaded by height and lit from the north-west, tinted green as wooded as its "
                  "map draws each height below its tree line; the real pistes drawn in their "
                  "colours (orange a ski route) and the real lifts in white edged black.",
              "the lifts climb from the valley to tops on the ridge and its shoulders, the pistes "
              "fall from the lifts' tops down the spurs and gullies, the woods fill the lower "
              "face and stop at a line",
              {{"face", id}}},
             draw_hints(face, h, px));
  }
}

const ge::lab::LabSpec kSpec{
    "real-hints",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen/real_hints.hpp",
    "mountain",
    "What does each real face's real ski area have (its lifts, its pistes, its woods by height), "
    "and does a bake off the open map data reproduce the hints in the tree?",
    {
        {"faces", "all", "the faces, comma-separated ids, or all"},
        {"size", "768", "the pictures' width, px, when four faces or fewer are named"},
        {"bake", "", "bake the hints off the map data and compare them with the tree's", true},
        {"fetch", "", "with --bake: download the map data not yet kept in --osm", true},
        {"write", "", "with --bake: write the faces baked and the index into the tree", true},
        {"osm", ".cache/osm", "where the map data is kept"},
    },
    "--faces=alpine-1,fell-1 --size=192",
    run,
};
GE_LAB(kSpec);

}  // namespace
