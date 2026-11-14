// A map plan read into the compiler's source (plan.hpp): the words, the shapes, convex pieces,
// the carve and the adds.
//
// ge:summary Reads a map plan: words, shapes, convex pieces, rooms carved and solids added
// ge:keywords map-compiler plan read_plan subtract carve convex-decomposition ear-clipping prism
// ge:keywords symmetry stairs ramp wall liquid
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/plan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "core/num_text.hpp"

namespace ge::brush::mapc {

namespace {

using P2 = std::pair<double, double>;
using Poly = std::vector<P2>;

std::string fmt(double v) {
  if (v == std::nearbyint(v)) return std::to_string(int64_t(v));
  return core::num::to_fixed(v, 3);
}

Side make_side(const PlaneD& p, const std::string& tex) {
  Side s;
  s.plane = p;
  s.texture = tex;
  axial_vecs(p.n, 0, 0, 0, 1, 1, s.vecs);
  return s;
}

double signed_area(const Poly& p) {
  double a = 0;
  for (size_t i = 0; i < p.size(); ++i) {
    const P2& u = p[i];
    const P2& v = p[(i + 1) % p.size()];
    a += u.first * v.second - v.first * u.second;
  }
  return a / 2;
}

double turn(const P2& a, const P2& b, const P2& c) {
  return (b.first - a.first) * (c.second - b.second) - (b.second - a.second) * (c.first - b.first);
}

bool convex(const Poly& p) {
  for (size_t i = 0; i < p.size(); ++i) {
    if (turn(p[(i + p.size() - 1) % p.size()], p[i], p[(i + 1) % p.size()]) < -1e-9) return false;
  }
  return true;
}

// Corners on a straight line and repeated corners dropped.
Poly clean(Poly p) {
  bool changed = true;
  while (changed && p.size() >= 3) {
    changed = false;
    for (size_t i = 0; i < p.size(); ++i) {
      const P2& a = p[(i + p.size() - 1) % p.size()];
      const P2& b = p[i];
      const P2& c = p[(i + 1) % p.size()];
      const bool same =
          std::fabs(a.first - b.first) < 1e-9 && std::fabs(a.second - b.second) < 1e-9;
      if (same || std::fabs(turn(a, b, c)) < 1e-9) {
        p.erase(p.begin() + std::ptrdiff_t(i));
        changed = true;
        break;
      }
    }
  }
  return p;
}

bool in_triangle(const P2& p, const P2& a, const P2& b, const P2& c) {
  return turn(a, b, p) >= -1e-9 && turn(b, c, p) >= -1e-9 && turn(c, a, p) >= -1e-9;
}

// A simple counter-clockwise polygon cut into convex pieces: ear clipping, then neighbouring
// pieces joined while their union stays convex.
std::vector<Poly> convex_pieces(const Poly& poly) {
  if (convex(poly)) return {poly};
  std::vector<Poly> tris;
  Poly rest = poly;
  int guard = 0;
  while (rest.size() > 3 && guard++ < 10000) {
    bool cut = false;
    for (size_t i = 0; i < rest.size() && !cut; ++i) {
      const P2& a = rest[(i + rest.size() - 1) % rest.size()];
      const P2& b = rest[i];
      const P2& c = rest[(i + 1) % rest.size()];
      if (turn(a, b, c) <= 1e-9) continue;
      bool ear = true;
      for (size_t j = 0; j < rest.size() && ear; ++j) {
        const P2& q = rest[j];
        if (q == a || q == b || q == c) continue;
        if (in_triangle(q, a, b, c)) ear = false;
      }
      if (!ear) continue;
      tris.push_back({a, b, c});
      rest.erase(rest.begin() + std::ptrdiff_t(i));
      cut = true;
    }
    if (!cut) break;
  }
  if (rest.size() == 3) tris.push_back(rest);
  // Join pieces that share an edge while the union is convex.
  bool joined = true;
  while (joined) {
    joined = false;
    for (size_t i = 0; i < tris.size() && !joined; ++i) {
      for (size_t j = i + 1; j < tris.size() && !joined; ++j) {
        const Poly& a = tris[i];
        const Poly& b = tris[j];
        for (size_t x = 0; x < a.size() && !joined; ++x) {
          const P2& a0 = a[x];
          const P2& a1 = a[(x + 1) % a.size()];
          for (size_t y = 0; y < b.size(); ++y) {
            if (!(b[y] == a1 && b[(y + 1) % b.size()] == a0)) continue;
            Poly m;
            for (size_t k = 1; k <= a.size(); ++k) m.push_back(a[(x + k) % a.size()]);  // a1..a0
            for (size_t k = 2; k < b.size(); ++k) m.push_back(b[(y + k) % b.size()]);
            m = clean(m);
            if (m.size() >= 3 && convex(m)) {
              tris[i] = m;
              tris.erase(tris.begin() + std::ptrdiff_t(j));
              joined = true;
            }
            break;
          }
        }
      }
    }
  }
  return tris;
}

// A prism over a convex counter-clockwise polygon, z0..z1.
Brush prism(const Poly& p, double z0, double z1, const std::string& bottom, const std::string& top,
            const std::string& side) {
  Brush b;
  b.sides.push_back(make_side({{0, 0, -1}, -z0}, bottom));
  b.sides.push_back(make_side({{0, 0, 1}, z1}, top));
  for (size_t i = 0; i < p.size(); ++i) {
    const P2& a = p[i];
    const P2& c = p[(i + 1) % p.size()];
    const V3 n = normalized({c.second - a.second, a.first - c.first, 0});
    b.sides.push_back(make_side({n, dot(n, {a.first, a.second, 0})}, side));
  }
  return b;
}

struct Words {
  std::vector<std::string> list;
  bool bad_quote = false;
};

Words split_words(std::string_view line) {
  Words w;
  std::string cur;
  bool any = false, quoted = false;
  for (char ch : line) {
    if (ch == '"') {
      quoted = !quoted;
      any = true;
      continue;
    }
    if (!quoted && ch == '#') break;
    if (!quoted && (ch == ' ' || ch == '\t' || ch == '\r')) {
      if (any) w.list.push_back(cur);
      cur.clear();
      any = false;
      continue;
    }
    cur += ch;
    any = true;
  }
  if (any) w.list.push_back(cur);
  w.bad_quote = quoted;
  return w;
}

bool number(const std::string& s, double* out) {
  if (s.empty()) return false;
  char* end = nullptr;
  *out = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size() && std::isfinite(*out);
}

bool point(const std::string& s, size_t dims, std::vector<double>* out) {
  out->clear();
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    double v = 0;
    if (!number(s.substr(at, comma == std::string::npos ? std::string::npos : comma - at), &v)) {
      return false;
    }
    out->push_back(v);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out->size() == dims;
}

enum class Op : uint8_t { carve, solid, liquid };

struct Shape {
  Op op = Op::solid;
  Poly poly;  // counter-clockwise
  double z0 = 0, z1 = 0;
  std::string floor, wall, ceil;  // carve: its faces; solid and liquid: bottom, sides, top
  // A stair or ramp over a box: steps (0: a ramp), and the way up (0 +x, 1 −x, 2 +y, 3 −y).
  bool stairs = false, ramp = false;
  int steps = 0, up = 0;
  bool once = false;  // not repeated by the symmetry
  int line = 0;
};

struct Ent {
  std::string cls;
  double at[3] = {};
  std::vector<std::pair<std::string, std::string>> keys;
  bool once = false;
  int line = 0;
};

struct Plan {
  std::vector<std::pair<std::string, std::string>> world;
  std::string floor = "floor", wall = "wall", ceil = "ceiling", solid = "wall";
  double pad = 32;
  std::vector<Shape> shapes;
  std::vector<Ent> ents;
  int sym = 0;  // 1 rotate, 2 mirror-x, 3 mirror-y
  double sym_at[2] = {};
};

const char* kUpNames[4] = {"+x", "-x", "+y", "-y"};

// The shape after the symmetry.
Shape mirrored(Shape s, int sym, const double c[2]) {
  for (P2& p : s.poly) {
    if (sym == 1 || sym == 2) p.first = 2 * c[0] - p.first;
    if (sym == 1 || sym == 3) p.second = 2 * c[1] - p.second;
  }
  if (sym != 1) std::reverse(s.poly.begin(), s.poly.end());
  // Turn the box's way up with it.
  if (sym == 1 || sym == 2) {
    if (s.up == 0 || s.up == 1) s.up ^= 1;
  }
  if (sym == 1 || sym == 3) {
    if (s.up == 2 || s.up == 3) s.up ^= 1;
  }
  return s;
}

Ent mirrored(Ent e, int sym, const double c[2]) {
  if (sym == 1 || sym == 2) e.at[0] = 2 * c[0] - e.at[0];
  if (sym == 1 || sym == 3) e.at[1] = 2 * c[1] - e.at[1];
  for (auto& [k, v] : e.keys) {
    if (k != "angle") continue;
    double a = 0;
    if (!number(v, &a) || a < 0) continue;  // −1 up and −2 down stay
    a = sym == 1 ? a + 180 : sym == 2 ? 180 - a : -a;
    a = std::fmod(std::fmod(a, 360) + 360, 360);
    v = fmt(a);
  }
  return e;
}

void bounds_of(const Brush& b, V3* lo, V3* hi) {
  std::vector<PlaneD> planes;
  planes.reserve(b.sides.size());
  for (const Side& s : b.sides) planes.push_back(s.plane);
  for (size_t i = 0; i < planes.size(); ++i) {
    Winding w = base_winding(planes[i]);
    for (size_t j = 0; j < planes.size() && !w.empty(); ++j) {
      if (j != i) w = clip_behind(w, planes[j], 0);
    }
    bounds_add(w, lo, hi);
  }
}

}  // namespace

bool tidy_brush(Brush* b) {
  std::vector<Side> kept;
  std::vector<V3> corners;
  for (size_t i = 0; i < b->sides.size(); ++i) {
    Winding w = base_winding(b->sides[i].plane);
    for (size_t j = 0; j < b->sides.size() && !w.empty(); ++j) {
      if (j != i) w = clip_behind(w, b->sides[j].plane, 0.001);
    }
    if (tiny(w)) continue;
    kept.push_back(b->sides[i]);
    corners.insert(corners.end(), w.begin(), w.end());
  }
  b->sides = std::move(kept);
  if (b->sides.size() < 4) return false;
  // Closed (no corner out at the base windings' reach) and with room behind every side.
  for (const V3& c : corners) {
    for (double x : c) {
      if (!(std::fabs(x) < kHuge / 2)) return false;
    }
  }
  for (const Side& s : b->sides) {
    double deepest = 0;
    for (const V3& c : corners) deepest = std::min(deepest, dist(s.plane, c));
    if (deepest > -0.1) return false;
  }
  return true;
}

std::vector<Brush> subtract(const Brush& b, const Brush& cut) {
  V3 blo = {1e30, 1e30, 1e30}, bhi = {-1e30, -1e30, -1e30};
  V3 clo = blo, chi = bhi;
  bounds_of(b, &blo, &bhi);
  bounds_of(cut, &clo, &chi);
  for (size_t k = 0; k < 3; ++k) {
    if (blo[k] >= chi[k] - 0.01 || clo[k] >= bhi[k] - 0.01) return {b};
  }
  std::vector<Brush> out;
  Brush rest = b;
  for (const Side& c : cut.sides) {
    Brush outside = rest;
    outside.sides.push_back(make_side(flip(c.plane), c.texture));
    if (tidy_brush(&outside)) out.push_back(std::move(outside));
    rest.sides.push_back(make_side(c.plane, c.texture));
    if (!tidy_brush(&rest)) return out;
  }
  // `rest` is inside the cut: gone.
  return out;
}

bool read_plan(std::string_view text, Source* out, std::string* error) {
  Plan plan;
  std::string errors;
  int line_no = 0;
  size_t at = 0;
  const auto fault = [&](const std::string& what, const std::string& next) {
    errors += "line " + std::to_string(line_no) + ": " + what + "; next: " + next + "\n";
  };
  while (at <= text.size()) {
    const size_t nl = text.find('\n', at);
    const std::string_view line =
        text.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
    at = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
    ++line_no;
    const Words words = split_words(line);
    if (words.bad_quote) {
      fault("a quote is not closed", "close it with \"");
      continue;
    }
    const std::vector<std::string>& w = words.list;
    if (w.empty()) continue;
    const std::string& verb = w[0];
    // Options: key=value words; the rest are positional.
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<std::string> pos;
    bool once = false;
    for (size_t i = 1; i < w.size(); ++i) {
      if (w[i] == "once") {
        once = true;
        continue;
      }
      const size_t eq = w[i].find('=');
      if (eq != std::string::npos && eq > 0) {
        opts.emplace_back(w[i].substr(0, eq), w[i].substr(eq + 1));
      } else {
        pos.push_back(w[i]);
      }
    }
    const auto opt = [&](const char* k) -> const std::string* {
      for (const auto& [a, b] : opts) {
        if (a == k) return &b;
      }
      return nullptr;
    };
    const auto opt_or = [&](const char* k, const std::string& d) {
      const std::string* v = opt(k);
      return v ? *v : d;
    };

    if (verb == "world") {
      for (const auto& kv : opts) plan.world.push_back(kv);
      continue;
    }
    if (verb == "textures") {
      plan.floor = opt_or("floor", plan.floor);
      plan.wall = opt_or("wall", plan.wall);
      plan.ceil = opt_or("ceil", plan.ceil);
      plan.solid = opt_or("solid", plan.solid);
      continue;
    }
    if (verb == "pad") {
      if (pos.size() != 1 || !number(pos[0], &plan.pad) || plan.pad < 8) {
        fault("pad takes one number of 8 or more", "write pad 32");
      }
      continue;
    }
    if (verb == "symmetry") {
      std::vector<double> c;
      if (const std::string* r = opt("rotate"); r && point(*r, 2, &c)) {
        plan.sym = 1;
        plan.sym_at[0] = c[0];
        plan.sym_at[1] = c[1];
      } else if (const std::string* x = opt("mirror-x"); x && number(*x, &plan.sym_at[0])) {
        plan.sym = 2;
      } else if (const std::string* y = opt("mirror-y"); y && number(*y, &plan.sym_at[1])) {
        plan.sym = 3;
      } else {
        fault("symmetry needs rotate=x,y, mirror-x=x or mirror-y=y", "write symmetry rotate=0,0");
      }
      continue;
    }
    if (verb == "light" || verb == "spawn" || verb == "entity") {
      Ent e;
      e.line = line_no;
      e.once = once;
      size_t p = 0;
      if (verb == "entity") {
        if (pos.empty()) {
          fault("entity needs a class and a point", "write entity item_health 64,64,24");
          continue;
        }
        e.cls = pos[p++];
      } else {
        e.cls = verb == "light" ? "light" : opt_or("class", "info_player_deathmatch");
      }
      std::vector<double> c;
      if (p >= pos.size() || !point(pos[p], 3, &c)) {
        fault(verb + " needs a point x,y,z", "write " + verb + " 64,64,24");
        continue;
      }
      std::copy(c.begin(), c.end(), e.at);
      for (const auto& [k, v] : opts) {
        if (k == "class") continue;
        e.keys.emplace_back(verb == "light" && k == "value" ? "light" : k, v);
      }
      plan.ents.push_back(std::move(e));
      continue;
    }

    // The shapes.
    Shape s;
    s.line = line_no;
    s.once = once;
    if (verb == "room") {
      s.op = Op::carve;
    } else if (verb == "liquid") {
      s.op = Op::liquid;
    } else if (verb == "solid" || verb == "wall" || verb == "stairs" || verb == "ramp") {
      s.op = Op::solid;
    } else {
      fault("'" + verb + "' is no statement of a plan",
            "use world, textures, pad, room, solid, wall, stairs, ramp, liquid, light, spawn, "
            "entity or symmetry");
      continue;
    }
    // A name is allowed after room and solid.
    size_t p = 0;
    if ((verb == "room" || verb == "solid") && p < pos.size() && pos[p] != "box" &&
        pos[p] != "poly" && pos[p] != "at") {
      ++p;
    }
    const std::string* z = opt("z");
    const size_t dots = z ? z->find("..") : std::string::npos;
    if (dots == std::string::npos || !number(z->substr(0, dots), &s.z0) ||
        !number(z->substr(dots + 2), &s.z1) || !(s.z1 > s.z0)) {
      fault(verb + " needs z=low..high with high above low", "write z=0..128");
      continue;
    }
    std::vector<double> c;
    bool ok = true;
    if (verb == "wall") {
      double thick = 16;
      if (const std::string* t = opt("thick"); t && (!number(*t, &thick) || !(thick > 0)))
        ok = false;
      if (p >= pos.size() || pos[p] != "line" || pos.size() - p < 3) {
        fault("wall needs line and two points or more", "write wall z=0..128 line 0,0 128,0");
        continue;
      }
      Poly pts;
      for (size_t i = p + 1; i < pos.size() && ok; ++i) {
        ok = point(pos[i], 2, &c);
        if (ok) pts.emplace_back(c[0], c[1]);
      }
      if (!ok) {
        fault("a point of the wall's line is not x,y", "write points like 128,64");
        continue;
      }
      const std::string tex = opt_or("tex", plan.solid);
      for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const double dx = pts[i + 1].first - pts[i].first, dy = pts[i + 1].second - pts[i].second;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (!(len > 0)) continue;
        const double ux = dx / len * thick / 2, uy = dy / len * thick / 2;
        Shape seg = s;
        const P2 a = {pts[i].first - ux, pts[i].second - uy};
        const P2 b = {pts[i + 1].first + ux, pts[i + 1].second + uy};
        // Counter-clockwise: the left side (−uy, ux) after the right.
        seg.poly = {{a.first + uy, a.second - ux},
                    {b.first + uy, b.second - ux},
                    {b.first - uy, b.second + ux},
                    {a.first - uy, a.second + ux}};
        seg.floor = seg.wall = seg.ceil = tex;
        plan.shapes.push_back(seg);
      }
      continue;
    }
    if (p < pos.size() && pos[p] == "box") {
      std::vector<double> d;
      if (pos.size() - p != 3 || !point(pos[p + 1], 2, &c) || !point(pos[p + 2], 2, &d)) {
        fault("box needs two corners x,y", "write box 0,0 256,128");
        continue;
      }
      const double x0 = std::min(c[0], d[0]), x1 = std::max(c[0], d[0]);
      const double y0 = std::min(c[1], d[1]), y1 = std::max(c[1], d[1]);
      if (!(x1 > x0 && y1 > y0)) {
        fault("the box has no area", "give its corners different x and y");
        continue;
      }
      s.poly = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    } else if (p < pos.size() && pos[p] == "at") {
      if (pos.size() - p != 4 || pos[p + 2] != "size" || !point(pos[p + 1], 2, &c)) {
        fault("at needs a corner and a size", "write at 0,0 size 256x128");
        continue;
      }
      const std::string& sz = pos[p + 3];
      const size_t x = sz.find('x');
      double w0 = 0, h0 = 0;
      if (x == std::string::npos || !number(sz.substr(0, x), &w0) ||
          !number(sz.substr(x + 1), &h0) || !(w0 > 0 && h0 > 0)) {
        fault("the size is not WIDTHxDEPTH", "write size 256x128");
        continue;
      }
      s.poly = {{c[0], c[1]}, {c[0] + w0, c[1]}, {c[0] + w0, c[1] + h0}, {c[0], c[1] + h0}};
    } else if (p < pos.size() && pos[p] == "poly") {
      for (size_t i = p + 1; i < pos.size() && ok; ++i) {
        ok = point(pos[i], 2, &c);
        if (ok) s.poly.emplace_back(c[0], c[1]);
      }
      s.poly = clean(s.poly);
      if (!ok || s.poly.size() < 3) {
        fault("poly needs three corners x,y or more", "write poly 0,0 256,0 128,192");
        continue;
      }
      if (signed_area(s.poly) < 0) std::reverse(s.poly.begin(), s.poly.end());
    } else {
      fault(verb + " needs a shape: box, at or poly", "write box 0,0 256,128");
      continue;
    }
    if (verb == "stairs" || verb == "ramp") {
      s.stairs = verb == "stairs";
      s.ramp = verb == "ramp";
      const std::string up = opt_or("up", "");
      s.up = -1;
      for (int k = 0; k < 4; ++k) {
        if (up == kUpNames[k]) s.up = k;
      }
      if (s.up < 0 || pos[p] == "poly") {
        fault(verb + " needs a box and up=+x, -x, +y or -y",
              "write " + verb + " z=0..64 box 0,0 64,128 up=+y");
        continue;
      }
      double steps = std::ceil((s.z1 - s.z0) / 16);
      if (const std::string* st = opt("steps"); st && (!number(*st, &steps) || steps < 1)) {
        fault("steps is a whole number of 1 or more", "write steps=4");
        continue;
      }
      s.steps = int(steps);
    }
    if (s.op == Op::carve) {
      s.floor = opt_or("floor", plan.floor);
      s.wall = opt_or("wall", plan.wall);
      s.ceil = opt_or("ceil", plan.ceil);
    } else if (s.op == Op::liquid) {
      const std::string kind = opt_or("kind", "water");
      if (kind != "water" && kind != "slime" && kind != "lava") {
        fault("kind is water, slime or lava", "write kind=water");
        continue;
      }
      const std::string tex = opt_or("tex", "*" + kind);
      if (tex.empty() || tex[0] != '*' ||
          contents_of_texture(tex) != contents_of_texture("*" + kind)) {
        fault("a liquid's texture starts with * and names its kind (" + tex + ")",
              "write tex=*" + kind + "1 or leave tex out");
        continue;
      }
      s.floor = s.wall = s.ceil = tex;
    } else {
      const std::string tex = opt_or("tex", plan.solid);
      s.floor = s.wall = tex;
      s.ceil = opt_or("top", tex);
    }
    for (const std::string* t : {&s.floor, &s.wall, &s.ceil}) {
      if (s.op != Op::liquid && !t->empty() && (*t)[0] == '*') {
        fault("'" + *t + "' is a liquid's texture on a " + verb,
              "fill pools with liquid; give walls, floors and ceilings a surface's texture");
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    if (s.op != Op::carve && contents_of_texture(s.floor) == kSky) {
      fault("a " + verb + " of sky", "open a room to the sky with its ceil=");
      continue;
    }
    plan.shapes.push_back(std::move(s));
  }
  if (!errors.empty()) {
    *error = errors;
    if (!error->empty() && error->back() == '\n') error->pop_back();
    return false;
  }
  if (plan.shapes.empty()) {
    *error = "the plan has no rooms; next: carve one: room hall z=0..128 box 0,0 256,256";
    return false;
  }
  if (plan.sym != 0) {
    const size_t n = plan.shapes.size(), m = plan.ents.size();
    for (size_t i = 0; i < n; ++i) {
      if (!plan.shapes[i].once)
        plan.shapes.push_back(mirrored(plan.shapes[i], plan.sym, plan.sym_at));
    }
    for (size_t i = 0; i < m; ++i) {
      if (!plan.ents[i].once) plan.ents.push_back(mirrored(plan.ents[i], plan.sym, plan.sym_at));
    }
  }

  // The block everything is carved from.
  V3 lo = {1e30, 1e30, 1e30}, hi = {-1e30, -1e30, -1e30};
  for (const Shape& s : plan.shapes) {
    for (const P2& p : s.poly) {
      lo[0] = std::min(lo[0], p.first);
      hi[0] = std::max(hi[0], p.first);
      lo[1] = std::min(lo[1], p.second);
      hi[1] = std::max(hi[1], p.second);
    }
    lo[2] = std::min(lo[2], s.z0);
    hi[2] = std::max(hi[2], s.z1 + 16);
  }
  for (size_t k = 0; k < 3; ++k) {
    lo[k] = std::floor(lo[k]) - plan.pad;
    hi[k] = std::ceil(hi[k]) + plan.pad;
  }
  std::vector<Brush> brushes = {box_brush(lo, hi, plan.solid)};
  for (const Shape& s : plan.shapes) {
    if (s.op == Op::carve) {
      const bool sky = contents_of_texture(s.ceil) == kSky;
      for (const Poly& piece : convex_pieces(s.poly)) {
        const Brush cut = prism(piece, s.z0, s.z1 + (sky ? 16 : 0), s.floor, s.ceil, s.wall);
        std::vector<Brush> next;
        for (const Brush& b : brushes) {
          std::vector<Brush> parts = subtract(b, cut);
          for (Brush& q : parts) next.push_back(std::move(q));
        }
        brushes = std::move(next);
        if (sky) {
          Brush top = prism(piece, s.z1, s.z1 + 16, s.ceil, s.ceil, s.ceil);
          top.line = s.line;
          brushes.push_back(std::move(top));
        }
      }
      continue;
    }
    if (s.stairs || s.ramp) {
      double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
      for (const P2& q : s.poly) {
        x0 = std::min(x0, q.first);
        y0 = std::min(y0, q.second);
        x1 = std::max(x1, q.first);
        y1 = std::max(y1, q.second);
      }
      const int axis = s.up / 2;
      const bool rising = (s.up % 2) == 0;
      const double a0 = axis == 0 ? x0 : y0, a1 = axis == 0 ? x1 : y1;
      if (s.stairs) {
        for (int k = 0; k < s.steps; ++k) {
          // Step k from the low end: its slice and its top.
          const double f0 = double(k) / s.steps, f1 = double(k + 1) / s.steps;
          double b0 = rising ? a0 + (a1 - a0) * f0 : a1 - (a1 - a0) * f1;
          double b1 = rising ? a0 + (a1 - a0) * f1 : a1 - (a1 - a0) * f0;
          // Each step reaches to the high end, so the risers are the only vertical faces.
          if (rising)
            b1 = a1;
          else
            b0 = a0;
          const double top = s.z0 + (s.z1 - s.z0) * f1;
          const Poly q = axis == 0 ? Poly{{b0, y0}, {b1, y0}, {b1, y1}, {b0, y1}}
                                   : Poly{{x0, b0}, {x1, b0}, {x1, b1}, {x0, b1}};
          Brush b = prism(q, s.z0, top, s.floor, s.ceil, s.wall);
          b.line = s.line;
          brushes.push_back(std::move(b));
        }
      } else {
        Brush b = prism(s.poly, s.z0, s.z1, s.floor, s.ceil, s.wall);
        // The top slanted: through the low edge at z0 and the high edge at z1.
        const double run = a1 - a0, rise = s.z1 - s.z0;
        V3 n = {0, 0, run};
        n[size_t(axis)] = rising ? -rise : rise;
        n = normalized(n);
        V3 low = {x0, y0, s.z0};
        if (!rising) low[size_t(axis)] = a1;
        b.sides[1] = make_side({n, dot(n, low)}, s.ceil);
        b.line = s.line;
        if (tidy_brush(&b)) brushes.push_back(std::move(b));
      }
      continue;
    }
    for (const Poly& piece : convex_pieces(s.poly)) {
      Brush b = prism(piece, s.z0, s.z1, s.floor, s.ceil, s.wall);
      b.line = s.line;
      brushes.push_back(std::move(b));
    }
  }

  out->entities.clear();
  Entity world;
  world.keys.emplace_back("classname", "worldspawn");
  for (const auto& kv : plan.world) world.set(kv.first, kv.second);
  world.brushes = std::move(brushes);
  out->entities.push_back(std::move(world));
  for (const Ent& e : plan.ents) {
    Entity x;
    x.line = e.line;
    x.keys.emplace_back("classname", e.cls);
    x.keys.emplace_back("origin", fmt(e.at[0]) + " " + fmt(e.at[1]) + " " + fmt(e.at[2]));
    for (const auto& kv : e.keys) x.set(kv.first, kv.second);
    out->entities.push_back(std::move(x));
  }
  set_contents(out);
  return true;
}

}  // namespace ge::brush::mapc
