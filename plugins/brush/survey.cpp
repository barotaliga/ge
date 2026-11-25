// A map surveyed (survey.hpp): leaf cells, liquid bodies, a slice, entities' places.
//
// ge:summary Leaf cells by family, liquid bodies joined by touching boxes, a slice, entity boxes
// ge:keywords survey cell liquid-bodies union slice entity-place glob
// ge:skill brush
// ge:test tests/brush_survey_test.cpp
#include "brush/survey.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace ge::brush {

namespace {

// Whether a sky face marked in leaf `l` comes within `pad` units of `p` (its box widened).
bool sky_face_near(const Map& map, const Leaf& l, const float p[3], float pad) {
  for (uint32_t k = l.first_mark; k < l.first_mark + l.marks && k < map.marks.size(); ++k) {
    const uint32_t si = map.marks[k];
    if (si >= map.surfaces.size()) continue;
    const Surface& s = map.surfaces[si];
    if (s.kind != SurfaceKind::sky) continue;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (uint32_t i = s.first; i < s.first + s.count && i < map.indices.size(); ++i) {
      const float* v = map.vertices[map.indices[i]].pos;
      for (int a = 0; a < 3; ++a) lo[a] = std::min(lo[a], v[a]), hi[a] = std::max(hi[a], v[a]);
    }
    bool in = true;
    for (int a = 0; a < 3; ++a) in = in && p[a] >= lo[a] - pad && p[a] <= hi[a] + pad;
    if (in) return true;
  }
  return false;
}

}  // namespace

Cell march(const Map& map, const double from[3], const double dir[3], double reach, double step) {
  if (map.leaves.empty() || step <= 0) return Cell::empty;
  const float start[3] = {float(from[0]), float(from[1]), float(from[2])};
  uint32_t last = leaf_at(map, start);
  const auto steps = static_cast<int>(reach / step + 1e-9);
  for (int k = 1; k <= steps; ++k) {
    const double s = step * k;
    const float p[3] = {float(from[0] + dir[0] * s), float(from[1] + dir[1] * s),
                        float(from[2] + dir[2] * s)};
    const uint32_t li = leaf_at(map, p);
    if (li >= map.leaves.size()) return Cell::empty;
    const Cell c = leaf_cell(map, map.leaves[li]);
    if (c == Cell::sky) return Cell::sky;
    if (c == Cell::solid) {
      const bool sky = map.family == Family::v38 && last < map.leaves.size() &&
                       sky_face_near(map, map.leaves[last], p, float(step));
      return sky ? Cell::sky : Cell::solid;
    }
    last = li;
  }
  return Cell::empty;
}

Cell leaf_cell(const Map& map, const Leaf& leaf) {
  switch (map.family) {
    case Family::v29:
    case Family::bsp2:
      switch (leaf.contents) {
        case -2:
          return Cell::solid;
        case -3:
          return Cell::water;
        case -4:
          return Cell::slime;
        case -5:
          return Cell::lava;
        case -6:
          return Cell::sky;
        default:
          return leaf.contents <= -9 ? Cell::water : Cell::empty;
      }
    case Family::v38:
      if (leaf.contents & 0x3) return Cell::solid;
      if (leaf.contents & 0x8) return Cell::lava;
      if (leaf.contents & 0x10) return Cell::slime;
      if (leaf.contents & 0x20) return Cell::water;
      return Cell::empty;
    case Family::v46:
      break;
  }
  return leaf.solid ? Cell::solid : Cell::empty;
}

std::vector<std::array<float, 3>> open_spots(const Map& map, float step, size_t most) {
  std::vector<std::array<float, 3>> out;
  if (map.models.empty() || step <= 0) return out;
  const Model& w = map.models[0];
  int n[3];
  for (int k = 0; k < 3; ++k) n[k] = static_cast<int>((w.maxs[k] - w.mins[k]) / step);
  for (int iz = 0; iz < n[2]; ++iz)
    for (int iy = 0; iy < n[1]; ++iy)
      for (int ix = 0; ix < n[0]; ++ix) {
        const float x = w.mins[0] + step * (static_cast<float>(ix) + 0.5f);
        const float y = w.mins[1] + step * (static_cast<float>(iy) + 0.5f);
        const float z = w.mins[2] + step * (static_cast<float>(iz) + 0.5f);
        const float p[3] = {x, y, z};
        const Cell c = cell_at(map, p);
        if (c == Cell::solid || c == Cell::sky) continue;
        out.push_back({x, y, z});
        if (out.size() >= most) return out;
      }
  return out;
}

