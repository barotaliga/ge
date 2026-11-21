// The pilot plays people's hop starts (pilot_recipes.hpp): the choice proven by the move, the play
// closed loop against the proof.
//
// ge:summary The pilot plays people's hop starts: the choice, the proof, the closed-loop play
// ge:keywords pilot recipe recipe_choose recipe_play mirror proof closed-loop
// ge:skill shooterbots bots
// ge:test tests/shooterbots_pilot_recipes_test.cpp
#include "shooterbots/pilot_recipes.hpp"

#include <algorithm>
#include <array>

#include "core/prng.hpp"
#include "shooter/pmove.hpp"
#include "shooterbots/look_ahead.hpp"
#include "shooterbots/pilot_parts.hpp"
#include "shooterbots/pilot_step_parts.hpp"

namespace ge::shooterbots::pilot_parts {
namespace {

constexpr float kTakeoffShare = 0.9f;  // a proof leaves the ground at this share of the recorded
constexpr float kPitDrop = 40;         // a proof whose feet fall this far under the start: a pit
constexpr float kEndOff = 48;          // its end this near the route's line, units
constexpr int kWallSlides = 2;         // and slides on at most this many walls or steps
constexpr float kScoreSpeed = 100;     // the speed's difference a point, units/s
constexpr float kScoreHeading = 45;    // the heading's difference a point (moving), degrees
constexpr float kMoving = 50;          // a heading is read above this speed, units/s
constexpr double kRetry_s = 0.5;       // none proven: the book looked at again after this, s
constexpr double kAbortRetry_s = 1.0;  // aborted: after this, s
constexpr float kRecipeHold_s = 0.1f;
// `startclear`: the flight after a start's takeoff looked along this long (straight on or turned
// either way at the air's most), and each second it falls short charged this many degrees.
constexpr float kClearLook_s = 0.4f, kClearCost = 200, kClearDt = 1.f / 77;
constexpr float kRecipeRoom = 160;  // the hop's stretch runs on this far past a start's reach  //
                                    // its jump asked, the last frame held this long for it, s

float off_line(Vec3f p, Vec3f a, Vec3f b) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float len2 = dx * dx + dy * dy;
  float f = len2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0;
  f = std::clamp(f, 0.f, 1.f);
  return across(p, {a.x + dx * f, a.y + dy * f, p.z});
}

// The feet's distance across from the route's line between indices k and `end`.
float off_route(const Pilot& p, Vec3f at, int32_t end) {
  float best = 1e30f;
  for (size_t j = p.k; j < static_cast<size_t>(end) && j + 1 < p.route.size(); j++)
    best = std::min(best,
                    off_line(at, node(*p.g, p.route[j]).origin, node(*p.g, p.route[j + 1]).origin));
  return best;
}

// The route's length across from `at` to index `end`, by the link it lies on.
float left_on_route(const Pilot& p, Vec3f at, int32_t end) {
  float left = 0;
  for (size_t j = p.k + 1; j <= static_cast<size_t>(end) && j < p.route.size(); j++)
    left += j == p.k + 1 ? across(at, node(*p.g, p.route[j]).origin)
                         : across(node(*p.g, p.route[j - 1]).origin, node(*p.g, p.route[j]).origin);
  return left;
}

uint8_t mirrored(uint8_t keys) {
  const uint8_t left = keys & kKeyLeft, right = keys & kKeyRight;
  return static_cast<uint8_t>((keys & ~(kKeyLeft | kKeyRight)) | (left ? kKeyRight : 0) |
                              (right ? kKeyLeft : 0));
}

// The point `look` units along the route from the link nearest `at` (from index k up to `end`):
// where the hop aims from there (pilot_hop.cpp `hop_aim`, from any link, not just the pilot's).
Vec3f aim_from(const Pilot& p, Vec3f at, float look, int32_t end) {
  const NavGraph& g = *p.g;
  size_t j = p.k;
  float best = 1e30f;
  for (size_t i = p.k; i < static_cast<size_t>(end) && i + 1 < p.route.size(); i++) {
    const float d = off_line(at, node(g, p.route[i]).origin, node(g, p.route[i + 1]).origin);
    if (d < best) best = d, j = i;
  }
  float f = 0;
  const Vec3f a = node(g, p.route[j]).origin, b0 = node(g, p.route[j + 1]).origin;
  off_segment(at, a, b0, &f);
  Vec3f from{a.x + (b0.x - a.x) * f, a.y + (b0.y - a.y) * f, b0.z};
  float left = look;
  for (size_t k = j; k + 1 <= static_cast<size_t>(end) && k + 1 < p.route.size(); k++) {
    const Vec3f b = node(g, p.route[k + 1]).origin;
    const float d = across(from, b);
    if (d >= left) {
      const float u = left / d;
      return {from.x + (b.x - from.x) * u, from.y + (b.y - from.y) * u, b.z};
    }
    left -= d;
    from = b;
  }
  return node(g, p.route[static_cast<size_t>(end)]).origin;
}

}  // namespace

