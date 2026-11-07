// A map's team plan, worked out from the map itself (map_plan.hpp).
//
// ge:summary A map's team plan from its shape and items: areas, worth, ways in, base, rush
// ge:keywords dijkstra run-times clusters regions ways-in hold-measure plan_text
// ge:skill bots
#include "shooters_edge/bots/map_plan.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

#include "core/num_text.hpp"
#include "shooters_edge/bots/knowledge.hpp"

namespace shooters_edge::bots {

namespace {

constexpr double kFar = std::numeric_limits<double>::infinity();
constexpr int32_t kWHeavySpiker = 8, kWGrenades = 16, kWRockets = 32, kWArc = 64;
constexpr float kEntrySpot = 128;  // the outer ends of two links into an area this near: one way in

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

bool planned(const ItemSpot& it) {
  if (it.node < 0) return false;
  switch (it.kind) {
    case ItemKind::armor1:
    case ItemKind::armor2:
    case ItemKind::armor3:
    case ItemKind::mega:
    case ItemKind::quad:
    case ItemKind::pent:
      return true;
    case ItemKind::weapon:
      return it.weapon == kWRockets || it.weapon == kWArc || it.weapon == kWGrenades;
    default:
      return false;
  }
}

bool is_power(const ItemSpot& it) { return it.kind == ItemKind::quad || it.kind == ItemKind::pent; }

// The quickest run from `from` to every node over the proven links (seconds; kFar: no way): no
// rocket jump, and with `dry` no swim either (on foot).
void runs_from(const sh::NavGraph& g, int32_t from, std::vector<double>& out, bool dry = false) {
  out.assign(g.nodes.size(), kFar);
  if (from < 0) return;
  using Entry = std::pair<double, int32_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
  out[static_cast<size_t>(from)] = 0;
  open.push({0, from});
  while (!open.empty()) {
    const auto [d, n] = open.top();
    open.pop();
    if (d > out[static_cast<size_t>(n)]) continue;
    const sh::NavNode& node = g.nodes[static_cast<size_t>(n)];
    for (int32_t k = 0; k < node.links; k++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(node.first_link) + static_cast<size_t>(k)];
      if (sh::nav_blast(l.kind) ||
          (dry && (l.kind == sh::NavKind::swim || g.nodes[static_cast<size_t>(l.to)].wet)))
        continue;
      const double nd = d + l.time_s;
      if (nd < out[static_cast<size_t>(l.to)]) {
        out[static_cast<size_t>(l.to)] = nd;
        open.push({nd, l.to});
      }
    }
  }
}

}  // namespace

double plan_worth(const ItemSpot& it) {
  switch (it.kind) {
    case ItemKind::armor3:
      return 160;  // 200 at 80 %
    case ItemKind::armor2:
      return 90;  // 150 at 60 %
    case ItemKind::armor1:
      return 30;  // 100 at 30 %
    case ItemKind::mega:
      return 100;
    case ItemKind::quad:
      return 400;
    case ItemKind::pent:
      return 500;
    case ItemKind::weapon:
      return it.weapon == kWRockets    ? 150
             : it.weapon == kWArc      ? 120
             : it.weapon == kWGrenades ? 40
                                       : 10;
    default:
      return 0;
  }
}

