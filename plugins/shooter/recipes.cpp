// Movement recipes read off a recorded player (recipes.hpp): hop starts cut out of his hops, each
// frame's keys read off the move.
//
// ge:summary Movement recipes read off a recorded player: the hop start's entry, chain and frames
// ge:keywords recipe hop-start read_hop_starts entry keys chain
// ge:skill shooter bots
// ge:test tests/shooter_recipes_test.cpp
#include "shooter/recipes.hpp"

#include <algorithm>
#include <array>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::shooter {

namespace {

constexpr double kBlastWindow_s = 0.1;
constexpr float kKeyAxis = 400;  // a key's axis, as the classic client builds it
constexpr double kRad = core::kPi / 180;

Vec3f turned(Vec3f v, float turn) {
  const float c = static_cast<float>(core::libm::cos(turn * kRad));
  const float s = static_cast<float>(core::libm::sin(turn * kRad));
  return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// A blast in frames [a, b]: an air frame's speed up `rise` or more over the last 0.1 s.
bool blast_in(const HopReading& hops, size_t a, size_t b, float rise) {
  size_t back = a;
  for (size_t i = a; i <= b && i < hops.frames.size(); i++) {
    while (back < i && hops.frames[i].t - hops.frames[back].t > kBlastWindow_s) back++;
    if (hops.frames[i].phase == HopPhase::air &&
        hops.frames[i].speed - hops.frames[back].speed >= rise)
      return true;
  }
  return false;
}

}  // namespace

void read_hop_starts(std::span<const TrackPoint> track, const HopReading& hops,
                     const HopStartSettings& set, std::vector<HopStart>& out) {
  const std::vector<HopFrame>& f = hops.frames;
  if (f.size() != track.size() || f.size() < 3) return;
  const std::vector<Hop>& hs = hops.hops;
  for (size_t k = 0; k < hs.size(); k++) {
    if (k > 0 && hs[k].chain == hs[k - 1].chain) continue;  // not a chain's first jump
    const size_t first = static_cast<size_t>(hs[k].takeoff);
    // The entry: the last slow ground frame before the takeoff, over the ground.
    int64_t entry = -1;
    for (size_t i = first; i-- > 0;) {
      if (f[i].phase != HopPhase::ground || f[first].t - f[i].t > set.back_s) break;
      if (f[i].speed < set.slow) {
        entry = static_cast<int64_t>(i);
        break;
      }
    }
    if (entry < 0) continue;
    // The chain: its takeoffs and its last landing.
    size_t last = k;
    while (last + 1 < hs.size() && hs[last + 1].chain == hs[k].chain) last++;
    if (last - k + 1 < static_cast<size_t>(set.least_takeoffs)) continue;
    // The low: the slowest ground frame before the takeoff, looked for as the entry was.
    float low = f[static_cast<size_t>(entry)].speed;
    size_t low_at = static_cast<size_t>(entry);
    for (size_t i = first; i-- > 0;) {
      if (f[i].phase != HopPhase::ground || f[first].t - f[i].t > set.back_s) break;
      if (f[i].speed < low) low = f[i].speed, low_at = i;
      if (f[i].speed < set.stood) break;  // a stand: the move starts from its last still frame
    }
    const size_t e0 = static_cast<size_t>(entry);
    const size_t chain_end = static_cast<size_t>(hs[last].land);
    size_t end = last - k + 1 >= static_cast<size_t>(set.takeoffs)
                     ? static_cast<size_t>(hs[k + static_cast<size_t>(set.takeoffs) - 1].takeoff)
                     : chain_end;
    int64_t reach = -1;
    for (size_t i = e0; i <= chain_end && f[i].t - f[e0].t <= set.within_s; i++)
      if (f[i].phase != HopPhase::cut && f[i].speed >= set.target) {
        reach = static_cast<int64_t>(i);
        break;
      }
    if (reach > static_cast<int64_t>(end)) end = static_cast<size_t>(reach);
    if (blast_in(hops, e0, end, set.blast_rise)) continue;

    HopStart s;
    s.entry = static_cast<int32_t>(e0);
    s.first = static_cast<int32_t>(first);
    s.end = static_cast<int32_t>(end);
    s.reach = static_cast<int32_t>(reach);
    s.t = f[e0].t;
    s.feet = f[e0].origin;
    s.speed_in = f[e0].speed;
    s.low = low;
    s.low_frame = static_cast<int32_t>(low_at);
    s.stood = low < set.stood;
    s.yaw_in = f[e0].yaw;
    const size_t off = std::min(first + 1, f.size() - 1);
    s.heading = f[off].heading;
    s.run_s = static_cast<float>(f[first].t - f[e0].t);
    s.speed_off = f[off].speed;
    s.lag_off = f[off].lag;
    s.made = reach >= 0;
    if (s.made) {
      s.to_target_s = static_cast<float>(f[static_cast<size_t>(reach)].t - f[e0].t);
      s.low_to_target_s = static_cast<float>(f[static_cast<size_t>(reach)].t - f[low_at].t);
    }
    std::array<int32_t, 16> held{};
    for (size_t i = low_at; i <= end; i++) {
      RecipeFrame r;
      r.t = static_cast<float>(f[i].t - f[e0].t);
      r.feet = f[i].origin;
      r.velocity = f[i].velocity;
      r.yaw = track[i].yaw;
      r.pitch = track[i].pitch;
      Vec3f w;
      if (read_wish(hops, i, set.keys, w)) r.keys = keys_of_wish(w, f[i].yaw, set.keys.least_wish);
      r.jump = f[i].takeoff;
      r.ground = f[i].phase == HopPhase::ground;
      if (r.jump && i >= first) s.takeoffs++;
      if (i < first && r.ground) held[r.keys]++;
      s.top = std::max(s.top, f[i].speed);
      s.frames.push_back(r);
    }
    for (uint8_t b = 1; b < 16; b++)
      if (held[b] > held[s.run_keys]) s.run_keys = b;
    s.speed_out = f[end].speed;
    out.push_back(std::move(s));
  }
}

void play_recipe(std::span<const RecipeFrame> frames, const MoveWorld& world,
                 const MoveSettings& move, Vec3f origin, float turn, int lead, RecipePlay& out,
                 std::vector<Vec3f>* trail, const Vec3f* velocity, std::vector<float>* trail_t) {
  out = RecipePlay{};
  if (frames.size() < 2) return;
  MoveState st;
  st.origin = origin;
  st.velocity = velocity ? *velocity : turned(frames[0].velocity, turn);
  st.angles = {frames[0].pitch, frames[0].yaw + turn, 0};
  st.maxspeed = move.maxspeed;
  bool was_ground = true;
  const size_t n = frames.size();
  for (size_t i = 0; i + 1 < n; i++) {
    const RecipeFrame& d = frames[std::min(n - 1, i + static_cast<size_t>(std::max(0, lead)))];
    MoveCmd cmd;
    const float ms = (frames[i + 1].t - frames[i].t) * 1000.f;
    cmd.msec = static_cast<uint8_t>(std::clamp(ms + 0.5f, 1.f, 250.f));
    cmd.angles = {d.pitch, d.yaw + turn, 0};
    cmd.forward = static_cast<int16_t>(((d.keys & kKeyForward) ? kKeyAxis : 0.f) -
                                       ((d.keys & kKeyBack) ? kKeyAxis : 0.f));
    cmd.side = static_cast<int16_t>(((d.keys & kKeyRight) ? kKeyAxis : 0.f) -
                                    ((d.keys & kKeyLeft) ? kKeyAxis : 0.f));
    if (frames[i + 1].jump) cmd.buttons |= kButtonJump;
    const MoveResult r = run_command(st, cmd, move, world);
    if (r.blocked != 0) out.blocked++;
    const float sp = root_of(st.velocity.x * st.velocity.x + st.velocity.y * st.velocity.y);
    const bool ground = st.ground >= 0;
    if (out.takeoff < 0 && was_ground && !ground && st.velocity.z > 0) {
      out.takeoff = static_cast<int32_t>(i + 1);
      out.speed_takeoff = sp;
    }
    was_ground = ground;
    out.top = std::max(out.top, sp);
    out.speed_end = sp;
    if (trail) trail->push_back(st.origin);
    if (trail_t) trail_t->push_back(frames[i + 1].t - frames[0].t);
  }
  out.end = st.origin;
}

void recipe_frames(const RecipeBook& book, size_t i, float way, Vec3f origin,
                   std::vector<RecipeFrame>& out) {
  out.clear();
  if (i >= book.entries.size()) return;
  const RecipeEntry& e = book.entries[i];
  const float h = (way + static_cast<float>(e.heading10) * 0.1f) * static_cast<float>(kRad);
  const Vec3f v0{static_cast<float>(e.speed) * static_cast<float>(core::libm::cos(h)),
                 static_cast<float>(e.speed) * static_cast<float>(core::libm::sin(h)), 0};
  const float c = static_cast<float>(core::libm::cos(way * kRad));
  const float sn = static_cast<float>(core::libm::sin(way * kRad));
  const float last_ms = e.count > 0 ? static_cast<float>(book.steps[e.first + e.count - 1u].ms) : 1;
  for (uint32_t k = 0; k < e.count && e.first + k < book.steps.size(); k++) {
    const RecipeStep& st = book.steps[e.first + k];
    RecipeFrame f;
    f.t = static_cast<float>(st.ms) * 0.001f;
    const float along =
        static_cast<float>(e.reach) * static_cast<float>(st.ms) / std::max(1.f, last_ms);
    f.feet = origin + Vec3f{c * along, sn * along, 0};
    f.velocity = k == 0 ? v0 : Vec3f{};
    f.yaw = way + static_cast<float>(st.yaw10) * 0.1f;
    f.keys = st.keys;
    f.jump = (st.flags & kStepJump) != 0;
    f.ground = (st.flags & kStepGround) != 0;
    out.push_back(f);
  }
}

}  // namespace ge::shooter
