// The recorded flights' parser and the frame-by-frame replay (toss_recording.hpp states the format
// and the tolerance).
//
// ge:summary Parses the recorded thrown entities and replays each through physics_toss
// ge:keywords parse_toss_recording replay_toss flight frame tolerance mismatch
// ge:skill shooter testing
// ge:test tests/shooter_toss_recording_test.cpp
#include "shooter/toss_recording.hpp"

#include <cmath>
#include <cstdlib>

#include "shooter/physics.hpp"

namespace ge::shooter {

namespace {

bool movetype_of(std::string_view w, int& m) {
  if (w == "bounce")
    m = kMovetypeBounce;
  else if (w == "flymissile")
    m = kMovetypeFlyMissile;
  else if (w == "toss")
    m = kMovetypeToss;
  else if (w == "fly")
    m = kMovetypeFly;
  else
    return false;
  return true;
}

struct Words {
  const std::vector<std::string_view>& t;
  size_t i = 0;
  bool ok = true;

  void key(std::string_view k) {
    if (i >= t.size() || t[i] != k) ok = false;
    i++;
  }
  std::string_view word() { return i < t.size() ? t[i++] : (ok = false, std::string_view{}); }
  float num() {
    float v = 0;
    if (i >= t.size() || !parse_float(t[i], v)) ok = false;
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

}  // namespace

bool parse_toss_recording(std::string_view text, TossRecording& out, std::string& error) {
  out = TossRecording{};
  enum class In { none, scene, flight } in = In::none;
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
      if (in != In::none || t.size() != 2) return fail("a scene opens outside a block");
      out.scenes.push_back({std::string(t[1]), {}});
      in = In::scene;
    } else if (t[0] == "brush") {
      Brush b;
      if (in != In::scene || t.size() != 2 || !parse_contents(t[1], b.contents))
        return fail("a brush needs a scene and a contents (solid, water, slime, lava)");
      out.scenes.back().brushes.push_back(b);
    } else if (t[0] == "plane") {
      if (in != In::scene || out.scenes.back().brushes.empty() || t.size() != 5)
        return fail("a plane needs a brush and four numbers");
      w.i = 1;
      Plane p;
      p.normal = w.vec();
      p.dist = w.num();
      if (!w.ok) return fail("a plane's numbers");
      p.type = plane_type(p.normal);
      out.scenes.back().brushes.back().planes.push_back(p);
    } else if (t[0] == "toss") {
      if (in != In::none || out.scenes.empty()) return fail("a flight needs a scene before it");
      TossFlight f;
      w.i = 1;
      f.name = std::string(w.word());
      w.key("movetype");
      if (!movetype_of(w.word(), f.movetype)) return fail("unknown movetype");
      w.key("origin");
      f.origin = w.vec();
      w.key("velocity");
      f.velocity = w.vec();
      w.key("frametime");
      float ft = 0;
      if (w.i < t.size() && parse_float(t[w.i], ft)) {
        // The frame time is a double in the loop: read it as written, not through a float.
        f.frametime = std::strtod(std::string(t[w.i]).c_str(), nullptr);
      } else {
        w.ok = false;
      }
      w.i++;
      w.key("gravity");
      f.gravity = w.num();
      w.key("maxvelocity");
      f.maxvelocity = w.num();
      if (!w.ok) return fail("a flight's header");
      f.scene = static_cast<int>(out.scenes.size()) - 1;
      out.flights.push_back(f);
      in = In::flight;
    } else if (t[0] == "frame") {
      if (in != In::flight) return fail("a frame outside a flight");
      w.i = 2;
      w.key("->");
      TossFrame fr;
      w.key("origin");
      fr.origin = w.vec();
      w.key("velocity");
      fr.velocity = w.vec();
      w.key("onground");
      fr.onground = w.num() != 0;
      w.key("hit");
      fr.hit = w.num() != 0;
      w.key("fraction");
      fr.fraction = w.num();
      w.key("normal");
      fr.normal = w.vec();
      if (!w.ok) return fail("a frame's fields");
      out.flights.back().frames.push_back(fr);
    } else if (t[0] == "end") {
      if (in == In::none) return fail("an end with nothing open");
      in = In::none;
    } else {
      return fail("unknown line '" + std::string(t[0]) + "'");
    }
  }
  if (in != In::none) return fail("the last block has no end");
  return true;
}

TossReplay replay_toss(const TossRecording& rec, const TossFlight& f) {
  TossReplay r;
  r.name = f.name;
  const ModelData world = build_brush_model(rec.scenes[static_cast<size_t>(f.scene)].brushes);
  ServerSettings set;
  set.move.gravity = f.gravity;
  set.maxvelocity = f.maxvelocity;
  set.maxclients = 0;
  set.max_edicts = 8;
  Server sv;
  create_server(sv, set, {world.model()}, Program{});
  const int e = spawn_edict(sv);
  Edict& ed = sv.edicts[static_cast<size_t>(e)];
  ed.movetype = f.movetype;
  ed.solid = kSolidBbox;
  ed.velocity = f.velocity;
  set_origin(sv, e, f.origin);

  for (size_t i = 0; i < f.frames.size(); i++) {
    HullTrace tr;
    physics_toss(sv, e, f.frametime, &tr);
    const Edict& now = sv.edicts[static_cast<size_t>(e)];
    TossCheck c;
    c.index = static_cast<int>(i);
    c.ours.origin = now.origin;
    c.ours.velocity = now.velocity;
    c.ours.onground = (now.flags & kFlagOnGround) != 0;
    c.ours.hit = tr.ent >= 0;
    c.ours.fraction = tr.fraction;
    c.ours.normal = tr.ent >= 0 ? tr.plane.normal : Vec3f{};
    const TossFrame& want = f.frames[i];
    c.position_error = max_diff(c.ours.origin, want.origin);
    c.velocity_error = max_diff(c.ours.velocity, want.velocity);
    c.trace_error = std::fabs(c.ours.fraction - want.fraction);
    const float nd = max_diff(c.ours.normal, want.normal);
    if (nd > c.trace_error) c.trace_error = nd;
    if (c.ours.onground != want.onground) c.mismatch += "onground ";
    if (c.ours.hit != want.hit) c.mismatch += "hit ";
    c.ok = c.mismatch.empty() && c.position_error <= kPositionTolerance &&
           c.velocity_error <= kVelocityTolerance && c.trace_error <= kTraceTolerance;
    if (!c.ok) r.failed++;
    if (c.position_error > r.max_position_error) {
      r.max_position_error = c.position_error;
      r.worst_position_frame = c.index;
    }
    if (c.velocity_error > r.max_velocity_error) {
      r.max_velocity_error = c.velocity_error;
      r.worst_velocity_frame = c.index;
    }
    if (c.trace_error > r.max_trace_error) r.max_trace_error = c.trace_error;
    r.frames.push_back(std::move(c));
  }
  return r;
}

}  // namespace ge::shooter