MapPlan make_map_plan(const sh::NavGraph& g, const std::vector<ItemSpot>& items,
                      const std::vector<sh::Vec3f>& spawns, const sh::Locs* locs,
                      const PlanSettings& set) {
  MapPlan plan;
  plan.nodes = static_cast<int64_t>(g.nodes.size());
  if (g.nodes.empty()) return plan;
  // The key items' runs to every node.
  std::vector<int32_t> keys;
  for (size_t i = 0; i < items.size(); i++)
    if (planned(items[i])) keys.push_back(static_cast<int32_t>(i));
  std::vector<std::vector<double>> run(keys.size());
  for (size_t k = 0; k < keys.size(); k++)
    runs_from(g, items[static_cast<size_t>(keys[k])].node, run[k], true);  // on foot
  const auto between = [&](size_t a, size_t b) {
    return std::min(run[a][static_cast<size_t>(items[static_cast<size_t>(keys[b])].node)],
                    run[b][static_cast<size_t>(items[static_cast<size_t>(keys[a])].node)]);
  };
  // Clusters: single linkage by run time.
  std::vector<int32_t> group(keys.size(), -1);
  int32_t groups = 0;
  for (size_t k = 0; k < keys.size(); k++) {
    if (group[k] >= 0) continue;
    group[k] = groups;
    std::vector<size_t> stack{k};
    while (!stack.empty()) {
      const size_t a = stack.back();
      stack.pop_back();
      for (size_t b = 0; b < keys.size(); b++)
        if (group[b] < 0 && is_power(items[static_cast<size_t>(keys[a])]) == false &&
            is_power(items[static_cast<size_t>(keys[b])]) == false &&
            between(a, b) <= set.cluster_s) {
          group[b] = groups;
          stack.push_back(b);
        }
    }
    groups++;
  }
  // The map's median floor (its dry floor: the water's cells are no floor).
  std::vector<float> zs;
  zs.reserve(g.nodes.size());
  for (const sh::NavNode& n : g.nodes)
    if (!n.wet) zs.push_back(n.origin.z);
  if (zs.empty()) zs.push_back(0);
  std::nth_element(zs.begin(), zs.begin() + static_cast<std::ptrdiff_t>(zs.size() / 2), zs.end());
  const float median = zs[zs.size() / 2];
  // The spawn points' runs, on foot (an area only the water leads to is visited, not held).
  std::vector<std::vector<double>> spawn_run(spawns.size());
  for (size_t s = 0; s < spawns.size(); s++)
    runs_from(g, sh::nav_nearest(g, spawns[s]), spawn_run[s], true);

  for (int32_t a = 0; a < groups; a++) {
    PlanArea area;
    area.region.assign(g.nodes.size(), 0);
    sh::Vec3f sum;
    for (size_t k = 0; k < keys.size(); k++) {
      if (group[k] != a) continue;
      const ItemSpot& it = items[static_cast<size_t>(keys[k])];
      area.items.push_back(keys[k]);
      sum = sum + it.at;
      area.worth += plan_worth(it) * 60.0 / std::max(1.f, it.respawn_s);
      area.power = area.power || it.kind == ItemKind::quad || it.kind == ItemKind::pent;
      for (size_t n = 0; n < g.nodes.size(); n++)
        if (run[k][n] <= set.region_s) area.region[n] = 1;
    }
    area.at = sum * (1.f / static_cast<float>(area.items.size()));
    area.height = area.at.z - median;
    if (locs && !locs->points.empty()) area.name = sh::loc_name_at(*locs, area.at);
    // The ways in: links from outside into the region, their outer ends merged by place.
    std::vector<sh::Vec3f> entries;
    for (size_t n = 0; n < g.nodes.size(); n++) {
      if (area.region[n] || g.nodes[n].wet) continue;
      const sh::NavNode& node = g.nodes[n];
      for (int32_t k = 0; k < node.links; k++) {
        const sh::NavLink& l =
            g.links[static_cast<size_t>(node.first_link) + static_cast<size_t>(k)];
        if (!area.region[static_cast<size_t>(l.to)] || l.kind == sh::NavKind::swim ||
            sh::nav_blast(l.kind))
          continue;
        bool known = false;
        for (const sh::Vec3f& e : entries) known = known || dist(e, node.origin) < kEntrySpot;
        if (!known) {
          entries.push_back(node.origin);
          area.entries.push_back(static_cast<int32_t>(n));
        }
      }
    }
    area.ways_in = static_cast<int32_t>(entries.size());
    // From the spawns: the mean of each spawn's quickest run to one of its items.
    double total = 0;
    int counted = 0;
    for (const std::vector<double>& sr : spawn_run) {
      double best = kFar;
      for (const int32_t i : area.items)
        best = std::min(best, sr[static_cast<size_t>(items[static_cast<size_t>(i)].node)]);
      if (best < kFar) {
        total += best;
        counted++;
      }
    }
    area.from_spawns_s = counted ? total / counted : 0;
    area.reached = counted * 2 >= static_cast<int>(spawn_run.size()) || spawn_run.empty();
    const double high = std::clamp(area.height / 64.0 * 0.1, 0.0, 0.3);
    area.hold =
        area.reached ? area.worth / (1 + 0.1 * area.ways_in * area.ways_in) * (1 + high) : 0;
    plan.areas.push_back(std::move(area));
  }
  std::stable_sort(plan.areas.begin(), plan.areas.end(),
                   [](const PlanArea& a, const PlanArea& b) { return a.worth > b.worth; });
  // Between the areas: the quickest run from one's items to the other's.
  const size_t na = plan.areas.size();
  plan.between_s.assign(na * na, 0);
  for (size_t a = 0; a < na; a++) {
    std::vector<double> from;
    runs_from(g, items[static_cast<size_t>(plan.areas[a].items[0])].node, from);
    for (size_t b = 0; b < na; b++) {
      double best = kFar;
      for (const int32_t i : plan.areas[b].items)
        best = std::min(best, from[static_cast<size_t>(items[static_cast<size_t>(i)].node)]);
      plan.between_s[a * na + b] = best;
    }
  }
  // The base and the forward area: the best two to hold; the powers' areas are rushed.
  for (size_t a = 0; a < na; a++) {
    const PlanArea& ar = plan.areas[a];
    if (!ar.reached) continue;  // no way there from most of the map: taken when it can be
    if (ar.power) {
      plan.rushes.push_back(static_cast<int32_t>(a));
      continue;
    }
    if (plan.base < 0 || ar.hold > plan.areas[static_cast<size_t>(plan.base)].hold) {
      plan.forward = plan.base;
      plan.base = static_cast<int32_t>(a);
    } else if (plan.forward < 0 || ar.hold > plan.areas[static_cast<size_t>(plan.forward)].hold) {
      plan.forward = static_cast<int32_t>(a);
    }
  }
  // The opening: the kinds' order, or the map's own; each item's late run off the spawns.
  const auto rank = [&](const ItemSpot& it) -> int {
    const std::string call = item_call(it);
    const bool key = it.kind == ItemKind::pent || it.kind == ItemKind::quad ||
                     it.kind == ItemKind::armor1 || it.kind == ItemKind::armor2 ||
                     it.kind == ItemKind::armor3 || it.kind == ItemKind::mega ||
                     it.kind == ItemKind::weapon;
    if (!key) return -1;  // a health box, ammunition: taken on the way
    if (!set.order.empty()) {
      for (size_t k = 0; k < set.order.size(); k++)
        if (set.order[k] == call) return static_cast<int>(k);
      return -1;
    }
    switch (it.kind) {
      case ItemKind::pent:
        return 0;
      case ItemKind::quad:
        return 1;
      case ItemKind::armor3:
        return 3;
      case ItemKind::armor2:
        return 4;
      case ItemKind::mega:
        return 7;
      case ItemKind::armor1:
        return 8;
      case ItemKind::weapon:
        return it.weapon == kWRockets       ? 2
               : it.weapon == kWArc         ? 5
               : it.weapon == kWHeavySpiker ? 6
                                            : -1;
      default:
        return -1;
    }
  };
  struct Ranked {
    int rank;
    double quickest;
    int32_t item;
  };
  std::vector<Ranked> ranked;
  for (size_t i = 0; i < items.size(); i++) {
    const ItemSpot& it = items[i];
    const int r = it.node < 0 ? -1 : rank(it);
    if (r < 0) continue;
    double quickest = kFar;
    for (const std::vector<double>& sr : spawn_run)
      quickest = std::min(quickest, sr[static_cast<size_t>(it.node)]);
    if (spawn_run.empty()) quickest = 0;
    if (quickest >= kFar) continue;  // no spawn's way there on foot
    ranked.push_back({r, quickest, static_cast<int32_t>(i)});
  }
  std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
    return a.rank != b.rank ? a.rank < b.rank : a.quickest < b.quickest;
  });
  for (const Ranked& r : ranked) {
    const ItemSpot& it = items[static_cast<size_t>(r.item)];
    plan.opening.push_back({r.item,
                            it.kind == ItemKind::pent   ? 4
                            : it.kind == ItemKind::quad ? 2
                                                        : 1,
                            r.quickest + set.late_s});
  }
  for (const int32_t r : plan.rushes) {
    const double run_s =
        plan.base >= 0
            ? plan.between_s[static_cast<size_t>(plan.base) * na + static_cast<size_t>(r)]
            : 0;
    plan.rush_leave_s.push_back((run_s < kFar ? run_s : 10) + set.rush_lead_s);
  }
  return plan;
}

