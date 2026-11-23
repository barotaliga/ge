// A map's place names as points (places.hpp): each place's middle, the nearest point, its name.
//
// ge:summary A map's place names: each place's middle, the nearest point and its name
// ge:keywords nearest_loc loc_name_at loc_places
// ge:skill shooter bots
// ge:test tests/classic_locs_test.cpp
#include "shooter/places.hpp"

#include <algorithm>

namespace ge::shooter {

std::vector<Loc> loc_places(const Locs& locs) {
  std::vector<Loc> out;
  std::vector<int> count;
  for (const Loc& l : locs.points) {
    size_t i = 0;
    while (i < out.size() && out[i].name != l.name) i++;
    if (i == out.size()) {
      out.push_back({l.name, {0, 0, 0}, {}});
      count.push_back(0);
    }
    out[i].at = out[i].at + l.at;
    count[i]++;
  }
  for (size_t i = 0; i < out.size(); i++)
    out[i].at = out[i].at * (1.f / static_cast<float>(count[i]));
  return out;
}

const Loc* nearest_loc(const Locs& locs, Vec3f p, float range, const LocSight& seen) {
  const Loc* best = nullptr;
  float best_d = 0;
  for (const Loc& l : locs.points) {
    const Vec3f d = l.at - p;
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (range > 0 && dd > range * range) continue;
    if (best && dd >= best_d) continue;
    if (seen && !seen(p, l.at)) continue;  // asked only of a point that would be nearer
    best = &l;
    best_d = dd;
  }
  return best;
}

std::string loc_name_at(const Locs& locs, Vec3f p, float range, const LocSight& seen) {
  const Loc* l = nearest_loc(locs, p, range, seen);
  return l ? l->name : std::string();
}

namespace {

// The nearest point still kept (an index into `points`), -1 with none; nearest_loc's rules.
int nearest_kept(const std::vector<Loc>& points, const std::vector<char>& kept, Vec3f p,
                 float range, const LocSight& seen) {
  int best = -1;
  float best_d = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    if (!kept[i]) continue;
    const Vec3f d = points[i].at - p;
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (range > 0 && dd > range * range) continue;
    if (best >= 0 && dd >= best_d) continue;
    if (seen && !seen(p, points[i].at)) continue;
    best = static_cast<int>(i);
    best_d = dd;
  }
  return best;
}

}  // namespace

int prune_locs(Locs& locs, const std::vector<Vec3f>& spots, float range, const LocSight& seen) {
  std::vector<Loc>& pts = locs.points;
  const size_t n = pts.size();
  if (n < 2) return 0;
  std::vector<char> kept(n, 1);
  // Each spot's name with every point, and the point that names it now.
  std::vector<int> owner(spots.size());
  std::vector<std::string> named(spots.size());
  for (size_t s = 0; s < spots.size(); ++s) {
    owner[s] = nearest_kept(pts, kept, spots[s], range, seen);
    if (owner[s] >= 0) named[s] = pts[size_t(owner[s])].name;
  }
  // The order tried: names with the most points first, each name's points last to first.
  std::vector<size_t> order(n);
  for (size_t i = 0; i < n; ++i) order[i] = n - 1 - i;
  std::vector<size_t> count(n, 0);
  for (size_t i = 0; i < n; ++i)
    for (size_t j = 0; j < n; ++j) count[i] += pts[i].name == pts[j].name ? 1 : 0;
  std::stable_sort(order.begin(), order.end(),
                   [&count](size_t a, size_t b) { return count[a] > count[b]; });
  int gone = 0;
  std::vector<size_t> mine;
  std::vector<int> next;
  for (bool took = true; took;) {
    took = false;
    for (const size_t i : order) {
      if (!kept[i]) continue;
      mine.clear();
      for (size_t s = 0; s < spots.size(); ++s)
        if (owner[s] == static_cast<int>(i)) mine.push_back(s);
      kept[i] = 0;
      next.assign(mine.size(), -1);
      bool same = true;
      for (size_t k = 0; k < mine.size() && same; ++k) {
        next[k] = nearest_kept(pts, kept, spots[mine[k]], range, seen);
        same = next[k] >= 0 && pts[size_t(next[k])].name == named[mine[k]];
      }
      if (!same) {
        kept[i] = 1;
        continue;
      }
      for (size_t k = 0; k < mine.size(); ++k) owner[mine[k]] = next[k];
      ++gone;
      took = true;
    }
  }
  std::vector<Loc> left;
  left.reserve(n - size_t(gone));
  for (size_t i = 0; i < n; ++i)
    if (kept[i]) left.push_back(std::move(pts[i]));
  pts = std::move(left);
  return gone;
}

}  // namespace ge::shooter
