// A MAP PLAN: a level written as an architect draws one, read into the compiler's source. The
// map starts as one block of solid; ROOMS are carved out of it, SOLIDS (pillars, ledges, walls,
// stairs, ramps) are added back, LIQUIDS fill pools, and ENTITIES stand where they are put. What
// a carve exposes takes the carve's textures, so a room's floor, walls and ceiling are its own.
// One statement a line; `#` starts a comment; words are split by spaces, a "quoted value" keeps
// its spaces; `key=value` sets an option. Map units, z up; a point is `x,y` or `x,y,z`.
//
//   world message="Two Towers" light=12 _sunlight=180 "_sun_mangle=30 -60 0"
//   textures floor=floor_a wall=wall_a ceil=ceil_a solid=wall_a   # the defaults below
//   pad 32                                     # the block's margin round everything (units)
//   room hall z=0..192 box 0,0 512,384 [floor= wall= ceil=]      # carved; ceil=sky1 opens it
//   room bend z=0..128 poly 0,0 256,0 256,128 128,256 0,256      # any simple polygon
//   solid pillar z=0..192 box 240,180 272,212 [tex= top=]
//   wall z=0..128 thick=16 line 0,0 128,0 128,96 [tex=]           # a wall along a line
//   stairs z=0..64 box 0,0 64,128 up=+y [steps=4] [tex= top=]     # steps rising toward +y
//   ramp z=0..64 box 0,0 64,256 up=+x [tex=]
//   liquid z=-64..-16 box 0,0 128,128 kind=water|slime|lava [tex=]  # a pool (carve its pit first)
//   light 256,192,160 [value=300] [wait=1] [style=0]
//   spawn 64,64,24 [angle=90] [class=info_player_deathmatch]
//   entity item_armor2 300,40,24 [key=value ...]
//   symmetry rotate=256,192 | mirror-x=256 | mirror-y=192   # everything again, turned or mirrored
//   modes duel,2on2,4on4,ffa,midair            # what the map is made for
//   area ra z=0..128 box 0,0 128,128 intent=camp [modes=4on4] [note="..."]   # what a place is for
//   trick up from 64,64,24 to 300,64,152 kind=rj [level=1] [modes=] [note="..."]  # a way it is
//   meant
//
// THE INTENT (`PlanIntent`, kept apart from the map): an AREA names what a place is meant for, so
// a lab can hold the bots' matches to it (`intent=` one of `kAreaIntents`: camp, a spot one player
// holds with few ways in; hold, a team's base; fight, where the sides meet; route, a way through;
// power, a power's spot; spawn, where players come in; perch, high ground seen from below). A
// TRICK is a way between two points the map means a skilled player to take (`kind=` one of
// `kTrickKinds`: jump, hop, head, rj, flip, gj, boost-rocket, boost-double), at a level 0
// (anyone), 1 or 2 (experts). Both are repeated by the symmetry (the copy's `copy` is 1).
//
// A ceiling of sky (`ceil=` a texture named `sky...`) opens the room 16 units higher and lays a
// sky brush there, so only that slab is sky. A room's or a solid's texture may not be a liquid's
// (`*...`): pools are liquids. `symmetry` repeats every shape and entity written before or after
// it, turned half round a point or mirrored across a line (an entity's `angle` turned with it);
// the word `once` anywhere in a statement keeps it out of the symmetry (a piece on the centre).
//
// ge:summary A map plan (rooms carved, solids added, liquids, entities) read into compiler source
// ge:keywords map-compiler plan floor-plan room carve csg subtract solid wall stairs ramp liquid
// ge:keywords symmetry mirror entity spawn light convex-decomposition text-format
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "brush/compile/source.hpp"

namespace ge::brush::mapc {

inline constexpr const char* kAreaIntents[] = {"camp",  "hold",  "fight", "route",
                                               "power", "spawn", "perch"};
inline constexpr const char* kTrickKinds[] = {"jump", "hop", "head",         "rj",
                                              "flip", "gj",  "boost-rocket", "boost-double"};
inline constexpr const char* kPlanModes[] = {"duel", "2on2", "4on4", "ffa", "midair"};

struct PlanArea {
  std::string name, intent, modes, note;        // modes: comma separated, empty for every mode
  std::vector<std::pair<double, double>> poly;  // counter-clockwise, x,y
  double z0 = 0, z1 = 0;
  int line = 0, copy = 0;  // copy 1: the symmetry's
};
struct PlanTrick {
  std::string name, kind, modes, note;
  double from[3] = {}, to[3] = {};
  int level = 0;
  int line = 0, copy = 0;
};
struct PlanIntent {
  std::string modes;  // the map's (`modes`), comma separated
  std::vector<PlanArea> areas;
  std::vector<PlanTrick> tricks;
};
// Whether `p` (x, y, z: a player's origin) stands in `a`.
bool area_holds(const PlanArea& a, double x, double y, double z);

// The plan in `text` read into `*out`; false with every fault found, one a line, in `*error`
// ("line N: what; next: what to write"). `intent`, when given, gets its areas and tricks.
bool read_plan(std::string_view text, Source* out, std::string* error,
               PlanIntent* intent = nullptr);
// Only the intent of `text`: a plan, or an `.intent` file of `modes`, `area`, `trick` and
// `symmetry` lines beside a map made anywhere else (no rooms needed). False with the faults.
bool read_intent(std::string_view text, PlanIntent* intent, std::string* error);

// A convex brush cut away from another: the pieces of `b` outside `cut` (whose sides face out of
// it), each new side taking the texture of the cut's side it lies on. Pieces with no volume are
// dropped.
std::vector<Brush> subtract(const Brush& b, const Brush& cut);
// A brush's sides that leave no polygon removed; false when it holds no volume.
bool tidy_brush(Brush* b);

}  // namespace ge::brush::mapc
