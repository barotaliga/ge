// LAB `se-people`: HOW PLAYERS SPEND A TEAM MATCH, read off multiview recordings (people's or our
// bots'), by the same rules for both, so a bot's numbers are set beside people's measured alike.
// Every `.mvd` in `--dir` (or `--file`) is read frame by frame (client/demo.hpp) and each player
// sampled alive every `--every` seconds of the match:
//
// - IN VIEW: an enemy alive within the bots' sight range (4096 units) with a clear line from the
//   player's eye to the enemy's head or middle through the map's world (hull 0), whichever way the
//   player looks: the share of the time a fight could be had;
// - FACED: one of them on the player's screen as the bots' view is (120 degrees across and 88.5
//   high, off the recorded view angles; bots/fighter.cpp): the share of the time a fight is in
//   front of him;
// - NEAR: an enemy alive within `--near` units, seen or not (where a player stands against the
//   other team);
// - THE LAUNCHER: had (its item bit), and had with rockets for it;
// - THE ARMS: the share of the time each weapon is owned, the mean health and armour, and the
//   weapon sounds a player-minute alive by the sound (what is fired: the launcher's, the
//   scattergun's);
// - ITEM CONTROL: each key item (the armours, the mega, the quad, the shield, the launcher; its
//   model and skin) the share of the samples it lay untaken, and the mean time from its showing
//   to its going (people take the yellow armour within 4 s of its showing);
// - SPEED: across the floor, between two samples alive (a jump past 1000 units/s, a teleport, left
//   out);
// - THE FIGHT'S FEET: with an enemy faced, the player's speed across the line to him and along it
//   (closing positive), the share of it over 200 units/s across, and how often the side across
//   turns (a strafe dodge's jink): the dodge measured alike for people and bots; on dry feet
//   (`fight.dry.*`) by the distance to him (`fight.near.*` under 300 units, `mid` to 700, `far`)
//   and with or without a loaded launcher (`fight.rl.*`, `fight.no-rl.*`): people close in
//   (+68 units/s on dry feet), less with the launcher (+38);
// - WET: the share of the time alive with the waist in water, slime or lava, and of the time an
//   enemy is faced.
//
// - THE HAND: the view's turns off the recorded angles at 40 Hz, alike for people and bots:
//   sweeps (one way across past 30 degrees/s, 30 degrees or more) a minute, their pitch against
//   their yaw (`tilt`: up per degree right; a right hand's drag right runs down), the peak rate
//   over the mean (a hand's bell about 1.9, a constant turn 1), the share held dead level, lifts
//   (a sweep of 60 or more, a pause of 0.05-0.35 s, a sweep on the same way) a minute, and
//   reversals (the turn's way flipping between two ticks, both past 60 degrees/s) a second: jitter.
// - THE KEYS (keys_read.hpp, `keys.*`): the movement keys read off the body, and the same split by
//   an enemy in view at the last sample (`keys.fight.*`) or not (`keys.calm.*`): people's W on a
//   steady run 48 % calm, 28 % in a fight; `keys.fight-share` the fight's part of the reads.
// - THE BUMPS (bumps_read.hpp, `bumps.*`): a run stopped by a wall or a player, a minute alive and
//   a minute at speed (e1m2: people's walls 1.9 and 2.5, the bots' 2.4-2.8 and 4.4-5.2).
//
// `--samples=<file.tsv>` writes every sample alive as a row (the recording, the clock, the slot,
// the team, the place, wet, the eyes under, the speed, an enemy faced) for a question the
// metrics do not answer: where people are in the water, and where the bots are.
//
// The map is the demos' own, from `--basedir`'s game folder; demos of another map are skipped.
// Without a demo it records a short match of our bots on the fixture halls (classic_team.hpp) and
// reads that, which is what `make labs-smoke` runs. Deterministic: the digest holds the counts.
//
// ge:summary Lab: people's and bots' matches measured alike: enemy in view, near, speed
// ge:keywords lab se-people people realism yardstick in-view line-of-sight engagement contact
// ge:keywords speed mvd multiview recordings real-matches bots-against-people positioning
// ge:keywords dodge strafe fight-across wet water swimming hand mouse sweep tilt lift jitter
// ge:keywords fight-along closing keys-fight keys-calm fight-feet-by-range bumps wall-bumps
// ge:keywords weapons-owned shots-per-min firepower launcher health armour
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "core/parallel.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/bumps_read.hpp"
#include "shooters_edge/labs/keys_read.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr float kSight = 4096;       // the bots' sight range (shooter/senses.hpp)
constexpr double kFightAfter = 0.5;  // a key read this long past the last enemy seen is a fight's
constexpr float kEye = 22;           // the eye over the origin
constexpr float kFastest = 1000;
constexpr float kFov = 120;        // the bots' view across, degrees (bots/fighter.cpp)
constexpr float kFovY = 88.5f;     // and its height
constexpr int32_t kLauncher = 32;  // the rocket launcher's item bit
constexpr double kFired_s = 0.3;   // a shot this near a sample counts as firing then

struct Player {
  std::string name, team;
  int slot = 0;
  int64_t alive = 0, in_view = 0, in_cone = 0, near = 0, moved = 0, rl = 0, rl_loaded = 0;
  // The view: samples after a live one, those turning under 10 and over 360 degrees a second, the
  // turn summed; and of the samples with an enemy in plain view, those he fired within kFired_s.
  int64_t turned_n = 0, still = 0, fast = 0, fired_in_view = 0;
  double speed = 0, turn = 0;
  // Moving (over kCalmSpeed) with no enemy in plain view: the view's yaw off the way he moves, in
  // the bands kCalmBands (degrees), and its turn, summed.
  int64_t calm_n = 0, calm_off[4] = {};
  double calm_turn = 0;
  // With an enemy faced, between two samples alive: the speed across the line to him and along it
  // (closing positive), summed; the samples over kAcross across; the side's turns (each side held
  // past kSideSpeed).
  int64_t fight_n = 0, fight_wide = 0, fight_turns = 0;
  // Alive with the waist under water, slime or lava (a swimmer), and of those, with an enemy faced.
  int64_t wet = 0, wet_faced = 0;
  double fight_across = 0, fight_along = 0;
  // The same on dry feet (out of the water: a swimmer's feet are the water's), by his distance to
  // the enemy faced (kFightBands: near, mid, far) and by whether he holds a loaded rocket launcher
  // (0 without, 1 with): samples, across and along summed.
  int64_t band_n[3] = {}, rl_n[2] = {};
  double band_across[3] = {}, band_along[3] = {}, rl_across[2] = {}, rl_along[2] = {};
  // The arms: samples with each weapon owned (kArmBits), the health and armour summed.
  int64_t owned[8] = {};
  double health = 0, armour = 0;
};
// The weapons by their item bit. What is in hand is not read: people's recordings keep neither the
// active weapon nor the view model up to date; the weapon sounds say what is fired.
constexpr int32_t kArmBits[8] = {4096, 1, 2, 4, 8, 16, 32, 64};
constexpr const char* kArmNames[8] = {"axe",      "shotgun",  "double",  "nailgun",
                                      "super-ng", "grenades", "rockets", "lightning"};
