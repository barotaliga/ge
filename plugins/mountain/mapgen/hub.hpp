// R29, R30 — THE HUB AND ITS WIND TUNNELS: the foot of the mountain, where every run that reaches
// the valley floor ends and the valley's bottom stations stand.
//
// A ski area's foot is the open, groomed snow along the bottom of the face that every finish runs
// out into and every valley lift leaves from, where a skier comes down one sector and crosses to
// the lift of another. So the hub is a BAND across the floor: from past the outermost finish or
// bottom station on one side to past the outermost on the other, its lower edge out over the flat
// floor, its upper edge where the ground starts to rise into the face (but always above every
// finish and every station), eased along x so it reads as one sweep of open snow. The woods thin
// into it over a fringe whose line wanders, so its edge is a forest's edge and not a ruled line.
//
// Along it, below every finish, run the two WIND TUNNELS (R30): straight lines across the floor
// from the station or finish at one end of the hub to the one at the other, one each way, each on
// a bed graded into the floor, so a skier who came down at one end is blown to the lifts at the
// other. Every number is `resort_rules::hub`'s and `resort_rules::tunnel`'s.
//
// Pure functions of the runs and the mountain, drawing nothing from any stream.
//
// ge:summary R29 R30 the hub across the valley floor and the two wind tunnels along it
// ge:keywords hub plan-hub hub-clear groom-hub lay-tunnels wind-tunnel hub-at outside-hub
// ge:keywords floor-point tunnel-bed valley-floor
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/terrain.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// The hub and the two lines its tunnels run along (z on the map), from `from` to `to` along x.
struct HubPlan {
  Hub hub;
  std::array<double, 2> lanes;
  double from;
  double to;
};

// The hub's two edges at `x`, read between the columns; none past either end of it.
struct HubEdges {
  double top;
  double bottom;
};
std::optional<HubEdges> hub_at(const Hub& hub, double x);

// How far outside the hub a plan point lies, m: 0 inside it; along z past an edge, along x past
// an end, and the two together past a corner.
double outside_hub(const Hub& hub, double x, double z);

// R29 — lay the hub round the floor's finishes and bottom stations (`floor`), with room below
// them for the tunnels (R30); its lower edge wanders off `seed`.
HubPlan plan_hub(const TerrainPlan& plan, const ge::core::Heightfield& ground,
                 std::span<const PlanPoint> floor, uint32_t seed);

// R29 — whether the woods leave a plan point open for the hub: inside it, or in its fringe where
// the wandering edge of the woods has not reached.
bool hub_clear(const Hub& hub, uint32_t seed, double x, double z);

// R29 — groom the hub: the packed share 1 over the band, faded out past its edge.
void groom_hub(const Hub& hub, ge::core::Heightfield& packed);

// R30 — the two wind tunnels along the hub, W1 along the upper line and W2 back along the lower,
// each on a bed graded into the floor until it falls nowhere steeper than a little inside
// `tunnel.grade`. Presses `ground`.
std::vector<WindTunnel> lay_tunnels(const HubPlan& h, ge::core::Heightfield& ground);

}  // namespace ge::mountain::mapgen
