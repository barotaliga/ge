// The recorded original-move steps' parser and replay (frame_recording.hpp states the format, the
// run and the tolerance).
//
// ge:summary Parses the recorded original-move steps and replays each through the original move
// ge:keywords parse_frame_recording replay_frames scenario frame tolerance mismatch
// ge:skill shooter testing
// ge:test tests/shooter_framemove_test.cpp
#include "shooter/frame_recording.hpp"

#include <cmath>

#include "shooter/physics.hpp"

namespace ge::shooter {

namespace {

constexpr float kEye = 22;  // the player's view_ofs z

struct Words {
  const std::vector<std::string_view>& t;
  size_t i = 0;
  bool ok = true;

  void key(std::string_view k) {
    if (i >= t.size() || t[i] != k) ok = false;
    i++;
  }
  float num() {
    float v = 0;
    if (i >= t.size() || !parse_float(t[i], v)) ok = false;
    i++;
    return v;
  }
  int whole() {
    int v = 0;
    if (i >= t.size() || !parse_int(t[i], v)) ok = false;
    i++;
    return v;
  }
  Vec3f vec() {
    Vec3f v;
    v.x = num();
    v.y = num();
    v.z = num();
    return v;
  }
};

float max_diff(Vec3f a, Vec3f b) {
  float m = 0;
  for (int i = 0; i < 3; i++) {
    const float d = std::fabs(a[i] - b[i]);
    if (d > m) m = d;
  }
  return m;
}

bool set_setting(FrameScenario& s, std::string_view kv) {
  const size_t eq = kv.find('=');
  if (eq == std::string_view::npos) return false;
  const std::string_view k = kv.substr(0, eq);
  float v = 0;
  if (!parse_float(kv.substr(eq + 1), v)) return false;
  FrameMoveSettings& m = s.settings;
  if (k == "gravity")
    s.gravity = v;
  else if (k == "maxvelocity")
    s.maxvelocity = v;
  else if (k == "friction")
    m.friction = v;
  else if (k == "edgefriction")
    m.edgefriction = v;
  else if (k == "stopspeed")
    m.stopspeed = v;
  else if (k == "maxspeed")
    m.maxspeed = v;
  else if (k == "accelerate")
    m.accelerate = v;
  else if (k == "nostep")
    m.nostep = v != 0;
  else if (k == "rollangle")
    m.rollangle = v;
  else if (k == "rollspeed")
    m.rollspeed = v;
  else
    return false;
  return true;
}

}  // namespace

bool parse_frame_recording(std::string_view text, FrameRecording& out, std::string& error) {
  out = FrameRecording{};
  std::string scene;
  int line_no = 0;
  size_t pos = 0;
  auto fail = [&](const std::string& what) {
    error = "line " + std::to_string(line_no) + ": " + what;
    return false;
  };
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    if (nl == std::string_view::npos) nl = text.size();
    const std::string_view raw = text.substr(pos, nl - pos);
    pos = nl + 1;
    line_no++;
    const std::vector<std::string_view> t = split_words(raw);
    if (t.empty() || t[0][0] == '#') continue;
    Words w{t};
    if (t[0] == "scene") {
      if (t.size() != 2) return fail("a scene needs one name");
      scene = std::string(t[1]);
    } else if (t[0] == "scenario") {
      if (t.size() != 2 || scene.empty()) return fail("a scenario needs a scene before it");
      FrameScenario s;
      s.name = std::string(t[1]);
      s.scene = scene;
      out.scenarios.push_back(s);
    } else if (out.scenarios.empty()) {
      return fail("'" + std::string(t[0]) + "' outside a scenario");
    } else if (t[0] == "settings") {
      for (size_t i = 1; i < t.size(); i++)
        if (!set_setting(out.scenarios.back(), t[i]))
          return fail("unknown setting '" + std::string(t[i]) + "'");
    } else if (t[0] == "start") {
      FrameScenario& s = out.scenarios.back();
      w.i = 1;
      w.key("origin");
      s.origin = w.vec();
      w.key("velocity");
      s.velocity = w.vec();
      w.key("dt");
      s.dt = w.num();  // a float step, promoted
      if (!w.ok || s.dt <= 0) return fail("a start's fields");
    } else if (t[0] == "frame") {
      FrameLine l;
      w.i = 1;
      l.index = w.whole();
      w.key("view");
      l.cmd.view.x = w.num();
      l.cmd.view.y = w.num();
      w.key("move");
      l.cmd.forward = w.num();
      l.cmd.side = w.num();
      l.cmd.up = w.num();
      w.key("jump");
      l.jump = w.whole() != 0;
      w.key("->");
      w.key("origin");
      l.after.origin = w.vec();
      w.key("velocity");
      l.after.velocity = w.vec();
      w.key("onground");
      l.after.onground = w.whole() != 0;
      w.key("waterlevel");
      l.after.waterlevel = w.whole();
      w.key("watertype");
      l.after.watertype = w.whole();
      w.key("jumpreleased");
      l.after.jumpreleased = w.whole() != 0;
      w.key("waterjump");
      l.after.waterjump = w.whole() != 0;
      w.key("teleport");
      l.after.teleport = w.num();
      w.key("clip");
      l.after.clip = w.whole();
      if (!w.ok || w.i != t.size()) return fail("a frame's fields");
      out.scenarios.back().lines.push_back(l);
    } else {
      return fail("unknown line '" + std::string(t[0]) + "'");
    }
  }
  return true;
}

