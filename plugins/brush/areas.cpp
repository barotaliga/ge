// Version 38 areas flooded over their open portals (areas.hpp).
//
// ge:summary Floods a version 38 map's areas over its open portals; who reaches whom
// ge:keywords area area-portal flood_areas areas_connected area_at
// ge:skill brush
// ge:test tests/brush_areas_test.cpp
#include "brush/areas.hpp"

namespace ge::brush {

void flood_areas(const Map& map, std::span<const uint8_t> open, std::vector<int32_t>* group) {
  group->assign(map.areas.size(), 0);
  std::vector<int32_t> stack;
  int32_t next = 0;
  for (size_t start = 1; start < map.areas.size(); ++start) {
    if ((*group)[start] != 0) continue;
    const int32_t g = ++next;
    (*group)[start] = g;
    stack.assign(1, int32_t(start));
    while (!stack.empty()) {
      const Area& a = map.areas[size_t(stack.back())];
      stack.pop_back();
      for (int32_t k = 0; k < a.portals; ++k) {
        const size_t at = size_t(a.first) + size_t(k);
        if (a.first < 0 || at >= map.area_portals.size()) break;
        const AreaPortal& p = map.area_portals[at];
        if (p.portal < 0 || size_t(p.portal) >= open.size() || open[size_t(p.portal)] == 0)
          continue;
        if (p.other <= 0 || size_t(p.other) >= map.areas.size() || (*group)[size_t(p.other)] != 0)
          continue;
        (*group)[size_t(p.other)] = g;
        stack.push_back(p.other);
      }
    }
  }
}

bool areas_connected(std::span<const int32_t> group, int32_t a, int32_t b) {
  if (a < 0 || b < 0 || size_t(a) >= group.size() || size_t(b) >= group.size()) return true;
  return group[size_t(a)] == group[size_t(b)];
}

int32_t area_at(const Map& map, const float p[3]) {
  if (map.areas.empty() || map.leaves.empty()) return 0;
  return map.leaves[leaf_at(map, p)].area;
}

}  // namespace ge::brush