constexpr double kAcross = 200;
constexpr double kFightBands[2] = {300, 700};  // near under the first, far past the second, units
constexpr double kSideSpeed = 100;
constexpr double kCalmSpeed = 100;
constexpr float kCalmBands[3] = {15, 45, 90};

// The key items, by their model (and the armour's skin): how long each sits on its spot untaken.
constexpr int kItemKinds = 7;
const char* const kItemNames[kItemKinds] = {"ga", "ya", "ra", "mh", "quad", "pent", "rl"};
int item_kind(const std::string& model, int skin) {
  if (model == "progs/armor.mdl") return skin >= 0 && skin <= 2 ? skin : -1;
  if (model == "maps/b_bh100.bsp") return 3;
  if (model == "progs/quaddama.mdl") return 4;
  if (model == "progs/invulner.mdl") return 5;
  if (model == "progs/g_rock2.mdl") return 6;
  return -1;
}

struct ItemControl {
  int64_t samples = 0, present = 0;  // each spot's samples, and those it was there in
  int64_t taken = 0;                 // times it went
  double waited = 0;                 // from showing to going, summed over the times it went
};

// One sample alive, for `--samples`.
struct Sample {
  double t = 0;
  int slot = 0;
  sh::Vec3f at;
  bool wet = false, under = false, faced = false;
  float speed = -1;  // -1: not moved since a sample alive
};

constexpr double kHandEvery = 0.025;  // the hand read at 40 Hz
constexpr double kSweepRate = 30;     // a sweep turns faster than this across, degrees/s
constexpr double kSweepMin = 30;      // a sweep this long or more is measured, degrees
constexpr double kLiftBefore = 60;    // a lift follows a sweep this long or more, degrees
constexpr double kLiftStillLo = 0.05, kLiftStillHi = 0.35;  // and a still stretch this long, s
constexpr double kReverseRate = 60;  // a reversal: both turns past this, degrees/s
constexpr double kTeleport = 3000;   // a turn past this is the server's (a teleport), degrees/s
constexpr double kHopSpeed = 340;    // a player this fast across is hopping (past a run's 320), u/s

// THE HAND (the view as a mouse turns it), every kHandEvery s for each player alive: rightward and
// upward turn rates off the recorded angles. A SWEEP is a run of ticks turning one way across
// past kSweepRate; one of kSweepMin degrees or more is measured: its pitch against its yaw (the
// tilt a hand's arc gives: a right hand's drag right runs down), its peak rate over its mean (a
// hand's bell 1.9, a constant turn 1.0), and whether its pitch held dead level. A LIFT is a sweep
// of kLiftBefore or more, a still stretch of kLiftStill_lo..hi s, then a sweep the same way of
// kSweepMin or more (the hand back across the pad). A REVERSAL is a tick whose turn runs the
// other way from the last one's, both past kReverseRate (jitter).
struct HandRead {
  int64_t ticks = 0, sweeps = 0, level = 0, lifts = 0, big = 0, reversals = 0, flat = 0,
          turning = 0;
  double xy = 0, xx = 0, ratio = 0, peak = 0, secs = 0, size = 0;
  double up_right = 0, right_right = 0, up_left = 0, right_left = 0;
};

void add(HandRead& a, const HandRead& h) {
  a.ticks += h.ticks;
  a.sweeps += h.sweeps;
  a.level += h.level;
  a.lifts += h.lifts;
  a.big += h.big;
  a.reversals += h.reversals;
  a.flat += h.flat;
  a.turning += h.turning;
  a.xy += h.xy;
  a.xx += h.xx;
  a.ratio += h.ratio;
  a.peak += h.peak;
  a.secs += h.secs;
  a.size += h.size;
  a.up_right += h.up_right;
  a.right_right += h.right_right;
  a.up_left += h.up_left;
  a.right_left += h.right_left;
}
struct HandState {
  bool live = false;
  float yaw = 0, pitch = 0;
  double rate = 0;  // the last tick's rightward rate
  int sign = 0;
  double sx = 0, sy = 0, dur = 0, peak = 0;  // the sweep under way
  double last_sweep = 0;                     // the last sweep measured's size, signed
  double still = 0;                          // still since it, s
};

struct Read {
  bool ok = false, other_map = false;
  std::vector<Sample> samples;  // with --samples only
  std::string error, map;
  double seconds = 0;
  std::vector<Player> players;  // by slot, those who played
  std::array<ItemControl, kItemKinds> items{};
  std::map<std::string, int64_t> shots;  // the players' weapon sounds, by the sound's name
  HandRead hand;
  HandRead hop;       // the hand while hopping alone (past kHopSpeed across)
  la::KeysRead keys;  // the movement keys read off the body (keys_read.hpp)
  // The same reads split by whether an enemy was in his line of sight at the last sample (within
  // kFightAfter of it): the fight's footwork against the rest.
  la::KeysRead keys_fight, keys_calm;
  la::BumpsRead bumps;  // the wall and player bumps read off the body (bumps_read.hpp)
  // Backpacks: how many fell, how many went before they ran out (taken), the time each lay
  // until taken summed, and those taken within 10 s.
  int64_t packs = 0, packs_taken = 0, packs_quick = 0;
  double pack_wait = 0;
};

// The sweep under way in `h` ends: measured into `r` when it is long enough.
void end_sweep(HandState& h, HandRead& r) {
  const double size = std::fabs(h.sx);
  if (h.sign != 0 && size >= kSweepMin) {
    r.sweeps++;
    r.size += size;
    r.xy += h.sx * h.sy;
    r.xx += h.sx * h.sx;
    r.ratio += std::fabs(h.sy) / size;
    r.peak += h.dur > 0 ? h.peak / (size / h.dur) : 1;
    r.level += std::fabs(h.sy) < 0.25;
    r.big += size >= 90;
    if (h.sx > 0) {
      r.up_right += h.sy;
      r.right_right += size;
    } else {
      r.up_left += h.sy;
      r.right_left += size;
    }
    const bool same = (h.last_sweep > 0) == (h.sx > 0);
    if (same && std::fabs(h.last_sweep) >= kLiftBefore && h.still >= kLiftStillLo &&
        h.still <= kLiftStillHi)
      r.lifts++;
    h.last_sweep = h.sx;
    h.still = 0;
  } else if (h.sign != 0) {
    h.still += h.dur;  // a small turn between two sweeps counts as the hand at rest
  }
  h.sign = 0;
  h.sx = h.sy = h.dur = h.peak = 0;
}