Cell cell_at(const Map& map, const float p[3]) {
  if (map.leaves.empty()) return Cell::empty;
  return leaf_cell(map, map.leaves[leaf_at(map, p)]);
}

const char* cell_name(Cell c) {
  static const char* const kNames[] = {"empty", "solid", "water", "slime", "lava", "sky"};
  return kNames[int(c)];
}

char cell_char(Cell c) { return ".#~%!^"[int(c)]; }

namespace {

bool touch(const Leaf& a, const Leaf& b) {
  for (int k = 0; k < 3; ++k) {
    if (a.maxs[k] < b.mins[k] || b.maxs[k] < a.mins[k]) return false;
  }
  return true;
}

uint32_t root(std::vector<uint32_t>& up, uint32_t i) {
  while (up[i] != i) i = up[i] = up[up[i]];
  return i;
}

}  // namespace

std::vector<LiquidBody> liquid_bodies(const Map& map) {
  std::vector<uint32_t> wet;
  std::vector<Cell> kind;
  for (uint32_t i = 0; i < map.leaves.size(); ++i) {
    const Cell c = leaf_cell(map, map.leaves[i]);
    if (c == Cell::water || c == Cell::slime || c == Cell::lava) {
      wet.push_back(i);
      kind.push_back(c);
    }
  }
  std::vector<uint32_t> up(wet.size());
  std::iota(up.begin(), up.end(), 0u);
  for (uint32_t a = 0; a < wet.size(); ++a) {
    for (uint32_t b = a + 1; b < wet.size(); ++b) {
      if (kind[a] == kind[b] && touch(map.leaves[wet[a]], map.leaves[wet[b]]))
        up[root(up, a)] = root(up, b);
    }
  }
  std::vector<LiquidBody> bodies;
  std::vector<int32_t> body_of(wet.size(), -1);
  for (uint32_t a = 0; a < wet.size(); ++a) {
    const uint32_t r = root(up, a);
    const Leaf& l = map.leaves[wet[a]];
    if (body_of[r] < 0) {
      body_of[r] = int32_t(bodies.size());
      LiquidBody b;
      b.kind = kind[a];
      std::copy(l.mins, l.mins + 3, b.mins);
      std::copy(l.maxs, l.maxs + 3, b.maxs);
      bodies.push_back(b);
    }
    LiquidBody& b = bodies[size_t(body_of[r])];
    for (int k = 0; k < 3; ++k) {
      b.mins[k] = std::min(b.mins[k], l.mins[k]);
      b.maxs[k] = std::max(b.maxs[k], l.maxs[k]);
    }
    ++b.leaves;
  }
  std::sort(bodies.begin(), bodies.end(), [](const LiquidBody& a, const LiquidBody& b) {
    if (a.depth() != b.depth()) return a.depth() > b.depth();
    if (a.kind != b.kind) return a.kind < b.kind;
    return std::lexicographical_compare(a.mins, a.mins + 3, b.mins, b.mins + 3);
  });
  return bodies;
}

