// The recordings' parser and the line-by-line replay (recording.hpp states the format and the
// tolerance).
//
// ge:summary Parses the recorded moves and replays each against player_move, line by line
// ge:keywords parse_recording replay build_scene_world tokens strtof tolerance mismatch
// ge:skill shooter testing
// ge:test tests/shooter_recording_test.cpp
#include "shooter/recording.hpp"

#include <cstdint>
#include <cstdlib>

namespace ge::shooter {

namespace {

std::vector<std::string_view> split(std::string_view line) {
  std::vector<std::string_view> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++;
    size_t j = i;
    while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') j++;
    if (j > i) out.push_back(line.substr(i, j - i));
    i = j;
  }
  return out;
}

bool number(std::string_view tok, float& out) {
  const std::string s(tok);
  char* end = nullptr;
  out = std::strtof(s.c_str(), &end);
  return end != s.c_str() && *end == '\0';
}

bool integer(std::string_view tok, int& out) {
  const std::string s(tok);
  char* end = nullptr;
  out = static_cast<int>(std::strtol(s.c_str(), &end, 10));
  return end != s.c_str() && *end == '\0';
}

// Reads `key` then `n` numbers from `t` at `i`; advances `i`.
struct Cursor {
  const std::vector<std::string_view>& t;
  size_t i = 0;
  bool ok = true;

