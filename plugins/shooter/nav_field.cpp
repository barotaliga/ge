// The fields to a goal from every node (nav_field.hpp): a reverse search over the layered graph.
//
// ge:summary Fields to a goal: the layered reverse search, the cache, the way read off a field
// ge:keywords nav-field reverse-search layers errand press door-wait route-table
// ge:skill shooter bots
// ge:test tests/shooter_nav_field_test.cpp
#include "shooter/nav_field.hpp"

#include <limits>

namespace ge::shooter {

namespace {

constexpr float kNone = std::numeric_limits<float>::infinity();

bool same_jumper(const NavJumper& a, const NavJumper& b) {
  return a.rockets == b.rockets && a.grenades == b.grenades && a.scripts == b.scripts &&
         a.escapes == b.escapes && a.gaps == b.gaps &&
         (a.gap_build_s >= 0) == (b.gap_build_s >= 0) && a.level == b.level &&
         a.extra_s == b.extra_s && a.lip_share == b.lip_share && a.wall_share == b.wall_share;
}

bool fires(const NavErrand& e, int32_t ent) {
  if (ent < 0) return false;
  for (const int32_t d : e.doors)
    if (d == ent) return true;
  return false;
}

// Whether `ent` (a link's or a node's mover) is shut in `layer`: a door some button fires, and not
// one the layer's own button fired.
bool shut_in(const NavFields& fs, int32_t ent, int32_t layer) {
  if (ent < 0) return false;
  if (layer > 0 && fires(fs.errands[static_cast<size_t>(layer - 1)], ent)) return false;
  for (const NavErrand& e : fs.errands)
    if (fires(e, ent)) return true;
  return false;
}

void build(NavFields& fs, NavField& f) {
  const NavGraph& g = *fs.g;
  const auto n = static_cast<int32_t>(g.nodes.size());
  const int32_t layers = f.layers;
  const size_t states = static_cast<size_t>(layers) * static_cast<size_t>(n);
  f.time_s.assign(states, kNone);
  f.next.assign(states, -1);
  fs.to_cross.assign(states, kNone);
  ai::Search& s = fs.search;
  s.reset(states);
  if (f.goal < 0 || f.goal >= n) return;
  for (int32_t L = 0; L < layers; L++) s.seed(L * n + f.goal);
  const NavJumper& j = f.jumper;
  s.run_all([&](int32_t st) {
    const int32_t L = st / n, b = st % n;
    const double here = s.cost(st);
    const float cross = fs.to_cross[static_cast<size_t>(st)];
    for (int32_t r = fs.rev_first[static_cast<size_t>(b)];
         r < fs.rev_first[static_cast<size_t>(b) + 1]; r++) {
      const int32_t a = fs.rev_from[static_cast<size_t>(r)];
      const NavLink& l = g.links[static_cast<size_t>(fs.rev_link[static_cast<size_t>(r)])];
      // Taken as `nav_route` takes it, a gap trick whatever its start (its speed built on the way).
      if (!j.takes(l) && !(l.gap && j.gap_build_s >= 0 && l.level <= j.level)) continue;
      const int32_t into = g.nodes[static_cast<size_t>(b)].mover;
      if (shut_in(fs, l.mover, L) || shut_in(fs, into, L)) continue;
      double c = j.cost_s(l) + (l.mover >= 0 ? fs.mover_penalty_s : 0);
      if (j.wall_share > 0 && l.kind == NavKind::walk && !g.room.empty()) {
        const uint8_t room = g.room[static_cast<size_t>(b)];
        if (room < 2) c += l.time_s * j.wall_share * (room == 0 ? 1.0f : 0.5f);
      }
      const int32_t from = L * n + a;
      if (!s.improves(from, here + c)) continue;
      s.reach(from, here + c, st);
      if (L > 0) {
        const NavErrand& e = fs.errands[static_cast<size_t>(L - 1)];
        const bool crosses = fires(e, l.mover) || fires(e, into);
        fs.to_cross[static_cast<size_t>(from)] =
            crosses ? static_cast<float>(c) : cross + static_cast<float>(c);
      }
    }
    // The press: at a node the layer's button is pressed from, from the first layer, when the
    // way on crosses its door in time.
    if (L > 0 && cross < kNone) {
      const NavErrand& e = fs.errands[static_cast<size_t>(L - 1)];
      bool presses = false;
      for (int32_t k = fs.press_first[static_cast<size_t>(b)];
           k < fs.press_first[static_cast<size_t>(b) + 1]; k++)
        presses = presses || fs.press_errand[static_cast<size_t>(k)] == L - 1;
      if (presses && (e.open_s <= 0 || cross <= e.open_s)) {
        const int32_t from = b;  // layer 0
        const double c = here + e.press_s;
        if (s.improves(from, c)) s.reach(from, c, st);
      }
    }
  });
  for (size_t k = 0; k < states; k++) {
    const auto st = static_cast<int32_t>(k);
    if (!s.reached(st)) continue;
    f.time_s[k] = static_cast<float>(s.cost(st));
    f.next[k] = s.prev(st);
  }
  fs.built++;
}

}  // namespace

void nav_fields_init(NavFields& fs, const NavGraph& g, std::span<const NavErrand> errands) {
  fs.g = &g;
  fs.errands.assign(errands.begin(), errands.end());
  fs.fields.clear();
  fs.built = 0;
  const size_t n = g.nodes.size();
  // Where each errand is pressed from, by node.
  fs.press_first.assign(n + 1, 0);
  const auto each_press = [&](auto&& fn) {
    for (size_t k = 0; k < fs.errands.size(); k++) {
      const NavErrand& e = fs.errands[k];
      if (e.button >= 0 && static_cast<size_t>(e.button) < n) fn(e.button, static_cast<int32_t>(k));
      for (const int32_t m : e.also)
        if (m >= 0 && static_cast<size_t>(m) < n && m != e.button) fn(m, static_cast<int32_t>(k));
    }
  };
  each_press([&](int32_t m, int32_t) { fs.press_first[static_cast<size_t>(m) + 1]++; });
  for (size_t a = 0; a < n; a++) fs.press_first[a + 1] += fs.press_first[a];
  fs.press_errand.assign(static_cast<size_t>(fs.press_first[n]), -1);
  {
    std::vector<int32_t> at(fs.press_first.begin(), fs.press_first.end() - 1);
    each_press([&](int32_t m, int32_t k) {
      fs.press_errand[static_cast<size_t>(at[static_cast<size_t>(m)]++)] = k;
    });
  }
  fs.rev_first.assign(n + 1, 0);
  for (size_t a = 0; a < n; a++) {
    const NavNode& nd = g.nodes[a];
    for (int32_t k = 0; k < nd.links; k++) {
      const NavLink& l = g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)];
      if (l.to >= 0 && static_cast<size_t>(l.to) < n) fs.rev_first[static_cast<size_t>(l.to) + 1]++;
    }
  }
  for (size_t a = 0; a < n; a++) fs.rev_first[a + 1] += fs.rev_first[a];
  fs.rev_from.assign(static_cast<size_t>(fs.rev_first[n]), -1);
  fs.rev_link.assign(static_cast<size_t>(fs.rev_first[n]), -1);
  std::vector<int32_t> fill(fs.rev_first.begin(), fs.rev_first.end() - 1);
  for (size_t a = 0; a < n; a++) {
    const NavNode& nd = g.nodes[a];
    for (int32_t k = 0; k < nd.links; k++) {
      const int32_t li = nd.first_link + k;
      const NavLink& l = g.links[static_cast<size_t>(li)];
      if (l.to < 0 || static_cast<size_t>(l.to) >= n) continue;
      const auto at = static_cast<size_t>(fill[static_cast<size_t>(l.to)]++);
      fs.rev_from[at] = static_cast<int32_t>(a);
      fs.rev_link[at] = li;
    }
  }
}

