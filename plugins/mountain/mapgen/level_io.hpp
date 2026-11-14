// A BUILT MAP DESCRIBED FOR THE SAVE FILE (docs/saves.md § Fast loads): every type a `Level`
// holds walked by `describe(io, T&)`, so the content cache (engine/save/cache.hpp) writes a
// generated map once and a later load reads it back instead of generating it again. Every field,
// in order; a field added to a type is added here with `io.since` and `kLevelVersion` raised (an
// older cache entry then reads it at its default or, past `kLevelVersion`, is rebuilt).
// `tests/mountain_level_io_test.cpp` holds the round trip to the map's digest on every kind of
// map, so a field left out is a failing test, not a wrong map.
//
// ge:summary A generated map's every type described for the save file and the content cache
// ge:keywords level describe save content-cache map serialize resort run lift heightfield
// ge:skill mountain save
// ge:test tests/mountain_level_io_test.cpp
#pragma once

#include "mountain/mapgen/types.hpp"
#include "save/io.hpp"

namespace ge::core {
template <class Io>
void describe(Io& io, Heightfield& v) {
  io(v.origin_x);
  io(v.origin_z);
  io(v.cell);
  io(v.cols);
  io(v.rows);
  io(v.data);
}
}  // namespace ge::core

namespace ge::mountain::mapgen {

inline constexpr uint16_t kLevelVersion = 1;

template <class Io>
void describe(Io& io, Vec3& v) {
  io(v.x);
  io(v.y);
  io(v.z);
}
template <class Io>
void describe(Io& io, PlanPoint& v) {
  io(v.x);
  io(v.z);
}
template <class Io>
void describe(Io& io, TreeDef& v) {
  io(v.x);
  io(v.z);
  io(v.y);
  io(v.height);
  io(v.radius);
  io(v.age);
  io(v.crown);
  io(v.kind);
  io(v.clump);
}
template <class Io>
void describe(Io& io, Checkpoint& v) {
  io(v.x);
  io(v.z);
  io(v.y);
  io(v.heading);
  io(v.width);
  io(v.s);
  io(v.colour);
  io(v.offset);
  io(v.span);
  io(v.pole);
  io(v.turn);
  io(v.panels);
  io(v.flags);
}
template <class Io>
void describe(Io& io, Spawn& v) {
  io(v.x);
  io(v.z);
  io(v.heading);
}
template <class Io>
void describe(Io& io, Mountain& v) {
  io(v.summit);
  io(v.base);
  io(v.vertical);
  io(v.altitude);
  io(v.tree_line);
  io(v.sea);
}
template <class Io>
void describe(Io& io, KickerShape& v) {
  io(v.deck);
  io(v.fall);
  io(v.dig);
}
template <class Io>
void describe(Io& io, Kicker& v) {
  io(v.id);
  io(v.x);
  io(v.z);
  io(v.y);
  io(v.heading);
  io(v.height);
  io(v.ramp);
  io(v.landing);
  io(v.width);
  io(v.on_track);
  io(v.s);
  io(v.trick);
  io(v.shape);
  io(v.route);
}
template <class Io>
void describe(Io& io, Cliff& v) {
  io(v.id);
  io(v.x);
  io(v.z);
  io(v.y);
  io(v.heading);
  io(v.drop);
  io(v.face);
  io(v.apron);
  io(v.landing);
  io(v.shelf);
  io(v.width);
  io(v.on_track);
  io(v.s);
}
template <class Io>
void describe(Io& io, Hub& v) {
  io(v.x0);
  io(v.step);
  io(v.top);
  io(v.bottom);
}
template <class Io>
void describe(Io& io, Course& v) {
  io(v.id);
  io(v.grade);
  io(v.runs);
  io(v.length);
  io(v.drop);
}
template <class Io>
void describe(Io& io, WindTunnel::Point& v) {
  io(v.x);
  io(v.z);
  io(v.y);
  io(v.s);
  io(v.heading);
}
template <class Io>
void describe(Io& io, WindTunnel& v) {
  io(v.id);
  io(v.points);
  io(v.length);
  io(v.width);
  io(v.speed);
}
template <class Io>
void describe(Io& io, SummitRamp::Head& v) {
  io(v.x);
  io(v.y);
  io(v.z);
}
template <class Io>
void describe(Io& io, SummitRamp::Foot& v) {
  io(v.x);
  io(v.y);
  io(v.z);
  io(v.s);
}
template <class Io>
void describe(Io& io, SummitRamp& v) {
  io(v.run);
  io(v.from);
  io(v.to);
  io(v.width);
}
template <class Io>
void describe(Io& io, Drift& v) {
  io(v.from);
  io(v.to);
}
template <class Io>
void describe(Io& io, Weather& v) {
  io(v.kind);
  io(v.snowfall);
  io(v.fog);
  io(v.wind);
  io(v.wind_from);
  io(v.evening);
  io(v.snowline);
  io(v.thunder);
  io(v.mean_c);
}
template <class Io>
void describe(Io& io, Sun& v) {
  io(v.hour);
  io(v.day_of_year);
  io(v.latitude);
  io(v.facing);
}
template <class Io>
void describe(Io& io, Run::Into& v) {
  io(v.run);
  io(v.s);
  io(v.from);
}
template <class Io>
void describe(Io& io, Run::Branch& v) {
  io(v.run);
  io(v.s);
}
template <class Io>
void describe(Io& io, Run& v) {
  io(v.id);
  io(v.kind);
  io(v.grade);
  io(v.points);
  io(v.length);
  io(v.from);
  io(v.into);
  io(v.branch);
  io(v.to);
  io(v.drifts);
}
template <class Io>
void describe(Io& io, Lift& v) {
  io(v.id);
  io(v.kind);
  io(v.bottom);
  io(v.top);
  io(v.ramps);
}
template <class Io>
void describe(Io& io, SkiRoute::Into& v) {
  io(v.run);
  io(v.s);
}
template <class Io>
void describe(Io& io, SkiRoute::Drop& v) {
  io(v.s);
  io(v.height);
}
template <class Io>
void describe(Io& io, SkiRoute& v) {
  io(v.id);
  io(v.points);
  io(v.length);
  io(v.from);
  io(v.into);
  io(v.steepest);
  io(v.wild);
  io(v.drops);
  io(v.woods);
  io(v.edge);
}
template <class Io>
void describe(Io& io, ResortMap& v) {
  io(v.runs);
  io(v.lifts);
  io(v.courses);
  io(v.course);
  io(v.village);
  io(v.floor_z);
  io(v.hub);
  io(v.tunnels);
  io(v.routes);
}
template <class Io>
void describe(Io& io, RaceCourse::LinePoint& v) {
  io(v.s);
  io(v.x);
}
template <class Io>
void describe(Io& io, RaceCourse::Trap& v) {
  io(v.s);
  io(v.x);
  io(v.z);
  io(v.heading);
  io(v.width);
}
template <class Io>
void describe(Io& io, RaceCourse::Nets& v) {
  io(v.gap);
  io(v.height);
  io(v.from);
  io(v.to);
}
template <class Io>
void describe(Io& io, RaceCourse::Zone& v) {
  io(v.from);
  io(v.to);
  io(v.length);
}
template <class Io>
void describe(Io& io, RaceCourse::Feature& v) {
  io(v.kind);
  io(v.from);
  io(v.to);
  io(v.lip);
  io(v.height);
  io(v.side);
  io(v.angle);
}
template <class Io>
void describe(Io& io, RaceCourse::Stretch& v) {
  io(v.from);
  io(v.to);
}
template <class Io>
void describe(Io& io, Level& v);
template <class Io>
void describe(Io& io, RaceCourse& v) {
  io(v.discipline);
  io(v.run);
  io(v.base);
  io(v.from);
  io(v.to);
  io(v.vertical);
  io(v.hairpins);
  io(v.verticals);
  io(v.delays);
  io(v.line);
  io(v.jumps);
  io(v.nets);
  io(v.trap);
  io(v.turns);
  io(v.zone);
  io(v.top);
  io(v.qualify);
  io(v.stop);
  io(v.width);
  io(v.features);
  io(v.stretch);
}
template <class Io>
void describe(Io& io, VenueSurface& v) {
  io(v.x);
  io(v.z);
  io(v.heading);
  io(v.fx);
  io(v.fz);
  io(v.dx);
  io(v.y);
  io(v.shift);
  io(v.half);
  io(v.end);
  io(v.narrow_until);
  io(v.narrow_half);
}
template <class Io>
void describe(Io& io, SkiJumpHill::Gate& v) {
  io(v.number);
  io(v.x);
  io(v.y);
  io(v.e);
}
template <class Io>
void describe(Io& io, SkiJumpHill& v) {
  io(v.base);
  io(v.hill);
  io(v.w);
  io(v.hs);
  io(v.from);
  io(v.to);
  io(v.vertical);
  io(v.straight);
  io(v.curve);
  io(v.table);
  io(v.edge);
  io(v.p);
  io(v.k);
  io(v.l);
  io(v.u);
  io(v.outrun);
  io(v.take_off);
  io(v.lip);
  io(v.gates);
  io(v.design);
  io(v.speed);
  io(v.metre);
  io(v.gate_factor);
  io(v.head_factor);
  io(v.tail_factor);
  io(v.width);
  io(v.deck);
}
template <class Io>
void describe(Io& io, MapLake& v) {
  io(v.x);
  io(v.z);
  io(v.radius);
  io(v.level);
  io(v.depth);
  io(v.ice);
  io(v.thaw);
  io(v.shore);
}
template <class Io>
void describe(Io& io, StreamPoint& v) {
  io(v.x);
  io(v.z);
  io(v.level);
  io(v.width);
  io(v.depth);
}
template <class Io>
void describe(Io& io, MapStream& v) {
  io(v.river);
  io(v.frozen);
  io(v.ice);
  io(v.points);
}
template <class Io>
void describe(Io& io, MapDam& v) {
  io(v.a);
  io(v.b);
  io(v.downhill);
  io(v.crest);
  io(v.height);
  io(v.lake);
}
template <class Io>
void describe(Io& io, Level& v) {
  io(v.seed);
  io(v.size);
  io(v.cell);
  io(v.ground);
  io(v.track);
  io(v.checkpoints);
  io(v.spawn);
  io(v.grid);
  io(v.trees);
  io(v.sun);
  io(v.laps);
  io(v.packed);
  io(v.kickers);
  io(v.cliffs);
  io(v.mountain);
  io(v.attempt);
  io(v.drifts);
  io(v.weather);
  io(v.version);
  io(v.region);
  io(v.face);
  io(v.grade);
  io(v.crust);
  io(v.resort);
  io(v.race);
  io(v.ski_jump);
  io(v.venue);
  io(v.lakes);
  io(v.streams);
  io(v.dams);
}

}  // namespace ge::mountain::mapgen