bool recipe_choose(Pilot& p, const PilotSense& s, float way, int32_t end) {
  const RecipeBook& book = p.set.recipes;
  if (book.entries.empty() || s.world == nullptr) return false;
  p.stats.recipe_asks++;
  const float speed = across({}, s.velocity);
  const float room = left_on_route(p, s.origin, end);
  const float vh = speed > kMoving ? heading(s.velocity.x, s.velocity.y) : way;
  const int tries = p.set.recipe_heading ? kRecipeTriesHeading : kRecipeTries;
  // Every entry of the level, each way round, by its distance from the pilot's state.
  struct Near {
    float score = 0;
    int32_t i = 0;
    int8_t mirror = 1;
  };
  std::array<Near, kRecipeTriesHeading> best;
  int n = 0;
  for (size_t i = 0; i < book.entries.size(); i++) {
    const RecipeEntry& e = book.entries[i];
    if (e.kind != RecipeKind::hop_start || e.level != p.set.recipe_level) continue;
    if (static_cast<float>(e.reach) + kRecipeRoom > room) continue;  // the hop's stretch too short
    for (const int8_t m : {int8_t{1}, int8_t{-1}}) {
      float score = (speed > e.speed ? speed - e.speed : e.speed - speed) / kScoreSpeed;
      if (speed > kMoving && e.speed > kMoving) {
        const float d = wrap(vh - way - static_cast<float>(m) * e.heading10 * 0.1f);
        score += (d < 0 ? -d : d) / kScoreHeading;
      }
      score += static_cast<float>(core::range(p.lane_rng, 0.0, 0.2));
      const Near c{score, static_cast<int32_t>(i), m};
      if (n < tries) {
        best[static_cast<size_t>(n++)] = c;
      } else {
        int worst = 0;
        for (int k = 1; k < n; k++)
          if (best[static_cast<size_t>(k)].score > best[static_cast<size_t>(worst)].score)
            worst = k;
        if (c.score < best[static_cast<size_t>(worst)].score) best[static_cast<size_t>(worst)] = c;
      }
    }
  }
  // Nearest first, by insertion (stable; GCC's -O3 reads std::sort's unguarded loop over a short
  // std::array as indexing past its end, -Warray-bounds).
  for (int i = 1; i < n; i++)
    for (int j = i;
         j > 0 && best[static_cast<size_t>(j)].score < best[static_cast<size_t>(j - 1)].score; j--)
      std::swap(best[static_cast<size_t>(j)], best[static_cast<size_t>(j - 1)]);
  MoveSettings move = p.g->move;
  move.maxspeed = s.maxspeed;
  // One candidate flown by the move from where the pilot stands: proven, and how far its takeoff
  // heads off the hop's aim from there (degrees; 0 when not asked).
  const auto fly = [&](int c, float* off_aim) {
    const Near& pick = best[static_cast<size_t>(c)];
    const RecipeEntry& e = book.entries[static_cast<size_t>(pick.i)];
    recipe_frames(book, static_cast<size_t>(pick.i), way, s.origin, p.recipe_frames);
    if (pick.mirror < 0)
      for (RecipeFrame& f : p.recipe_frames) {
        f.yaw = way - (f.yaw - way);
        f.keys = mirrored(f.keys);
      }
    // (`startease`) begun at the pilot's own view, the recording's reached in recipe_ease_s
    if (p.set.recipe_ease_s > 0 && !p.recipe_frames.empty()) {
      const float off = wrap(p.yaw - p.recipe_frames.front().yaw);
      const float t0 = p.recipe_frames.front().t;
      for (RecipeFrame& f : p.recipe_frames) {
        const float u = std::min(1.f, (f.t - t0) / p.set.recipe_ease_s);
        f.yaw += off * (1 - u * u * (3 - 2 * u));  // eased out: smooth at both ends
      }
    }
    p.recipe_trail.clear();
    p.recipe_trail_t.clear();
    RecipePlay r;
    play_recipe(p.recipe_frames, *s.world, move, s.origin, 0, 0, r, &p.recipe_trail, &s.velocity,
                &p.recipe_trail_t);
    if (r.takeoff < 0 || r.speed_takeoff < kTakeoffShare * static_cast<float>(e.takeoff) ||
        r.blocked > kWallSlides || off_route(p, r.end, end) > kEndOff)
      return false;
    for (const Vec3f& o : p.recipe_trail)
      if (o.z < s.origin.z - kPitDrop) return false;
    if (off_aim != nullptr) {
      const float look = std::max(64.f, r.speed_takeoff * p.set.hop_look_s);
      const float aim = yaw_at(r.at_takeoff, aim_from(p, r.at_takeoff, look, end));
      const float d = wrap(heading(r.velocity_takeoff.x, r.velocity_takeoff.y) - aim);
      *off_aim = d < 0 ? -d : d;
      if (p.set.recipe_clear) {
        // (`startclear`) The flight after the takeoff: the clearest of straight on and either turn.
        float clear = 0;
        for (const int side : {0, 1, -1})
          clear = std::max(clear, arc_clear(*s.world, r.at_takeoff, r.velocity_takeoff, side,
                                            kClearDt, kClearLook_s));
        *off_aim += (kClearLook_s - clear) * kClearCost;
      }
    }
    return true;
  };
  int chosen = -1;
  if (p.set.recipe_heading) {
    // (`starthead`) Every candidate flown; the proven one taking off nearest the hop's aim.
    float least = 1e30f;
    for (int c = 0; c < n; c++) {
      float off = 0;
      if (fly(c, &off) && off < least) least = off, chosen = c;
    }
    if (chosen >= 0 && !fly(chosen, nullptr)) chosen = -1;  // flown again: its frames and trail
  } else {
    for (int c = 0; c < n && chosen < 0; c++)
      if (fly(c, nullptr)) chosen = c;
  }
  if (chosen >= 0) {
    p.recipe = best[static_cast<size_t>(chosen)].i;
    p.recipe_jumped = false;
    p.recipe_s = 0;
    p.stats.recipe_commits++;
    return true;
  }
  p.recipe = -1;
  p.recipe_retry_s = p.clock_s + kRetry_s;
  return false;
}