// One 40 Hz tick of a live player's view (`yaw`, `pitch` as recorded: pitch down positive).
void hand_tick(HandState& h, HandRead& r, float yaw, float pitch, double dt) {
  if (!h.live) {
    h = HandState{};
    h.live = true;
    h.yaw = yaw;
    h.pitch = pitch;
    return;
  }
  float dyaw = yaw - h.yaw;
  while (dyaw > 180) dyaw -= 360;
  while (dyaw < -180) dyaw += 360;
  const double rx = -dyaw / dt, ry = -(pitch - h.pitch) / dt;  // to the right, up
  h.yaw = yaw;
  h.pitch = pitch;
  r.ticks++;
  r.secs += dt;
  if (std::fabs(rx) > kTeleport) {
    h.sign = 0;
    h.sx = h.sy = h.dur = h.peak = 0;
    h.last_sweep = 0;
    h.rate = 0;
    return;
  }
  if (std::fabs(rx) > kReverseRate && std::fabs(h.rate) > kReverseRate && rx * h.rate < 0)
    r.reversals++;
  if (std::fabs(rx) > kReverseRate) {
    r.turning++;
    r.flat += ry == 0;
  }
  h.rate = rx;
  const int sg = rx > kSweepRate ? 1 : rx < -kSweepRate ? -1 : 0;
  if (sg != h.sign) end_sweep(h, r);
  if (sg != 0) {
    h.sign = sg;
    h.sx += rx * dt;
    h.sy += ry * dt;
    h.dur += dt;
    h.peak = std::fmax(h.peak, std::fabs(rx));
  } else {
    h.still += dt;
  }
}

// The hand's measures under `key` (HandRead).
void hand_metrics(gl::Lab& lab, const std::string& key, const HandRead& hand) {
  const auto ratio = [](double a, double b) { return b > 0 ? a / b : 0; };
  const double sw = static_cast<double>(hand.sweeps), mins = hand.secs / 60;
  lab.metric(key + "sweeps-per-min", ratio(sw, mins), "per-min");
  lab.metric(key + "big-per-min", ratio(static_cast<double>(hand.big), mins), "per-min");
  lab.metric(key + "sweep-size", ratio(hand.size, sw), "deg");
  lab.metric(key + "tilt", ratio(hand.xy, hand.xx), "deg-up-per-deg-right");
  lab.metric(key + "up-per-right", ratio(hand.up_right, hand.right_right), "deg/deg");
  lab.metric(key + "up-per-left", ratio(hand.up_left, hand.right_left), "deg/deg");
  lab.metric(key + "sweep-pitch", ratio(hand.ratio, sw), "deg/deg");
  lab.metric(key + "sweep-peak", ratio(hand.peak, sw), "peak/mean");
  lab.metric(key + "sweep-level", ratio(static_cast<double>(hand.level), sw), "share");
  lab.metric(key + "lifts-per-min", ratio(static_cast<double>(hand.lifts), mins), "per-min");
  lab.metric(key + "lifts-per-big",
             ratio(static_cast<double>(hand.lifts), static_cast<double>(hand.big)), "ratio");
  lab.metric(key + "reversals-per-s", ratio(static_cast<double>(hand.reversals), hand.secs),
             "per-s");
  lab.metric(key + "flat-pitch",
             ratio(static_cast<double>(hand.flat), static_cast<double>(hand.turning)), "share");
}

// The keys read off the bodies (keys_read.hpp): each key set's share of the ground's reads (no key
// among them) and of the air's, the wish's angle off its key set on the ground (a keyboard's sits
// on the eight), the key set's changes a second, a side key's hold, and the part-way share of the
// steady ground speeds (a key is down or up).
void keys_metrics(gl::Lab& lab, const std::string& pre, la::KeysRead& k) {
  const auto ratio = [](double a, double b) { return b > 0 ? a / b : 0; };
  int64_t ground = 0, air = 0;
  for (size_t i = 0; i < 8; i++) ground += k.ground[i];
  for (size_t i = 0; i < 3; i++) air += k.air[i];
  const double g = static_cast<double>(ground), a = static_cast<double>(air);
  for (size_t i = 0; i < 8; i++)
    lab.metric(pre + "ground." + la::kKeySets[i], ratio(static_cast<double>(k.ground[i]), g),
               "share");
  lab.metric(pre + "air.left", ratio(static_cast<double>(k.air[0]), a), "share");
  lab.metric(pre + "air.none", ratio(static_cast<double>(k.air[1]), a), "share");
  lab.metric(pre + "air.right", ratio(static_cast<double>(k.air[2]), a), "share");
  lab.metric(pre + "off-p50", la::keys_quantile(k.off, 0.5), "deg");
  lab.metric(pre + "off-p90", la::keys_quantile(k.off, 0.9), "deg");
  lab.metric(pre + "side-changes-per-s", ratio(static_cast<double>(k.changes), k.read_s), "per-s");
  lab.metric(pre + "side-run-p50", la::keys_quantile(k.side_runs, 0.5), "s");
  lab.metric(pre + "side-run-p90", la::keys_quantile(k.side_runs, 0.9), "s");
  lab.metric(pre + "part-share", ratio(static_cast<double>(k.part), static_cast<double>(k.steady)),
             "share");
}

// A slot's team: its user info's `team`, else its shirt's colour (a recording without the key).
std::string team_of(const cl::DemoState& s, int i) {
  const std::string& t = s.teams[static_cast<size_t>(i)];
  return t.empty() ? "colour " + std::to_string(s.bottom[static_cast<size_t>(i)]) : t;
}

bool clear(const sh::Hull& h, sh::Vec3f a, sh::Vec3f b) {
  const sh::HullTrace tr = sh::trace_hull(h, a, b);
  return !tr.startsolid && tr.fraction >= 1;
}

