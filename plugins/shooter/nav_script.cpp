// Scripted ways (nav_script.hpp): the keys a script presses, and its proof by the move.
//
// ge:summary The scripted way's keys and its proof by player_move
// ge:keywords nav-script script_keys prove_script scripted-route
// ge:skill shooter bots
// ge:test tests/shooter_nav_test.cpp
#include "shooter/nav_script.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/hull.hpp"

namespace ge::shooter {

namespace {

constexpr float kStepUp = 18;     // the move climbs this without a jump
constexpr float kOnHeight = 4;    // a step is reached within this of its height, up or down
constexpr float kStepNear = 20;   // and within this across, or past it toward the next
constexpr float kFellBelow = 40;  // standing this far under the lowest step: fallen off
// The entry is left only this slow, units/s (a run, not a hop's carried speed), and either near
// standing or running within kEntryAim of the way to the first step after it.
constexpr float kEntrySpeed = 330;
constexpr float kSlowInto = 64;  // the entry and the last step are slowed into from this far
constexpr float kEntryStill = 100;
constexpr float kEntryAim = 0.87f;  // cos 30 degrees

float across(Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  return root_of(dx * dx + dy * dy);
}

float yaw_at(Vec3f from, Vec3f to) {
  const double r =
      core::libm::atan2(static_cast<double>(to.y - from.y), static_cast<double>(to.x - from.x));
  return static_cast<float>(r * (180.0 / core::kPi));
}

// Lined up at the entry: slow enough, and standing or running the script's way.
bool lined_up(const NavScript& sc, Vec3f vel) {
  const float speed = root_of(vel.x * vel.x + vel.y * vel.y);
  if (speed > kEntrySpeed) return false;
  if (speed < kEntryStill || sc.steps.size() < 2) return true;
  const Vec3f a = sc.steps[0].at, b = sc.steps[1].at;
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float len = root_of(dx * dx + dy * dy);
  return len > 0 && (vel.x * dx + vel.y * dy) / (speed * len) >= kEntryAim;
}

bool reached_step(const NavScript& sc, size_t i, Vec3f o, bool onground, float arrive) {
  const ScriptStep& st = sc.steps[i];
  const float dz = o.z - st.at.z;
  if (!onground || dz > kOnHeight || dz < -kOnHeight) return false;
  const bool last = i + 1 >= sc.steps.size();
  const float h = across(o, st.at);
  if (last) return h <= arrive;
  if (h <= kStepNear) return true;
  if (i == 0) return false;  // the entry is stood near, never passed
  // Past it toward the next step, on the line between them: a hop lands beyond a narrow tread's
  // middle (a point beside the way is not past it).
  const Vec3f next = sc.steps[i + 1].at;
  const float dx = next.x - st.at.x, dy = next.y - st.at.y;
  const float len = root_of(dx * dx + dy * dy);
  if (len <= 0) return false;
  const float rx = o.x - st.at.x, ry = o.y - st.at.y;
  const float lateral = (rx * dy - ry * dx) / len;
  return rx * dx + ry * dy > 0 && lateral <= kStepNear && lateral >= -kStepNear;
}

}  // namespace

ScriptKeys script_keys(const NavScript& sc, size_t& i, Vec3f origin, Vec3f velocity, bool onground,
                       float arrive) {
  ScriptKeys k;
  while (i < sc.steps.size() && reached_step(sc, i, origin, onground, arrive)) {
    // The entry is left only lined up: a player carried in fast or from the side brakes there
    // and turns down the script (its proof starts standing).
    if (i == 0 && sc.steps.size() > 1 && !lined_up(sc, velocity)) {
      k.yaw = yaw_at(origin, sc.steps[1].at);
      k.forward = 0;
      return k;
    }
    i++;
  }
  if (i >= sc.steps.size()) {
    k.done = true;
    return k;
  }
  const ScriptStep& st = sc.steps[i];
  const float h = across(origin, st.at);
  k.yaw = h > 0.01f ? yaw_at(origin, st.at) : 0;
  // The entry and the last step slowed into, so they are stood on, not run past (late hands
  // circle a point run at from close by).
  const bool last = i + 1 >= sc.steps.size();
  k.forward = (last || i == 0) && h < kSlowInto ? (h / kSlowInto > 0.2f ? h / kSlowInto : 0.2f) : 1;
  k.jump = st.hop && onground && st.at.z > origin.z + kStepUp && h <= kHopReach;
  return k;
}

NavProof prove_script(const MoveWorld& world, const MoveSettings& move, Vec3f from,
                      const NavScript& sc, const NavSettings& set, std::vector<Vec3f>* path) {
  NavProof p;
  if (sc.steps.empty()) return p;
  MoveState s;
  s.origin = from;
  s.maxspeed = move.maxspeed;
  float lowest = from.z;
  for (const ScriptStep& st : sc.steps)
    if (st.at.z < lowest) lowest = st.at.z;
  const float step_s = static_cast<float>(set.msec) * 0.001f;
  float yaw = yaw_at(from, sc.steps[0].at);
  size_t i = 0;
  bool jumped = false;  // the jump key is let go for a command after each press
  p.lo = from + kPlayerMins;
  p.hi = from + kPlayerMaxs;
  if (path) path->push_back(from);
  for (int n = 0;; n++) {
    const float t = static_cast<float>(n + 1) * step_s;
    const ScriptKeys k = script_keys(sc, i, s.origin, s.velocity, s.ground >= 0, set.arrive);
    if (k.done) {
      p.reached = true;
      return p;
    }
    if (i >= sc.steps.size() || across(s.origin, sc.steps[i].at) > 0.01f) yaw = k.yaw;
    MoveCmd c;
    c.msec = static_cast<uint8_t>(set.msec);
    c.angles = {0, yaw, 0};
    c.forward = static_cast<int16_t>(move.maxspeed * k.forward);
    c.buttons = k.jump && !jumped ? kButtonJump : 0;
    jumped = k.jump && !jumped;
    player_move(s, c, move, world);
    p.commands++;
    if (path) path->push_back(s.origin);
    const Vec3f blo = s.origin + kPlayerMins, bhi = s.origin + kPlayerMaxs;
    for (int a = 0; a < 3; a++) {
      if (blo[a] < p.lo[a]) p.lo[a] = blo[a];
      if (bhi[a] > p.hi[a]) p.hi[a] = bhi[a];
    }
    p.end = s.origin;
    p.time_s = t;
    if (s.ground >= 0 && s.origin.z < lowest - kFellBelow) return p;  // fell off
    if (t >= kScriptLimit_s) return p;
  }
}

}  // namespace ge::shooter
