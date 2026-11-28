// Elevators (`make_elevator`, `nav_cut_shafts`): a car with a stop at each floor that moves only
// when its rider presses a button beside its top, a floor up or down a press (Dissolution of
// Eternity's, r1m7's three floors). The nav rides it as a moving platform between its stops; the
// buttons are kept on it for the pilot to press. A link of the map's own that crosses the shaft
// the car runs up is proven with the car empty; it is kept only where it holds with the car at
// every floor (the car stays where its rider left it).
//
// ge:summary Elevators in the nav: a stop per floor and the buttons that send the car, and the
// ge:summary links across its shaft kept only where they hold with the car at every floor
// ge:keywords nav elevator car floor button call shaft moving-platform stop make_elevator
// ge:keywords nav_cut_shafts new-plat dissolution
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_test.cpp
#include <cstdlib>
#include <cstring>
#include <span>

#include "core/math.hpp"
#include "shooter/brush_hull.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/nav.hpp"

namespace ge::shooterbots {

bool make_elevator(NavMover& m, int floors, float apart, bool at_top, float speed, float car_mid_z,
                   std::span<const ElevatorButton> buttons) {
  m.stops.clear();
  m.stop_wait_s.clear();
  m.calls.clear();
  if (floors < 2 || apart <= 0) return false;
  const float first = at_top ? -apart * static_cast<float>(floors - 1) : 0.f;
  for (int k = 0; k < floors; k++) {
    m.stops.push_back({m.rest.x, m.rest.y, m.rest.z + first + apart * static_cast<float>(k)});
    m.stop_wait_s.push_back(0);
  }
  m.speed = speed > 0 ? speed : 150.f;
  // A button sends the car a floor from the floor whose car stands under the button's middle by
  // less than a floor (one at the car's own level); pressed from its top there.
  const float mid0 = car_mid_z + first;
  for (const ElevatorButton& b : buttons) {
    const float level = ((b.lo.z + b.hi.z) * 0.5f - mid0) / apart;
    if (level < 0 || level >= static_cast<float>(floors)) continue;
    NavMover::Call c;
    c.lo = b.lo;
    c.hi = b.hi;
    c.stop = static_cast<int32_t>(level);
    c.up = !b.down;
    if (c.up ? c.stop + 1 >= floors : c.stop == 0) continue;  // a floor it cannot go past
    m.calls.push_back(c);
  }
  if (m.calls.empty()) {
    m.stops.clear();
    m.stop_wait_s.clear();
    return false;
  }
  return true;
}

bool elevator_of(NavMover& m, const EntityBlock& b, std::span<const EntityBlock> blocks,
                 std::span<const ge::shooter::BrushModel> models) {
  const char* model = block_value(b, "model");
  const char* name = block_value(b, "targetname");
  const int own = model && model[0] == '*' ? std::atoi(model + 1) : 0;
  if (own <= 0 || static_cast<size_t>(own) >= models.size() || name == nullptr) return false;
  std::vector<ElevatorButton> buttons;
  for (const EntityBlock& o : blocks) {
    const char* cn = block_value(o, "classname");
    const char* om = block_value(o, "model");
    const char* target = block_value(o, "target");
    const int k = om && om[0] == '*' ? std::atoi(om + 1) : 0;
    if (!cn || std::strcmp(cn, "func_elvtr_button") != 0 || k <= 0 ||
        static_cast<size_t>(k) >= models.size() || !target || std::strcmp(target, name) != 0)
      continue;
    const char* sf = block_value(o, "spawnflags");
    const ge::shooter::BrushModel& bm = models[static_cast<size_t>(k)];
    buttons.push_back({bm.mins, bm.maxs, sf != nullptr && (std::atoi(sf) & 1) != 0});
  }
  const ge::shooter::BrushModel& car = models[static_cast<size_t>(own)];
  const char* cnt = block_value(b, "cnt");
  const char* h = block_value(b, "height");
  const char* sp = block_value(b, "speed");
  const char* sf = block_value(b, "spawnflags");
  const float hv = h ? static_cast<float>(std::atof(h)) : 0.f;
  return make_elevator(m, cnt ? std::atoi(cnt) : 0, hv > 0 ? hv : (car.maxs.z - car.mins.z) - 8,
                       sf != nullptr && (std::atoi(sf) & 8) != 0,
                       sp ? static_cast<float>(std::atof(sp)) : 150.f,
                       (car.mins.z + car.maxs.z) * 0.5f, buttons);
}

int64_t nav_cut_shafts(const MoveWorld& world, const MoveSettings& move, NavGraph& g,
                       std::span<const NavMover> movers) {
  int64_t cut = 0;
  for (const NavMover& m : movers) {
    if (!elevator(m) || world.count >= kMaxSolids) continue;
    // The car's whole way, its box at the bottom stop to its box at the top.
    Vec3f lo = m.absmin, hi = m.absmax;
    for (const Vec3f& at : m.stops) {
      const Vec3f d = at - m.rest;
      lo = {std::min(lo.x, m.absmin.x + d.x), std::min(lo.y, m.absmin.y + d.y),
            std::min(lo.z, m.absmin.z + d.z)};
      hi = {std::max(hi.x, m.absmax.x + d.x), std::max(hi.y, m.absmax.y + d.y),
            std::max(hi.z, m.absmax.z + d.z)};
    }
    std::vector<NavLink> links;
    links.reserve(g.links.size());
    for (size_t a = 0; a < g.nodes.size(); a++) {
      NavNode& n = g.nodes[a];
      const int32_t first = static_cast<int32_t>(links.size());
      for (int32_t k = 0; k < n.links; k++) {
        const NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
        const Vec3f to = g.nodes[static_cast<size_t>(l.to)].origin;
        const bool own = l.mover == m.ent || n.mover == m.ent ||
                         g.nodes[static_cast<size_t>(l.to)].mover == m.ent;
        const bool proven = l.kind == NavKind::walk || l.kind == NavKind::drop ||
                            (l.kind == NavKind::jump && !l.updraft);
        const Vec3f blo{std::min(n.origin.x, to.x) + kPlayerMins.x,
                        std::min(n.origin.y, to.y) + kPlayerMins.y,
                        std::min(n.origin.z, to.z) + kPlayerMins.z};
        const Vec3f bhi{std::max(n.origin.x, to.x) + kPlayerMaxs.x,
                        std::max(n.origin.y, to.y) + kPlayerMaxs.y,
                        std::max(n.origin.z, to.z) + kPlayerMaxs.z};
        bool holds = true;
        if (!own && proven && blo.x < hi.x && bhi.x > lo.x && blo.y < hi.y && bhi.y > lo.y &&
            blo.z < hi.z && bhi.z > lo.z) {
          for (size_t s = 0; s < m.stops.size() && holds; s++) {
            // Not at a floor that puts the car where the link starts (r1m7's pit under the car at
            // the bottom: a jump out of it, fallen into with the car away, holds whenever he can
            // stand there; cut, he waited in the pit for the car for good).
            const Vec3f d = m.stops[s] - m.rest;
            const Vec3f clo = m.absmin + d, chi = m.absmax + d;
            if (n.origin.x + kPlayerMins.x < chi.x - 1 && n.origin.x + kPlayerMaxs.x > clo.x + 1 &&
                n.origin.y + kPlayerMins.y < chi.y - 1 && n.origin.y + kPlayerMaxs.y > clo.y + 1 &&
                n.origin.z + kPlayerMins.z < chi.z - 1 && n.origin.z + kPlayerMaxs.z > clo.z + 1)
              continue;
            MoveWorld there = world;
            there.solids[there.count++] = mover_solid(m, m.stops[s]);
            holds = prove_link(there, move, n.origin, to, l.kind, l.speed, g.set).reached;
          }
        }
        if (!holds) {
          cut++;
          continue;
        }
        links.push_back(l);
      }
      n.first_link = first;
      n.links = static_cast<int32_t>(links.size()) - first;
    }
    g.links = std::move(links);
  }
  return cut;
}

}  // namespace ge::shooterbots