void stand_frame_player(Server& sv, int e, Vec3f origin, Vec3f velocity) {
  {
    Edict& ed = sv.edicts[static_cast<size_t>(e)];
    ed.movetype = kMovetypeWalk;
    ed.solid = kSolidSlidebox;
    ed.health = 100;
    ed.view_ofs = {0, 0, kEye};
    ed.flags = kFlagJumpReleased;
    ed.velocity = velocity;
  }
  set_size(sv, e, kPlayerMins, kPlayerMaxs);
  set_origin(sv, e, origin);
  sv.edicts[static_cast<size_t>(e)].oldorigin = origin;
  frame_water_level(sv, e);
}

FrameReplay replay_frames(const Recording& scenes, const FrameScenario& f) {
  FrameReplay r;
  r.name = f.name;
  const RecordedScene* scene = nullptr;
  for (const RecordedScene& s : scenes.scenes)
    if (s.name == f.scene) scene = &s;
  if (scene == nullptr) {
    r.error = "no scene '" + f.scene + "' in the classic move's recording";
    return r;
  }
  const ModelData world = build_brush_model(scene->brushes);
  ServerSettings set;
  set.move.gravity = f.gravity;
  set.maxvelocity = f.maxvelocity;
  set.maxclients = 0;
  set.max_edicts = 8;
  Server sv;
  create_server(sv, set, {world.model()}, Program{});
  sv.time = 1.0;
  const int e = spawn_edict(sv);
  stand_frame_player(sv, e, f.origin, f.velocity);

  for (const FrameLine& l : f.lines) {
    frame_think(sv, e, l.cmd, f.settings, f.dt);
    frame_program_move(sv, e, l.jump, f.dt);
    const int clip = frame_move(sv, e, f.settings, f.dt);
    sv.time += f.dt;

    const Edict& now = sv.edicts[static_cast<size_t>(e)];
    FrameCheck c;
    c.index = l.index;
    c.ours.origin = now.origin;
    c.ours.velocity = now.velocity;
    c.ours.onground = (now.flags & kFlagOnGround) != 0;
    c.ours.waterlevel = now.waterlevel;
    c.ours.watertype = now.watertype;
    c.ours.jumpreleased = (now.flags & kFlagJumpReleased) != 0;
    c.ours.waterjump = (now.flags & kFlagWaterJump) != 0;
    c.ours.teleport = now.teleport_time;
    c.ours.clip = clip;
    const FrameState& want = l.after;
    c.position_error = max_diff(c.ours.origin, want.origin);
    c.velocity_error = max_diff(c.ours.velocity, want.velocity);
    if (c.ours.onground != want.onground) c.mismatch += "onground ";
    if (c.ours.waterlevel != want.waterlevel) c.mismatch += "waterlevel ";
    if (c.ours.watertype != want.watertype) c.mismatch += "watertype ";
    if (c.ours.jumpreleased != want.jumpreleased) c.mismatch += "jumpreleased ";
    if (c.ours.waterjump != want.waterjump) c.mismatch += "waterjump ";
    if (std::fabs(c.ours.teleport - want.teleport) > kPositionTolerance) c.mismatch += "teleport ";
    if (c.ours.clip != want.clip) c.mismatch += "clip ";
    c.ok = c.mismatch.empty() && c.position_error <= kPositionTolerance &&
           c.velocity_error <= kVelocityTolerance;
    if (!c.ok) r.failed++;
    if (c.position_error > r.max_position_error) {
      r.max_position_error = c.position_error;
      r.worst_position_line = c.index;
    }
    if (c.velocity_error > r.max_velocity_error) {
      r.max_velocity_error = c.velocity_error;
      r.worst_velocity_line = c.index;
    }
    r.lines.push_back(std::move(c));
  }
  return r;
}

}  // namespace ge::shooter