  bool key(std::string_view k) {
    if (i < t.size() && t[i] == k) {
      i++;
      return true;
    }
    ok = false;
    return false;
  }
  float num() {
    float v = 0;
    if (i >= t.size() || !number(t[i], v)) ok = false;
    i++;
    return v;
  }
  int whole() {
    int v = 0;
    if (i >= t.size() || !integer(t[i], v)) ok = false;
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

bool contents_of(std::string_view w, int& c) {
  if (w == "solid")
    c = kContentsSolid;
  else if (w == "water")
    c = kContentsWater;
  else if (w == "slime")
    c = kContentsSlime;
  else if (w == "lava")
    c = kContentsLava;
  else
    return false;
  return true;
}

bool move_type_of(std::string_view w, MoveType& t) {
  if (w == "normal")
    t = MoveType::normal;
  else if (w == "dead")
    t = MoveType::dead;
  else if (w == "fly")
    t = MoveType::fly;
  else if (w == "spectator")
    t = MoveType::spectator;
  else if (w == "old_spectator")
    t = MoveType::old_spectator;
  else if (w == "none")
    t = MoveType::none;
  else if (w == "lock")
    t = MoveType::lock;
  else
    return false;
  return true;
}

bool movevar(std::string_view kv, MoveSettings& set, MoveState& st) {
  const size_t eq = kv.find('=');
  if (eq == std::string_view::npos) return false;
  const std::string_view k = kv.substr(0, eq);
  float v = 0;
  if (!number(kv.substr(eq + 1), v)) return false;
  if (k == "gravity")
    set.gravity = v;
  else if (k == "stopspeed")
    set.stopspeed = v;
  else if (k == "maxspeed")
    set.maxspeed = st.maxspeed = v;
  else if (k == "spectatormaxspeed")
    set.spectatormaxspeed = v;
  else if (k == "accelerate")
    set.accelerate = v;
  else if (k == "airaccelerate")
    set.airaccelerate = v;
  else if (k == "wateraccelerate")
    set.wateraccelerate = v;
  else if (k == "friction")
    set.friction = v;
  else if (k == "waterfriction")
    set.waterfriction = v;
  else if (k == "entgravity")
    st.entgravity = v;
  else if (k == "bunnyspeedcap")
    set.bunnyspeedcap = v;
  else if (k == "ktjump")
    set.ktjump = v;
  else if (k == "slidefix")
    set.slidefix = v != 0;
  else if (k == "airstep")
    set.airstep = v != 0;
  else if (k == "pground")
    set.pground = v != 0;
  else
    return false;
  return true;
}

bool command_line(const std::vector<std::string_view>& t, RecordedLine& line) {
  Cursor c{t};
  c.key("cmd");
  line.index = c.whole();
  c.key("msec");
  line.cmd.msec = static_cast<uint8_t>(c.whole());
  c.key("angles");
  line.cmd.angles = c.vec();
  c.key("move");
  line.cmd.forward = static_cast<int16_t>(c.whole());
  line.cmd.side = static_cast<int16_t>(c.whole());
  line.cmd.up = static_cast<int16_t>(c.whole());
  c.key("buttons");
  line.cmd.buttons = static_cast<uint8_t>(c.whole());
  c.key("->");
  RecordedState& a = line.after;
  c.key("origin");
  a.origin = c.vec();
  c.key("velocity");
  a.velocity = c.vec();
  c.key("onground");
  a.onground = c.whole() != 0;
  c.key("groundent");
  a.groundent = c.whole();
  c.key("waterlevel");
  a.waterlevel = c.whole();
  c.key("watertype");
  a.watertype = c.whole();
  c.key("jump_held");
  a.jump_held = c.whole() != 0;
  c.key("waterjumptime");
  a.waterjump_time = c.num();
  c.key("blocked");
  a.blocked = c.whole();
  c.key("touch");
  while (c.ok && c.i < t.size()) a.touches.push_back(c.whole());
  return c.ok;
}

float largest_difference(Vec3f a, Vec3f b) {
  float m = 0;
  for (int i = 0; i < 3; i++) {
    const float d = a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
    if (!(d <= m)) m = d;  // a NaN wins
  }
  return m;
}

}  // namespace

bool parse_recording(std::string_view text, Recording& out, std::string& error) {
  out = {};
  enum class In : uint8_t { nothing, scene, scenario } in = In::nothing;
  size_t pos = 0;
  int number_of_line = 0;
  auto fail = [&](const std::string& what) {
    error = "recording line " + std::to_string(number_of_line) + ": " + what;
    return false;
  };
  while (pos <= text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string_view::npos) eol = text.size();
    const std::string_view raw = text.substr(pos, eol - pos);
    pos = eol + 1;
    number_of_line++;
    const std::vector<std::string_view> t = split(raw);
    if (t.empty() || t[0][0] == '#') continue;
    const std::string_view w = t[0];

    if (w == "end") {
      if (in == In::nothing) return fail("'end' outside a scene or a scenario");
      in = In::nothing;
    } else if (w == "scene") {
      if (t.size() != 2) return fail("expected 'scene <name>'");
      out.scenes.push_back({std::string(t[1]), {}});
      in = In::scene;
    } else if (w == "brush") {
      Brush b;
      if (in != In::scene || t.size() != 2 || !contents_of(t[1], b.contents))
        return fail("expected 'brush solid|water|slime|lava' inside a scene");
      out.scenes.back().brushes.push_back(b);
    } else if (w == "plane") {
      if (in != In::scene || out.scenes.back().brushes.empty() || t.size() != 5)
        return fail("expected 'plane nx ny nz dist' after a brush");
      Cursor c{t, 1};
      Plane p;
      p.normal = c.vec();
      p.dist = c.num();
      if (!c.ok) return fail("a plane's numbers do not parse");
      out.scenes.back().brushes.back().planes.push_back(p);
    } else if (w == "scenario") {
      if (t.size() != 2 || out.scenes.empty())
        return fail("expected 'scenario <name>' after a scene");
      RecordedScenario sc;
      sc.name = std::string(t[1]);
      sc.scene = static_cast<int>(out.scenes.size()) - 1;
      out.scenarios.push_back(sc);
      in = In::scenario;
    } else if (in != In::scenario) {
      return fail("'" + std::string(w) + "' outside a scenario");
    } else if (w == "movevars") {
      RecordedScenario& sc = out.scenarios.back();
      for (size_t i = 1; i < t.size(); i++)
        if (!movevar(t[i], sc.settings, sc.start))
          return fail("unknown or malformed setting '" + std::string(t[i]) + "'");
    } else if (w == "start") {
      MoveState& st = out.scenarios.back().start;
      Cursor c{t, 1};
      c.key("origin");
      st.origin = c.vec();
      c.key("velocity");
      st.velocity = c.vec();
      c.key("pm_type");
      if (c.i >= t.size() || !move_type_of(t[c.i], st.type)) return fail("unknown pm_type");
      c.i++;
      // The recordings start with the water type unset (0); a spectator, which never
      // categorises its position, keeps printing it.
      st.watertype = 0;
      c.key("onground");
      const bool on = c.whole() != 0;
      st.ground = on ? 0 : -1;
      if (on) st.ground_normal = {0, 0, 1};
      c.key("jump_held");
      st.jump_held = c.whole() != 0;
      c.key("waterjumptime");
      st.waterjump_time = c.num();
      if (!c.ok)
        return fail(
            "expected 'start origin x y z velocity x y z pm_type t onground b "
            "jump_held b waterjumptime t'");
    } else if (w == "physent") {
      Cursor c{t, 1};
      RecordedBox b;
      c.key("box");
      c.key("origin");
      b.origin = c.vec();
      c.key("mins");
      b.mins = c.vec();
      c.key("maxs");
      b.maxs = c.vec();
      if (!c.ok) return fail("expected 'physent box origin x y z mins x y z maxs x y z'");
      out.scenarios.back().boxes.push_back(b);
    } else if (w == "cmd") {
      RecordedLine line;
      if (!command_line(t, line)) return fail("a cmd line does not parse");
      out.scenarios.back().lines.push_back(std::move(line));
    } else {
      return fail("unknown line '" + std::string(w) + "'");
    }
  }
  if (in != In::nothing) return fail("the file ends inside a scene or a scenario");
  return true;
}

void build_scene_world(const RecordedScene& scene, const std::vector<RecordedBox>& boxes,
                       SceneWorld& out) {
  out.point_hull = build_point_hull(scene.brushes);
  out.clip_hull = build_box_hull(scene.brushes, kPlayerMins, kPlayerMaxs);
  out.world = {};
  Solid& w = out.world.solids[0];
  w.kind = SolidKind::brush;
  w.point_hull = out.point_hull.hull();
  w.clip_hull = out.clip_hull.hull();
  out.world.count = 1;
  for (const RecordedBox& b : boxes) {
    if (out.world.count >= kMaxSolids) break;
    Solid& s = out.world.solids[out.world.count++];
    s.kind = SolidKind::box;
    s.origin = b.origin;
    s.mins = b.mins;
    s.maxs = b.maxs;
  }
}

ReplayResult replay(const Recording& rec, const RecordedScenario& sc) {
  ReplayResult r;
  r.name = sc.name;
  SceneWorld sw;
  build_scene_world(rec.scenes[static_cast<size_t>(sc.scene)], sc.boxes, sw);
  MoveState s = sc.start;
  for (const RecordedLine& line : sc.lines) {
    const MoveResult mr = player_move(s, line.cmd, sc.settings, sw.world);
    LineCheck c;
    c.index = line.index;
    RecordedState& o = c.ours;
    o.origin = s.origin;
    o.velocity = s.velocity;
    o.onground = s.ground != -1;
    o.groundent = s.ground;
    o.waterlevel = s.waterlevel;
    o.watertype = s.watertype;
    o.jump_held = s.jump_held;
    o.waterjump_time = s.waterjump_time;
    o.blocked = mr.blocked;
    o.touches.assign(mr.touches.ents, mr.touches.ents + mr.touches.count);

    const RecordedState& w = line.after;
    c.position_error = largest_difference(o.origin, w.origin);
    c.velocity_error = largest_difference(o.velocity, w.velocity);
    auto differs = [&](const char* field, int ours, int theirs) {
      if (ours == theirs) return;
      c.mismatch += std::string(c.mismatch.empty() ? "" : ", ") + field + " " +
                    std::to_string(ours) + " want " + std::to_string(theirs);
    };
    differs("onground", o.onground, w.onground);
    differs("groundent", o.groundent, w.groundent);
    differs("waterlevel", o.waterlevel, w.waterlevel);
    differs("watertype", o.watertype, w.watertype);
    differs("jump_held", o.jump_held, w.jump_held);
    differs("blocked", o.blocked, w.blocked);
    if (o.touches != w.touches) {
      auto list = [](const std::vector<int>& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); i++) out += (i ? " " : "") + std::to_string(v[i]);
        return out + "]";
      };
      c.mismatch += std::string(c.mismatch.empty() ? "" : ", ") + "touch " + list(o.touches) +
                    " want " + list(w.touches);
    }
    const float wj = o.waterjump_time > w.waterjump_time ? o.waterjump_time - w.waterjump_time
                                                         : w.waterjump_time - o.waterjump_time;
    if (!(wj <= kPositionTolerance))
      c.mismatch += std::string(c.mismatch.empty() ? "" : ", ") + "waterjumptime off by " +
                    std::to_string(wj);
    c.ok = c.mismatch.empty() && c.position_error <= kPositionTolerance &&
           c.velocity_error <= kVelocityTolerance;
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
  return r;
}

}  // namespace ge::shooter
