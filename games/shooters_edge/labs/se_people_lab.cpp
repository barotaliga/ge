// LAB `se-people`: HOW PLAYERS SPEND A TEAM MATCH, read off multiview recordings (people's or our
// bots'), by the same rules for both, so a bot's numbers are set beside people's measured alike.
// Every `.mvd` in `--dir` (or `--file`) is read frame by frame (client/demo.hpp) and each player
// sampled alive every `--every` seconds of the match:
//
// - IN VIEW: an enemy alive within the bots' sight range (4096 units) with a clear line from the
//   player's eye to the enemy's head or middle through the map's world (hull 0), whichever way the
//   player looks: the share of the time a fight could be had;
// - FACED: one of them on the player's screen as the bots' view is (120 by 88.5 degrees, off the
//   recorded view angles); NEAR: an enemy alive within `--near` units, seen or not;
// - THE LAUNCHER: had (its item bit), and had with rockets for it;
// - THE ARMS: the share of the time each weapon is owned, the mean health and armour, and the
//   weapon sounds a player-minute alive by the sound (what is fired);
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
// - WET: the share of the time alive with the waist in water, slime or lava, and faced.
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
// - THE GLANCE, THE LAUNCHER, THE LAUNCHER FIGHT, THE FIGHT'S END (`glance.key.*`, `launcher.*`,
//   `blast.*`, `knock.*`, `death.*`, `jumps.*`, `chase.*`): the *_read.hpp of each.
// - A KEY ITEM AS IT SHOWS (`show.<item>.*`: who is near, who takes it, how soon, taken full) and
//   THE ARMOUR WORN by type (`armour.wear.*`): show_read.hpp.
//
// `--samples=<file.tsv>` writes every sample alive as a row (the recording, the clock, the slot,
// the team, the place, wet, the eyes under, the speed, an enemy faced) for a question the
// metrics do not answer: where people and bots are, with items, health and armour. `--events=`
// writes the key items' pickups and takers, the deaths and each player's final frags.
// The map is the demos' own, from `--basedir`'s game folder; demos of another map are skipped.
// Without a demo it records a short match of our bots on the fixture halls (qw_team.hpp) and
// reads that, which is what `make labs-smoke` runs. Deterministic: the digest holds the counts.
//
// ge:summary Lab: people's and bots' matches measured alike: enemy in view, near, speed
// ge:keywords lab se-people people realism yardstick in-view line-of-sight engagement contact
// ge:keywords speed mvd multiview recordings real-matches bots-against-people positioning
// ge:keywords dodge strafe fight-across wet water swimming hand mouse sweep tilt lift jitter
// ge:keywords fight-along closing keys-fight keys-calm fight-feet-by-range bumps wall-bumps
// ge:keywords weapons-owned shots-per-min firepower launcher health armour glance key-spot
// ge:keywords chase fight-speed blast knock-up launcher-fight
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
#include "shooters_edge/labs/areas_read.hpp"
#include "shooters_edge/labs/blast_read.hpp"
#include "shooters_edge/labs/bumps_read.hpp"
#include "shooters_edge/labs/chain_read.hpp"
#include "shooters_edge/labs/chase_read.hpp"
#include "shooters_edge/labs/glance_read.hpp"
#include "shooters_edge/labs/hand_read.hpp"
#include "shooters_edge/labs/keys_read.hpp"
#include "shooters_edge/labs/launcher_read.hpp"
#include "shooters_edge/labs/launcher_use_report.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/show_read.hpp"
#include "shooters_edge/labs/show_report.hpp"
#include "shooters_edge/net/qw_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr float kSight = 4096;       // the bots' sight range (shooterbots/senses.hpp)
constexpr double kFightAfter = 0.5;  // a key read this long past the last enemy seen is a fight's
constexpr float kEye = 22;           // the eye over the origin
constexpr float kFastest = 1000;
constexpr float kFov = 120;                // the bots' view across, degrees (bots/fighter.cpp)
constexpr float kFovY = 88.5f;             // and its height
constexpr int32_t kLauncher = 32;          // the rocket launcher's item bit
constexpr int32_t kQuadBit = 4194304;      // the quad's item bit
constexpr double kFired_s = 0.3;           // a shot this near a sample counts as firing then
constexpr float kRlBands[2] = {192, 400};  // se-team's `team.finger.<band>` bands, units
constexpr double kRecent_s = 1.5;
constexpr double kSpellGap_s = 1;  // a launcher spell broken by a gap this long (a death)
constexpr float kSpellSpot = 160;  // begun this near a launcher's spot: taken there, units
constexpr int kRlKind = 6;         // kItemNames' "rl"
constexpr float kBlindHurt = 150, kBlindRoom = 400;  // a blind rocket's burst: enemy near, units
constexpr float kBlindFar =
    600;  // and a long shot, units  // a rocket this soon after an enemy left plain view is at him

struct Player {
  std::string name, team;
  int slot = 0;
  int64_t alive = 0, in_view = 0, in_cone = 0, near = 0, moved = 0, rl = 0, rl_loaded = 0;
  // The view: samples after a live one, those turning under 10 and over 360 degrees a second, the
  // turn summed; and of the samples with an enemy in plain view, those he fired within kFired_s.
  int64_t turned_n = 0, still = 0, fast = 0, fired_in_view = 0;
  // (M18r.md 18b) The launcher's rockets (its sound) by the shooter's sight then: an enemy in plain
  // view, one gone from it within kRecent_s, none (fired blind: at a door, a spawn, a sound); and
  // the samples with the launcher loaded, those with an enemy in plain view.
  int64_t rl_shots_view = 0, rl_shots_recent = 0, rl_shots_blind = 0;
  int64_t rl_up_n = 0, rl_up_view = 0;
  // In plain view, by the nearest enemy's range (kRlBands): the rockets, the loaded samples.
  int64_t rl_shots_band[3] = {}, rl_view_band[3] = {};
  // The rockets after losing him by how long since he was in plain view: 0.4, 0.8, kRecent_s s.
  int64_t rl_recent_age[3] = {};
  // The blind rockets: read, an enemy within the burst's reach of where the crosshair meets the
  // world and within a room's, a key item's spot within a room's, fired kBlindFar or further.
  int64_t rl_blind_n = 0, rl_blind_hurt = 0, rl_blind_room = 0, rl_blind_spot = 0, rl_blind_far = 0;
  int64_t rl_blind_open = 0;  // and of those in a room's reach, one the burst sees
  double speed = 0, turn = 0;
  // Moving (over kCalmSpeed) with no enemy in plain view: the view's yaw off the way he moves, in
  // the bands kCalmBands (degrees), and its turn, summed.
  int64_t calm_n = 0, calm_off[4] = {};
  double calm_turn = 0;
  // With an enemy faced, between two samples alive: the speed across the line to him and along it
  // (closing positive), summed; the samples over kAcross across; the side's turns (each side held
  // past kSideSpeed).
  int64_t fight_n = 0, fight_wide = 0, fight_turns = 0;
  double fight_speed = 0;  // and the speed itself, summed over those samples
  // Alive with the waist under water, slime or lava (a swimmer), and of those, with an enemy faced.
  int64_t wet = 0, wet_faced = 0;
  // AN ENEMY APPEARING: one who comes into a clear line (none the sample before, alive both), and
  // of those, met on the screen, beside it (within 120 degrees of the view's yaw), or behind: where
  // he looks is where enemies come from, or not.
  int64_t appear = 0, appear_faced = 0, appear_beside = 0;
  // and those met moving (past 150 units/s), by where he lies against the run: ahead (within 60
  // degrees of it), beside, behind (more than 120 off), each with those on the screen.
  int64_t run_appear[3] = {}, run_faced[3] = {};
  double fight_across = 0, fight_along = 0;
  // The same on dry feet (out of the water: a swimmer's feet are the water's), by his distance to
  // the enemy faced (kFightBands: near, mid, far) and by whether he holds a loaded rocket launcher
  // (0 without, 1 with): samples, across and along summed.
  int64_t band_n[3] = {}, rl_n[2] = {};
  double band_across[3] = {}, band_along[3] = {}, rl_across[2] = {}, rl_along[2] = {};
  // And by both launchers: his own (loaded) and the one faced's, 2 * mine + his: samples, along.
  int64_t pair_n[4] = {};
  double pair_along[4] = {};
  // The arms: samples with each weapon owned (kArmBits), the health and armour summed.
  int64_t owned[8] = {};
  int64_t held[8] = {};  // and each one up in hand (the active weapon's stat)
  // The lob (grenade launcher) carried, with the launcher and without, and the samples of each.
  int64_t gl_with = 0, gl_without = 0, gl_rl_n = 0, gl_no_rl_n = 0;
  int64_t gl_shots[3] = {};  // its grenades with an enemy in plain view, just lost, none (blind)
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
// A key spot's row name: its item, then where it stands ("ra@2480,-1992,40").
std::string show_key(int kind, ge::shooter::Vec3f at) {
  return std::string(kItemNames[static_cast<size_t>(kind)]) + "@" + to_fixed(at.x, 0) + "," +
         to_fixed(at.y, 0) + "," + to_fixed(at.z, 0);
}
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
  float speed = -1;                           // -1: not moved since a sample alive
  int32_t items = 0, health = 0, armour = 0;  // his stats then (items: the item bits)
};