void read_one(std::vector<uint8_t> bytes, const la::StudyMap& m, double every, float near,
              bool keep, Read& out) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, out.error)) return;
  const sh::Hull& world = m.bsp.models[0].hulls[0];
  // The bots' screen (shooter/senses.hpp `sees_player`): each half angle as a cosine and sine.
  const sh::Vec3f wide = sh::angle_vectors({0, kFov * 0.5f, 0}).forward;
  const sh::Vec3f high = sh::angle_vectors({0, kFovY * 0.5f, 0}).forward;
  std::vector<Player> by(32);
  std::vector<sh::Vec3f> was(32);
  std::vector<uint8_t> was_alive(32, 0);
  std::vector<float> was_yaw(32, 0);
  std::vector<int8_t> was_side(32, 0);     // the side he last moved across his faced enemy's line
  std::vector<double> fired_at(32, -1e9);  // each player's last weapon sound
  // Each key item's entity: its kind, its model, whether it was there at the last sample, since
  // when.
  struct Spot {
    int kind = -1, model = 0;
    bool there = false;
    double since = -1;
  };
  std::vector<Spot> spots;
  std::vector<double> pack_since;  // by entity: the backpack lying there since (-1: none)
  double next = 0, t0 = -1;
  std::vector<HandState> hands(32), hops(32);
  std::vector<sh::Vec3f> hand_was(32);
  double hand_next = 0, hand_at = -1;
  std::vector<la::KeysState> keys(32);
  std::vector<la::BumpState> bumps(32);
  std::vector<double> seen_at(32, -1e9);  // by slot: the last sample with an enemy in view
  const sh::Hull& body = m.bsp.models[0].hulls[1];
  for (;;) {
    const cl::DemoRead rd = cl::next_frame(d, out.error);
    if (rd == cl::DemoRead::error) return;
    if (rd == cl::DemoRead::end) break;
    const cl::DemoState& s = d.state;
    if (!s.map.empty() && s.map != m.name) {
      out.other_map = true;
      out.map = s.map;
      out.ok = true;
      return;
    }
    if (s.intermission || s.players.size() < 32) continue;
    // Every frame's weapon sounds (the weapon channel, the player's own entity): his shots.
    for (const cl::DemoSound& snd : s.started)
      if (snd.channel == 1 && snd.entity >= 1 && snd.entity <= 32) {
        fired_at[static_cast<size_t>(snd.entity - 1)] = s.time;
        if (snd.sound > 0 && static_cast<size_t>(snd.sound) < s.sounds.size())
          out.shots[s.sounds[static_cast<size_t>(snd.sound)]]++;
      }
    if (t0 < 0) t0 = s.time;
    // The keys, at every update of a live player (his origin moved: a multiview recording keeps
    // a player's last state between his updates).
    for (int i = 0; i < 32; i++) {
      const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
      la::KeysState& k = keys[static_cast<size_t>(i)];
      const bool live = p.present && !p.dead && !s.names[static_cast<size_t>(i)].empty();
      if (!live) {
        la::bumps_reset(bumps[static_cast<size_t>(i)]);
        if (k.n > 0) {
          la::KeysRead one;
          la::keys_reset(k, one);
          la::add(out.keys, one);
          const bool fight = s.time - seen_at[static_cast<size_t>(i)] <= every + kFightAfter;
          la::add(fight ? out.keys_fight : out.keys_calm, one);
        }
        continue;
      }
      if (k.n > 0) {
        const sh::Vec3f last = la::keys_back(k, 0).at;
        if (last.x == p.origin.x && last.y == p.origin.y && last.z == p.origin.z) continue;
      }
      const sh::HullTrace down =
          sh::trace_hull(body, p.origin, {p.origin.x, p.origin.y, p.origin.z - 2});
      const bool ground = down.startsolid || down.fraction < 1;
      const bool wet = sh::point_contents(world, p.origin) <= sh::kContentsWater;
      la::KeysRead one;
      if (wet)
        la::keys_reset(k, one);
      else
        la::keys_update(k, one, s.time, p.origin, p.angles.y, ground);
      // The bumps: a wall's or another player's (bumps_read.hpp), dry only.
      la::BumpState& bs = bumps[static_cast<size_t>(i)];
      if (wet) {
        la::bumps_reset(bs);
      } else {
        bool near_other = false;
        for (int j = 0; j < 32 && !near_other; j++) {
          const cl::DemoEntity& e = s.players[static_cast<size_t>(j)];
          if (j == i || !e.present || e.dead || s.names[static_cast<size_t>(j)].empty()) continue;
          near_other = std::fabs(e.origin.x - p.origin.x) < la::kBumpTouch &&
                       std::fabs(e.origin.y - p.origin.y) < la::kBumpTouch &&
                       std::fabs(e.origin.z - p.origin.z) < 64;
        }
        const float health =
            static_cast<size_t>(i) < d.player_stats.size()
                ? static_cast<float>(d.player_stats[static_cast<size_t>(i)][cl::kStatHealth])
                : 0.f;
        la::bumps_update(bs, out.bumps, body, s.time, p.origin, ground, health, near_other);
      }
      la::add(out.keys, one);
      const bool fight = s.time - seen_at[static_cast<size_t>(i)] <= every + kFightAfter;
      la::add(fight ? out.keys_fight : out.keys_calm, one);
    }
    // The hand, every kHandEvery of the match for every live player.
    if (s.time >= hand_next) {
      const double dt = hand_at < 0 ? 0 : s.time - hand_at;
      hand_at = s.time;
      hand_next = s.time + kHandEvery;
      for (int i = 0; i < 32; i++) {
        const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
        HandState& h = hands[static_cast<size_t>(i)];
        HandState& hh = hops[static_cast<size_t>(i)];
        const bool live = p.present && !p.dead && !s.names[static_cast<size_t>(i)].empty();
        const sh::Vec3f was_at = hand_was[static_cast<size_t>(i)];
        hand_was[static_cast<size_t>(i)] = p.origin;
        if (!live || dt <= 0 || dt > 4 * kHandEvery) {
          h.live = hh.live = false;
          if (live) hand_tick(h, out.hand, p.angles.y, p.angles.x, kHandEvery);
          continue;
        }
        hand_tick(h, out.hand, p.angles.y, p.angles.x, dt);
        // Hopping (fast across, as only a hop or a blast moves a player): its own read.
        const double ax = p.origin.x - was_at.x, ay = p.origin.y - was_at.y;
        if (std::sqrt(ax * ax + ay * ay) / dt > kHopSpeed)
          hand_tick(hh, out.hop, p.angles.y, p.angles.x, dt);
        else
          hh.live = false;
      }
    }
    if (s.time < next) continue;
    next = s.time + every;
    out.seconds = s.time - t0;
    // The key items: an entity seen with a key item's model is that item's spot from then on.
    if (spots.size() < s.entities.size()) spots.resize(s.entities.size());
    for (size_t e = 33; e < s.entities.size(); e++) {
      const cl::DemoEntity& en = s.entities[e];
      Spot& sp = spots[e];
      if (sp.kind < 0 && en.present && en.model > 0 &&
          static_cast<size_t>(en.model) < s.models.size()) {
        sp.kind = item_kind(s.models[static_cast<size_t>(en.model)], en.skin);
        sp.model = en.model;
      }
      if (sp.kind < 0) continue;
      const bool there = en.present && en.model == sp.model;
      ItemControl& ic = out.items[static_cast<size_t>(sp.kind)];
      ic.samples++;
      ic.present += there;
      if (there && !sp.there) sp.since = s.time;
      if (!there && sp.there && sp.since >= 0) {
        ic.taken++;
        ic.waited += s.time - sp.since;
      }
      sp.there = there;
    }
    // Backpacks: one appearing fell; one gone before its two minutes were up was taken.
    if (pack_since.size() < s.entities.size()) pack_since.resize(s.entities.size(), -1);
    for (size_t e = 33; e < s.entities.size(); e++) {
      const cl::DemoEntity& en = s.entities[e];
      const bool pack = en.present && en.model > 0 &&
                        static_cast<size_t>(en.model) < s.models.size() &&
                        s.models[static_cast<size_t>(en.model)] == "progs/backpack.mdl";
      double& since = pack_since[e];
      if (pack && since < 0) {
        since = s.time;
        out.packs++;
      } else if (!pack && since >= 0) {
        const double lay = s.time - since;
        if (lay < 115) {
          out.packs_taken++;
          out.pack_wait += lay;
          out.packs_quick += lay < 10;
        }
        since = -1;
      }
    }
    for (int i = 0; i < 32; i++) {
      const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
      const bool live = p.present && !p.dead && !s.names[static_cast<size_t>(i)].empty();
      if (!live) {
        was_alive[static_cast<size_t>(i)] = 0;
        continue;
      }
      Player& pl = by[static_cast<size_t>(i)];
      pl.name = s.names[static_cast<size_t>(i)];
      pl.slot = i;
      pl.team = team_of(s, i);
      pl.alive++;
      const sh::Vec3f eye{p.origin.x, p.origin.y, p.origin.z + kEye};
      bool view = false, cone = false, close = false;
      int faced = -1;
      const sh::ViewAxes ax = sh::angle_vectors(p.angles);
      for (int j = 0; j < 32; j++) {
        const cl::DemoEntity& e = s.players[static_cast<size_t>(j)];
        if (j == i || !e.present || e.dead || s.names[static_cast<size_t>(j)].empty() ||
            team_of(s, j) == pl.team)
          continue;
        const sh::Vec3f to = e.origin - p.origin;
        const float d2 = to.x * to.x + to.y * to.y + to.z * to.z;
        close = close || d2 < near * near;
        if (cone || d2 > kSight * kSight) continue;
        const sh::Vec3f head{e.origin.x, e.origin.y, e.origin.z + kEye};
        if (!clear(world, eye, head) && !clear(world, eye, e.origin)) continue;
        view = true;
        const sh::Vec3f h = head - eye;
        const float ahead = sh::dot(ax.forward, h);
        cone = sh::dot(h, h) < 1 ||
               (ahead > 0 && std::fabs(sh::dot(ax.right, h)) * wide.x <= ahead * wide.y &&
                std::fabs(sh::dot(ax.up, h)) * high.x <= ahead * high.y);
        if (cone) faced = j;
      }
      pl.in_view += view;
      if (view) seen_at[static_cast<size_t>(i)] = s.time;
      pl.fired_in_view += view && s.time - fired_at[static_cast<size_t>(i)] <= kFired_s;
      pl.in_cone += cone;
      const bool wet = sh::point_contents(world, p.origin) <= sh::kContentsWater;
      pl.wet += wet;
      pl.wet_faced += wet && cone;
      if (static_cast<size_t>(i) < d.player_stats.size()) {
        const auto& st = d.player_stats[static_cast<size_t>(i)];
        const bool has = (st[cl::kStatItems] & kLauncher) != 0;
        pl.rl += has;
        pl.rl_loaded += has && st[cl::kStatRockets] > 0;
        for (int w = 0; w < 8; w++) {
          pl.owned[w] += (st[cl::kStatItems] & kArmBits[w]) != 0;
        }
        pl.health += st[cl::kStatHealth];
        pl.armour += st[cl::kStatArmour];
      }
      pl.near += close;
      Sample sm;
      if (was_alive[static_cast<size_t>(i)]) {
        const sh::Vec3f dv = p.origin - was[static_cast<size_t>(i)];
        const double v =
            std::sqrt(static_cast<double>(dv.x) * dv.x + static_cast<double>(dv.y) * dv.y) / every;
        if (v < kFastest) {
          pl.speed += v;
          pl.moved++;
          sm.speed = static_cast<float>(v);
        }
      }
      if (keep) {
        sm.t = s.time - t0;
        sm.slot = i;
        sm.at = p.origin;
        sm.wet = wet;
        sm.under = sh::point_contents(world, eye) <= sh::kContentsWater;
        sm.faced = cone;
        out.samples.push_back(sm);
      }
      if (was_alive[static_cast<size_t>(i)]) {
        float dy = p.angles.y - was_yaw[static_cast<size_t>(i)];
        while (dy > 180) dy -= 360;
        while (dy < -180) dy += 360;
        const double rate = std::fabs(dy) / every;
        pl.turned_n++;
        pl.turn += rate;
        pl.still += rate < 10;
        pl.fast += rate > 360;
        // Where he looks while on the move with nobody in view: off the way he moves.
        const sh::Vec3f dv = p.origin - was[static_cast<size_t>(i)];
        const double v =
            std::sqrt(static_cast<double>(dv.x) * dv.x + static_cast<double>(dv.y) * dv.y) / every;
        if (!view && v > kCalmSpeed && v < kFastest) {
          float off = p.angles.y -
                      static_cast<float>(ge::core::libm::atan2(dv.y, dv.x) * 180 / ge::core::kPi);
          while (off > 180) off -= 360;
          while (off < -180) off += 360;
          off = std::fabs(off);
          pl.calm_n++;
          pl.calm_off[off < kCalmBands[0]   ? 0
                      : off < kCalmBands[1] ? 1
                      : off < kCalmBands[2] ? 2
                                            : 3]++;
          pl.calm_turn += rate;
        }
      }
      // His feet with an enemy faced: across the line to him and along it.
      if (faced >= 0 && was_alive[static_cast<size_t>(i)]) {
        const sh::Vec3f dv = p.origin - was[static_cast<size_t>(i)];
        const sh::Vec3f to = s.players[static_cast<size_t>(faced)].origin - p.origin;
        const double len =
            std::sqrt(static_cast<double>(to.x) * to.x + static_cast<double>(to.y) * to.y);
        const double vx = dv.x / every, vy = dv.y / every;
        if (len > 1 && std::sqrt(vx * vx + vy * vy) < kFastest) {
          const double ux = to.x / len, uy = to.y / len;
          const double along = vx * ux + vy * uy, across = vx * uy - vy * ux;
          pl.fight_n++;
          pl.fight_across += std::fabs(across);
          pl.fight_along += along;
          pl.fight_wide += std::fabs(across) > kAcross;
          const int band = len < kFightBands[0] ? 0 : len < kFightBands[1] ? 1 : 2;
          if (!wet) {
            pl.band_n[band]++;
            pl.band_across[band] += std::fabs(across);
            pl.band_along[band] += along;
            bool rl = false;
            if (static_cast<size_t>(i) < d.player_stats.size()) {
              const auto& st = d.player_stats[static_cast<size_t>(i)];
              rl = (st[cl::kStatItems] & kLauncher) != 0 && st[cl::kStatRockets] > 0;
            }
            pl.rl_n[rl]++;
            pl.rl_across[rl] += std::fabs(across);
            pl.rl_along[rl] += along;
          }
          if (std::fabs(across) > kSideSpeed) {
            const int8_t side = across > 0 ? 1 : -1;
            int8_t& ws = was_side[static_cast<size_t>(i)];
            pl.fight_turns += ws != 0 && side != ws;
            ws = side;
          }
        }
      } else {
        was_side[static_cast<size_t>(i)] = 0;
      }
      was_yaw[static_cast<size_t>(i)] = p.angles.y;
      was[static_cast<size_t>(i)] = p.origin;
      was_alive[static_cast<size_t>(i)] = 1;
    }
  }
  for (Player& p : by)
    if (p.alive > 0) out.players.push_back(std::move(p));
  out.ok = true;
}