std::vector<std::string> opening_order_of(std::string_view map) {
  std::string name(map);
  if (name.rfind("maps/", 0) == 0) name.erase(0, 5);
  if (name.size() > 4 && name.ends_with(".bsp")) name.resize(name.size() - 4);
  // Written from how the maps are played (the pent first wherever it can be reached in time).
  if (name == "dm3") return {"pent", "quad", "rl", "ra", "ya", "sng", "mh"};
  return {};
}

int32_t plan_area_of(const MapPlan& plan, int32_t n) {
  if (n < 0) return -1;
  for (size_t a = 0; a < plan.areas.size(); a++)
    if (static_cast<size_t>(n) < plan.areas[a].region.size() &&
        plan.areas[a].region[static_cast<size_t>(n)])
      return static_cast<int32_t>(a);
  return -1;
}

std::string plan_text(const MapPlan& plan, const std::vector<ItemSpot>& items) {
  using ge::core::num::to_fixed;
  std::string out;
  for (size_t a = 0; a < plan.areas.size(); a++) {
    const PlanArea& ar = plan.areas[a];
    std::string what;
    for (const int32_t i : ar.items)
      what += std::string(what.empty() ? "" : "+") + item_call(items[static_cast<size_t>(i)]);
    out += "area " + std::to_string(a) + " " + (ar.name.empty() ? "-" : ar.name) + ": " + what +
           ", worth " + to_fixed(ar.worth, 0) + "/min, " + std::to_string(ar.ways_in) +
           " ways in, height " + to_fixed(ar.height, 0) + ", " + to_fixed(ar.from_spawns_s, 1) +
           " s from the spawns, hold " + to_fixed(ar.hold, 0) +
           (ar.reached ? "" : ", no way there from most spawns") + "\n";
  }
  const auto name = [&](int32_t a) {
    return a < 0 ? std::string("none")
                 : std::to_string(a) + " (" + plan.areas[static_cast<size_t>(a)].name + ")";
  };
  out += "base " + name(plan.base) + ", forward " + name(plan.forward);
  for (size_t r = 0; r < plan.rushes.size(); r++)
    out += ", rush " + name(plan.rushes[r]) + " leaving " + to_fixed(plan.rush_leave_s[r], 1) +
           " s before it is due";
  out += "\n";
  if (!plan.opening.empty()) {
    out += "opening";
    for (const OpenItem& o : plan.opening)
      out += " > " + std::string(item_call(items[static_cast<size_t>(o.item)])) + " (" +
             std::to_string(o.cap) + ", late past " + to_fixed(o.late_s, 1) + " s)";
    out += "\n";
  }
  return out;
}

}  // namespace shooters_edge::bots