// An event, for `--events`: a key item taken ('p', `kind` the item kind, `at` the item's spot,
// `slot` its taker), a key item back on its spot ('s', `slot` -1), a player killed ('d', `at`
// where), a player's frags at the end ('f', `value`).
struct Event {
  double t = 0;
  char what = 'p';
  int slot = -1, kind = -1;
  int32_t value = 0;
  sh::Vec3f at;
};

constexpr double kHopSpeed = 340;  // a player this fast across is hopping (past a run's 320), u/s

struct Read {
  bool ok = false, other_map = false;
  std::vector<Sample> samples;  // with --samples only
  std::vector<Event> events;    // with --events only
  std::string error, map;
  double seconds = 0;
  std::vector<Player> players;  // by slot, those who played
  std::array<ItemControl, kItemKinds> items{};
  std::array<la::AreaCount, kItemKinds> areas{};  // the key items' areas (areas_read.hpp)
  std::vector<la::AreaSpot> area_spots;           // and each spot's own
  std::map<std::string, int64_t> shots;           // the players' weapon sounds, by the sound's name
  int64_t rl_sounds = 0, rl_sounds_held = 0;      // the launcher's, and with its stat up then
  // THE LAUNCHER CARRIED, by spell (M18r.md 18b): spells begun at a launcher's spot (0) or out of
  // a backpack (1), and their seconds summed (from the first sample carrying it to the last).
  int64_t rl_spells[2] = {};
  double rl_spell_s[2] = {};
  la::HandRead hand;
  la::HandRead hop;                    // the hand while hopping alone (past kHopSpeed across)
  la::HandRead hand_fight, hand_calm;  // the hand split as the keys are (kFightAfter)
  la::KeysRead keys;                   // the movement keys read off the body (keys_read.hpp)
  // The same reads split by whether an enemy was in his line of sight at the last sample (within
  // kFightAfter of it): the fight's footwork against the rest.
  la::KeysRead keys_fight, keys_calm;
  la::BumpsRead bumps;        // the wall and player bumps read off the body (bumps_read.hpp)
  la::GlanceRead glance;      // the glances at key item spots passed (glance_read.hpp)
  la::LauncherRead launcher;  // what a team does after taking a launcher (launcher_read.hpp)
  la::ChaseRead chase;        // a fight's end with the enemy gone from sight (chase_read.hpp)
  la::BlastRead blast;        // the launcher fight: bursts, knock-ups, last blows (blast_read.hpp)
  std::array<la::ShowRead, kItemKinds> show{};     // each key item as it shows (show_read.hpp)
  std::map<std::string, la::ShowRead> show_spots;  // and by its spot ("x,y,z"), its kind first
  la::ChainRead chain;                             // a player's round of them (chain_read.hpp)
  std::array<la::FightRead, kItemKinds> fight{};   // the fight at each showing (show_read.hpp)
  la::ArmourWear wear;                             // the armour worn by type (show_read.hpp)
  // Backpacks: how many fell, how many went before they ran out (taken), the time each lay
  // until taken summed, and those taken within 10 s.
  int64_t packs = 0, packs_taken = 0, packs_quick = 0;
  int64_t deaths = 0;  // live players seen dead at the next sample
  double pack_wait = 0;
};

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
              bool keep, bool keep_events, Read& out) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, out.error)) return;
  const sh::Hull& world = m.bsp.models[0].hulls[0];
  // The bots' screen (shooterbots/senses.hpp `sees_player`): each half angle as a cosine and sine.
  const sh::Vec3f wide = sh::angle_vectors({0, kFov * 0.5f, 0}).forward;
  const sh::Vec3f high = sh::angle_vectors({0, kFovY * 0.5f, 0}).forward;
  std::vector<Player> by(32);
  std::vector<sh::Vec3f> was(32);
  std::vector<uint8_t> was_alive(32, 0);
  std::vector<float> was_yaw(32, 0);
  std::vector<int8_t> was_side(32, 0);     // the side he last moved across his faced enemy's line
  std::vector<double> fired_at(32, -1e9);  // each player's last weapon sound
  std::vector<int64_t> rl_new(32, 0);      // and his launcher's rockets since his last sample
  // His carrying spell: begun when, from where (0 a spot, 1 a pack), carried at the last sample.
  std::vector<double> spell_t0(32, -1), spell_last(32, -1);
  std::vector<int> spell_src(32, 0);
  const auto spell_close = [&](size_t k) {
    if (spell_t0[k] < 0) return;
    out.rl_spells[spell_src[k]]++;
    out.rl_spell_s[spell_src[k]] += spell_last[k] - spell_t0[k];
    spell_t0[k] = -1;
  };
  std::vector<int64_t> gl_new(32, 0);  // and his lob's grenades
  // Each key item's entity: its kind, its model, whether it was there at the last sample, since
  // when.
  struct Spot {
    int kind = -1, model = 0;
    sh::Vec3f at;  // where it lies
    bool there = false;
    double since = -1;
    int area = -1;  // its area (index into `areas`)
  };
  std::vector<Spot> spots;
  std::vector<la::AreaSpot> areas;  // each key item's area, as its spot is first seen
  std::vector<la::AreaPlayer> live_now;
  std::vector<int> taken_now;                 // the areas whose item went since the last sample
  std::vector<int> shown_now;                 // and those whose item showed again
  std::vector<la::LauncherSpot> lspots;       // by area: a launcher spot's last take
  std::vector<la::LauncherPlayer> lplayers;   // the live players this sample (launcher_read.hpp)
  std::vector<uint8_t> had_rl(32, 0);         // by slot: a launcher held at the last sample
  std::vector<la::ChainPlayer> chain_at(32);  // by slot: its last key take this life
  std::vector<la::ShowSpot> sspots;           // by area: a key spot as it shows (show_read.hpp)
  std::vector<la::ShowPlayer> splayers;       // the live players this sample
  std::vector<int> shown_key;                 // the areas whose key item showed again
  std::vector<int32_t> was_armour(32, 0);     // by slot: the armour worn at the last sample
  std::vector<int32_t> was_frags(32, 0);      // by slot: the frags at the last sample
  la::FightLog flog;                          // the fight at a showing, joined at the end
  std::vector<la::SpotTakes> spot_takes;      // by area: its takes by slot (show_read.hpp)
  std::vector<uint8_t> area_alive(32, 0);     // by slot: alive at the last sample
  std::vector<la::AreaPlayer> area_last(32);  // and where, of which team
  double area_at = -1;
  std::vector<double> pack_since;  // by entity: the backpack lying there since (-1: none)
  double next = 0, t0 = -1;
  std::vector<la::HandState> hands(32), hops(32), fights(32), calms(32);
  std::vector<sh::Vec3f> hand_was(32);
  double hand_next = 0, hand_at = -1;
  std::vector<la::KeysState> keys(32);
  std::vector<la::BumpState> bumps(32);
  std::vector<double> seen_at(32, -1e9);  // by slot: the last sample with an enemy in view
  // The fights' ends (chase_read.hpp): each pair's state, this sample's clear lines and cones, the
  // players as the read wants them, the team names numbered.
  std::vector<la::ChasePair> chase_pairs(size_t{32} * 32);
  // The launcher fight (blast_read.hpp), read every frame.
  std::vector<la::BlastTrack> blast_tracks(32);
  std::vector<la::BlastPlayer> blast_players(32);
  std::vector<la::BlastEffect> blast_effects;
  std::vector<uint8_t> clear_now(size_t{32} * 32, 0), cone_now(size_t{32} * 32, 0);
  std::vector<la::ChasePlayer> chase_players(32);
  std::vector<std::string> team_names;
  std::vector<uint8_t> clear_was(size_t{32} * 32,
                                 0);  // by pair (i, j): j in a clear line of i last sample
  const sh::Hull& body = m.bsp.models[0].hulls[1];
  // The glances (glance_read.hpp): the key spots' entities as found, each player's pass by each,
  // his own tally, and his origin over the last kRunTicks ticks (his run's way).
  constexpr int kRunTicks = 4;
  std::vector<size_t> key_ents;
  std::vector<std::vector<la::GlancePass>> passes(32);
  std::vector<std::array<int64_t, 2>> glance_mine(32, {0, 0});
  std::vector<std::array<sh::Vec3f, kRunTicks>> run_ring(32);
  std::vector<int> run_n(32, 0);
  for (;;) {
    const cl::DemoRead rd = cl::next_frame(d, out.error);
    if (rd == cl::DemoRead::error) return;
    if (rd == cl::DemoRead::end) {
      for (size_t k = 0; k < 32; k++) spell_close(k);
      break;
    }
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
        if (snd.sound > 0 && static_cast<size_t>(snd.sound) < s.sounds.size()) {
          const std::string& name = s.sounds[static_cast<size_t>(snd.sound)];
          out.shots[name]++;
          // The active weapon's stat checked against the launcher's own sound: held when fired?
          if (name == "weapons/sgun1.wav" &&
              static_cast<size_t>(snd.entity - 1) < d.player_stats.size()) {
            out.rl_sounds++;
            rl_new[static_cast<size_t>(snd.entity - 1)]++;
          }
          if (name == "weapons/grenade.wav") {
            gl_new[static_cast<size_t>(snd.entity - 1)]++;
            out.rl_sounds_held +=
                d.player_stats[static_cast<size_t>(snd.entity - 1)][cl::kStatActiveWeapon] == 32;
          }
        }
      }
    if (t0 < 0) t0 = s.time;
    // The launcher fight (blast_read.hpp): this frame's bursts, beams and pellets against every
    // live player, on the ground or not.
    blast_effects.clear();
    for (const cl::DemoEffect& e : s.effects) {
      if (e.kind == cl::DemoEffectKind::explosion) blast_effects.push_back({0, e.at});
      if (e.kind == cl::DemoEffectKind::beam) blast_effects.push_back({1, e.end});
      if (e.kind == cl::DemoEffectKind::gunshot) blast_effects.push_back({2, e.at});
    }
    for (int i = 0; i < 32; i++) {
      const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
      la::BlastPlayer& bp = blast_players[static_cast<size_t>(i)];
      bp.sent = p.present && !s.names[static_cast<size_t>(i)].empty();
      bp.live = bp.sent && !p.dead;
      bp.at = p.origin;
      bp.ground = bp.live && sh::point_contents(world, {p.origin.x, p.origin.y, p.origin.z - 26}) ==
                                 sh::kContentsSolid;
      bp.fired = fired_at[static_cast<size_t>(i)];
      bp.frags = s.frags[static_cast<size_t>(i)];
      bp.quad = static_cast<size_t>(i) < d.player_stats.size() &&
                (d.player_stats[static_cast<size_t>(i)][cl::kStatItems] & kQuadBit) != 0;
      bp.fight = s.time - seen_at[static_cast<size_t>(i)] <= 1.0;
    }
    la::blast_frame(blast_tracks, out.blast, s.time, blast_players, blast_effects);
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
      hand_next = s.time + la::kHandEvery;
      for (int i = 0; i < 32; i++) {
        const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
        la::HandState& h = hands[static_cast<size_t>(i)];
        la::HandState& hh = hops[static_cast<size_t>(i)];
        la::HandState& hf = fights[static_cast<size_t>(i)];
        la::HandState& hc = calms[static_cast<size_t>(i)];
        const bool live = p.present && !p.dead && !s.names[static_cast<size_t>(i)].empty();
        const sh::Vec3f was_at = hand_was[static_cast<size_t>(i)];
        hand_was[static_cast<size_t>(i)] = p.origin;
        auto& mine = glance_mine[static_cast<size_t>(i)];
        auto& pass = passes[static_cast<size_t>(i)];
        if (!live) {
          for (size_t k = 0; k < pass.size(); k++)
            la::glance_end(pass[k], out.glance, spots[key_ents[k]].kind, spots[key_ents[k]].at,
                           mine);
          run_n[static_cast<size_t>(i)] = 0;
        } else if (dt > 0) {
          // The glances: every key spot against his eye and view this tick.
          out.glance.live_s += dt;
          auto& ring = run_ring[static_cast<size_t>(i)];
          int& rn = run_n[static_cast<size_t>(i)];
          // (the oldest of the ring: kRunTicks back once full, the first tick alive until then)
          const sh::Vec3f back = rn >= kRunTicks ? ring[static_cast<size_t>(rn % kRunTicks)]
                                 : rn > 0        ? ring[0]
                                                 : p.origin;
          ring[static_cast<size_t>(rn % kRunTicks)] = p.origin;
          rn++;
          const sh::Vec3f run{p.origin.x - back.x, p.origin.y - back.y, 0};
          const sh::Vec3f eye{p.origin.x, p.origin.y, p.origin.z + kEye};
          const sh::ViewAxes ax = sh::angle_vectors(p.angles);
          const bool fight = s.time - seen_at[static_cast<size_t>(i)] <= every + kFightAfter;
          pass.resize(key_ents.size());
          for (size_t k = 0; k < key_ents.size(); k++) {
            const Spot& sp = spots[key_ents[k]];
            const sh::Vec3f pt{sp.at.x, sp.at.y, sp.at.z + la::kGlanceUp};
            const sh::Vec3f to = pt - eye;
            const float dist = std::sqrt(sh::dot(to, to));
            const float reach = pass[k].on ? la::kGlanceFar : la::kGlanceNear;
            const bool line = dist <= reach && clear(world, eye, pt);
            if (!line && !pass[k].on) continue;
            const float ahead = sh::dot(ax.forward, to);
            const bool screen = ahead > 0 &&
                                std::fabs(sh::dot(ax.right, to)) * wide.x <= ahead * wide.y &&
                                std::fabs(sh::dot(ax.up, to)) * high.x <= ahead * high.y;
            la::glance_tick(pass[k], out.glance, sp.kind, sp.at, mine, s.time, p.origin, eye,
                            ax.forward, pt, screen, run, line, dist, fight, sp.there);
          }
        }
        if (!live || dt <= 0 || dt > 4 * la::kHandEvery) {
          h.live = hh.live = hf.live = hc.live = false;
          if (live) la::hand_tick(h, out.hand, p.angles.y, p.angles.x, la::kHandEvery);
          continue;
        }
        la::hand_tick(h, out.hand, p.angles.y, p.angles.x, dt);
        // And by a fight (an enemy in his sight within kFightAfter) against the rest.
        const bool fighting = s.time - seen_at[static_cast<size_t>(i)] <= every + kFightAfter;
        la::hand_tick(fighting ? hf : hc, fighting ? out.hand_fight : out.hand_calm, p.angles.y,
                      p.angles.x, dt);
        (fighting ? hc : hf).live = false;
        // Hopping (fast across, as only a hop or a blast moves a player): its own read.
        const double ax = p.origin.x - was_at.x, ay = p.origin.y - was_at.y;
        if (std::sqrt(ax * ax + ay * ay) / dt > kHopSpeed)
          la::hand_tick(hh, out.hop, p.angles.y, p.angles.x, dt);
        else
          hh.live = false;
      }
    }
    if (s.time < next) continue;
    next = s.time + every;
    std::fill(clear_now.begin(), clear_now.end(), uint8_t{0});
    std::fill(cone_now.begin(), cone_now.end(), uint8_t{0});
    for (la::ChasePlayer& cp : chase_players) cp.live = false;
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
        sp.at = en.origin;
        if (sp.kind >= 1) key_ents.push_back(e);  // a key spot to glance at: all but the green
        if (sp.kind >= 0) {
          sp.area = static_cast<int>(areas.size());
          areas.push_back({});
          areas.back().kind = sp.kind;
          areas.back().at = en.origin;
          // The powerups' windows: the quad from 15 s before it is back (60 s), the pent 30 (300).
          if (sp.kind == 4) areas.back().respawn_s = 60, areas.back().lead_s = 15;
          if (sp.kind == 5) areas.back().respawn_s = 300, areas.back().lead_s = 30;
        }
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
      if (there && !sp.there && sp.area >= 0 && sp.kind == 6) shown_now.push_back(sp.area);
      if (there && !sp.there && sp.area >= 0) shown_key.push_back(sp.area);
      if (!there && sp.there && sp.area >= 0) {
        taken_now.push_back(sp.area);
        areas[static_cast<size_t>(sp.area)].taken_at = s.time;
      }
      if (sp.area >= 0) areas[static_cast<size_t>(sp.area)].present = there;
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
    // The key areas: who holds each by its pickups, a holder killed there, how closely
    // (areas_read.hpp).
    live_now.clear();
    lplayers.clear();
    splayers.clear();
    for (int i = 0; i < 32; i++) {
      const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
      const bool live = p.present && !p.dead && !s.names[static_cast<size_t>(i)].empty();
      const auto k = static_cast<size_t>(i);
      if (live) {
        area_last[k] = {p.origin, team_of(s, i)};
        live_now.push_back(area_last[k]);
        const bool has =
            k < d.player_stats.size() && (d.player_stats[k][cl::kStatItems] & kLauncher) != 0;
        lplayers.push_back({i, area_last[k].team, p.origin, has});
        splayers.push_back({i, area_last[k].team, p.origin});
      } else if (area_alive[k] && p.present && p.dead) {
        la::areas_death(areas, out.areas, area_last[k].team, area_last[k].at);
        out.deaths++;
        flog.deaths.push_back(
            {s.time, area_last[k].at, {i, area_last[k].team, had_rl[k] != 0, was_armour[k]}});
        la::chain_death(chain_at[k]);
        if (keep_events) out.events.push_back({s.time - t0, 'd', i, -1, 0, area_last[k].at});
      }
      area_alive[k] = live ? 1 : 0;
      if (p.present && !s.names[k].empty() && s.frags[k] > was_frags[k])
        flog.frags.push_back({s.time, {i, team_of(s, i), had_rl[k] != 0, was_armour[k]}});
      was_frags[k] = s.frags[k];
    }
    if (lspots.size() < areas.size()) lspots.resize(areas.size());
    if (sspots.size() < areas.size()) sspots.resize(areas.size());
    for (const int a : taken_now) {
      la::area_pickup(areas[static_cast<size_t>(a)], out.areas, live_now);
      // Its taker by the same rule (areas_read.hpp: the nearest live player within kTakerNear).
      const la::AreaSpot& sp = areas[static_cast<size_t>(a)];
      int taker = -1;
      float best = la::kTakerNear;
      for (int i = 0; i < 32; i++) {
        const cl::DemoEntity& p = s.players[static_cast<size_t>(i)];
        if (!p.present || p.dead || s.names[static_cast<size_t>(i)].empty()) continue;
        const float dd = la::area_dist(p.origin, sp.at);
        if (dd < best) best = dd, taker = i;
      }
      if (sp.kind == 6 && taker >= 0) {
        la::LauncherSpot& ls = lspots[static_cast<size_t>(a)];
        ls.at = sp.at;
        for (const la::LauncherPlayer& lp : lplayers)
          if (lp.slot == taker)
            la::launcher_take(ls, out.launcher, s.time, lp,
                              had_rl[static_cast<size_t>(taker)] != 0);
      }
      if (taker >= 0) {
        // The key item as it shows (show_read.hpp): taken full is over 100 armour for an armour,
        // a launcher held for the launcher, at the sample before.
        la::ShowSpot& ss = sspots[static_cast<size_t>(a)];
        ss.kind = sp.kind;
        ss.at = sp.at;
        const auto tk = static_cast<size_t>(taker);
        const bool full = sp.kind <= 2 ? was_armour[tk] > 100 : sp.kind == 6 && had_rl[tk] != 0;
        for (const la::ShowPlayer& sp2 : splayers)
          if (sp2.slot == taker) {
            la::fight_taken(flog, a, s.time, sp2.team);
            if (spot_takes.size() <= static_cast<size_t>(a)) spot_takes.resize(areas.size());
            la::spot_take(spot_takes[static_cast<size_t>(a)], sp2);
            const bool rl = had_rl[tk] != 0;
            la::show_taken_spot(ss, out.show_spots[show_key(sp.kind, sp.at)], s.time, sp2, full,
                                rl);
            la::show_taken(ss, out.show[static_cast<size_t>(sp.kind)], s.time, sp2, full, rl);
          }
      }
      if (taker >= 0)
        la::chain_take(out.chain, chain_at[static_cast<size_t>(taker)], s.time,
                       show_key(sp.kind, sp.at));
      if (keep_events) out.events.push_back({s.time - t0, 'p', taker, sp.kind, 0, sp.at});
    }
    taken_now.clear();
    for (const int a : shown_now)
      la::launcher_shown(lspots[static_cast<size_t>(a)], out.launcher, s.time, lplayers);
    shown_now.clear();
    for (const int a : shown_key) {
      la::ShowSpot& ss = sspots[static_cast<size_t>(a)];
      if (ss.kind >= 0) {
        // The spot's own read takes the showing too: show_shown sets the spot's state once.
        la::ShowSpot copy = ss;
        if (ss.slot >= 0 && ss.shown < 0) {
          la::FightShow fs{a, ss.kind, ss.at, s.time, ss.team, -1, {}};
          for (const la::LauncherPlayer& lp : lplayers) {
            const auto k = static_cast<size_t>(lp.slot);
            const bool strong = lp.has && k < d.player_stats.size() &&
                                d.player_stats[k][cl::kStatArmour] >= la::kStrongArmour;
            if (!strong || la::show_dist(lp.at, ss.at) > la::kShowNear) continue;
            fs.strong = true;
            fs.strong_holder = fs.strong_holder || lp.team == ss.team;
          }
          flog.shows.push_back(fs);
        }
        la::show_shown(ss, out.show[static_cast<size_t>(ss.kind)], s.time, splayers);
        la::show_shown(copy, out.show_spots[show_key(ss.kind, ss.at)], s.time, splayers);
      }
      if (keep_events && ss.kind >= 0)
        out.events.push_back({s.time - t0, 's', -1, ss.kind, 0, ss.at});
    }
    shown_key.clear();
    for (const la::LauncherSpot& ls : lspots)
      la::launcher_sample(ls, out.launcher, s.time, lplayers);
    for (size_t k = 0; k < 32; k++) had_rl[k] = 0, was_armour[k] = 0;
    for (const la::ShowPlayer& sp2 : splayers) {
      const auto k = static_cast<size_t>(sp2.slot);
      if (k < d.player_stats.size()) was_armour[k] = d.player_stats[k][cl::kStatArmour];
    }
    for (const la::LauncherPlayer& lp : lplayers) had_rl[static_cast<size_t>(lp.slot)] = lp.has;
    la::areas_update(areas, out.areas, live_now, s.time, area_at < 0 ? 0.0 : s.time - area_at);
    area_at = s.time;
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
      {
        la::ChasePlayer& cp = chase_players[static_cast<size_t>(i)];
        cp.live = true;
        cp.at = p.origin;
        const auto tn = std::find(team_names.begin(), team_names.end(), pl.team);
        cp.team = static_cast<int>(tn - team_names.begin());
        if (tn == team_names.end()) team_names.push_back(pl.team);
        cp.body = 100;
        cp.rl = false;
        if (static_cast<size_t>(i) < d.player_stats.size()) {
          const auto& st = d.player_stats[static_cast<size_t>(i)];
          cp.body = la::chase_body(st[cl::kStatHealth], st[cl::kStatArmour],
                                   la::armour_type(st[cl::kStatItems], st[cl::kStatArmour]));
          cp.rl = (st[cl::kStatItems] & kLauncher) != 0 && st[cl::kStatRockets] > 0;
        }
      }
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
      // Enemies appearing (above): every enemy's clear line now against the sample before.
      float near_clear = 1e9f;  // and the nearest enemy in plain view, units
      for (int j = 0; j < 32; j++) {
        const cl::DemoEntity& e = s.players[static_cast<size_t>(j)];
        uint8_t& was_clear = clear_was[static_cast<size_t>(i) * 32 + static_cast<size_t>(j)];
        if (j == i || !e.present || e.dead || s.names[static_cast<size_t>(j)].empty() ||
            team_of(s, j) == pl.team) {
          was_clear = 0;
          continue;
        }
        const sh::Vec3f head{e.origin.x, e.origin.y, e.origin.z + kEye};
        const sh::Vec3f h = head - eye;
        const bool now = sh::dot(h, h) <= kSight * kSight &&
                         (clear(world, eye, head) || clear(world, eye, e.origin));
        if (now) {
          near_clear = std::fmin(near_clear, std::sqrt(sh::dot(h, h)));
          const size_t pij = static_cast<size_t>(i) * 32 + static_cast<size_t>(j);
          clear_now[pij] = 1;
          const float ahead = sh::dot(ax.forward, h);
          cone_now[pij] =
              sh::dot(h, h) < 1 ||
              (ahead > 0 && std::fabs(sh::dot(ax.right, h)) * wide.x <= ahead * wide.y &&
               std::fabs(sh::dot(ax.up, h)) * high.x <= ahead * high.y);
        }
        if (now && !was_clear && was_alive[static_cast<size_t>(i)]) {
          pl.appear++;
          const float ahead = sh::dot(ax.forward, h);
          const bool on = ahead > 0 && std::fabs(sh::dot(ax.right, h)) * wide.x <= ahead * wide.y &&
                          std::fabs(sh::dot(ax.up, h)) * high.x <= ahead * high.y;
          const float flat = std::sqrt(h.x * h.x + h.y * h.y);
          const float fx = ax.forward.x, fy = ax.forward.y;
          const float fl = std::sqrt(fx * fx + fy * fy);
          const float c = flat > 1 && fl > 0 ? (fx * h.x + fy * h.y) / (flat * fl) : 1.f;
          pl.appear_faced += on;
          pl.appear_beside += !on && c > -0.5f;
          const sh::Vec3f mv = p.origin - was[static_cast<size_t>(i)];
          const float ms = std::sqrt(mv.x * mv.x + mv.y * mv.y) / static_cast<float>(every);
          if (ms > 150 && flat > 1) {
            const float rc =
                (mv.x * h.x + mv.y * h.y) / (std::sqrt(mv.x * mv.x + mv.y * mv.y) * flat);
            const int band = rc > 0.5f ? 0 : rc > -0.5f ? 1 : 2;
            pl.run_appear[band]++;
            pl.run_faced[band] += on;
          }
        }
        was_clear = now ? 1 : 0;
      }
      pl.in_view += view;
      if (view) seen_at[static_cast<size_t>(i)] = s.time;
      pl.fired_in_view += view && s.time - fired_at[static_cast<size_t>(i)] <= kFired_s;
      {
        const int64_t rk = rl_new[static_cast<size_t>(i)];
        rl_new[static_cast<size_t>(i)] = 0;
        const bool recent = !view && s.time - seen_at[static_cast<size_t>(i)] <= kRecent_s;
        pl.rl_shots_view += view ? rk : 0;
        {
          const int64_t gk = gl_new[static_cast<size_t>(i)];
          gl_new[static_cast<size_t>(i)] = 0;
          pl.gl_shots[view ? 0 : recent ? 1 : 2] += gk;
        }
        if (view && rk > 0)
          pl.rl_shots_band[near_clear < kRlBands[0] ? 0 : near_clear < kRlBands[1] ? 1 : 2] += rk;
        const bool rl_up = static_cast<size_t>(i) < d.player_stats.size() &&
                           (d.player_stats[static_cast<size_t>(i)][cl::kStatItems] & 32) != 0 &&
                           d.player_stats[static_cast<size_t>(i)][cl::kStatRockets] > 0;
        if (view && rl_up)
          pl.rl_view_band[near_clear < kRlBands[0] ? 0 : near_clear < kRlBands[1] ? 1 : 2]++;
        pl.rl_shots_recent += recent ? rk : 0;
        if (!view && !recent && rk > 0) {
          // A blind rocket: where the crosshair meets the world, how far that is, and who (unseen)
          // and what lies near it: an enemy (the burst's reach, a room), a key item's spot.
          const sh::HullTrace tr = sh::trace_hull(world, eye, eye + ax.forward * 3000.f);
          const sh::Vec3f burst = tr.endpos;
          float enemy = 1e9f, spot = 1e9f;
          bool open = false;  // an enemy within a room's reach the burst has a clear line to
          const sh::Vec3f back = burst - ax.forward * 8.f;  // off the wall it bursts on
          for (int j = 0; j < 32; j++) {
            const cl::DemoEntity& e = s.players[static_cast<size_t>(j)];
            if (j == i || !e.present || e.dead || s.names[static_cast<size_t>(j)].empty() ||
                team_of(s, j) == pl.team)
              continue;
            const sh::Vec3f h = e.origin - burst;
            const float dj = std::sqrt(sh::dot(h, h));
            enemy = std::fmin(enemy, dj);
            open = open || (dj <= kBlindRoom && clear(world, back, e.origin));
          }
          for (const Spot& sp : spots) {
            if (sp.kind < 0) continue;
            const sh::Vec3f h = sp.at - burst;
            spot = std::fmin(spot, std::sqrt(sh::dot(h, h)));
          }
          pl.rl_blind_open += open ? rk : 0;
          const sh::Vec3f fly = burst - eye;
          pl.rl_blind_n += rk;
          pl.rl_blind_hurt += enemy <= kBlindHurt ? rk : 0;
          pl.rl_blind_room += enemy <= kBlindRoom ? rk : 0;
          pl.rl_blind_spot += spot <= kBlindRoom ? rk : 0;
          pl.rl_blind_far += std::sqrt(sh::dot(fly, fly)) >= kBlindFar ? rk : 0;
        }
        if (recent && rk > 0) {
          const double gone = s.time - seen_at[static_cast<size_t>(i)];
          pl.rl_recent_age[gone < 0.4 ? 0 : gone < 0.8 ? 1 : 2] += rk;
        }
        pl.rl_shots_blind += !view && !recent ? rk : 0;
      }
      pl.in_cone += cone;
      const bool wet = sh::point_contents(world, p.origin) <= sh::kContentsWater;
      pl.wet += wet;
      pl.wet_faced += wet && cone;
      if (static_cast<size_t>(i) < d.player_stats.size()) {
        const auto& st = d.player_stats[static_cast<size_t>(i)];
        const bool has = (st[cl::kStatItems] & kLauncher) != 0;
        pl.rl += has;
        {
          // The spell: a gap of a second (a death between) closes it; a start at a launcher's
          // spot (within kSpellSpot) is a take, anywhere else a backpack's.
          const auto k = static_cast<size_t>(i);
          if (spell_t0[k] >= 0 && (!has || s.time - spell_last[k] > kSpellGap_s)) spell_close(k);
          if (has && spell_t0[k] < 0) {
            int src = 1;
            for (const Spot& sp : spots)
              if (sp.kind == kRlKind) {
                const sh::Vec3f h = sp.at - p.origin;
                if (sh::dot(h, h) <= kSpellSpot * kSpellSpot) src = 0;
              }
            spell_t0[k] = s.time;
            spell_src[k] = src;
          }
          if (has) spell_last[k] = s.time;
        }
        pl.rl_loaded += has && st[cl::kStatRockets] > 0;
        pl.rl_up_n += has && st[cl::kStatRockets] > 0;
        pl.rl_up_view += has && st[cl::kStatRockets] > 0 && view;
        for (int w = 0; w < 8; w++) {
          pl.owned[w] += (st[cl::kStatItems] & kArmBits[w]) != 0;
          pl.held[w] += st[cl::kStatActiveWeapon] == kArmBits[w];
        }
        {
          const bool rl = (st[cl::kStatItems] & kArmBits[6]) != 0;
          const bool gl = (st[cl::kStatItems] & kArmBits[5]) != 0;
          pl.gl_rl_n += rl;
          pl.gl_no_rl_n += !rl;
          pl.gl_with += rl && gl;
          pl.gl_without += !rl && gl;
        }
        pl.health += st[cl::kStatHealth];
        pl.armour += st[cl::kStatArmour];
        const auto wt =
            static_cast<size_t>(la::armour_type(st[cl::kStatItems], st[cl::kStatArmour]));
        out.wear.n[wt]++;
        out.wear.points[wt] += st[cl::kStatArmour];
        out.wear.strong +=
            (st[cl::kStatItems] & kLauncher) != 0 && st[cl::kStatArmour] >= la::kStrongArmour;
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
        if (static_cast<size_t>(i) < d.player_stats.size()) {
          const auto& st = d.player_stats[static_cast<size_t>(i)];
          sm.items = st[cl::kStatItems];
          sm.health = st[cl::kStatHealth];
          sm.armour = st[cl::kStatArmour];
        }
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
          pl.fight_speed += std::sqrt(vx * vx + vy * vy);
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
            bool his = false;
            if (static_cast<size_t>(faced) < d.player_stats.size()) {
              const auto& hs = d.player_stats[static_cast<size_t>(faced)];
              his = (hs[cl::kStatItems] & kLauncher) != 0 && hs[cl::kStatRockets] > 0;
            }
            const int pk = (rl ? 2 : 0) + (his ? 1 : 0);
            pl.pair_n[pk]++;
            pl.pair_along[pk] += along;
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
    la::chase_sample(chase_pairs, out.chase, s.time, every, chase_players, clear_now, cone_now);
  }
  for (Player& p : by)
    if (p.alive > 0) {
      if (keep_events)
        out.events.push_back(
            {out.seconds, 'f', p.slot, -1, d.state.frags[static_cast<size_t>(p.slot)], {}});
      out.players.push_back(std::move(p));
    }
  for (size_t i = 0; i < 32; i++) {
    for (size_t k = 0; k < passes[i].size(); k++)
      la::glance_end(passes[i][k], out.glance, spots[key_ents[k]].kind, spots[key_ents[k]].at,
                     glance_mine[i]);
    if (glance_mine[i][0] > 0) out.glance.players.push_back(glance_mine[i]);
  }
  la::areas_end(areas, out.areas);
  out.area_spots = std::move(areas);
  la::fight_join(flog, out.fight);
  for (size_t a = 0; a < spot_takes.size() && a < out.area_spots.size(); a++) {
    const la::AreaSpot& sp = out.area_spots[a];
    if (sp.kind < 0 || sp.kind >= kItemKinds) continue;
    la::spot_owners(spot_takes[a], out.show[static_cast<size_t>(sp.kind)]);
    la::spot_owners(spot_takes[a], out.show_spots[show_key(sp.kind, sp.at)]);
  }
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
               !lab.arg("samples").empty(), !lab.arg("events").empty(), reads[i]);
  });
  if (const std::string to = lab.arg("samples"); !to.empty()) {
    std::string tsv =
        "recording\tt\tslot\tname\tteam\tx\ty\tz\twet\tunder\tfaced\tspeed\titems\thealth\tarmour"
        "\n";
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
              sm.speed < 0 ? std::string("-") : to_fixed(sm.speed, 0), std::to_string(sm.items),
              std::to_string(sm.health), std::to_string(sm.armour)}) {
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
  if (const std::string to = lab.arg("events"); !to.empty()) {
    std::string tsv = "recording\tt\tevent\tslot\tname\tteam\tkind\tvalue\tx\ty\tz\n";
    for (size_t i = 0; i < n; i++) {
      const Read& r = reads[i];
      if (!r.ok || r.other_map) continue;
      for (const Event& ev : r.events) {
        std::string name = "-", team = "-";
        for (const Player& p : r.players)
          if (p.slot == ev.slot) {
            name = p.name;
            team = p.team;
          }
        const char* what = ev.what == 'p'   ? "pickup"
                           : ev.what == 's' ? "shown"
                           : ev.what == 'd' ? "death"
                                            : "frags";
        for (const std::string& cell :
             {paths.empty() ? std::string("fixture") : paths[i], to_fixed(ev.t, 2),
              std::string(what), std::to_string(ev.slot), name, team,
              ev.kind < 0 ? std::string("-")
                          : std::string(kItemNames[static_cast<size_t>(ev.kind)]),
              std::to_string(ev.value), to_fixed(ev.at.x, 0), to_fixed(ev.at.y, 0),
              to_fixed(ev.at.z, 0)}) {
          tsv += cell;
          tsv += '\t';
        }
        tsv.back() = '\n';
      }
    }
    if (!gl::write_file(to, {reinterpret_cast<const uint8_t*>(tsv.data()), tsv.size()}))
      lab.fail("se-people: --events " + to + " was not written",
               "next: pass --events=<a writable path>");
  }

  gl::LabTable per{
      "each recording: the share of the time alive with an enemy in plain view, one "
      "within --near, and the speed (units/s)",
      {"recording", "seconds", "players", "in view", "faced", "near", "speed"},
      {}};
  Player all;
  std::array<ItemControl, kItemKinds> items{};
  std::array<la::AreaCount, kItemKinds> areas{};
  std::vector<la::AreaSpot> area_spots;
  std::map<std::string, int64_t> shots;
  int64_t rl_sounds = 0, rl_sounds_held = 0;
  int64_t rl_spells[2] = {};
  double rl_spell_s[2] = {};
  la::HandRead hand, hop, hand_fight, hand_calm;
  la::KeysRead keys, keys_fight, keys_calm;
  la::BumpsRead bumps;
  la::GlanceRead glance;
  la::LauncherRead launcher;
  la::ChaseRead chase;
  la::BlastRead blast;
  std::array<la::ShowRead, kItemKinds> show{};
  std::map<std::string, la::ShowRead> show_spots;
  std::array<la::FightRead, kItemKinds> fight{};
  la::ArmourWear wear;
  la::ChainRead chain;
  int used = 0, skipped = 0;
  double seconds = 0;
  int64_t packs = 0, packs_taken = 0, packs_quick = 0;
  int64_t deaths = 0;  // live players seen dead at the next sample
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
      la::add(areas[static_cast<size_t>(k)], r.areas[static_cast<size_t>(k)]);
    }
    la::areas_merge(area_spots, r.area_spots);
    for (const auto& [name, k] : r.shots) shots[name] += k;
    rl_sounds += r.rl_sounds;
    for (int b = 0; b < 2; b++) {
      rl_spells[b] += r.rl_spells[b];
      rl_spell_s[b] += r.rl_spell_s[b];
    }
    rl_sounds_held += r.rl_sounds_held;
    packs += r.packs;
    deaths += r.deaths;
    packs_taken += r.packs_taken;
    packs_quick += r.packs_quick;
    pack_wait += r.pack_wait;
    la::add(hand, r.hand);
    la::add(hop, r.hop);
    la::add(hand_fight, r.hand_fight);
    la::add(hand_calm, r.hand_calm);
    la::add(keys, r.keys);
    la::add(keys_fight, r.keys_fight);
    la::add(keys_calm, r.keys_calm);
    la::add(bumps, r.bumps);
    la::add(glance, r.glance);
    la::add(launcher, r.launcher);
    la::add(chase, r.chase);
    la::add(blast, r.blast);
    for (size_t k = 0; k < kItemKinds; k++) la::add(show[k], r.show[k]);
    for (const auto& [key, sr] : r.show_spots) la::add(show_spots[key], sr);
    for (size_t k = 0; k < kItemKinds; k++) la::add(fight[k], r.fight[k]);
    la::add(wear, r.wear);
    la::add(chain, r.chain);
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
      all.rl_shots_view += p.rl_shots_view;
      all.rl_shots_recent += p.rl_shots_recent;
      all.rl_shots_blind += p.rl_shots_blind;
      all.rl_up_n += p.rl_up_n;
      all.rl_up_view += p.rl_up_view;
      for (int b = 0; b < 3; b++) {
        all.rl_shots_band[b] += p.rl_shots_band[b];
        all.rl_view_band[b] += p.rl_view_band[b];
        all.rl_recent_age[b] += p.rl_recent_age[b];
      }
      all.rl_blind_n += p.rl_blind_n;
      all.rl_blind_hurt += p.rl_blind_hurt;
      all.rl_blind_room += p.rl_blind_room;
      all.rl_blind_spot += p.rl_blind_spot;
      all.rl_blind_far += p.rl_blind_far;
      all.rl_blind_open += p.rl_blind_open;
      all.gl_with += p.gl_with;
      for (int b = 0; b < 3; b++) all.gl_shots[b] += p.gl_shots[b];
      all.gl_without += p.gl_without;
      all.gl_rl_n += p.gl_rl_n;
      all.gl_no_rl_n += p.gl_no_rl_n;
      all.calm_n += p.calm_n;
      all.calm_turn += p.calm_turn;
      for (int b = 0; b < 4; b++) all.calm_off[b] += p.calm_off[b];
      all.wet += p.wet;
      all.wet_faced += p.wet_faced;
      all.appear += p.appear;
      all.appear_faced += p.appear_faced;
      all.appear_beside += p.appear_beside;
      for (int q = 0; q < 3; q++) {
        all.run_appear[q] += p.run_appear[q];
        all.run_faced[q] += p.run_faced[q];
      }
      all.fight_n += p.fight_n;
      all.fight_speed += p.fight_speed;
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
      for (int b = 0; b < 4; b++) {
        all.pair_n[b] += p.pair_n[b];
        all.pair_along[b] += p.pair_along[b];
      }
      for (int w = 0; w < 8; w++) {
        all.owned[w] += p.owned[w];
        all.held[w] += p.held[w];
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
    lab.metric(std::string("people.held.") + kArmNames[w], of(all.held[w]), "share");
  }
  // (M18r.md 18b) the launcher's use (launcher_use_report.hpp)
  la::report_launcher_use(lab, all, every, shots, rl_sounds, rl_sounds_held, rl_spells, rl_spell_s);
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
  const auto of_appear = [&](int64_t k) {
    return all.appear > 0 ? static_cast<double>(k) / static_cast<double>(all.appear) : 0;
  };
  lab.metric("people.appear-per-min",
             all.alive > 0
                 ? static_cast<double>(all.appear) * 60 / (static_cast<double>(all.alive) * every)
                 : 0,
             "per-min");
  lab.metric("people.appear-faced", of_appear(all.appear_faced), "share");
  lab.metric("people.appear-beside", of_appear(all.appear_beside), "share");
  lab.metric("people.appear-behind", of_appear(all.appear - all.appear_faced - all.appear_beside),
             "share");
  {
    static constexpr const char* kRun[3] = {"ahead", "beside", "behind"};
    const int64_t runs = all.run_appear[0] + all.run_appear[1] + all.run_appear[2];
    for (int q = 0; q < 3; q++) {
      lab.metric(std::string("people.appear-run.") + kRun[q],
                 runs > 0 ? static_cast<double>(all.run_appear[q]) / static_cast<double>(runs) : 0,
                 "share");
      lab.metric(std::string("people.appear-run.") + kRun[q] + ".faced",
                 all.run_appear[q] > 0 ? static_cast<double>(all.run_faced[q]) /
                                             static_cast<double>(all.run_appear[q])
                                       : 0,
                 "share");
    }
  }
  lab.metric("people.fight-across", of_fight(all.fight_across), "units/s");
  lab.metric("people.fight-along", of_fight(all.fight_along), "units/s");
  lab.metric("people.fight-wide", of_fight(static_cast<double>(all.fight_wide)), "share");
  lab.metric("people.fight-speed", of_fight(all.fight_speed), "units/s");
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
  // The closing by both launchers (his own, the one faced's): who presses in on whom.
  const char* const kPairNames[4] = {"none-vs-none", "none-vs-rl", "rl-vs-none", "rl-vs-rl"};
  for (int b = 0; b < 4; b++) {
    const std::string pre = std::string("people.fight.") + kPairNames[b] + ".";
    lab.metric(pre + "share", per_n(static_cast<double>(all.pair_n[b]), dry_n), "share");
    lab.metric(pre + "along", per_n(all.pair_along[b], all.pair_n[b]), "units/s");
  }
  lab.metric("people.fight-side-turns", of_fight(static_cast<double>(all.fight_turns)) / every,
             "per s");
  // Backpacks: dropped a player-minute alive, the share taken, the time one lay until taken, the
  // share taken within 10 s.
  // Deaths a player-minute alive (a live player seen dead at the next sample): the game's pace.
  lab.metric("people.deaths-per-min",
             all.alive > 0
                 ? static_cast<double>(deaths) * 60 / (static_cast<double>(all.alive) * every)
                 : 0,
             "per-min");
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
  // The key areas (areas_read.hpp): held (a team took the item twice in a row), held and covered
  // (a holder within 768), contested (a holder killed there, until taken twice in a row again),
  // unheld; the holder's changes and the holders killed there a minute; the main holder's share of
  // the held time; the enemy nearer than any holder; the holders' and the enemies' nearest by band.
  for (int k = 0; k < kItemKinds; k++) {
    const la::AreaCount& a = areas[static_cast<size_t>(k)];
    if (a.samples == 0) continue;
    const std::string key = std::string("people.area.") + kItemNames[k] + ".";
    const auto share = [&](int64_t v) {
      return static_cast<double>(v) / static_cast<double>(a.samples);
    };
    const auto per_min = [&](int64_t v) {
      return a.seconds > 0 ? 60.0 * static_cast<double>(v) / a.seconds : 0;
    };
    lab.metric(key + "held", share(a.held), "share");
    lab.metric(key + "covered", share(a.covered), "share");
    lab.metric(key + "contested", share(a.contested), "share");
    lab.metric(key + "unheld", share(a.unheld), "share");
    lab.metric(key + "enemy-nearer", share(a.enemy_nearer), "share");
    lab.metric(key + "changes-per-min", per_min(a.changes), "count");
    lab.metric(key + "lost-per-min", per_min(a.lost), "count");
    lab.metric(key + "pickups-per-min", per_min(a.pickups), "count");
    lab.metric(key + "main-holder", a.main_n > 0 ? a.main_held / static_cast<double>(a.main_n) : 0,
               "share");
    int64_t kn = 0, en = 0;
    for (int b = 0; b < la::kAreaBands; b++) {
      kn += a.keeper_near[static_cast<size_t>(b)];
      en += a.enemy_near[static_cast<size_t>(b)];
    }
    for (int b = 0; b < la::kAreaBands; b++) {
      const std::string band = std::to_string(b * static_cast<int>(la::kAreaBand));
      std::string keeper = key;
      keeper += "keeper-near.";
      keeper += band;
      std::string enemy = key;
      enemy += "enemy-near.";
      enemy += band;
      lab.metric(keeper,
                 kn > 0 ? static_cast<double>(a.keeper_near[static_cast<size_t>(b)]) /
                              static_cast<double>(kn)
                        : 0,
                 "share");
      lab.metric(enemy,
                 en > 0 ? static_cast<double>(a.enemy_near[static_cast<size_t>(b)]) /
                              static_cast<double>(en)
                        : 0,
                 "share");
    }
  }
  // Each key area by its spot (a map with two of a kind: which one the teams hold).
  {
    gl::LabTable spots{
        "each key area by its spot: held, held and covered, contested, unheld, a "
        "holder in it (within 256), the holder's changes and holders killed there "
        "a minute, the main holder's share",
        {"item", "spot", "held", "covered", "contested", "unheld", "holder in it", "changes/min",
         "lost/min", "main holder"},
        {}};
    for (const la::AreaSpot& a : area_spots) {
      const la::AreaCount& c = a.count;
      if (c.samples == 0) continue;
      const auto share = [&](int64_t v) {
        return to_fixed(static_cast<double>(v) / static_cast<double>(c.samples), 3);
      };
      const auto per_min = [&](int64_t v) {
        return to_fixed(c.seconds > 0 ? 60.0 * static_cast<double>(v) / c.seconds : 0, 2);
      };
      int64_t kn = 0;
      for (const int64_t v : c.keeper_near) kn += v;
      spots.rows.push_back(
          {kItemNames[a.kind],
           to_fixed(a.at.x, 0) + "," + to_fixed(a.at.y, 0) + "," + to_fixed(a.at.z, 0),
           share(c.held), share(c.covered), share(c.contested), share(c.unheld),
           to_fixed(kn > 0 ? static_cast<double>(c.keeper_near[0]) / static_cast<double>(kn) : 0,
                    3),
           per_min(c.changes), per_min(c.lost),
           to_fixed(c.main_n > 0 ? c.main_held / static_cast<double>(c.main_n) : 0, 3)});
    }
    lab.table(spots);
  }
  // The hand: how the view turns, off every live player's angles at 40 Hz (HandRead); `hop` the
  // same while hopping alone, and the share of the hand's time it is.
  la::hand_metrics(lab, "people.hand.", hand);
  la::hand_metrics(lab, "people.hop.", hop);
  la::hand_metrics(lab, "people.hand.fight.", hand_fight);
  la::hand_metrics(lab, "people.hand.calm.", hand_calm);
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
  // What a team does after taking a launcher (launcher_read.hpp).
  {
    const auto lshare = [](int64_t part, int64_t whole) {
      return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0;
    };
    const la::LauncherRead& l = launcher;
    const int64_t next = l.next_self + l.next_mate_without + l.next_mate_with + l.next_enemy;
    const double per_back = l.back > 0 ? 1.0 / static_cast<double>(l.back) : 0;
    lab.metric("people.launcher.takes-per-min",
               seconds > 0 ? static_cast<double>(l.takes) * 60 / seconds : 0, "per-min");
    lab.metric("people.launcher.with-one", lshare(l.with_one, l.takes), "share");
    lab.metric("people.launcher.stay", lshare(l.stay_in, l.stay_n), "share");
    lab.metric("people.launcher.back.mates-waiting",
               static_cast<double>(l.mates_waiting) * per_back, "count");
    lab.metric("people.launcher.back.mates-any", lshare(l.mates_any, l.back), "share");
    lab.metric("people.launcher.back.mate-on", lshare(l.mates_on, l.back), "share");
    lab.metric("people.launcher.back.enemy-on", lshare(l.enemies_on, l.back), "share");
    lab.metric("people.launcher.back.enemies-waiting",
               static_cast<double>(l.enemies_waiting) * per_back, "count");
    lab.metric("people.launcher.next.self", lshare(l.next_self, next), "share");
    lab.metric("people.launcher.next.mate-without", lshare(l.next_mate_without, next), "share");
    lab.metric("people.launcher.next.mate-with", lshare(l.next_mate_with, next), "share");
    lab.metric("people.launcher.next.enemy", lshare(l.next_enemy, next), "share");
    lab.metric("people.launcher.next.wait",
               l.next_waited > 0 ? l.next_wait / static_cast<double>(l.next_waited) : 0, "s");
    la::LauncherRead w = launcher;
    lab.metric("people.launcher.next.wait-p50", la::keys_quantile(w.waits, 0.5), "s");
    lab.metric("people.launcher.next.wait-p75", la::keys_quantile(w.waits, 0.75), "s");
    lab.metric("people.launcher.next.wait-mate-p50", la::keys_quantile(w.waits_mate, 0.5), "s");
    lab.metric("people.launcher.next.wait-nobody-p50", la::keys_quantile(w.waits_nobody, 0.5), "s");
  }
  // A fight's end with the enemy gone from sight alive: followed or not (chase_read.hpp).
  {
    const auto cshare = [](int64_t part, int64_t whole) {
      return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0;
    };
    const la::ChaseRead& c = chase;
    const int64_t read = c.gone - c.near_already;
    lab.metric("people.chase.fights-per-min",
               seconds > 0 ? static_cast<double>(c.fights) * 60 / seconds : 0, "per-min");
    lab.metric("people.chase.gone", cshare(c.gone, c.fights), "share");
    lab.metric("people.chase.followed", cshare(c.followed, read), "share");
    lab.metric("people.chase.followed-high", cshare(c.by_followed[0], c.by_n[0]), "share");
    lab.metric("people.chase.followed-low", cshare(c.by_followed[1], c.by_n[1]), "share");
    lab.metric("people.chase.followed-his-rl", cshare(c.by_followed[2], c.by_n[2]), "share");
    lab.metric("people.chase.followed-even", cshare(c.by_followed[3], c.by_n[3]), "share");
    lab.metric("people.chase.low-share", cshare(c.by_n[1], read), "share");
    lab.metric("people.chase.his-rl-share", cshare(c.by_n[2], read), "share");
    lab.metric("people.chase.seen-again", cshare(c.seen_again, read), "share");
    lab.metric("people.chase.kill-followed", cshare(c.kill_followed, c.followed), "share");
    lab.metric("people.chase.kill-left", cshare(c.kill_left, read - c.followed), "share");
    lab.metric("people.chase.fight-faced", c.gone > 0 ? c.faced_s / static_cast<double>(c.gone) : 0,
               "s");
  }
  // The launcher fight (blast_read.hpp): where the bursts land, the knock-ups and what follows,
  // each death's last blow, the jumps.
  {
    const la::BlastRead& b = blast;
    const auto bshare = [](int64_t part, int64_t whole) {
      return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0;
    };
    int64_t put = 0;
    for (int k = 0; k < la::kBlastKinds; k++) put += b.by_kind[k];
    const double mins = b.alive_s / 60;
    lab.metric("people.blast.bursts-per-min", mins > 0 ? static_cast<double>(put) / mins : 0,
               "per-player-min");
    lab.metric("people.blast.body", bshare(b.by_kind[la::kBlastBodyHit], put), "share");
    lab.metric("people.blast.feet", bshare(b.by_kind[la::kBlastFeet], put), "share");
    lab.metric("people.blast.air", bshare(b.by_kind[la::kBlastAir], put), "share");
    lab.metric("people.blast.near", bshare(b.by_kind[la::kBlastNear], put), "share");
    lab.metric("people.blast.wide", bshare(b.wide, b.bursts - b.own), "share");
    lab.metric("people.knock.per-min", mins > 0 ? static_cast<double>(b.knocks) / mins : 0,
               "per-player-min");
    lab.metric("people.knock.burst-after", bshare(b.knock_burst, b.knocks), "share");
    lab.metric("people.knock.beam-after", bshare(b.knock_beam, b.knocks), "share");
    lab.metric("people.knock.died", bshare(b.knock_death, b.knocks), "share");
    lab.metric("people.death.air", bshare(b.deaths_air, b.deaths), "share");
    const char* blows[] = {"blast", "beam", "pellets", "other"};
    for (int k = 0; k < la::kBlowKinds; k++) {
      lab.metric(std::string("people.death.blow.") + blows[k], bshare(b.blow[k], b.deaths),
                 "share");
      lab.metric(std::string("people.death.blow.") + blows[k] + ".air",
                 bshare(b.blow_air[k], b.blow[k]), "share");
      lab.metric(std::string("people.death.quad.") + blows[k], bshare(b.quad_blow[k], b.quad_kills),
                 "share");
    }
    lab.metric("people.death.quad-kills", static_cast<double>(b.quad_kills), "count");
    lab.metric("people.jumps.per-min", mins > 0 ? static_cast<double>(b.jumps) / mins : 0,
               "per-player-min");
    lab.metric("people.jumps.fight-per-min",
               b.fight_s > 0 ? static_cast<double>(b.fight_jumps) * 60 / b.fight_s : 0,
               "per-fight-min");
  }
  // Each key item as it shows, the fight there, and the armour worn by type (show_report.hpp).
  la::report_show(lab, show, show_spots, fight, wear, kItemNames);
  la::report_chain(lab, chain, blast.alive_s / 60);  // the rounds of them (chain_read.hpp)
  // The glances at key item spots passed (glance_read.hpp).
  {
    const auto share = [](int64_t part, int64_t whole) {
      return whole > 0 ? static_cast<double>(part) / static_cast<double>(whole) : 0;
    };
    int64_t passes = 0, off = 0, seen = 0, aside = 0, shown = 0, shown_aside = 0;
    for (int k = 0; k < kItemKinds; k++) {
      const auto i = static_cast<size_t>(k);
      passes += glance.passes[i];
      off += glance.off[i];
      seen += glance.glanced[i];
      aside += glance.aside[i];
      shown += glance.shown[i];
      shown_aside += glance.shown_aside[i];
      if (k == 0) continue;
      const std::string key = std::string("people.glance.key.") + kItemNames[k];
      lab.metric(key + ".passes-per-min",
                 glance.live_s > 0 ? static_cast<double>(glance.passes[i]) * 60 / glance.live_s : 0,
                 "per-min");
      lab.metric(key + ".off-share", share(glance.off[i], glance.passes[i]), "share");
      lab.metric(key + ".glanced", share(glance.glanced[i], glance.off[i]), "share");
      lab.metric(key + ".shown", share(glance.shown[i], glance.off[i]), "share");
      lab.metric(key + ".seen", share(glance.seen[i], glance.passes[i]), "share");
      lab.metric(key + ".shown-aside", share(glance.shown_aside[i], glance.off[i]), "share");
    }
    lab.metric("people.glance.key.passes-per-min",
               glance.live_s > 0 ? static_cast<double>(passes) * 60 / glance.live_s : 0, "per-min");
    lab.metric("people.glance.key.off-share", share(off, passes), "share");
    lab.metric("people.glance.key.off-per-min",
               glance.live_s > 0 ? static_cast<double>(off) * 60 / glance.live_s : 0, "per-min");
    lab.metric("people.glance.key.glanced", share(seen, off), "share");
    lab.metric("people.glance.key.aside", share(aside, off), "share");
    lab.metric("people.glance.key.shown", share(shown, off), "share");
    int64_t seen_all = 0;
    for (const int64_t v : glance.seen) seen_all += v;
    lab.metric("people.glance.key.seen", share(seen_all, passes), "share");
    gl::LabTable by_spot{
        "each key spot passed (glance_read.hpp): passes a minute alive, the share starting with it "
        "off the screen, seen at all, and of those off: shown aside, glanced",
        {"item", "spot", "passes/min", "off", "seen", "shown aside", "glanced"},
        {}};
    for (const auto& [at, sp] : glance.spots)
      by_spot.rows.push_back(
          {kItemNames[sp.kind], at,
           to_fixed(glance.live_s > 0 ? static_cast<double>(sp.passes) * 60 / glance.live_s : 0, 2),
           to_fixed(share(sp.off, sp.passes), 3), to_fixed(share(sp.seen, sp.passes), 3),
           to_fixed(share(sp.shown_aside, sp.off), 3), to_fixed(share(sp.glanced, sp.off), 3)});
    lab.table(by_spot);
    lab.metric("people.glance.key.shown-aside", share(shown_aside, off), "share");
    lab.metric("people.glance.key.calm.glanced", share(glance.calm_glanced, glance.calm_off),
               "share");
    lab.metric("people.glance.key.fight.glanced", share(glance.fight_glanced, glance.fight_off),
               "share");
    lab.metric("people.glance.key.calm-share", share(glance.calm_off, off), "share");
    lab.metric("people.glance.key.there.glanced", share(glance.there_glanced, glance.there_off),
               "share");
    lab.metric("people.glance.key.gone.glanced", share(glance.gone_glanced, glance.gone_off),
               "share");
    lab.metric("people.glance.key.len-p50", la::keys_quantile(glance.len, 0.5), "s");
    lab.metric("people.glance.key.len-p90", la::keys_quantile(glance.len, 0.9), "s");
    lab.metric("people.glance.key.first-p50", la::keys_quantile(glance.first, 0.5), "s");
    lab.metric("people.glance.key.aside-deg-p50", la::keys_quantile(glance.aside_deg, 0.5), "deg");
    lab.metric("people.glance.key.aside-deg-p90", la::keys_quantile(glance.aside_deg, 0.9), "deg");
    // Each player's own share (those with 8 passes off or more): the spread.
    std::vector<float> mine;
    for (const auto& pl : glance.players)
      if (pl[0] >= 8) mine.push_back(static_cast<float>(share(pl[1], pl[0])));
    lab.metric("people.glance.key.players", static_cast<double>(mine.size()), "count");
    lab.metric("people.glance.key.player-p25", la::keys_quantile(mine, 0.25), "share");
    lab.metric("people.glance.key.player-p50", la::keys_quantile(mine, 0.5), "share");
    lab.metric("people.glance.key.player-p75", la::keys_quantile(mine, 0.75), "share");
  }
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
        {"dir", "",
         "a folder of recordings (.mvd or .ged) of one map (people's, or se-team --save's)"},
        {"file", "", "one recording's path (with --dir: read too)"},
        {"basedir", "", "a folder holding the game folder the recordings' map is read from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "e1m2", "with --basedir: the recordings' map"},
        {"every", "0.1", "a sample every this many seconds of the match"},
        {"near", "1000", "an enemy this near is near, units"},
        {"samples", "", "write every sample alive to this .tsv (place, wet, eyes under, speed)"},
        {"events", "",
         "write the key items' pickups (their taker), the deaths and the final frags to this .tsv"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