// A short match of our bots on the fixture halls, recorded: what the lab reads without a demo.
bool bots_match(std::vector<uint8_t>& mvd, std::string& error) {
  std::vector<uint8_t> bytes;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes, &error))
    return false;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) return false;
  cn::TeamMatchOptions o;
  o.map = bytes;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.seed = 1;
  o.frames = static_cast<int>(std::lround(0.5 * 60 * 1000 / shooters_edge::bots::kPilotMsec));
  const cn::TeamMatchReport r = cn::run_team_match(o);
  if (!r.ok) {
    error = r.error;
    return false;
  }
  mvd = r.mvd;
  return true;
}

void run(gl::Lab& lab) {
  std::string error;
  const std::string dir = lab.arg("dir"), file = lab.arg("file");
  std::vector<std::string> paths;
  if (!file.empty()) paths.push_back(file);
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
      if (e.is_regular_file() && la::is_recording(e.path())) paths.push_back(e.path().string());
    if (ec) {
      lab.fail("se-people: the folder " + dir + " does not list: " + ec.message(),
               "next: pass --dir=<a folder of recordings (.mvd or .ged)>");
      return;
    }
    std::sort(paths.begin() + (file.empty() ? 0 : 1), paths.end());
  }
  std::vector<uint8_t> made;
  if (paths.empty() && !bots_match(made, error)) {
    lab.fail("se-people: the bots' match: " + error, "next: run tests/shooters_edge_team_test.cpp");
    return;
  }
  la::StudyMap m;
  if (!la::load_study_map("", paths.empty() ? "" : lab.arg("basedir"), lab.arg("game"),
                          lab.arg("mapname"), m, error)) {
    lab.fail("se-people: " + error, "next: make lab LAB=se-people ARGS=--help; skill: bots");
    return;
  }
  const double every = std::max(0.02, lab.arg_num("every"));
  const auto near = static_cast<float>(lab.arg_num("near"));
  const size_t n = paths.empty() ? 1 : paths.size();
  std::vector<Read> reads(n);
  ge::core::parallel_bands(n, n, [&](size_t b, size_t e) {
    for (size_t i = b; i < e; i++)
      read_one(paths.empty() ? made : la::read_whole(paths[i]), m, every, near,
               !lab.arg("samples").empty(), reads[i]);
  });
  if (const std::string to = lab.arg("samples"); !to.empty()) {
    std::string tsv = "recording\tt\tslot\tname\tteam\tx\ty\tz\twet\tunder\tfaced\tspeed\n";
    for (size_t i = 0; i < n; i++) {
      const Read& r = reads[i];
      if (!r.ok || r.other_map) continue;
      for (const Sample& sm : r.samples) {
        // The slot's name and team as the read kept them (by slot; the last that held it).
        std::string name, team;
        for (const Player& p : r.players)
          if (p.slot == sm.slot) {
            name = p.name;
            team = p.team;
          }
        for (const std::string& cell :
             {std::to_string(i), to_fixed(sm.t, 2), std::to_string(sm.slot), name, team,
              to_fixed(sm.at.x, 0), to_fixed(sm.at.y, 0), to_fixed(sm.at.z, 0),
              std::string(sm.wet ? "1" : "0"), std::string(sm.under ? "1" : "0"),
              std::string(sm.faced ? "1" : "0"),
              sm.speed < 0 ? std::string("-") : to_fixed(sm.speed, 0)}) {
          tsv += cell;
          tsv += '\t';
        }
        tsv.back() = '\n';
      }
    }
    if (!gl::write_file(to, {reinterpret_cast<const uint8_t*>(tsv.data()), tsv.size()}))
      lab.fail("se-people: --samples " + to + " was not written",
               "next: pass --samples=<a writable path>");
  }

  gl::LabTable per{
      "each recording: the share of the time alive with an enemy in plain view, one "
      "within --near, and the speed (units/s)",
      {"recording", "seconds", "players", "in view", "faced", "near", "speed"},
      {}};
  Player all;
  std::array<ItemControl, kItemKinds> items{};
  std::map<std::string, int64_t> shots;
  HandRead hand, hop;
  la::KeysRead keys, keys_fight, keys_calm;
  la::BumpsRead bumps;
  int used = 0, skipped = 0;
  double seconds = 0;
  int64_t packs = 0, packs_taken = 0, packs_quick = 0;
  double pack_wait = 0;
  for (size_t i = 0; i < n; i++) {
    const Read& r = reads[i];
    const std::string label =
        paths.empty() ? "bots on the halls" : std::filesystem::path(paths[i]).filename().string();
    if (!r.ok || r.other_map) {
      if (skipped++ < 3)
        lab.note("skipped " + label + ": " + (r.ok ? "the map is " + r.map : r.error));
      continue;
    }
    used++;
    seconds += r.seconds;
    for (int k = 0; k < kItemKinds; k++) {
      items[static_cast<size_t>(k)].samples += r.items[static_cast<size_t>(k)].samples;
      items[static_cast<size_t>(k)].present += r.items[static_cast<size_t>(k)].present;
      items[static_cast<size_t>(k)].taken += r.items[static_cast<size_t>(k)].taken;
      items[static_cast<size_t>(k)].waited += r.items[static_cast<size_t>(k)].waited;
    }
    for (const auto& [name, k] : r.shots) shots[name] += k;
    packs += r.packs;
    packs_taken += r.packs_taken;
    packs_quick += r.packs_quick;
    pack_wait += r.pack_wait;
    add(hand, r.hand);
    add(hop, r.hop);
    la::add(keys, r.keys);
    la::add(keys_fight, r.keys_fight);
    la::add(keys_calm, r.keys_calm);
    la::add(bumps, r.bumps);
    Player sum;
    for (const Player& p : r.players) {
      sum.alive += p.alive;
      sum.in_view += p.in_view;
      sum.in_cone += p.in_cone;
      sum.rl += p.rl;
      sum.rl_loaded += p.rl_loaded;
      sum.near += p.near;
      sum.speed += p.speed;
      sum.moved += p.moved;
      sum.turned_n += p.turned_n;
      sum.still += p.still;
      sum.fast += p.fast;
      sum.turn += p.turn;
      sum.fired_in_view += p.fired_in_view;
      all.calm_n += p.calm_n;
      all.calm_turn += p.calm_turn;
      for (int b = 0; b < 4; b++) all.calm_off[b] += p.calm_off[b];
      all.wet += p.wet;
      all.wet_faced += p.wet_faced;
      all.fight_n += p.fight_n;
      all.fight_wide += p.fight_wide;
      all.fight_turns += p.fight_turns;
      all.fight_across += p.fight_across;
      all.fight_along += p.fight_along;
      for (int b = 0; b < 3; b++) {
        all.band_n[b] += p.band_n[b];
        all.band_across[b] += p.band_across[b];
        all.band_along[b] += p.band_along[b];
      }
      for (int b = 0; b < 2; b++) {
        all.rl_n[b] += p.rl_n[b];
        all.rl_across[b] += p.rl_across[b];
        all.rl_along[b] += p.rl_along[b];
      }
      for (int w = 0; w < 8; w++) {
        all.owned[w] += p.owned[w];
      }
      all.health += p.health;
      all.armour += p.armour;
    }
    all.alive += sum.alive;
    all.in_view += sum.in_view;
    all.in_cone += sum.in_cone;
    all.rl += sum.rl;
    all.rl_loaded += sum.rl_loaded;
    all.near += sum.near;
    all.speed += sum.speed;
    all.turned_n += sum.turned_n;
    all.still += sum.still;
    all.fast += sum.fast;
    all.turn += sum.turn;
    all.fired_in_view += sum.fired_in_view;
    all.moved += sum.moved;
    const auto share = [&](int64_t k) {
      return to_fixed(sum.alive > 0 ? static_cast<double>(k) / static_cast<double>(sum.alive) : 0,
                      3);
    };
    if (per.rows.size() < 40)
      per.rows.push_back(
          {label, to_fixed(r.seconds, 0), std::to_string(r.players.size()), share(sum.in_view),
           share(sum.in_cone), share(sum.near),
           to_fixed(sum.moved > 0 ? sum.speed / static_cast<double>(sum.moved) : 0, 0)});
  }
  lab.table(per);
  const auto of = [&](int64_t k) {
    return all.alive > 0 ? static_cast<double>(k) / static_cast<double>(all.alive) : 0;
  };
  lab.metric("people.recordings", used, "count");
  lab.metric("people.skipped", skipped, "count");
  lab.metric("people.seconds", seconds, "s");
  lab.metric("people.in-view", of(all.in_view), "share");
  lab.metric("people.faced", of(all.in_cone), "share");
  lab.metric("people.near", of(all.near), "share");
  lab.metric("people.rl-had", of(all.rl), "share");
  lab.metric("people.rl-loaded", of(all.rl_loaded), "share");
  for (int w = 0; w < 8; w++) {
    lab.metric(std::string("people.owned.") + kArmNames[w], of(all.owned[w]), "share");
  }
  // The weapon sounds a player-minute alive, by the sound (a weapon's own: its firing).
  for (const auto& [name, k] : shots)
    if (all.alive > 0)
      lab.metric("people.shots-per-min." + name,
                 static_cast<double>(k) * 60 / (static_cast<double>(all.alive) * every), "per-min");
  lab.metric("people.health", all.alive > 0 ? all.health / static_cast<double>(all.alive) : 0,
             "hp");
  lab.metric("people.armour", all.alive > 0 ? all.armour / static_cast<double>(all.alive) : 0,
             "points");
  lab.metric("people.speed", all.moved > 0 ? all.speed / static_cast<double>(all.moved) : 0,
             "units/s");
  const auto per_turn = [&](double k) {
    return all.turned_n > 0 ? k / static_cast<double>(all.turned_n) : 0;
  };
  lab.metric("people.view-still", per_turn(static_cast<double>(all.still)), "share");
  lab.metric("people.view-fast", per_turn(static_cast<double>(all.fast)), "share");
  lab.metric("people.view-turn", per_turn(all.turn), "degrees/s");
  lab.metric("people.fire-in-view",
             all.in_view > 0
                 ? static_cast<double>(all.fired_in_view) / static_cast<double>(all.in_view)
                 : 0,
             "share");
  // On the move with nobody in view: how far the view lies off the way he moves, and its turn.
  const auto of_calm = [&](double k) {
    return all.calm_n > 0 ? k / static_cast<double>(all.calm_n) : 0;
  };
  const char* const kCalmNames[4] = {"under-15", "15-45", "45-90", "over-90"};
  for (int b = 0; b < 4; b++)
    lab.metric(std::string("people.moving-view-off.") + kCalmNames[b],
               of_calm(static_cast<double>(all.calm_off[b])), "share");
  lab.metric("people.moving-view-turn", of_calm(all.calm_turn), "degrees/s");
  // The fight's feet: across the line to the enemy faced, along it, and the side's turns a second.
  const auto of_fight = [&](double k) {
    return all.fight_n > 0 ? k / static_cast<double>(all.fight_n) : 0;
  };
  lab.metric("people.wet", of(all.wet), "share");
  lab.metric(
      "people.faced-wet",
      all.in_cone > 0 ? static_cast<double>(all.wet_faced) / static_cast<double>(all.in_cone) : 0,
      "share");
  lab.metric("people.fight-across", of_fight(all.fight_across), "units/s");
  lab.metric("people.fight-along", of_fight(all.fight_along), "units/s");
  lab.metric("people.fight-wide", of_fight(static_cast<double>(all.fight_wide)), "share");
  // On dry feet, by the distance to him and by a loaded rocket launcher: each part's share of the
  // dry samples, its speed across and along (closing positive).
  const auto per_n = [](double k, int64_t count) {
    return count > 0 ? k / static_cast<double>(count) : 0;
  };
  const int64_t dry_n = all.rl_n[0] + all.rl_n[1];
  double dry_across = 0, dry_along = 0;
  for (int b = 0; b < 2; b++) dry_across += all.rl_across[b], dry_along += all.rl_along[b];
  lab.metric("people.fight.dry.share", of_fight(static_cast<double>(dry_n)), "share");
  lab.metric("people.fight.dry.across", per_n(dry_across, dry_n), "units/s");
  lab.metric("people.fight.dry.along", per_n(dry_along, dry_n), "units/s");
  const char* const kBandNames[3] = {"near", "mid", "far"};
  for (int b = 0; b < 3; b++) {
    const std::string pre = std::string("people.fight.") + kBandNames[b] + ".";
    lab.metric(pre + "share", per_n(static_cast<double>(all.band_n[b]), dry_n), "share");
    lab.metric(pre + "across", per_n(all.band_across[b], all.band_n[b]), "units/s");
    lab.metric(pre + "along", per_n(all.band_along[b], all.band_n[b]), "units/s");
  }
  const char* const kRlNames[2] = {"no-rl", "rl"};
  for (int b = 0; b < 2; b++) {
    const std::string pre = std::string("people.fight.") + kRlNames[b] + ".";
    lab.metric(pre + "share", per_n(static_cast<double>(all.rl_n[b]), dry_n), "share");
    lab.metric(pre + "across", per_n(all.rl_across[b], all.rl_n[b]), "units/s");
    lab.metric(pre + "along", per_n(all.rl_along[b], all.rl_n[b]), "units/s");
  }
  lab.metric("people.fight-side-turns", of_fight(static_cast<double>(all.fight_turns)) / every,
             "per s");
  // Backpacks: dropped a player-minute alive, the share taken, the time one lay until taken, the
  // share taken within 10 s.
  if (all.alive > 0)
    lab.metric("people.packs-per-min",
               static_cast<double>(packs) * 60 / (static_cast<double>(all.alive) * every),
               "per-min");
  lab.metric("people.packs-taken",
             packs > 0 ? static_cast<double>(packs_taken) / static_cast<double>(packs) : 0,
             "share");
  lab.metric("people.pack-wait", packs_taken > 0 ? pack_wait / static_cast<double>(packs_taken) : 0,
             "s");
  lab.metric("people.packs-quick",
             packs > 0 ? static_cast<double>(packs_quick) / static_cast<double>(packs) : 0,
             "share");
  // Item control: the share of the time each key item lay untaken, and how long it lay from showing
  // to being taken (a team that times it takes it as it shows).
  for (int k = 0; k < kItemKinds; k++) {
    const ItemControl& ic = items[static_cast<size_t>(k)];
    if (ic.samples == 0) continue;
    const std::string key = kItemNames[k];
    lab.metric("people.item-lying." + key,
               static_cast<double>(ic.present) / static_cast<double>(ic.samples), "share");
    lab.metric("people.item-wait." + key,
               ic.taken > 0 ? ic.waited / static_cast<double>(ic.taken) : 0, "s");
  }
  // The hand: how the view turns, off every live player's angles at 40 Hz (HandRead); `hop` the
  // same while hopping alone, and the share of the hand's time it is.
  hand_metrics(lab, "people.hand.", hand);
  hand_metrics(lab, "people.hop.", hop);
  lab.metric("people.hop.share", hand.secs > 0 ? hop.secs / hand.secs : 0, "share");
  keys_metrics(lab, "people.keys.", keys);
  keys_metrics(lab, "people.keys.fight.", keys_fight);
  keys_metrics(lab, "people.keys.calm.", keys_calm);
  int64_t fight_reads = 0, all_reads = 0;
  for (size_t i = 0; i < 8; i++) {
    fight_reads += keys_fight.ground[i];
    all_reads += keys.ground[i];
  }
  // The bumps off the body (bumps_read.hpp), a minute of the dry, live time read.
  const double bump_min = bumps.live_s / 60;
  const auto per_min = [&](int64_t k) {
    return bump_min > 0 ? static_cast<double>(k) / bump_min : 0;
  };
  lab.metric("people.bumps.read-min", bump_min, "min");
  lab.metric("people.bumps.wall-per-min", per_min(bumps.wall), "per-min");
  lab.metric("people.bumps.wall-air-per-min", per_min(bumps.wall_air), "per-min");
  lab.metric("people.bumps.player-per-min", per_min(bumps.player), "per-min");
  // and a minute run at a bump's speed (a slower player meets fewer chances)
  lab.metric("people.bumps.fast-share", bumps.live_s > 0 ? bumps.fast_s / bumps.live_s : 0,
             "share");
  lab.metric("people.bumps.wall-per-fast-min",
             bumps.fast_s > 0 ? static_cast<double>(bumps.wall) * 60 / bumps.fast_s : 0, "per-min");
  lab.metric("people.keys.fight-share",
             all_reads > 0 ? static_cast<double>(fight_reads) / static_cast<double>(all_reads) : 0,
             "share");
  if (used == 0)
    lab.fail("se-people: no recording of " + m.name + " read",
             "next: pass --mapname=<the recordings' map> with --basedir, or --dir of that map's");
}

const gl::LabSpec kSpec{
    "se-people",
    gl::LabKind::sim,
    "game:shooters_edge",
    "how players spend a team match, off multiview recordings: an enemy in plain view, one near, "
    "the speed (games/shooters_edge/labs/se_people_lab.cpp)",
    "bots",
    "How much of a team match do players spend with an enemy in plain view, near one, and how "
    "fast do they move: people's recordings and our bots' measured by the same rules?",
    {
        {"dir", "", "a folder of recordings (.mvd or .ged) of one map (people's, or se-team --save's)"},
        {"file", "", "one recording's path (with --dir: read too)"},
        {"basedir", "", "a folder holding the game folder the recordings' map is read from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "e1m2", "with --basedir: the recordings' map"},
        {"every", "0.1", "a sample every this many seconds of the match"},
        {"near", "1000", "an enemy this near is near, units"},
        {"samples", "", "write every sample alive to this .tsv (place, wet, eyes under, speed)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
