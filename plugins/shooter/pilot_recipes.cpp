// The pilot plays people's hop starts (pilot_recipes.hpp): the choice proven by the move, the play
// closed loop against the proof.
//
// ge:summary The pilot plays people's hop starts: the choice, the proof, the closed-loop play
// ge:keywords pilot recipe recipe_choose recipe_play mirror proof closed-loop
// ge:skill shooter bots
// ge:test tests/shooter_pilot_recipes_test.cpp
#include "shooter/pilot_recipes.hpp"

#include <algorithm>
#include <array>

#include "core/prng.hpp"
#include "shooter/pilot_parts.hpp"
#include "shooter/pmove.hpp"

namespace ge::shooter::pilot_parts {
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

}  // namespace

bool recipe_choose(Pilot& p, const PilotSense& s, float way, int32_t end) {
  const RecipeBook& book = p.set.recipes;
  if (book.entries.empty() || s.world == nullptr) return false;
  p.stats.recipe_asks++;
  const float speed = across({}, s.velocity);
  const float room = left_on_route(p, s.origin, end);
  const float vh = speed > kMoving ? heading(s.velocity.x, s.velocity.y) : way;
  // Every entry of the level, each way round, by its distance from the pilot's state.
  struct Near {
    float score = 0;
    int32_t i = 0;
    int8_t mirror = 1;
  };
  std::array<Near, kRecipeTries> best;
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
      if (n < kRecipeTries) {
        best[static_cast<size_t>(n++)] = c;
      } else {
        size_t worst = 0;
        for (size_t k = 1; k < best.size(); k++)
          if (best[k].score > best[worst].score) worst = k;
        if (c.score < best[worst].score) best[worst] = c;
      }
    }
  }
  std::sort(best.begin(), best.begin() + n,
            [](const Near& a, const Near& b) { return a.score < b.score; });
  MoveSettings move;
  move.maxspeed = s.maxspeed;
  for (int c = 0; c < n; c++) {
    const Near& pick = best[static_cast<size_t>(c)];
    const RecipeEntry& e = book.entries[static_cast<size_t>(pick.i)];
    recipe_frames(book, static_cast<size_t>(pick.i), way, s.origin, p.recipe_frames);
    if (pick.mirror < 0)
      for (RecipeFrame& f : p.recipe_frames) {
        f.yaw = way - (f.yaw - way);
        f.keys = mirrored(f.keys);
      }
    p.recipe_trail.clear();
    p.recipe_trail_t.clear();
    RecipePlay r;
    play_recipe(p.recipe_frames, *s.world, move, s.origin, 0, 0, r, &p.recipe_trail, &s.velocity,
                &p.recipe_trail_t);
    if (r.takeoff < 0 || r.speed_takeoff < kTakeoffShare * static_cast<float>(e.takeoff) ||
        r.blocked > kWallSlides || off_route(p, r.end, end) > kEndOff)
      continue;
    bool pit = false;
    for (const Vec3f& o : p.recipe_trail) pit = pit || o.z < s.origin.z - kPitDrop;
    if (pit) continue;
    p.recipe = pick.i;
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

}  // namespace ge::shooter::pilot_parts
