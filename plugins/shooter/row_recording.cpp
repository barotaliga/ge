// The recorded scenarios through the classic row, step by step and as a run (row_recording.hpp).
//
// ge:summary Replays each recorded scenario through the engine mover's classic row, two measures
// ge:keywords replay_row row_covers scene-model box-model step-error run-error discrete
// ge:skill shooter move testing
// ge:test tests/shooter_row_recording_test.cpp
#include "shooter/row_recording.hpp"

#include <memory>
#include <vector>

#include "shooter/row.hpp"

namespace ge::shooter {

namespace {

float largest(Vec3f a, Vec3f b) {
  float m = 0;
  for (int i = 0; i < 3; i++) {
    const float d = a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
    if (!(d <= m)) m = d;
  }
  return m;
}

Brush box_brush(Vec3f mins, Vec3f maxs) {
  Brush b;
  b.planes = {{{1, 0, 0}, maxs.x},   {{-1, 0, 0}, -mins.x}, {{0, 1, 0}, maxs.y},
              {{0, -1, 0}, -mins.y}, {{0, 0, 1}, maxs.z},   {{0, 0, -1}, -mins.z}};
  return b;
}

// The scene and its boxes as brush models, and the row's world over them.
struct Scene {
  std::vector<ModelData> data;
  std::vector<BrushModel> models;
  std::vector<Vec3f> origins;
  RowWorld world;
};

void build(const Recording& rec, const RecordedScenario& sc, Scene& s) {
  s.data.push_back(build_brush_model(rec.scenes[static_cast<size_t>(sc.scene)].brushes));
  s.origins.push_back({});
  for (const RecordedBox& b : sc.boxes) {
    s.data.push_back(build_brush_model({box_brush(b.mins, b.maxs)}));
    s.origins.push_back(b.origin);
  }
  for (const ModelData& d : s.data) s.models.push_back(d.model());
  s.world.hulls = {s.models.data(), s.origins.data(), static_cast<int>(s.models.size())};
  build_row_world(s.world);
}

RecordedState state_of(const RowBody& r) {
  MoveState c;
  row_state(r, c);
  RecordedState o;
  o.origin = c.origin;
  o.velocity = c.velocity;
  o.onground = c.ground != -1;
  o.groundent = c.ground;
  o.waterlevel = c.waterlevel;
  o.watertype = c.watertype;
  o.jump_held = c.jump_held;
  o.waterjump_time = c.waterjump_time;
  return o;
}

LineCheck check(int index, const RecordedState& ours, const RecordedState& want) {
  LineCheck c;
  c.index = index;
  c.ours = ours;
  c.position_error = largest(ours.origin, want.origin);
  c.velocity_error = largest(ours.velocity, want.velocity);
  auto differs = [&](const char* field, int a, int b) {
    if (a == b) return;
    c.mismatch += std::string(c.mismatch.empty() ? "" : ", ") + field + " " + std::to_string(a) +
                  " want " + std::to_string(b);
  };
  differs("onground", ours.onground, want.onground);
  differs("groundent", ours.groundent, want.groundent);
  differs("waterlevel", ours.waterlevel, want.waterlevel);
  differs("watertype", ours.watertype, want.watertype);
  c.ok = c.mismatch.empty() && c.position_error <= kPositionTolerance &&
         c.velocity_error <= kVelocityTolerance;
  return c;
}

void add(ReplayResult& r, LineCheck c) {
  if (!c.ok) r.failed++;
  if (r.worst_position_line < 0 || !(c.position_error <= r.max_position_error)) {
    r.max_position_error = c.position_error;
    r.worst_position_line = c.index;
  }
  if (r.worst_velocity_line < 0 || !(c.velocity_error <= r.max_velocity_error)) {
    r.max_velocity_error = c.velocity_error;
    r.worst_velocity_line = c.index;
  }
  r.lines.push_back(std::move(c));
}

// The classic state a recorded line leaves (what the next command starts from).
MoveState recorded(const RecordedScenario& sc, const RecordedState& a) {
  MoveState s = sc.start;
  s.origin = a.origin;
  s.velocity = a.velocity;
  s.ground = a.onground ? a.groundent : -1;
  s.waterlevel = a.waterlevel;
  s.watertype = a.watertype;
  s.jump_held = a.jump_held;
  s.waterjump_time = a.waterjump_time;
  return s;
}

}  // namespace

bool row_covers(const RecordedScenario& sc) {
  const MoveSettings& m = sc.settings;
  if (sc.start.type == MoveType::old_spectator) return false;
  if (m.ktjump != 1 || m.slidefix || m.airstep || m.pground) return false;
  return sc.name.rfind("nudge", 0) != 0;
}

RowReplay replay_row(const Recording& rec, const RecordedScenario& sc) {
  RowReplay out;
  out.step.name = out.run.name = sc.name;
  const auto scene = std::make_unique<Scene>();
  build(rec, sc, *scene);
  const RowWorld& world = scene->world;
  const ClassicRows rows = classic_rows(sc.settings, sc.start.maxspeed, sc.start.entgravity);

  RowBody run = row_body(sc.start, rows, world);
  MoveState before = sc.start;
  for (const RecordedLine& line : sc.lines) {
    RowBody one = row_body(before, rows, world);
    row_command(one, line.cmd, rows, world);
    LineCheck c = check(line.index, state_of(one), line.after);
    if (!c.mismatch.empty()) out.step_discrete++;
    add(out.step, std::move(c));

    row_command(run, line.cmd, rows, world);
    LineCheck d = check(line.index, state_of(run), line.after);
    if (!d.mismatch.empty()) out.run_discrete++;
    add(out.run, std::move(d));
    before = recorded(sc, line.after);
  }
  return out;
}

}  // namespace ge::shooter