bool slice_map(const Map& map, int axis, double at, double step, Slice* out, std::string* error,
               int max_cells, const float* near, double radius, int hull) {
  if (map.models.empty() || axis < 0 || axis > 2) {
    *error = "the map has no world model to slice";
    return false;
  }
  const Model& w = map.models[0];
  if (at < w.mins[axis] || at > w.maxs[axis]) {
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "%c=%g is outside the world's box (%g .. %g); next: a plane inside it",
                  "xyz"[axis], at, double(w.mins[axis]), double(w.maxs[axis]));
    *error = buf;
    return false;
  }
  if (hull != 0 &&
      (hull < 1 || hull > 3 || (map.family != Family::v29 && map.family != Family::bsp2))) {
    *error =
        "--hull takes 1, 2 or 3 on a version 29 or BSP2 map (38 and 46 have no clip hulls); "
        "next: --hull=1 on such a map, or no --hull";
    return false;
  }
  Slice s;
  s.axis = axis;
  s.at = at;
  s.u_axis = axis == 0 ? 1 : 0;
  s.v_axis = axis == 2 ? 1 : 2;
  // The grid's box on the plane: the world's, or the window round `near` inside it.
  double u_lo = w.mins[s.u_axis], u_hi = w.maxs[s.u_axis];
  double v_lo = w.mins[s.v_axis], v_hi = w.maxs[s.v_axis];
  if (near) {
    u_lo = std::max(u_lo, double(near[s.u_axis]) - radius);
    u_hi = std::min(u_hi, double(near[s.u_axis]) + radius);
    v_lo = std::max(v_lo, double(near[s.v_axis]) - radius);
    v_hi = std::min(v_hi, double(near[s.v_axis]) + radius);
    if (u_hi <= u_lo || v_hi <= v_lo) {
      *error = "the window round --near misses the world's box; next: a point inside the map";
      return false;
    }
  }
  const double u_len = u_hi - u_lo;
  const double v_len = v_hi - v_lo;
  if (step <= 0) step = std::max(8.0, std::ceil(std::max(u_len, v_len) / 96.0 / 8.0) * 8.0);
  s.step = step;
  const int cols = std::max(1, int(std::ceil(u_len / step)));
  const int rows = std::max(1, int(std::ceil(v_len / step)));
  if (cols > max_cells || rows > max_cells) {
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "a step of %g makes %d x %d cells, more than %d a side; next: a larger --step",
                  step, cols, rows, max_cells);
    *error = buf;
    return false;
  }
  s.u0 = u_lo + step / 2;
  s.v0 = v_hi - step / 2;
  s.rows.assign(size_t(rows), std::string(size_t(cols), '.'));
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      float p[3];
      p[axis] = float(at);
      p[s.u_axis] = float(s.u0 + c * step);
      p[s.v_axis] = float(s.v0 - r * step);
      if (hull != 0) {
        Leaf leaf;  // the hull's contents read as a leaf's (29 and BSP2 share the numbers)
        leaf.contents = hull_contents(map, p, 0, hull);
        s.rows[size_t(r)][size_t(c)] = cell_char(leaf_cell(map, leaf));
      } else {
        s.rows[size_t(r)][size_t(c)] = cell_char(leaf_cell(map, map.leaves[leaf_at(map, p)]));
      }
    }
  }
  *out = std::move(s);
  return true;
}

bool entity_place(const Map& map, const Entity& e, float mins[3], float maxs[3]) {
  double o[3] = {0, 0, 0};
  const bool has_origin =
      std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &o[0], &o[1], &o[2]) == 3;
  const std::string& model = e.get("model");
  unsigned n = 0;
  if (model.size() > 1 && model[0] == '*' && std::sscanf(model.c_str() + 1, "%u", &n) == 1 &&
      n < map.models.size()) {
    for (int k = 0; k < 3; ++k) {
      mins[k] = float(map.models[n].mins[k] + o[k]);
      maxs[k] = float(map.models[n].maxs[k] + o[k]);
    }
    return true;
  }
  if (!has_origin) return false;
  for (int k = 0; k < 3; ++k) mins[k] = maxs[k] = float(o[k]);
  return true;
}

double box_distance(const float mins[3], const float maxs[3], const float p[3]) {
  double sum = 0;
  for (int k = 0; k < 3; ++k) {
    const double d = p[k] < mins[k]   ? double(mins[k]) - p[k]
                     : p[k] > maxs[k] ? double(p[k]) - maxs[k]
                                      : 0;
    sum += d * d;
  }
  return std::sqrt(sum);
}

bool glob_match(std::string_view glob, std::string_view name) {
  size_t g = 0, n = 0, star = std::string_view::npos, mark = 0;
  while (n < name.size()) {
    if (g < glob.size() && (glob[g] == '?' || glob[g] == name[n])) {
      ++g, ++n;
    } else if (g < glob.size() && glob[g] == '*') {
      star = g++;
      mark = n;
    } else if (star != std::string_view::npos) {
      g = star + 1;
      n = ++mark;
    } else {
      return false;
    }
  }
  while (g < glob.size() && glob[g] == '*') ++g;
  return g == glob.size();
}

}  // namespace ge::brush