bool recipe_play(Pilot& p, const PilotSense& s, float dt, RecipeWant& w) {
  const std::vector<RecipeFrame>& f = p.recipe_frames;
  if (p.recipe < 0 || f.size() < 2) {
    p.recipe = -1;
    return false;
  }
  if (p.recipe_jumped && !s.onground) {  // in the air: done, the hop flies on
    p.stats.recipe_takeoffs++;
    p.stats.recipe_speed += across({}, s.velocity);
    p.recipe = -1;
    return false;
  }
  // The proof's feet at this time: off by more, abort before the takeoff.
  size_t j = 0;
  while (j + 1 < p.recipe_trail_t.size() && p.recipe_trail_t[j + 1] <= p.recipe_s) j++;
  if (p.recipe_s > 0 && j < p.recipe_trail.size() && !p.recipe_jumped &&
      across(s.origin, p.recipe_trail[j]) > kRecipeOff) {
    p.stats.recipe_aborts++;
    p.recipe = -1;
    p.recipe_retry_s = p.clock_s + kAbortRetry_s;
    return false;
  }
  size_t i = 0;
  while (i + 1 < f.size() && f[i + 1].t <= p.recipe_s) i++;
  if (i + 1 >= f.size()) {
    // Ran out on the ground: its jump asked, the last frame held a few commands while the jump
    // key lands; never asked (or long past), the hop's from here.
    if (!p.recipe_jumped || p.recipe_s > f.back().t + kRecipeHold_s) {
      p.recipe = -1;
      return false;
    }
    i = f.size() - 2;
  }
  // The keys asked a keyboard's press early: those of the frame that much ahead (its jump too).
  size_t ik = i;
  while (ik + 2 < f.size() && f[ik + 1].t <= p.recipe_s + p.set.recipe_keys_s) ik++;
  const RecipeFrame& d = f[ik];
  w.yaw = f[i].yaw;
  w.forward = (d.keys & kKeyForward) ? 1.f : (d.keys & kKeyBack) ? -1.f : 0.f;
  w.side = (d.keys & kKeyRight) ? 1.f : (d.keys & kKeyLeft) ? -1.f : 0.f;
  w.jump = f[ik + 1].jump;
  w.curve = (d.keys & kKeyForward) && !(d.keys & kKeyBack) ? static_cast<int8_t>(w.side > 0   ? 1
                                                                                 : w.side < 0 ? -1
                                                                                              : 0)
                                                           : kNoCurveKeys;
  if (w.jump) p.recipe_jumped = true;
  p.recipe_s += dt;
  return true;
}

}  // namespace ge::shooterbots::pilot_parts
