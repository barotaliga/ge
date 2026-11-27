// The way through's parts (way_through.cpp): the small reads its init and its plan share: a
// spot's distance, an entity's origin and edict, boxes touched and crossed, the node in a box,
// walls and lifts by their edict, which links are open and the search past the barred ones.
//
// ge:summary The way through's shared parts: boxes, edicts, open links, the searches past bars
// ge:keywords way_parts touches crosses shut_crossings link_open reach_past reach_back
// ge:keywords locked_moved is_wall is_lift say_way name_linked_doors way_idle idle hopeful
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp tests/shooterbots_way_parts_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ai/path.hpp"
#include "shooter/map_spec.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/nav.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots::way_parts {

// A turning door this far or less over an exit is its hatch, units.
constexpr float kHatchOver = 256;

float dist(sh::Vec3f a, sh::Vec3f b);

// The time he has had nothing to do since (`WayThrough::idle_since`), after a plan at `t`: -1 while
// he has something (`busy`: a step, a monster, a gun, a place to see), else when it began. A clock
// left running from an earlier idle spell lifted every door the first moment city1's bot dropped
// off its graph, and he ran to a secret door shut till its button.
double way_idle(double since, bool busy, double t);

sh::Vec3f origin_of(const char* org);

int edict_of(const ge::shooterbots::BotView& s, const char* model);

// Quake's doors that touch move as one, and a group with a name opens only to what fires it: an
// unnamed leaf of a named group is as shut as its named mate (e4m1's flood gate, opened by the
// button that fires its partner, kept him pressing on it for the whole map). Each unnamed leaf
// takes its group's name. Quake II pairs doors by an explicit team instead.
void name_linked_doors(const ge::shooterbots::BotView& s, const ge::shooter::MapSpec& spec,
                       std::vector<EntityBlock>& blocks);

// A player's box (origin at its middle, 16 across, 24 down and 32 up) touches the box.
bool touches(sh::Vec3f p, sh::Vec3f lo, sh::Vec3f hi);

// Where a brush entity's model stood when the map spawned it (its `origin`, else the map's own):
// a level entered again keeps its doors and buttons where they were left.
sh::Vec3f spawned_at(const EntityBlock& b);

// Whether locked door `k` has moved off where it stood shut (a turning bridge: turned; a bridge
// slid out of the floor, `kLockedOut`: all the way out to where the map drew it, `locked_at`, so
// one still rising or sinking again, as e2m6's pool bridges do on their clock, is still shut), or
// is gone.
bool locked_moved(const WayThrough& w, const ge::shooterbots::BotView& s, size_t k);

// Every node a player at `from` gets to past no door in `barred` (nav_reach with the doors), on
// the links his `jumper` takes (no rocket jump without a launcher: base1's exit, reached by one
// down the lift's shaft, stood him still at the start with no route the pilot would take).
// A link from `a` to `b` goes through the wall from `lo` to `hi`: it passes the wall's middle on
// its thin axis (x or y), the line meets the wall's width there, and a player at either end
// stands at the wall's height (a drop through a hidden lift's doorway starts level with it).
bool crosses(sh::Vec3f a, sh::Vec3f b, sh::Vec3f lo, sh::Vec3f hi);

// The links shut while the doors in `barred` stand shut: those crossing a secret wall among them.
void shut_crossings(WayThrough& w, const std::vector<int32_t>& barred, size_t links);

bool is_wall(const WayThrough& w, int32_t e);

// A shut door whose own button's place is on its top (a lift at rest, called from on it): the
// pilot takes him onto it.
bool is_lift(const WayThrough& w, const Fighter& f, int32_t e);

// Whether link `li` is open to him past the doors in `barred` (after shut_crossings).
bool link_open(const NavGraph& g, size_t li, const std::vector<int32_t>& barred,
               const std::vector<int32_t>& node_door, const NavJumper& jumper, const WayThrough* w);

void reach_past(const NavGraph& g, int32_t from, const std::vector<int32_t>& barred,
                const std::vector<int32_t>& node_door, const NavJumper& jumper,
                ge::ai::Search& search, WayThrough* w = nullptr);

// The nodes he gets back to `to` from, on the open links (the reverse of reach_past, the crossings
// as reach_past last left them): `w.returns`.
void reach_back(const NavGraph& g, int32_t to, const std::vector<int32_t>& barred,
                const std::vector<int32_t>& node_door, const NavJumper& jumper, WayThrough& w);

// What his way to `node` looks like from where he stands, for the plan's log: the fighter's and the
// pilot's state, what a straight trace to the next node meets, the next links.
void say_way(WayThrough& w, const Fighter& f, const NavGraph& g, const ge::shooterbots::BotView& s,
             int me, sh::Vec3f self, int32_t node, const char* what);

}  // namespace ge::shooterbots::way_parts