const NavField& nav_field(NavFields& fs, int32_t goal, const NavJumper& jumper) {
  for (const NavField& f : fs.fields)
    if (f.goal == goal && same_jumper(f.jumper, jumper)) return f;
  NavField f;
  f.goal = goal;
  f.jumper = jumper;
  f.layers = 1 + static_cast<int32_t>(fs.errands.size());
  build(fs, f);
  fs.fields.push_back(std::move(f));
  return fs.fields.back();
}

float nav_field_time(const NavFields& fs, const NavField& f, int32_t from, int32_t layer) {
  const auto n = static_cast<int32_t>(fs.g->nodes.size());
  if (from < 0 || from >= n || layer < 0 || layer >= f.layers) return kNone;
  return f.time_s[static_cast<size_t>(layer * n + from)];
}

bool nav_field_way(const NavFields& fs, const NavField& f, int32_t from, NavFieldWay& out,
                   int32_t layer) {
  out = {};
  const auto n = static_cast<int32_t>(fs.g->nodes.size());
  if (from < 0 || from >= n || layer < 0 || layer >= f.layers) return false;
  int32_t st = layer * n + from;
  out.time_s = f.time_s[static_cast<size_t>(st)];
  if (!(out.time_s < kNone)) return false;
  out.nodes.push_back(from);
  // Each step lowers the time left, so the walk ends; the bound guards a table built wrong.
  for (size_t guard = 0; guard < f.time_s.size(); guard++) {
    const int32_t nx = f.next[static_cast<size_t>(st)];
    if (nx < 0) return true;
    const int32_t L = st / n, nl = nx / n, node = nx % n;
    if (nl != L) {  // a press: the same node, the button's layer
      if (out.errand < 0) {
        out.errand = nl - 1;
        out.press_at = out.nodes.size() - 1;
      }
    } else {
      out.nodes.push_back(node);
    }
    st = nx;
  }
  return false;
}

}  // namespace ge::shooter
