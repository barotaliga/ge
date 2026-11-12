// The fighter (fighter.hpp): its stats, the map's items timed off what is sent, the mode, the goal,
// the weapon and the trigger.
//
// ge:summary The fighter: stats, item timing, mode, goal, weapon and trigger on the baseline mod
// ge:keywords fighter fighter_init fighter_command read_stats item-spot weapon-table tolerance
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/fighter.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "ai/choose.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "progs/entity_text.hpp"
#include "shooter/brink.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/buttons.hpp"
#include "shooters_edge/bots/ears.hpp"
#include "shooters_edge/bots/footwork.hpp"
#include "shooters_edge/bots/hop_start_book.hpp"
#include "shooters_edge/bots/item_routes.hpp"
#include "shooters_edge/bots/keyboard.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/bots/map_plan.hpp"
#include "shooters_edge/bots/packs.hpp"
#include "shooters_edge/bots/spam.hpp"
#include "shooters_edge/bots/team.hpp"
#include "shooters_edge/bots/temper.hpp"
#include "shooters_edge/bots/watch.hpp"
#include "shooters_edge/bots/water.hpp"

namespace shooters_edge::bots {

namespace {

namespace pg = ge::progs;
namespace ai = ge::ai;

// The baseline mod's weapon bits (games/shooters_edge/qc/common/defs.qc) and ammunition maxima
// (items.qc).
constexpr float kAmmoMax = 100;
constexpr int32_t kItQuad = 4194304;  // the quad's IT_ bit
constexpr int32_t kItPent = 1048576;  // the shield's

constexpr float kRetreatBelow = 50;   // health plus armour's protection: weak under this
constexpr float kStrongAgain = 80;    // and strong again at this
constexpr double kChase_s = 3;        // a contact lost this long ago is still chased
constexpr double kModeBeat_s = 0.5;   // a person's beat: a mode held this long (`beat`)
constexpr double kGoalTire_s = 30;    // one goal item held this long in vain is let go of
constexpr double kHealTire_s = 8;     // and sooner by a weak player, who cannot wait for it
constexpr double kTired_s = 20;       // for this long
constexpr double kTired = 0.2;        // its worth meanwhile, as a share
constexpr float kRetakeStrong = 120;  // strength (a launcher counted) to go and take an area back
constexpr double kRetake_s = 20;      // a key area believed lost this long ago is still retaken
constexpr double kHeardChase_s = 2;   // and one heard this long ago is gone after
constexpr float kCornered = 350;      // units: an enemy in sight this near is fought (the double's
                                      // range: a back turned on him this close is shot)
constexpr float kCowardCornered = 192;  // a coward still runs from one farther than this
constexpr double kImpulseGap_s = 0.5;
constexpr double kWait_s = 20;      // an item back within this is worth going to wait for
constexpr double kHealWait_s = 3;   // but a retreat goes for health that is there
constexpr double kKeepGoal = 1.5;   // the goal's score is held up by this against the others
constexpr double kDecide_s = 4;     // with no news, its item goal is chosen again this often
// A chain's pickup run through (`chains`): the item still believed there this long after is the
// news of its taking still to come, not a miss to go back for, s.
constexpr double kViaNews_s = 0.6;
// The turn back taken round (`uturn`): at this speed or more on the ground, a wish this far off the
// velocity is pressed this far across it instead (degrees); past `kTurnStraight` (straight back)
// the view's side is the side.
constexpr double kTurnBackSpeed = 150, kTurnBack = 110, kTurnAcross = 95, kTurnStraight = 170;
constexpr double kTurnRoom = 48;  // and the room it needs across, units (the circle's ~32 and a box)
constexpr float kRun = 320;         // units/s, the run to an item
constexpr double kCallAgain_s = 8;  // an enemy in sight is called out again after this
constexpr double kTold_s = 10;      // a point at an item is followed this long
constexpr float kRocketNear = 400;  // a rocket at a man nearer than this is aimed at his middle
// A hurt player does not walk back into a fight his side is losing: health within this of a danger
// spot (where he died, where a mate died, an area called lost, a call for help: mind.hpp) is worth
// this share while he heals, and health an enemy he knows of now stands nearer to than he does is
// worth half.
constexpr double kDangerMemory_s = 40;
constexpr float kDangerReach = 384;
constexpr double kDangerScale = 0.25;
constexpr double kEnemyNearScale = 0.5;
constexpr double kEnemyKnown_s = 3;  // an enemy seen or heard this lately is known of now
constexpr double kHotSpot_s = 60;    // two danger spots this lately near an item: no healing there
constexpr double kMegaHealBonus = 10;  // the mega's worth to a healer over the health it gives
constexpr double kSwimExit_s = 3;      // a climb out of the water kept this long at most
constexpr double kSwimWayFlat = 48;    // a swim's way nearer than this across is risen or sunk to
constexpr double kLanding_s = 2;   // out of the water, its exit node walked onto this long at most
constexpr double kBaseHeal = 1.4;  // health in the team's base is worth this more to a healer
constexpr double kLauncherWorth = 240;   // the launcher lacked: worth a long run
constexpr double kBridgeSpare_s = 2.5;   // a bridge left this long before it goes back, s
constexpr float kRocketJumpHealth = 70;  // the health a rocket jump is tried with (it costs ~50)
// A hopper plans the map's gap tricks only this fast or faster, units/s (people take off at
// 420 to 500; a gap's jump is flown only when the move says it lands across).
constexpr float kGapPlanSpeed = 380;
constexpr float kRocketJumpMoves = 4;  // and the moves trait (an amateur jumper's)
// THE ROCKET JUMPER'S LEVEL by the moves trait (amateur from 4, intermediate from 6, expert from 8)
// and what a rocket jump must save before he tries it, s: people's rates (the `se-rj` lab on dm2:
// an expert jumps 1.9 times in ten minutes, an intermediate 0.5, an amateur 0.3) say the less sure
// the hands, the more a jump must be worth.
constexpr float kRocketLevelMoves[3] = {4, 6, 8};
constexpr float kRocketLevelExtra_s[3] = {3, 1.5f, 0.5f};
constexpr float kRocketsLow = 25;          // a launcher with fewer rockets than this wants more
constexpr float kRocketsStock = 50;        // rockets stocked with no launcher yet, up to this
constexpr double kRocketsStockWorth = 15;  // and worth this from none
constexpr float kBlastClear = 140;  // a rocket or grenade is held with a mate this near its aim
// What a fighter sees: a player's screen, 120 degrees across and as high as a 16:9 monitor shows
// at that width (2 atan(tan 60 * 9/16)), people's own field of view.
constexpr float kViewWide = 120;
// A short goal on the way: within this many units, and its floor within this of the player's.
constexpr float kOnTheWay = 256;
constexpr float kOnTheWayRise = 40;
constexpr float kFeet = 24;         // a player's origin over the floor
constexpr double kShortHold_s = 3;  // a short goal is given up after this
constexpr float kBeside = 96;       // nearer than this, any way it lies
constexpr float kAhead = 0.34f;     // else the cosine off its motion, at least
// Swimming, the view turns this many degrees a second past a band this wide off the swim's way
// (an about-face faster), as the calm view walks (shooter/bot.cpp).
constexpr double kSwimTurn = 180;
constexpr double kSwimAbout = 360;
constexpr float kSwimBand = 20;
constexpr float kViewHigh = 88.5f;
// The finger clicks while the view is within this many times the trigger's tolerance of him.
constexpr float kClickWide = 2.5f;
// A view set for a shot (a button, a doorway) is fired at once the hand is this near it, degrees.
constexpr float kHandThere = 3;

// How far, in degrees, the view command `cmd` carries is off the aim `b` wants.
float view_off(const sh::Bot& b, const sh::MoveCmd& cmd) {
  float dy = static_cast<float>(cmd.angles.y - b.wanted[0]);
  dy -= 360.f * static_cast<float>(std::floor((dy + 180.f) / 360.f));
  const float dp = static_cast<float>(cmd.angles.x - b.wanted[1]);
  return std::sqrt(dy * dy + dp * dp);
}

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

float wrap180(float deg) {
  return deg - 360.f * static_cast<float>(std::floor((deg + 180.f) / 360.f));
}

// A swim toward (dx, dy, dz) from here pressed under `view` (kept as the command's): forward and
// side along the view's axes, what they leave of the way pressed straight up.
void swim_under_view(sh::MoveCmd& cmd, sh::Vec3f view, double dx, double dy, double dz) {
  cmd.angles.x = view.x;
  cmd.angles.y = view.y;
  const sh::ViewAxes ax = sh::angle_vectors({view.x, view.y, 0});
  const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (len <= 1) return;
  const double k = 320 / len, wx = dx * k, wy = dy * k, wz = dz * k;
  const double fw = wx * ax.forward.x + wy * ax.forward.y + wz * ax.forward.z;
  const double sd = wx * ax.right.x + wy * ax.right.y + wz * ax.right.z;
  const double up = wz - fw * ax.forward.z - sd * ax.right.z;
  cmd.forward = sh::quantize_axis(static_cast<float>(fw));
  cmd.side = sh::quantize_axis(static_cast<float>(sd));
  cmd.up = sh::quantize_axis(static_cast<float>(up));
}

// Swimming (or wading on the swim's way), the view a person's: the way the swim pressed (its view
// and the keys under it, as one wish in the world) kept, the view turned toward its way at a hand's
// pace and still within a band of it, and the same wish pressed with forward, side and up under the
// view it has. Not with an enemy in sight (the aim's view), nor on the last leg out of an exit cell
// (a water jump needs the view level at the edge).
void swim_view(Fighter& f, const server::RulesCommon& s, int me, int msec, bool swim,
               sh::MoveCmd& cmd) {
  const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  bool seen = false;
  if (f.bot.aiming)
    for (const sh::Contact& k : f.bot.senses.contacts) seen |= k.ent == f.bot.target && k.visible;
  const bool wading =
      self.waterlevel >= 1 && (swim || f.bot.pilot.pilot.mode == sh::PilotMode::swim);
  // The mouse hand turns a swimmer's view itself (shooter/bot.hpp).
  const bool hand = f.bot.mouse && f.bot.aim.mouse && f.features[sh::Feature::pools];
  if ((self.waterlevel < 2 && !wading) || seen || f.swim_exit >= 0 ||
      !f.features[sh::Feature::pace] || hand) {
    f.swim_view_on = false;
    f.view_sent[0] = cmd.angles.x;
    f.view_sent[1] = cmd.angles.y;
    return;
  }
  const sh::ViewAxes want = sh::angle_vectors({cmd.angles.x, cmd.angles.y, 0});
  const float fw = cmd.forward, sd = cmd.side;
  const sh::Vec3f wish{want.forward.x * fw + want.right.x * sd,
                       want.forward.y * fw + want.right.y * sd,
                       want.forward.z * fw + want.right.z * sd + static_cast<float>(cmd.up)};
  if (!f.swim_view_on) f.swim_view_on = true;  // from the view last sent
  const double dt = msec / 1000.0;
  const float dy = wrap180(cmd.angles.y - f.view_sent[1]);
  const float dp = cmd.angles.x - f.view_sent[0];
  const double rate = (dy > 120 || dy < -120) ? kSwimAbout : kSwimTurn;
  const auto toward = [&](float off, double r) {
    if (off > -kSwimBand && off < kSwimBand) return 0.f;
    const double want_step = off > 0 ? off - kSwimBand * 0.5 : off + kSwimBand * 0.5;
    return static_cast<float>(std::fmax(-r * dt, std::fmin(r * dt, want_step)));
  };
  const float yaw = wrap180(f.view_sent[1] + toward(dy, rate));
  const float pitch = f.view_sent[0] + toward(dp, kSwimTurn * 0.5);
  const sh::ViewAxes ax = sh::angle_vectors({pitch, yaw, 0});
  const float nf = sh::dot(wish, ax.forward), ns = sh::dot(wish, ax.right);
  cmd.angles.x = pitch;
  cmd.angles.y = yaw;
  f.bot.set_by = "swimview";
  cmd.forward = sh::quantize_axis(nf);
  cmd.side = sh::quantize_axis(ns);
  cmd.up = sh::quantize_axis(wish.z - nf * ax.forward.z - ns * ax.right.z);
  f.view_sent[0] = pitch;
  f.view_sent[1] = yaw;
}

float protection(float type, float value) { return type * value; }

// What item `it` is worth to a player with `st` now (0: nothing).
double raw_value(const ItemSpot& it, const Stats& st) {
  const float worn = protection(st.armortype, st.armor);
  switch (it.kind) {
    case ItemKind::health:
      return st.health >= 100 ? 0 : std::fmin(it.amount, 100 - st.health);
    case ItemKind::mega:
      return st.health >= 250 ? 0 : 20 + 0.8 * (250 - st.health);
    case ItemKind::armor1:
      return std::fmax(0.f, protection(0.3f, 100) - worn);
    case ItemKind::armor2:
      return std::fmax(0.f, protection(0.6f, 150) - worn);
    case ItemKind::armor3:
      return std::fmax(0.f, protection(0.8f, 200) - worn) * 1.2;
    case ItemKind::quad:
      return (st.items & kItQuad) ? 0 : 150;
    case ItemKind::pent:
      return (st.items & kItPent) ? 0 : 130;
    case ItemKind::weapon: {
      if (st.items & it.weapon) return 4;
      // The launcher wins a team game (people take it 60 times in 20 min): worth a long run
      // while it is there, not a long wait while it is away.
      if (it.weapon == st.arsenal->launcher) return it.present ? kLauncherWorth : 120;
      const BotWeapon* w = weapon_row(*st.arsenal, it.weapon);
      return w ? w->worth : 30;
    }
    default:
      break;
  }
  // Ammunition: worth what it fills of a weapon carried that uses it.
  const BotArsenal& arsenal = *st.arsenal;
  int a = arsenal.item_ammo(it.kind);
  if (a < 0) a = 0;
  bool uses = false;
  for (const BotWeapon& w : arsenal.weapons) uses |= w.ammo == a && (st.items & w.bit) != 0;
  const float have = ammo_of(st, a);
  // Rockets are stocked for the launcher still to come, as people stock them: worth something to
  // a player who has neither launcher yet.
  if (a == arsenal.launcher_ammo && !uses)
    return have >= kRocketsStock ? 1
                                 : std::fmax(1.0, kRocketsStockWorth * (1 - have / kRocketsStock));
  if (!uses || have >= kAmmoMax) return 1;
  const double fill = 30.0 * (kAmmoMax - have) / kAmmoMax;
  // Rockets for a launcher in hand running dry are worth half the launcher: it is no use without.
  if (a == arsenal.launcher_ammo && (st.items & arsenal.launcher) != 0 && have < kRocketsLow)
    return std::fmax(fill, 0.5 * kLauncherWorth * (1.0 - have / kRocketsLow));
  return fill;
}

// What an item is worth to it now, as it rates the map.
double item_value(const ItemSpot& it, const Stats& st) { return raw_value(it, st) * it.weight; }

bool heals(const ItemSpot& it) {
  return it.kind == ItemKind::health || it.kind == ItemKind::mega || it.kind == ItemKind::armor1 ||
         it.kind == ItemKind::armor2 || it.kind == ItemKind::armor3;
}

float strength(const Stats& st) { return st.health + protection(st.armortype, st.armor); }

bool alive_slot(const server::RulesCommon& s, int slot) {
  return s.sv.clients[static_cast<size_t>(slot)].active &&
         s.world.entities[static_cast<size_t>(slot) + 1].health > 0;
}

sh::Vec3f at_of(const ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

void set_mode(Fighter& f, FightMode next, double t, ai::Why* why, const char* because) {
  if (f.mode == next) return;
  f.stats.modes++;
  f.held = t - f.mode_at;
  f.mode_at = t;
  ai::enter(f.mode, f.held, next, why, fight_mode_name, because);
}

// Goes to node `node`; with `then` (a node, `chains`) on through it (the point `at`) to `then`.
void go_to(Fighter& f, int32_t node, int32_t then = -1, sh::Vec3f at = {}) {
  if (node < 0) return;
  if (f.reach_at >= 0 && !f.reach.reached(node)) return;  // no way there from here
  sh::Pilot& p = f.bot.pilot.pilot;
  const sh::PilotMode pm = p.mode;
  const bool active = pm != sh::PilotMode::idle && pm != sh::PilotMode::arrived;
  if (f.features[sh::Feature::chains]) {
    if (then == node) then = -1;
    if (active && node == f.goal_node) {
      if (p.via_k >= 0 && p.route[static_cast<size_t>(p.via_k)] == node) {
        // Through it already: on to the chain's goal, or another, or none.
        if (then == p.goal) return;
        sh::pilot_unchain(p);
        f.bot.pilot.goals.assign(1, p.goal);
        f.chain_node = -1;
        if (then >= 0 && sh::bot_chain(f.bot, then, at)) f.chain_node = then;
        return;
      }
      if (p.goal == node) {  // on the way to it: chained on now, if asked
        if (then >= 0 && sh::bot_chain(f.bot, then, at)) {
          f.chain_node = then;
          f.stats.chained++;
        }
        return;
      }
      // Just run through it on a chain: the pickup's news is a moment off.
      if (node == p.via_left && p.clock_s - p.via_left_at < kViaNews_s) return;
      // (otherwise the pilot is on to a goal the fighter has left: planned again below)
    } else if (active && p.goal == node && node == f.chain_node) {
      // The chain's goal is the goal now (its pickup run through, or taken): on, no plan again.
      if (p.via_k >= 0) {
        p.via_left = p.route[static_cast<size_t>(p.via_k)];
        p.via_left_at = p.clock_s;
        p.via_k = -1;
      }
      f.goal_node = node;
      f.chain_node = -1;
      if (then >= 0 && sh::bot_chain(f.bot, then, at)) {
        f.chain_node = then;
        f.stats.chained++;
      }
      return;
    } else if (pm == sh::PilotMode::arrived && node == f.goal_node) {
      // Standing on it (waiting for it): on to the next when there is one.
      if (then >= 0 && sh::bot_chain(f.bot, then, at)) {
        f.chain_node = then;
        f.stats.chained++;
      }
      return;
    }
    f.goal_node = node;
    f.chain_node = then;
    if (then >= 0) f.stats.chained++;
    sh::bot_goal(f.bot, node, then, at);
    return;
  }
  if (node == f.goal_node && pm != sh::PilotMode::idle && pm != sh::PilotMode::arrived) return;
  if (node == f.goal_node && pm == sh::PilotMode::arrived) return;  // standing on it
  f.goal_node = node;
  sh::bot_goal(f.bot, node);
}

bool door_shut(const server::RulesCommon& s, const ItemSpot& it) {
  if (it.door_ent < 0) return false;
  const sh::Vec3f d = s.world.entities[static_cast<size_t>(it.door_ent)].origin - it.door_rest;
  return d.x * d.x + d.y * d.y + d.z * d.z < 1;
}

// Goes for item `i`: through its button first while its door is shut. With `then` (an item,
// `chains`) and `i` there to take, on through it to `then` without a stop.
void go_item(Fighter& f, const server::RulesCommon& s, int32_t i, int32_t then = -1) {
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (f.goal_item != i) f.stats.goal_changes++;
  f.goal_item = i;
  if (it.dive && f.reach_at >= 0 && !f.reach.reached(it.node)) {
    // Through the water: to the dive's node, then the field (fighter_command) takes it in.
    f.unlocking = false;
    if (f.last_dive != i) f.dives++;
    f.last_dive = i;
    f.diving = i;
    go_to(f, it.dive_node);
    return;
  }
  f.diving = -1;
  const bool unlock = door_shut(s, it) && it.button_node >= 0;
  if (unlock && !f.unlocking) f.buttons++;
  f.unlocking = unlock;
  int32_t then_node = -1;
  if (then >= 0 && then != i && !unlock && it.present) {
    const ItemSpot& n = f.items[static_cast<size_t>(then)];
    const bool by_dive = n.dive && f.reach_at >= 0 && !f.reach.reached(n.node);
    if (n.node >= 0 && n.node != it.node && !by_dive && !door_shut(s, n)) then_node = n.node;
  }
  f.chain_item = then_node >= 0 ? then : -1;
  go_to(f, unlock ? it.button_node : it.node, then_node, it.at);
}

// What the player holds once it has taken `it` (`chains`: the goal after it chosen as if taken).
Stats after_taking(Stats st, const ItemSpot& it) {
  switch (it.kind) {
    case ItemKind::health:
      if (st.health < 100) st.health = std::fmin(100.f, st.health + it.amount);
      break;
    case ItemKind::mega:
      st.health = std::fmin(250.f, st.health + 100);
      break;
    case ItemKind::armor1:
    case ItemKind::armor2:
    case ItemKind::armor3: {
      const float type = it.kind == ItemKind::armor1 ? 0.3f : it.kind == ItemKind::armor2 ? 0.6f : 0.8f;
      const float value = it.kind == ItemKind::armor1 ? 100.f : it.kind == ItemKind::armor2 ? 150.f : 200.f;
      if (protection(type, value) > protection(st.armortype, st.armor)) {
        st.armortype = type;
        st.armor = value;
      }
      break;
    }
    case ItemKind::quad:
      st.items |= kItQuad;
      break;
    case ItemKind::pent:
      st.items |= kItPent;
      break;
    case ItemKind::weapon:
      st.items |= it.weapon;
      break;
    case ItemKind::shells:
      st.shells += it.amount;
      break;
    case ItemKind::nails:
      st.nails += it.amount;
      break;
    case ItemKind::rockets:
      st.rockets += it.amount;
      break;
    case ItemKind::cells:
      st.cells += it.amount;
      break;
  }
  return st;
}

// The best item for the player at `me` with `st`: worth over time to have it, `away` from a known
// enemy (when `heal`, health and armour alone, and never nearer the enemy than this player is).
int32_t best_item(const Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me,
                  double t, bool heal, const sh::Vec3f* enemy, ai::Why* why, int32_t skip = -1) {
  const auto score = [&](int32_t i) {
    const ItemSpot& it = f.items[static_cast<size_t>(i)];
    if (i == skip || it.node < 0 || (heal && !heals(it))) return 0.0;
    // Only what it knows of (or sees). One gone is waited for at its time back as the player
    // knows it: exact when timed, a guess (late) when not (knowledge.hpp).
    if (!item_in_mind(f, i)) return 0.0;
    if (f.trap && (*f.trap)[static_cast<size_t>(it.node)]) return 0.0;  // no way back out of it
    // No way there from here, unless by its dive (from a node there is a way to).
    const bool by_dive = f.reach_at >= 0 && !f.reach.reached(it.node);
    if (by_dive && (!it.dive || !f.reach.reached(it.dive_node) || it.dive_s > kDiveMost_s ||
                    (i == f.dive_off && t < f.dive_off_until)))
      return 0.0;
    if (door_shut(s, it) &&
        (it.button_node < 0 || (f.reach_at >= 0 && !f.reach.reached(it.button_node))))
      return 0.0;  // shut, and its button out of reach
    if (it.after >= 0 && f.features[sh::Feature::routes] && item_in_mind(f, it.after)) {
      // An item rule: not while the one to be had first is believed up or due.
      const ItemSpot& first = f.items[static_cast<size_t>(it.after)];
      if (first.present || first.back_at - t < it.after_due_s) return 0.0;
    }
    // The map's weights are a team's plan (what to hold); a weak player, or one alone, wants
    // what does him good soonest.
    double v = heal || !f.team ? raw_value(it, st) : item_value(it, st);
    // Healing, the mega health is worth the health it gives back to 100, as a box is: a hurt player
    // takes the health near him, not the room every hurt player of both teams runs to.
    if (heal && it.kind == ItemKind::mega) v = std::fmax(0.f, 100 - st.health) + kMegaHealBonus;
    if (v <= 0) return 0.0;
    // The team's base (the map plan's) is where a hurt player falls back to.
    if (heal && f.plan && f.plan->base >= 0 && plan_area_of(*f.plan, it.node) == f.plan->base)
      v *= kBaseHeal;
    if (heal) {  // where two or more of its side fell lately is not healed at at all
      int fell = 0;
      for (const auto& [where, when] : f.danger)
        fell += t - when < kHotSpot_s && dist(where, it.at) < kDangerReach ? 1 : 0;
      if (fell >= 2) return 0.0;
    }
    const double run = dist(me, it.at) / kRun + (by_dive ? it.dive_s : 0);
    const double wait = it.present ? 0 : it.back_at - t;
    // Only the items a game turns on are waited for; the rest are taken when they are there.
    const bool key = key_item(it);
    if (wait > (heal || (!key && f.team) ? kHealWait_s : kWait_s)) return 0.0;
    double sc = v / (1 + std::fmax(run, wait));
    if (i == f.goal_item) sc *= kKeepGoal;  // the goal held unless another is clearly better
    if (i == f.tired_item && t < f.tired_until) sc *= kTired;  // gone for in vain lately
    if (enemy && (heal || f.outmatched) && dist(*enemy, it.at) < dist(me, it.at)) sc *= 0.2;
    // A teammate who said he goes for it has it; one in sight it is no use to, or the quad's
    // carrier, gets it; a strong player leaves health to the others; a point is followed.
    if (mate_going(f, i, t)) sc *= 0.15;
    if (gives_up(f, s, f.slot + 1, i, st, t)) return 0.0;
    if (f.team && heals(it) && it.kind != ItemKind::mega && strength(st) >= 150) sc *= 0.3;
    if (f.mind.told == i && t - f.mind.told_t < kTold_s) sc *= 4;
    // A weak player falls back to his own: health near his area is worth more.
    if (f.team && heal && f.mind.claim >= 0 &&
        dist(it.at, f.items[static_cast<size_t>(f.mind.claim)].at) < kLeash)
      sc *= 1.5;
    if (heal) {
      for (const auto& [where, when] : f.danger)
        if (t - when < kDangerMemory_s && dist(where, it.at) < kDangerReach) sc *= kDangerScale;
      for (const sh::Contact& k : f.bot.senses.contacts) {
        if (k.ent < 0) continue;
        const bool seen = ai::known(k.seen) && ai::age(k.seen, t) <= kEnemyKnown_s;
        const bool heard = ai::known(k.heard) && ai::age(k.heard, t) <= kEnemyKnown_s;
        if (!seen && !heard) continue;
        if (dist(at_of(seen ? k.seen : k.heard), it.at) < dist(me, it.at)) sc *= kEnemyNearScale;
      }
    }
    return sc;
  };
  const auto name = [&](int32_t i) { return f.items[static_cast<size_t>(i)].name.c_str(); };
  return ai::pick_best(static_cast<int32_t>(f.items.size()), score, why,
                       heal ? "heal_goal" : "item_goal", name, "worth over the time to have it", 0);
}

// THE TURN BACK TAKEN ROUND (fighter_command, `uturn`): `cmd`'s wish turned across `velocity`
// when it lies more than kTurnBack off it at a run; false (unchanged) otherwise.
bool turn_back_round(sh::MoveCmd& cmd, sh::Vec3f origin, sh::Vec3f velocity,
                     const sh::MoveWorld* world) {
  const double vx = velocity.x, vy = velocity.y;
  const double speed = std::sqrt(vx * vx + vy * vy);
  const double fw = cmd.forward, sd = cmd.side;
  const double wish = std::sqrt(fw * fw + sd * sd);
  if (speed < kTurnBackSpeed || wish < 1) return false;
  constexpr double kRad = ge::core::kPi / 180.0;
  const double view = cmd.angles.y * kRad;
  const double vh = ge::core::libm::atan2(vy, vx);
  const auto wrap_rad = [](double a) {
    while (a > ge::core::kPi) a -= 2 * ge::core::kPi;
    while (a < -ge::core::kPi) a += 2 * ge::core::kPi;
    return a;
  };
  const double off = wrap_rad(view + ge::core::libm::atan2(-sd, fw) - vh);
  const double abs_off = off < 0 ? -off : off;
  if (abs_off < kTurnBack * kRad) return false;
  // The side the wish lies on; straight back, the side the view is turned to.
  double side = off > 0 ? 1 : -1;
  if (abs_off > kTurnStraight * kRad) side = wrap_rad(view - vh) >= 0 ? 1 : -1;
  // The circle needs room on its side (the map's solids, when known): the other side, or none.
  const auto room = [&](double sgn) {
    if (world == nullptr) return true;
    const double a = vh + sgn * kTurnAcross * kRad;
    const sh::Vec3f to{origin.x + static_cast<float>(ge::core::libm::cos(a) * kTurnRoom),
                       origin.y + static_cast<float>(ge::core::libm::sin(a) * kTurnRoom), origin.z};
    return sh::walk_clear(*world, origin, to);
  };
  if (!room(side)) {
    if (!room(-side)) return false;
    side = -side;
  }
  const double rel = vh + side * kTurnAcross * kRad - view;
  cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * wish));
  cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * wish));
  return true;
}

// THE GOAL AFTER (`chains`): the item the player would go for once it has item `i`, chosen as
// it would choose then (from where `i` stands, holding what `i` gives); -1 with none, or `i` not
// there to take now (it waits for it there).
int32_t chain_after(const Fighter& f, const server::RulesCommon& s, const Stats& st, int32_t i,
                    double t, bool heal, const sh::Vec3f* enemy) {
  if (!f.features[sh::Feature::chains] || i < 0) return -1;
  const ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (!it.present) return -1;
  return best_item(f, s, after_taking(st, it), it.at, t, heal, enemy, nullptr, i);
}

// A SHORT GOAL ON THE WAY: an item there now that a person turns aside for, on its own floor within
// kOnTheWay and ahead of where it runs (or beside it), that a way reaches, nobody of its side said
// he takes, and not the goal itself; the nearest, or -1. Taken before the long goal, as a person
// picks up what lies by his way: the goal is chosen again once it has it (a pickup is news).
int32_t on_the_way(const Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me,
                   sh::Vec3f velocity, double t) {
  const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
  int32_t best = -1;
  float best_d = kOnTheWay;
  for (size_t k = 0; k < f.items.size(); k++) {
    const auto i = static_cast<int32_t>(k);
    const ItemSpot& it = f.items[k];
    if (i == f.goal_item || !it.present || it.node < 0 || !item_in_mind(f, i)) continue;
    if (f.reach_at >= 0 && !f.reach.reached(it.node)) continue;
    if (std::fabs(it.at.z - (me.z - kFeet)) > kOnTheWayRise || door_shut(s, it)) continue;
    // What a person turns aside for: health it lacks, better armour, a weapon it has not got,
    // rockets; shells and nails are taken as they lie on the way, not gone for.
    const bool ammo =
        it.kind == ItemKind::shells || it.kind == ItemKind::nails || it.kind == ItemKind::cells;
    if (ammo || (it.kind == ItemKind::weapon && (st.items & it.weapon) != 0)) continue;
    if (raw_value(it, st) <= 0 || mate_going(f, i, t) || gives_up(f, s, f.slot + 1, i, st, t))
      continue;
    const float d = dist(me, it.at);
    // Ahead of where it runs (within about 70 degrees of its motion), or beside it.
    if (d > kBeside && speed > 100) {
      const float dx = it.at.x - me.x, dy = it.at.y - me.y;
      const float flat = std::sqrt(dx * dx + dy * dy);
      if (flat > 1 && (dx * velocity.x + dy * velocity.y) / (flat * speed) < kAhead) continue;
    }
    if (d < best_d) {
      best = i;
      best_d = d;
    }
  }
  return best;
}

// A backpack within `reach` worth going for (ammunition short or a weapon lacking), or -1: one
// in sight on its screen (no wall, door or water's surface between), or the one already gone for
// while it lies there.
int32_t pack_near(const Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me,
                  float reach) {
  const BotArsenal& arsenal = *f.arsenal;
  bool short_of = false;
  for (int k = 0; k < arsenal.ammo_count; k++) short_of |= st.ammo[k] < arsenal.short_of[k];
  short_of |= (st.items & (arsenal.launcher | arsenal.lob)) == 0;
  // A novice wants every pack he sees (`packinfer`), whatever he carries.
  short_of |= f.sense == MapSense::novice && f.features[sh::Feature::packinfer];
  if (!short_of) return -1;
  sh::SenseSettings look = f.bot.senses.set;
  look.range = reach;
  const sh::Vec3f eye{me.x, me.y, me.z + look.view_height};
  int32_t best = -1;
  float best_d = reach;
  for (const sh::ViewEntity& v : f.bot.view.entities) {
    if (v.modelindex <= 0 || static_cast<size_t>(v.modelindex) >= s.models.size() ||
        s.models[static_cast<size_t>(v.modelindex)] != "progs/backpack.mdl")
      continue;
    const float d = dist(v.origin, me);
    if (d >= best_d) continue;
    // A pack lies on the floor: seen at a few units over it (as a player's origin less his eye).
    if (v.ent != f.pack_ent &&
        !sh::sees_player(s.sv, look, f.slot + 1, eye, {f.bot.pitch, f.bot.yaw, 0},
                         {v.origin.x, v.origin.y, v.origin.z + 8 - look.view_height}))
      continue;
    best_d = d;
    best = v.ent;
  }
  return best;
}

// The role's goal (team.hpp), true when it set one; false leaves the goal to the items.
bool team_goal(Fighter& f, const server::RulesCommon& s, const Stats& st, sh::Vec3f me, double t,
               const sh::Contact* c, ai::Why* why) {
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  // A backpack close by is had first, unless retreating with someone about.
  if (f.mode != FightMode::retreat) {
    const int32_t pack = pack_near(f, s, st, me, 400);
    if (pack >= 0) {
      if (pack != f.pack_ent) f.stats.packs++;
      f.pack_ent = pack;
      go_to(f, sh::nav_nearest(g, s.world.entities[static_cast<size_t>(pack)].origin));
      return true;
    }
    f.pack_ent = -1;  // none in sight: the next is chosen by sight again
    // A pack it knows a death left (packs.hpp): one from a fight with rockets in it from far,
    // any other near and with nothing else on.
    if (f.features[sh::Feature::packinfer]) {
      const int32_t pn = packs_known_goal(f, me, st, t, f.mode == FightMode::roam, c && c->visible);
      if (pn >= 0) {
        go_to(f, pn);
        if (why) {
          why->decide("pack");
          why->choose("known pack", "a death's pack it knows of, worth the way");
        }
        return true;
      }
    }
  }
  if (!f.team || f.mode == FightMode::retreat) return false;
  // A pack a mate called, when this player is the one to fetch it (packs.hpp).
  if (const int32_t pn = f.features[sh::Feature::packs] ? packs_goal(f, me, st, t) : -1; pn >= 0) {
    go_to(f, pn);
    return true;
  }
  const Mind& mn = f.mind;
  const auto node_of = [&](int32_t i) { return f.items[static_cast<size_t>(i)].node; };
  const auto can_reach = [&](int32_t i) {
    return i >= 0 && (f.reach_at < 0 || f.reach.reached(node_of(i)));
  };
  const auto at = [&](int32_t i) { return f.items[static_cast<size_t>(i)].at; };
  const auto note = [&](const char* role, const char* because) {
    if (!why) return;
    why->decide("role_goal");
    why->choose(role, because);
  };
  // The quad item and the areas a carrier sweeps (the best armour's, the launcher's).
  int32_t quad = -1, armour = -1, launcher = -1;
  for (size_t i = 0; i < f.items.size(); i++) {
    const ItemSpot& it = f.items[i];
    if (it.node < 0) continue;
    const auto ii = static_cast<int32_t>(i);
    if (it.kind == ItemKind::quad && quad < 0) quad = ii;
    if (it.kind == ItemKind::armor3 || (it.kind == ItemKind::armor2 && armour < 0)) armour = ii;
    if (it.kind == ItemKind::weapon && it.weapon == f.arsenal->launcher && launcher < 0)
      launcher = ii;
  }
  switch (f.role) {
    case Role::hold: {
      const int32_t area = mn.claim;
      if (!can_reach(area) || strength(st) < kHoldStrong) return false;
      // He chases out of his area no further than the leash; else he stands on it.
      if (f.mode == FightMode::chase && c && dist(at_of(c->seen), at(area)) < kLeash) {
        go_to(f, sh::nav_nearest(g, at_of(c->seen)));
        note("hold", "chasing within the leash");
        return true;
      }
      // The item gone and not due for a while: out to fire at the ways in (spam.hpp).
      if (spam_plan(f, s, st, t)) {
        go_to(f, f.spam_node);
        note("hold", "out to fire at a way in while the item is away");
        return true;
      }
      // The item gone and back soon, or there: wait on it; else the items near it.
      const int32_t i = best_item(f, s, st, me, t, false, nullptr, nullptr);
      if (i >= 0 && dist(at(i), at(area)) < kLeash * 0.6f) {
        go_item(f, s, i);
      } else {
        go_item(f, s, area);
      }
      note("hold", "on the area it claimed");
      return true;
    }
    case Role::rush:
      if (!can_reach(quad)) return false;
      go_item(f, s, quad);
      note("rush", "the quad is due: waiting on it");
      return true;
    case Role::carry: {
      if (c && ai::known(c->seen) && ai::age(c->seen, t) < 3) {
        go_to(f, sh::nav_nearest(g, at_of(c->seen)));
        note("carry", "the quad at the enemy in sight");
        return true;
      }
      // The key areas cleared in turn: the one it is not at.
      int32_t sweep = -1;
      for (int32_t i : {armour, launcher})
        if (can_reach(i) && dist(at(i), me) > 250 && (sweep < 0 || i == f.goal_item)) sweep = i;
      if (sweep < 0 && mn.enemy_t > t - 6) {
        go_to(f, sh::nav_nearest(g, mn.enemy_at));
        note("carry", "the quad to the enemy last called out");
        return true;
      }
      if (sweep < 0) return false;
      f.goal_item = sweep;
      go_to(f, node_of(sweep));
      note("carry", "the quad through the key areas");
      return true;
    }
    case Role::push: {
      // The team's push on its base (mind.hpp): gathered outside it first, then in together.
      const int32_t area = mn.push_item;
      if (!can_reach(area)) return false;
      if (f.mode == FightMode::chase && c && dist(at_of(c->seen), at(area)) < kLeash) {
        go_to(f, sh::nav_nearest(g, at_of(c->seen)));
        note("push", "chasing in the base");
        return true;
      }
      if (mn.push_go_t < 0) {
        go_to(f, sh::nav_nearest(g, mn.push_at));
        note("push", "gathering outside the base");
        return true;
      }
      go_item(f, s, area);
      note("push", "into the base together");
      return true;
    }
    case Role::free:
      // A teammate's call for help is answered by a player strong enough to give it.
      if (f.mode != FightMode::retreat && t < mn.help_until && strength(st) >= 120 &&
          dist(mn.help_at, me) < 1500) {
        go_to(f, sh::nav_nearest(g, mn.help_at));
        note("free", "a teammate called for help");
        return true;
      }
      // A key area believed lost (worked out, or a mate's `lost`) is taken back by a strong player.
      if ((f.mode == FightMode::roam || f.mode == FightMode::chase) && mn.retake >= 0 &&
          t - mn.retake_t < kRetake_s && strength(st) >= kRetakeStrong && can_reach(mn.retake)) {
        if (f.goal_item != mn.retake) f.team->stats.retakes++;
        go_item(f, s, mn.retake);
        note("free", "taking back a key area no teammate is in");
        return true;
      }
      // A teammate's callout is chased when this player is strong and has nothing in sight.
      if (f.mode == FightMode::roam && t - mn.enemy_t < 3 && strength(st) >= 150 &&
          (st.items & f.arsenal->launcher) != 0 && dist(mn.enemy_at, me) < 1200) {
        go_to(f, sh::nav_nearest(g, mn.enemy_at));
        note("free", "a teammate's callout, strong enough to help");
        return true;
      }
      return false;
  }
  return false;
}

// The weapon: the best loaded at `range`, asked for by its impulse; a blast weapon only while
// `blast_ok` (a companion's foe far enough from every player, companion.hpp).
void choose_weapon(Fighter& f, const Stats& st, float range, double t, ai::Why* why,
                   bool blast_ok = true) {
  const auto score = [&](int32_t i) {
    const BotWeapon& w = f.arsenal->weapons[static_cast<size_t>(i)];
    if (!blast_ok && w.blast) return -1.0;
    return loaded(st, w) ? weapon_score(w, range) : -1.0;
  };
  const std::span<const BotWeapon> weapons = f.arsenal->weapons;
  const auto name = [&](int32_t i) { return weapons[static_cast<size_t>(i)].name; };
  const int32_t w = ai::pick_best(static_cast<int32_t>(weapons.size()), score, why, "weapon", name,
                                  "the best loaded at the range");
  if (w >= 0) {
    const BotWeapon& best = weapons[static_cast<size_t>(w)];
    if (best.bit != st.weapon && loaded(st, best) && t - f.impulse_at >= kImpulseGap_s)
      f.want_weapon = best.number;
  }
}

// The bridges as seen now (fighter.hpp `Bridge`): on a change, the links onto those still in are
// shut and the way planned again.
void watch_bridges(Fighter& f, const server::RulesCommon& s) {
  if (f.bridges.empty()) return;
  bool changed = !f.bridges_seen;
  f.bridges_seen = true;
  for (Fighter::Bridge& b : f.bridges) {
    if (b.ent < 0 || static_cast<size_t>(b.ent) >= s.world.entities.size()) continue;
    const sh::Vec3f o = s.world.entities[static_cast<size_t>(b.ent)].origin;
    bool in = false;
    if (b.floor) {
      in = dist(o, b.spawn) >= 1;  // a floor door: there only at rest
    } else {
      // A bridge: a floor only all the way out where the map drew it, and not in the last
      // moments of its wait (it goes back from under whoever is on it: dm2's red armour's).
      const bool out = dist(o, sh::Vec3f{}) < 1;
      if (!out) b.out_since = -1;
      if (out && b.out_since < 0) b.out_since = s.sv.time;
      in = !out || (b.wait > 0 && s.sv.time - b.out_since > b.wait - kBridgeSpare_s);
    }
    changed = changed || in != b.in;
    b.in = in;
  }
  if (!changed) return;
  const sh::NavGraph& g = *f.bot.pilot.pilot.g;
  std::vector<uint8_t> on(g.nodes.size(), 0);
  for (const Fighter::Bridge& b : f.bridges)
    if (b.in)
      for (const int32_t n : b.nodes) on[static_cast<size_t>(n)] = 1;
  std::vector<sh::NavExtraLink>& shut = f.bot.pilot.pilot.shut_links;
  shut.clear();
  for (size_t a = 0; a < g.nodes.size(); a++) {
    const sh::NavNode& n = g.nodes[a];
    for (int32_t k = 0; k < n.links; k++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(k)];
      if (on[static_cast<size_t>(l.to)] && !on[a])
        shut.push_back({static_cast<int32_t>(a), l.to, l.kind, l.time_s});
    }
  }
  f.goal_node = -1;  // planned again
}

// A DOOR ON THE WAY (`locked`): the doors only something else opens, while they rest shut. When
// the route planned crosses one a known button opens, the button is gone to (round the door: no
// route through it meanwhile) and pressed (walked into, or shot: button_poke_cmd) before the goal,
// as a person hits the button by a shut door (dm2's yellow armour and its teleporter, behind a
// touch button's doors). A button no way reaches but through its own door (pressed from the far
// side) leaves that door off the routes a while. A door no known button opens (a trigger walked
// into opens it, or nothing does) is waited at as before (the pilot bars it after `wait_s`).
bool door_resting(const Fighter& f, const server::RulesCommon& s, int ent) {
  for (size_t k = 0; k < f.locked.size(); k++)
    if (f.locked[k] == ent)
      return dist(s.world.entities[static_cast<size_t>(ent)].origin, f.locked_rest[k]) < 1;
  return false;
}

// Whether this fighter runs the doors' errands: `locked`, and its own player (a companion and
// the campaign's leader keep their own doors: companion.cpp, way_through.cpp).
bool locked_errands(const Fighter& f) {
  return f.features[sh::Feature::locked] && f.escort.leader < 0;
}

void door_errand(Fighter& f, const server::RulesCommon& s, double t) {
  sh::Pilot& p = f.bot.pilot.pilot;
  p.shut.clear();
  if (f.door_button >= 0) {
    const bool open = !door_resting(f, s, f.door_ent);
    // No way to the button but through its door: that door off the routes a while.
    const bool no_way = p.mode == sh::PilotMode::idle &&
                        f.goal_node == f.button_spots[static_cast<size_t>(f.door_button)].node &&
                        p.route.empty();
    if (!open && !no_way && t < f.door_until) {
      p.shut.push_back(f.door_ent);  // round it to the button
      return;
    }
    if (no_way) {
      f.door_avoid = f.door_ent;
      f.door_avoid_until = t + kDoorAvoid_s;
      f.reach_at = -1;  // where it can get to, worked out again without the door
    }
    f.door_button = -1;
    f.door_ent = -1;
    f.poke = -1;
    f.goal_node = -1;  // the goal planned again, through the door now open (or round it)
  }
  if (f.door_avoid >= 0 && t < f.door_avoid_until && door_resting(f, s, f.door_avoid))
    p.shut.push_back(f.door_avoid);
  // A shut one on the route planned: its button first.
  const sh::NavGraph& g = *p.g;
  for (size_t i = p.k; i + 1 < p.route.size(); i++) {
    const sh::NavNode& n = g.nodes[static_cast<size_t>(p.route[i])];
    int32_t mover = -1;
    for (int32_t j = 0; j < n.links; j++) {
      const sh::NavLink& l = g.links[static_cast<size_t>(n.first_link) + static_cast<size_t>(j)];
      if (l.to == p.route[i + 1]) mover = l.mover;
    }
    if (mover < 0 || !door_resting(f, s, mover)) continue;
    for (size_t b = 0; b < f.button_spots.size(); b++) {
      const ButtonSpot& bs = f.button_spots[b];
      bool opens = false;
      for (const int d : bs.doors) opens |= d == mover;
      if (!opens || bs.node < 0 || (f.reach_at >= 0 && !f.reach.reached(bs.node))) continue;
      f.door_button = static_cast<int32_t>(b);
      f.door_ent = mover;
      f.door_until = t + kDoorErrand_s;
      f.poke = f.door_button;
      f.poke_until = f.door_until;
      f.stats.door_errands++;
      p.shut.push_back(mover);
      return;
    }
  }
}

void think(Fighter& f, const server::RulesCommon& s, int me, const Stats& st, ai::Why* why) {
  const double t = s.sv.time;
  const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  watch_bridges(f, s);
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};

  // Where it can get to from here, worked out once a second (a teleporter or a drop is one way).
  if (f.reach_at < 0 || t - f.reach_at >= 1) {
    const sh::NavGraph& g = *f.bot.pilot.pilot.g;
    // Rocket jumps only while it can afford one: the launcher and a rocket, the health for the
    // blast (about half a rocket's 120), a player who moves well enough to try (nav.hpp). Grenade
    // jumps only when it must: the grenade launcher and no rocket launcher (dm2's quad with no
    // launcher on the team), on the same terms.
    sh::NavJumper& jumper = f.bot.pilot.pilot.set.jumper;
    const float moves = f.traits[sh::Trait::moves];
    const BotArsenal& arsenal = *f.arsenal;
    const float rockets = ammo_of(st, arsenal.launcher_ammo);
    const bool afford = rockets >= 1 && st.health >= kRocketJumpHealth && f.escort.leader < 0 &&
                        moves >= kRocketJumpMoves;
    jumper.rockets = (st.items & arsenal.launcher) != 0 && afford && f.features[sh::Feature::rj];
    jumper.grenades = (st.items & arsenal.lob) != 0 && (st.items & arsenal.launcher) == 0 &&
                      afford && f.features[sh::Feature::gj];
    jumper.level = moves >= kRocketLevelMoves[2] ? 2 : moves >= kRocketLevelMoves[1] ? 1 : 0;
    jumper.extra_s = kRocketLevelExtra_s[jumper.level];
    jumper.scripts = f.features[sh::Feature::routes];  // the map's scripted ways to key items
    // The map's gap tricks (shooter/nav_gaps.hpp): a hopper's, out of its hop, and planned only
    // while it already moves near a gap's speed (a slow one would walk up to it and give it up).
    const float across_speed =
        std::sqrt(self.velocity.x * self.velocity.x + self.velocity.y * self.velocity.y);
    jumper.gaps = f.bot.pilot.pilot.set.hop && f.features[sh::Feature::gapjump] &&
                  across_speed >= kGapPlanSpeed;
    // Slower, one far enough along the route to hop up to its speed first (`gapbuild`).
    jumper.gap_build_s = f.bot.pilot.pilot.set.hop && f.features[sh::Feature::gapjump] &&
                                 f.features[sh::Feature::gapbuild]
                             ? kGapBuild_s
                             : -1;
    // Out of a pit by rocket (shooter/escape.hpp): the launcher and a rocket; in water the health
    // for the blast too (lava and slime kill whoever stays, so any health tries it there).
    int liquid = sh::kContentsEmpty;
    if (self.waterlevel >= 1 && f.bot.pilot.world != nullptr)
      sh::water_level(*f.bot.pilot.world, self.origin, &liquid);
    const bool burns = liquid == sh::kContentsLava || liquid == sh::kContentsSlime;
    jumper.escapes = (st.items & arsenal.launcher) != 0 && rockets >= 1 &&
                     (burns || st.health >= kRocketJumpHealth) && f.features[sh::Feature::escape];
    // The doors its own errands keep off the routes kept off the reach too (door_errand).
    const std::span<const int32_t> shut = locked_errands(f)
                                              ? std::span<const int32_t>(f.bot.pilot.pilot.shut)
                                              : std::span<const int32_t>();
    f.reach_n = sh::nav_reach(g, sh::nav_nearest(g, self.origin), f.reach,
                              f.bot.pilot.pilot.set.jumper, shut);
    f.reach_at = t;
  }
  bool escaping = false;  // on the way out of a pocket: no other goal this think
  // A dive is chosen again every think (go_item); one under way keeps the escape out of it.
  const bool was_diving = f.diving >= 0;
  // A dive once in the water is swum to its end: no other goal turns it round under water (only
  // fighter_command gives it up, late or short of air).
  const int32_t committed = was_diving && self.waterlevel >= 1 ? f.diving : -1;
  f.diving = -1;
  // Off the graph (knocked onto a ledge no proof stands on): back to the nearest node by sight
  // of the plan, below rather than above, walked straight at with a hop now and then.
  {
    const sh::NavGraph& g = *f.bot.pilot.pilot.g;
    f.lost_to = -1;
    // Where it stands reaches only a small share of the map (a pool or a pit the links prove no
    // swim or climb out of): it heads for the nearest node outside what it can reach, swimming up
    // as well as across.
    // A pocket it came into for an item still there (the launcher's room a dive leads into) is no
    // pit: it is left once the item is taken, or another goal outside it wins.
    const bool wanted_here = f.goal_item >= 0 &&
                             f.items[static_cast<size_t>(f.goal_item)].present &&
                             f.items[static_cast<size_t>(f.goal_item)].node >= 0 &&
                             f.reach.reached(f.items[static_cast<size_t>(f.goal_item)].node);
    const bool pool = !was_diving && !wanted_here && f.reach_at >= 0 &&
                      static_cast<size_t>(f.reach_n) * 4 < g.nodes.size();
    const bool swim = self.waterlevel >= 2;
    sh::Vec3f way;
    f.swimming = !was_diving && f.swim && (self.waterlevel >= 1 || pool) &&
                 (pool || sh::nav_nearest(g, self.origin) < 0) &&
                 sh::swim_next(*f.swim, g, self.origin, way);
    if (f.swimming) {
      f.lost++;
    } else {
      // Dry in a pocket whose way out is under water: into the water at its nearest way out, by a
      // route when one leads there.
      const auto wet_node = [&](size_t n) {
        sh::Vec3f to;
        return sh::swim_next(*f.swim, g, g.nodes[n].origin, to);
      };
      if (pool && f.swim) {
        int32_t pick = -1;
        float best = 0;
        for (size_t n = 0; n < g.nodes.size(); n++) {
          if (!f.reach.reached(static_cast<int32_t>(n)) || !wet_node(n)) continue;
          const float d = dist(g.nodes[n].origin, self.origin);
          if (pick < 0 || d < best) {
            pick = static_cast<int32_t>(n);
            best = d;
          }
        }
        if (pick >= 0) {
          f.lost++;
          f.goal_item = -1;
          go_to(f, pick);
          escaping = true;
        }
      }
      if (!escaping && !was_diving && (pool || sh::nav_nearest(g, self.origin) < 0)) {
        f.lost++;
        // The nearest few by distance; the first of them in a clear line is walked or swum at (out
        // of a pocket with water: the nearest nodes over water).
        constexpr int kLook = 64;
        int32_t near[kLook];
        float near_d[kLook];
        int nn = 0;
        for (size_t n = 0; n < g.nodes.size(); n++) {
          const sh::Vec3f d = g.nodes[n].origin - self.origin;
          if (pool && f.reach.reached(static_cast<int32_t>(n))) continue;
          if (pool && f.swim && !wet_node(n)) continue;
          if (d.z > (swim || pool ? 240 : 48)) continue;
          const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
          if (nn == kLook && dd >= near_d[kLook - 1]) continue;
          int k = nn < kLook ? nn++ : kLook - 1;
          for (; k > 0 && near_d[k - 1] > dd; k--) {
            near[k] = near[k - 1];
            near_d[k] = near_d[k - 1];
          }
          near[k] = static_cast<int32_t>(n);
          near_d[k] = dd;
        }
        for (int k = 0; k < nn && f.lost_to < 0; k++) {
          const sh::HullTrace tr = sh::trace_move(s.sv, self.origin, {-16, -16, -24}, {16, 16, 32},
                                                  g.nodes[static_cast<size_t>(near[k])].origin,
                                                  sh::kTraceNoMonsters, me);
          if (tr.fraction >= 1) f.lost_to = near[k];
        }
        if (f.lost_to < 0 && nn > 0) f.lost_to = near[0];
        if (!pool) f.reach_at = -1;  // worked out again once back on
      }
    }
  }

  // The items, as far as it can see them (knowledge.hpp); a key item taken is called out.
  ItemNews news = knowledge_update(f, s, me, eye, st, t);
  // And as far as it can hear them (ears.hpp): a pickup heard is an item taken, its taker placed.
  const EarNews ears = fighter_hear(f, s, me, eye, t, f.mode == FightMode::fight);
  if (news.taken < 0) news.taken = ears.taken;
  if (news.taken >= 0) {
    const ItemSpot& it = f.items[static_cast<size_t>(news.taken)];
    Said said;
    said.item = item_call(it);
    if (key_item(it) && news.by_me) {
      said.kind = SayKind::took;
      mind_say(f, t, said, it.at);
    } else if (key_item(it) && it.timed) {
      said.kind = SayKind::timer;
      said.secs = static_cast<int>(it.respawn_s);
      mind_say(f, t, said, it.at);
    }
  }

  // A co-op companion (companion.hpp) chooses its own mode and goal: the leader, not the items.
  if (f.escort.leader >= 0) {
    if (committed >= 0) {
      f.diving = f.last_dive = committed;
    } else if (!escaping) {
      escort_think(f, s, me, st, t, why);
    }
    if (f.diving < 0) f.last_dive = -1;
    const sh::Contact* foe = sh::freshest(f.bot.senses, eye);
    // No blast where it would reach a player (the companion itself among them): another weapon.
    const bool blast_ok = !foe || escort_blast_clear(f, s, at_of(foe->seen));
    choose_weapon(f, st, foe ? dist(eye, at_of(foe->seen)) : 400.f, t, why, blast_ok);
    return;
  }

  const sh::Contact* c = sh::freshest(f.bot.senses, eye);
  const bool seen_now = c && c->visible;
  if (seen_now) {  // where it saw an enemy itself (the push reads it, mind.hpp)
    f.mind.sighted_at = at_of(c->seen);
    f.mind.sighted_t = t;
  }
  // Lost from sight a moment ago, or heard a moment ago (the newer of the two is where it is).
  const bool by_ear = c && ai::known(c->heard) && (!ai::known(c->seen) || c->heard.at > c->seen.at);
  // The odds as this player sizes them (temper.hpp): his might against the enemy's as he believes
  // it. His temperament says which fights he takes, and whether he goes after a sound at all;
  // anyone else hears it, keeps away from it and goes on with his items.
  const float mine = sh::might(own_might(st));
  const float theirs = c ? foe_might(f, c->ent, t) : 0;
  // An enemy in sight this close is fought whatever the odds: turning away only shows him a back.
  const bool cornered =
      seen_now &&
      dist(eye, at_of(c->seen)) < (f.temper == sh::Temper::coward ? kCowardCornered : kCornered);
  const bool engage = c && (cornered || sh::takes_fight(f.temper, f.edge, mine, theirs));
  const bool recent = c && ((ai::known(c->seen) && ai::age(c->seen, t) <= kChase_s && engage) ||
                            (by_ear && ai::age(c->heard, t) <= kHeardChase_s &&
                             ai::age(c->heard, t) >= f.hear_react_s &&
                             sh::chases_sound(f.temper, f.edge, f.rash, mine, theirs)));
  const sh::Vec3f enemy =
      c ? at_of(by_ear || !ai::known(c->seen) ? c->heard : c->seen) : sh::Vec3f{};
  const float range = c ? dist(eye, enemy) : 400;

  // The mode.
  const float strong = strength(st);
  const float below = sh::retreat_below(f.temper);
  const bool weak = f.mode == FightMode::retreat
                        ? strong < std::fmin(below + (kStrongAgain - kRetreatBelow), 100.f)
                        : strong < below;
  const int32_t heal_at =
      c && weak ? best_item(f, s, st, self.origin, t, true, &enemy, nullptr) : -1;
  // Help is called over a key area being lost (bots/mind.hpp `wants_help`).
  if (f.team && t - f.mind.help_said_at >= 15 && wants_help(f, self.origin, weak, t)) {
    f.mind.help_said_at = t;
    Said help;
    help.kind = SayKind::help;
    help.status = status_of(st);
    mind_say(f, t, help, self.origin);
  }
  f.outmatched = false;
  // A coward backs off when weak with someone about, health worth going for or not.
  const bool backs_off =
      (heal_at >= 0 && item_value(f.items[static_cast<size_t>(heal_at)], st) > 0) ||
      (c && weak && f.temper == sh::Temper::coward);
  // A PERSON'S BEAT (`beat`): a fight is held through a peek (the contact out of sight no longer
  // than a beat, and nothing calls for a retreat), and any mode a beat after it is entered unless
  // a sighting calls for a fight. Without it a contact lost for a frame turned a fight into a
  // chase and back: dm2, 3 ten-minute seeds, the mode's changes 0.76 -> 0.46 a bot-second, its
  // median hold 0.42 -> 1.04 s (under 0.2 s: 20 -> 5 %); 12 seeds head to head +1.5 frags a
  // match, speed even.
  const bool beat = f.features[sh::Feature::beat];
  const bool peek = beat && f.mode == FightMode::fight && !seen_now && !backs_off && c &&
                    ai::known(c->seen) && ai::age(c->seen, t) <= kModeBeat_s;
  const bool fresh = beat && t - f.mode_at < kModeBeat_s;
  if (peek) {
    // the fight held: the enemy out of sight a moment
  } else if (backs_off && !cornered) {
    if (!fresh || f.mode == FightMode::roam)
      set_mode(f, FightMode::retreat, t, why, "weak with someone about: health or armour first");
  } else if (seen_now && !engage) {
    f.outmatched = true;
    f.stats.outmatched++;
    if (!fresh || f.mode == FightMode::roam)
      set_mode(f, FightMode::retreat, t, why, "outmatched: armour, a weapon or the team first");
  } else if (seen_now) {
    set_mode(f, FightMode::fight, t, why, "a contact in sight");
  } else if (recent) {
    if (!seen_now && by_ear) f.stats.sound_chases++;
    if (!fresh) set_mode(f, FightMode::chase, t, why, "lost from sight or heard a moment ago");
  } else if (!fresh) {
    set_mode(f, FightMode::roam, t, why, "no one known");
  }

  // A sighting is the team's once it is called out (where, by its place's name), unless it was
  // called lately, or a mate's line just said so.
  if (f.team && seen_now && t - f.called_at >= kCallAgain_s) {
    const sh::Vec3f there = at_of(c->seen);
    Said en;
    en.kind = SayKind::enemy;
    en.loc = place_of(f, there);
    if (!en.loc.empty() && mind_say(f, t, en, there)) {
      f.called_at = t;
      f.team->stats.callouts++;
    }
  }

  // A short goal on the way (on_the_way), held until it is had, gone or kShortHold_s has passed,
  // unless the goal itself is that near, the way is a dive's, an escape's or the map's, or it
  // backs off (a retreat chooses its health away from the enemy, best_item):
  // people's small health 3.6 a player-minute against the bots' 0.5 without it.
  int32_t near = -1;
  if (!escaping && committed < 0 && f.objective < 0 && f.diving < 0 &&
      f.mode != FightMode::retreat && f.features[sh::Feature::shortgoals]) {
    const bool held = f.short_item >= 0 && t < f.short_until &&
                      f.items[static_cast<size_t>(f.short_item)].present;
    if (held) {
      near = f.short_item;
    } else if (f.goal_item < 0 ||
               dist(f.items[static_cast<size_t>(f.goal_item)].at, self.origin) > kOnTheWay) {
      near = on_the_way(f, s, st, self.origin, self.velocity, t);
      if (near >= 0) {
        f.short_until = t + kShortHold_s;
        f.stats.on_the_way++;
      }
    }
  }
  f.short_item = near;

  // A shut door on the way: its button first (door_errand).
  if (locked_errands(f) && !escaping && committed < 0 && f.objective < 0) door_errand(f, s, t);

  // The goal: the role's first, then the items.
  if (committed >= 0) {
    // Kept on the dive's field whatever the reach from the water says (the nearest node under
    // water may stand in the pocket already).
    f.diving = f.last_dive = committed;
  } else if (!escaping && f.objective >= 0 && f.mode != FightMode::retreat) {
    go_to(f, f.objective);  // the way through the map (way_through.hpp) before any item
    f.goal_item = -1;       // and no item's errand (a shot at its button) kept from before
    f.unlocking = false;
  } else if (!escaping && f.door_button >= 0) {
    go_to(f, f.button_spots[static_cast<size_t>(f.door_button)].node);  // a shut door's button
  } else if (near >= 0) {
    // On to the long goal through it (`chains`), when that is there to take.
    const int32_t long_goal = f.goal_item != near ? f.goal_item : f.chain_item;
    const int32_t after = long_goal >= 0 && long_goal != near &&
                                  f.items[static_cast<size_t>(long_goal)].present &&
                                  f.features[sh::Feature::chains]
                              ? long_goal
                              : -1;
    go_item(f, s, near, after);
    if (why) {
      why->decide("on_the_way");
      why->fact("item", near);
      why->choose(f.items[static_cast<size_t>(near)].name.c_str(), "worth having, by its way");
    }
  } else if (!escaping && !team_goal(f, s, st, self.origin, t, c, why)) {
    if (f.mode == FightMode::chase && c) {
      go_to(f, sh::nav_nearest(*f.bot.pilot.pilot.g, enemy));
    } else if (f.mode == FightMode::roam && button_poke(f, s, me, t)) {
      go_to(f, f.button_spots[static_cast<size_t>(f.poke)].node);  // a novice's whim
    } else {
      // Outmatched, anything that evens the odds will do, away from the enemy.
      // Weak with nobody about, it recovers too, rather than walk into the next fight for the
      // item worth most.
      const bool heal =
          (f.mode == FightMode::retreat && !f.outmatched) || (f.mode == FightMode::roam && weak);
      // Committed: the goal held while nothing new has happened and its item is still to be had;
      // only news, or a goal gone, makes the player think again (as a person does).
      if (st.health < f.news_health) f.blows++;
      f.news_health = st.health;
      const int64_t now_news = f.bot.senses.stats.acquired + f.stats.taken + f.stats.heard_taken +
                               f.stats.heard_spawns + f.mind.read + f.stats.pickups +
                               f.stats.respawns + f.blows;
      const bool kept = f.features[sh::Feature::commit] && f.goal_item >= 0 &&
                        now_news == f.decided_news && t - f.decided_at < kDecide_s &&
                        f.items[static_cast<size_t>(f.goal_item)].present && !heal;
      int32_t i = f.goal_item;
      if (!kept) {
        i = best_item(f, s, st, self.origin, t, heal, c ? &enemy : nullptr, why);
        f.decided_news = now_news;
        f.decided_at = t;
      }
      const int32_t mate = f.outmatched ? nearest_mate_node(f, self.origin, t) : -1;
      if (i < 0 && mate >= 0) {
        go_to(f, mate);  // nothing to take: to the team
      } else if (i >= 0) {
        go_item(f, s, i, chain_after(f, s, st, i, t, heal, c ? &enemy : nullptr));
      } else if (f.sense == MapSense::novice && (!f.team || f.mind.claim < 0)) {
        // Nothing in mind: a novice walks the map to learn it.
        go_to(f, explore_node(f, self.origin));
      } else if (f.team && f.mind.claim >= 0 &&
                 f.reach.reached(f.items[static_cast<size_t>(f.mind.claim)].node)) {
        // Nothing worth having: back to its own area.
        go_to(f, f.items[static_cast<size_t>(f.mind.claim)].node);
      }
    }
  }

  // A goal no way leads to any more (the way there was a drop, or a teleporter): the nearest item
  // there is a way to and that is not underfoot, so the player never stands planning in vain.
  if (!escaping && f.lost_to < 0 && f.reach_at >= 0 && f.objective < 0 &&
      (f.goal_node < 0 || !f.reach.reached(f.goal_node))) {
    int32_t pick = -1;
    float best = 0;
    for (size_t i = 0; i < f.items.size(); i++) {
      const ItemSpot& it = f.items[i];
      if (it.node < 0 || !f.reach.reached(it.node) || !item_in_mind(f, static_cast<int32_t>(i)))
        continue;
      const float d = dist(it.at, self.origin);
      if (d < 200) continue;
      if (pick < 0 || d < best) {
        pick = static_cast<int32_t>(i);
        best = d;
      }
    }
    if (pick >= 0) {
      go_item(f, s, pick);
    } else {
      go_to(f, explore_node(f, self.origin));  // nothing it knows of: it walks to learn the map
    }
  }

  if (f.diving < 0) f.last_dive = -1;
  choose_weapon(f, st, range, t, why);
  // A blast jump on the way next: its launcher in hand for it (a rocket jump's, a grenade
  // jump's).
  const sh::Pilot& pl = f.bot.pilot.pilot;
  if ((pl.set.jumper.rockets || pl.set.jumper.grenades || pl.set.jumper.escapes) &&
      t - f.impulse_at >= kImpulseGap_s)
    for (size_t k = pl.k; k + 1 < pl.route.size() && k < pl.k + 3; k++) {
      const sh::NavNode& from = pl.g->nodes[static_cast<size_t>(pl.route[k])];
      int number = 0;  // the launcher's
      for (int32_t q = 0; q < from.links; q++) {
        const sh::NavLink& l =
            pl.g->links[static_cast<size_t>(from.first_link) + static_cast<size_t>(q)];
        if (l.to != pl.route[k + 1]) continue;
        const BotArsenal& arsenal = *f.arsenal;
        const BotWeapon* rl = weapon_row(arsenal, arsenal.launcher);
        const BotWeapon* gl = weapon_row(arsenal, arsenal.lob);
        if (l.kind == sh::NavKind::rocket && rl && st.weapon != rl->bit) number = rl->number;
        if (l.kind == sh::NavKind::grenade && gl && st.weapon != gl->bit) number = gl->number;
      }
      if (number > 0) {
        f.want_weapon = number;
        break;
      }
    }
}

// A living teammate within kFireClearance of the line from `eye` to `target`, nearer than it.
bool mate_in_line(const Fighter& f, const server::RulesCommon& s, sh::Vec3f eye, sh::Vec3f target) {
  if (!f.team) return false;
  const sh::Vec3f line = target - eye;
  const float len2 = line.x * line.x + line.y * line.y + line.z * line.z;
  if (len2 < 1) return false;
  for (const Fighter* m : f.team->members) {
    if (m == &f || m->slot < 0 || !alive_slot(s, m->slot)) continue;
    const sh::Vec3f p = s.world.entities[static_cast<size_t>(m->slot) + 1].origin;
    const sh::Vec3f to = p - eye;
    const float u = (to.x * line.x + to.y * line.y + to.z * line.z) / len2;
    if (u <= 0 || u >= 1) continue;
    const sh::Vec3f off = to - line * u;
    if (off.x * off.x + off.y * off.y + off.z * off.z < kFireClearance * kFireClearance)
      return true;
  }
  return false;
}

// A living teammate within `reach` of `at` (a blast's aim: its splash would hurt him).
bool mate_near(const Fighter& f, const server::RulesCommon& s, sh::Vec3f at, float reach) {
  if (!f.team) return false;
  for (const Fighter* m : f.team->members) {
    if (m == &f || m->slot < 0 || !alive_slot(s, m->slot)) continue;
    if (dist(s.world.entities[static_cast<size_t>(m->slot) + 1].origin, at) < reach) return true;
  }
  return false;
}

}  // namespace

void fighter_go(Fighter& f, int32_t node) { go_to(f, node); }

float fighter_strength(const Stats& st) {
  return st.health + protection(st.armortype, st.armor) +
         ((st.items & st.arsenal->launcher) ? 50.f : 0.f);
}

Stats read_stats(const server::RulesCommon& s, int ent, const BotArsenal& arsenal) {
  const server::PlayerNumbers& n = s.world.clients[static_cast<size_t>(ent) - 1].numbers;
  Stats st;
  st.arsenal = &arsenal;
  st.health = n.health;
  st.armor = n.armour;
  st.armortype = n.armour_type;
  arsenal.read(n, st);
  return st;
}

sh::PilotSettings fighter_pilot(const sh::Features& features, const sh::Traits& traits,
                                sh::AimSkill skill, sh::PilotHands& hands) {
  // Moves: an expert's the bunny hop on practised hands (shooter/pilot.hpp); an average player's
  // the simple hop (forward and the side key of the turn) on steady hands; a novice's a walk and a
  // strafe on a person's hands.
  const sh::AimSkill moves = sh::tier_of(traits[sh::Trait::moves]);
  const bool expert = moves == sh::AimSkill::expert;
  const bool average = moves == sh::AimSkill::average;
  sh::PilotSettings ps;
  ps.hop = (expert || average) && features[sh::Feature::hop];
  ps.hop_simple = average;
  ps.hop_stairs_walked = features[sh::Feature::stairs];
  ps.slopes = features[sh::Feature::slopes];
  ps.backstep = features[sh::Feature::backstep];
  ps.runup_keys = features[sh::Feature::runupkeys];
  ps.gap_circle = features[sh::Feature::gapbuild];
  ps.curve_keys = features[sh::Feature::keys] && features[sh::Feature::curvekeys];
  ps.under_way = features[sh::Feature::underway];
  ps.retarget = features[sh::Feature::retarget];
  ps.hop_headroom = features[sh::Feature::headroom];
  ps.hop_lips = features[sh::Feature::liphop];
  // The circle jump by skill: an expert curves up to 0.3 s for ~430 (people's standing jumps
  // take off at 433, p50), an average player a shorter curve for less (people's p25: 377).
  ps.hop_circle = features[sh::Feature::circlejump];
  ps.circle.target = expert ? 430.f : 380.f;
  ps.circle.most_s = expert ? 0.3f : 0.2f;
  // People's hop starts (`recipes`): a hopper copies its level's out of the book.
  if (ps.hop && features[sh::Feature::recipes]) ps.recipes = hop_start_book();
  ps.recipe_level = expert ? 2 : average ? 1 : 0;
  ps.recipe_keys_s = features[sh::Feature::keys] ? (expert ? 0.03f : average ? 0.04f : 0.05f) : 0.f;
  ps.brink = features[sh::Feature::brink];
  ps.ahead = features[sh::Feature::ahead];
  ps.ahead_bodies = features[sh::Feature::bodies];
  // Its lane off the route's line (shooter/pilot.hpp): a person never walks the line; the less
  // practised wander wider. A perfect player keeps to it.
  ps.lane = skill == sh::AimSkill::perfect || !features[sh::Feature::lane] ? 0.f
            : expert                                                       ? 0.45f
            : average                                                      ? 0.6f
                                                                           : 0.75f;
  ps.air_steer = features[sh::Feature::airsteer];
  hands = expert    ? sh::PilotHands::practised
          : average ? sh::PilotHands::steady
                    : sh::PilotHands::human;
  return ps;
}

void fighter_init(Fighter& f, const sh::NavGraph& g, const server::RulesCommon& s, int slot,
                  const char* entities, sh::AimSkill skill, const sh::Traits& traits, double hz,
                  uint32_t seed) {
  sh::PilotHands hands = sh::PilotHands::none;
  const sh::PilotSettings ps = fighter_pilot(f.features, traits, skill, hands);
  // The aim: the scattergun's to start with, another weapon's taken up as it is
  // (`fighter_command`).
  f.aim_slot = 0;
  sh::bot_init(f.bot, g, ps, hands,
               skill == sh::AimSkill::perfect
                   ? sh::aim_profile(skill)
                   : sh::aim_profile(traits[sh::Trait::aim_sg], traits[sh::Trait::reaction]),
               hz, seed);
  f.slot = slot;
  f.skill = skill;
  f.traits = traits;
  f.temper = sh::temper_of(traits);
  f.sense = map_sense_of(sh::tier_of(traits[sh::Trait::knowledge]));
  f.timing = map_sense_of(sh::tier_of(traits[sh::Trait::timing]));
  f.comms = map_sense_of(sh::tier_of(traits[sh::Trait::comms]));
  f.teamwork = map_sense_of(sh::tier_of(traits[sh::Trait::teamplay]));
  f.hearing = map_sense_of(sh::tier_of(traits[sh::Trait::hearing]));
  f.camp = map_sense_of(sh::tier_of(traits[sh::Trait::camping]));
  f.rash = sh::tier_of(traits[sh::Trait::discipline]) == sh::AimSkill::novice;
  f.edge = sh::edge_of(sh::mean_aim(traits));
  f.hear_react_s =
      f.bot.aim.react_med > 0
          ? f.bot.aim.react_med +
                static_cast<float>(sh::scale(traits[sh::Trait::reaction], {0.2, 0.15, 0.1, 0.08}))
          : 0;
  f.rng = ge::core::make_rng(seed * 2654435761u + 12345u);
  f.keys_rng = ge::core::make_rng(seed * 2246822519u + 777u);
  f.clean_keys = static_cast<float>(sh::scale(traits[sh::Trait::moves], {0.08, 0.2, 0.4, 0.5}));
  fighter_keyboard_init(f, traits[sh::Trait::moves], seed);
  sh::dodge_init(f.dodge, seed);
  f.press_rng = ge::core::make_rng(seed ^ 0x2545f491u);
  f.dodge_k = sh::dodge_skill(traits[sh::Trait::dodge]);
  const int me = slot + 1;
  sh::client_view(s.sv, me, f.bot.view);
  std::vector<pg::EntityBlock> blocks;
  std::string error;
  f.items.clear();
  if (!pg::parse_entity_text(entities, blocks, error)) return;
  for (const pg::EntityBlock& b : blocks) {
    const char* cn = pg::block_value(b, "classname");
    if (!cn) continue;
    const char* flags = pg::block_value(b, "spawnflags");
    ItemSpot it;
    if (!item_of(cn, flags ? std::atoi(flags) : 0, it, *f.arsenal)) continue;
    sh::Vec3f origin;
    parse_vec(pg::block_value(b, "origin"), origin);
    // The entity sent nearest the map's place, across the floor.
    float best = 48;
    for (const sh::ViewEntity& v : f.bot.view.entities) {
      if (v.ent <= s.sv.set.maxclients) continue;
      const server::WorldEntity& e = s.world.entities[static_cast<size_t>(v.ent)];
      // The middle of its box on the floor; its origin when its row carries no box (the sequel's).
      const bool boxed = e.absmax.x > e.absmin.x || e.absmax.z > e.absmin.z;
      const sh::Vec3f mid = boxed ? sh::Vec3f{(e.absmin.x + e.absmax.x) * 0.5f,
                                              (e.absmin.y + e.absmax.y) * 0.5f, e.absmin.z}
                                  : e.origin;
      const float dx = v.origin.x - origin.x, dy = v.origin.y - origin.y;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d < best) {
        best = d;
        it.ent = v.ent;
        it.at = mid;
      }
    }
    if (it.ent < 0) continue;
    it.node = sh::nav_nearest(g, {it.at.x, it.at.y, it.at.z + 24});
    f.items.push_back(it);
  }
  // The map's item rules (item_routes.hpp): an item no goal while another is to be had first.
  for (const ItemRule& r : item_rules(s.map_name))
    for (ItemSpot& it : f.items) {
      const float dx = it.at.x - r.x, dy = it.at.y - r.y, dz = it.at.z - r.z;
      if (std::strcmp(item_call(it), r.item) != 0 || dx * dx + dy * dy + dz * dz > 48 * 48)
        continue;
      for (size_t q = 0; q < f.items.size(); q++)
        if (std::strcmp(item_call(f.items[q]), r.after) == 0) {
          it.after = static_cast<int32_t>(q);
          it.after_due_s = r.due_s;
          break;
        }
    }
  knowledge_init(f);
  buttons_init(f, g, s, entities);
  exits_init(f, g, s, entities);
  ears_init(f, sh::tier_of(traits[sh::Trait::hearing]), entities);
  spam_init(f, entities);
  // A screen 120 degrees across (`screen`), or the round cone of the senses' own setting.
  if (f.features[sh::Feature::screen]) {
    f.bot.senses.set.fov = kViewWide;
    f.bot.senses.set.fov_y = kViewHigh;
  }
  f.bot.keep = f.features[sh::Feature::keep];
  f.bot.paced = f.features[sh::Feature::pace];
  f.bot.trial = f.features[sh::Feature::trial];
  // A change of course read a reaction late (`eye`), and the aim from its eye ahead (`selfaim`).
  f.bot.senses.set.course_s = f.features[sh::Feature::eye] ? f.bot.aim.react_med : 0;
  f.bot.selfaim = f.features[sh::Feature::selfaim];
  f.bot.corner = f.features[sh::Feature::corner];
  f.bot.glance = f.features[sh::Feature::glance];
  f.bot.lookway = f.features[sh::Feature::lookway];
  f.bot.listen = f.features[sh::Feature::listen];
  f.bot.pools = f.bot.pilot.pilot.set.floor_swim = f.features[sh::Feature::pools];
}

sh::MoveCmd fighter_command(Fighter& f, const server::RulesCommon& s, int slot, int msec) {
  const int me = slot + 1;
  const Stats st = read_stats(s, me, *f.arsenal);
  const double t = s.sv.time;
  f.bot.pilot.grenade_ready = st.weapon == f.arsenal->lob;  // a grenade jump fires with it in hand
  f.bot.pilot.rocket_ready = st.weapon == f.arsenal->launcher;  // a rocket out of a pit with this
  // A companion's view is its own (it faces the leader).
  f.bot.calm = f.escort.leader < 0 && f.features[sh::Feature::calm];
  if (f.features[sh::Feature::packs] || f.features[sh::Feature::packinfer])
    packs_hear(f, s, me, t);  // shots heard by who fired, cries, its own shot, packs (packs.hpp)
  ai::Why* why = f.bot.pilot.why ? f.bot.pilot.why->follow(slot, f.bot.pilot.command) : nullptr;
  f.stats.commands++;
  const bool dead = st.health <= 0;
  if (!dead && s.world.entities[static_cast<size_t>(me)].waterlevel >= 2) {
    const auto& e = s.world.entities[static_cast<size_t>(me)];
    f.stats.wet++;
    int64_t* w = f.stats.wet_why;
    w[static_cast<int>(f.mode)]++;
    w[4] += f.bot.aiming;
    const sh::Pilot& pl = f.bot.pilot.pilot;
    const sh::NavGraph* g = pl.g;
    w[5] += g && f.goal_node >= 0 && g->nodes[static_cast<size_t>(f.goal_node)].wet;
    w[6] += e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y < 50 * 50;
    const int how = f.bot.aiming ? 0 : pl.mode == sh::PilotMode::swim ? 1 : 2;
    const double across = std::sqrt(static_cast<double>(e.velocity.x) * e.velocity.x +
                                    static_cast<double>(e.velocity.y) * e.velocity.y);
    f.stats.wet_speed[how] += across;
    f.stats.wet_n[how]++;
    f.stats.wet_still[how] += across < 50;
    w[7] += f.bot.pilot.pilot.mode == sh::PilotMode::swim;
    w[8] += f.bot.pilot.pilot.mode == sh::PilotMode::walk;
    w[9] += e.waterlevel >= 3;
    w[10] += f.role == Role::hold;
    w[11] += g && pl.goal >= 0 && !g->nodes[static_cast<size_t>(pl.goal)].wet;  // passing through
  }
  if (!f.was_dead && dead) {  // where it died: a danger spot (`best_item` heals away from it)
    mind_danger(f, s.world.entities[static_cast<size_t>(me)].origin, t);
    f.bar = Stats{};  // the respawn's full bar is no pickup
  }
  if (f.was_dead && !dead) {
    f.stats.respawns++;
    f.goal_node = -1;
    f.next_think = 0;
  }
  f.was_dead = dead;
  if (!dead) {
    const sh::Vec3f v = s.world.entities[static_cast<size_t>(me)].velocity;
    const double across =
        std::sqrt(static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y);
    // An aimed command is counted apart (as the pilot's `kPilotModes`th mode: under the aim).
    const auto pm = f.bot.aiming ? static_cast<size_t>(sh::kPilotModes)
                                 : static_cast<size_t>(f.bot.pilot.pilot.mode);
    f.stats.speed_sum += across;
    f.stats.mode_speed[pm] += across;
    f.stats.mode_n[pm]++;
  }

  // A goal held too long without getting it (a lift held up top, a way that keeps failing) is
  // let go of for a while: whatever it is worth, nothing else would be had meanwhile.
  if (f.goal_item != f.goal_was) {
    f.goal_was = f.goal_item;
    f.goal_since = t;
  } else if (f.goal_item >= 0 &&
             t - f.goal_since > (f.mode == FightMode::retreat ? kHealTire_s : kGoalTire_s)) {
    f.tired_item = f.goal_item;
    f.tired_until = t + kTired_s;
    f.goal_since = t;
  }
  mega_watch(f, st.health, t);  // the mega it took, timed from its own health (knowledge.hpp)
  mind_plan(f, s, me, st, t);   // its role and its calls, off its own beliefs (mind.hpp)
  if (!dead && t >= f.next_think) {
    f.next_think = t + kThink_s;
    think(f, s, me, st, why);
  }
  if (!dead && f.bot.pilot.pilot.mode == sh::PilotMode::arrived) f.next_think = 0;

  const BotWeapon* held = weapon_row(*f.arsenal, st.weapon);
  // Another kind of weapon taken up is held as its aim trait holds it.
  const int slot_of_aim = !held               ? f.aim_slot
                          : held->number <= 3 ? 0
                          : held->number <= 5 ? 1
                                              : held->number - 4;
  if (slot_of_aim != f.aim_slot && f.skill != sh::AimSkill::perfect) {
    f.aim_slot = slot_of_aim;
    sh::bot_aim(f.bot, sh::aim_profile(f.traits.v[static_cast<size_t>(slot_of_aim)],
                                       f.traits[sh::Trait::reaction]));
  }
  f.bot.shot_speed = held ? held->shot_speed : 0;
  // A novice fires at the plain distance; the rest at where he will be, on his arc in the air.
  f.bot.lob = held && held->bit == f.arsenal->lob;
  const bool leads = f.skill != sh::AimSkill::novice && f.features[sh::Feature::lead];
  f.bot.intercepts = leads ? 3 : 1;
  f.bot.target_arc = leads;
  f.bot.aim_dz = held ? held->aim_dz : 0;
  // A rocket near at hand is aimed at his middle for the direct blow; farther off at his feet,
  // where the blast reaches him whichever way he steps.
  if (held && held->bit == f.arsenal->launcher) {
    for (const sh::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && ai::known(k.seen) &&
          dist(s.world.entities[static_cast<size_t>(me)].origin, at_of(k.seen)) < kRocketNear)
        f.bot.aim_dz = 0;
  }

  if (f.escort.leader >= 0) escort_sense(f, s, me, t);
  if (f.hunts) hunt_sense(f, s);
  if (f.features[sh::Feature::watch])
    watch_look(f, s, me, dead, t, why);  // the ways in watched (watch.hpp)
  // Or one an enemy comes by, a rocket ready (spam.hpp).
  if (f.features[sh::Feature::spam]) spam_news_look(f, s, me, st, t);
  // The weapon's rhythm as its holder knows it: a shot is its ammunition going down, the next
  // ready the weapon's refire after it; the hand times its stroke to that (bot.hpp THE MOUSE
  // HAND), and only the arc's beam is traced.
  f.bot.mouse = f.features[sh::Feature::mouse];
  if (held) {
    const float ammo = ammo_of(st, held->ammo);
    if (st.weapon == f.shot_weapon && ammo < f.shot_ammo) f.ready_at = t + held->refire_s;
    f.shot_weapon = st.weapon;
    f.shot_ammo = ammo;
  }
  f.bot.shot_due_s = std::fmax(0.0, f.ready_at - t);
  f.bot.beam = held && held->bit == f.arsenal->beam;
  sh::MoveCmd cmd = sh::bot_source(f.bot).next(&f.bot, s.sv, slot, msec);
  f.stats.mode_commands[static_cast<int>(f.mode)]++;
  if (held) f.stats.weapon_commands[held->number]++;

  // The last step onto an item: the nav's node beside it is not the item, so once the pilot has
  // arrived the fighter walks the rest of the way straight at it (the axes under the view).
  if (!dead && f.bot.pilot.pilot.mode == sh::PilotMode::arrived && f.goal_item >= 0) {
    const ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    const sh::Vec3f to = f.unlocking ? it.button_at : it.at;
    const double dx = to.x - me_at.x, dy = to.y - me_at.y;
    const double reach = f.unlocking ? 160 : 96;
    const bool on_it = f.goal_node == (f.unlocking ? it.button_node : it.node);
    if (on_it && (it.present || f.unlocking) && dx * dx + dy * dy > 12 * 12 &&
        dx * dx + dy * dy < reach * reach) {
      const double rel = ge::core::libm::atan2(dy, dx) - cmd.angles.y * (ge::core::kPi / 180.0);
      cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
      cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    }
  }

  // A button opened by a shot: from where the line meets it, the view on it and the trigger.
  if (!dead && f.unlocking && f.goal_item >= 0 && !f.bot.aiming) {
    const ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    const sh::Vec3f eye{me_at.x, me_at.y, me_at.z + 22};
    const sh::Vec3f d = it.button_at - eye;
    const double flat = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y);
    if (it.button_shoot && flat < 900) {
      const sh::HullTrace tr =
          sh::trace_move(s.sv, eye, {}, {}, it.button_at, sh::kTraceNoMonsters, me);
      if (tr.fraction >= 1 || tr.ent == it.button_ent) {
        cmd.angles.y =
            static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
        cmd.angles.x =
            static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
        cmd.forward = 0;
        cmd.side = 0;
        f.bot.set_by = "button";
        if (f.stats.commands & 1) cmd.buttons |= sh::kButtonAttack;
      }
    }
  }

  if (!dead && f.features[sh::Feature::dodge])
    fighter_dodge(f, s, me, t, why, cmd);  // the side steps (footwork.hpp)

  sh::Vec3f way;
  const bool swim = water_way(f, s, me, t, msec, why, way);  // every command: it keeps the air
  if (s.world.entities[static_cast<size_t>(me)].waterlevel == 0) f.swim_exit = -1;  // out
  if (!dead && swim) {
    // Swimming the field's way out: the view on the next cell (pitch down positive), forward,
    // and jump held, which rises in water and climbs out over the edge at the top; with an enemy
    // in sight, the view kept on him (below).
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    const double dx = way.x - me_at.x, dy = way.y - me_at.y, dz = way.z - me_at.z;
    const double flat = std::sqrt(dx * dx + dy * dy);
    const sh::Vec3f aim_view = cmd.angles;  // the aim's, while it holds him in sight
    bool fights = false;
    if (f.bot.aiming)
      for (const sh::Contact& k : f.bot.senses.contacts)
        fights |= k.ent == f.bot.target && k.visible;
    if (flat < kSwimWayFlat && f.bot.mouse && f.bot.aim.mouse) {
      // Nearly straight above or below (rising to the surface, sinking onto a lift): the view
      // kept where the hand has it, as a person's is, and the way pressed under it. Turned to,
      // the bearing of a point overhead swings with every unit of drift.
      swim_under_view(cmd, aim_view, dx, dy, dz);
    } else {
      cmd.angles.y = static_cast<float>(ge::core::libm::atan2(dy, dx) * (180.0 / ge::core::kPi));
      cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(dz, flat) * (180.0 / ge::core::kPi));
      cmd.forward = sh::quantize_axis(320);
      cmd.side = 0;
      f.bot.set_by = "swimway";
    }
    if (dz > 8) cmd.buttons |= sh::kButtonJump;
    // In an exit cell, the last leg out as the field proved it (shooter/swim.hpp `swim_out`):
    // chest deep and level, so the move's water jump lifts it over the edge.
    const sh::SwimField* field = f.diving >= 0 && f.items[static_cast<size_t>(f.diving)].dive
                                     ? f.items[static_cast<size_t>(f.diving)].dive
                                     : f.swim;
    int32_t exit_node = -1;
    // Kept once it is in an exit cell, until it is out or kSwimExit_s has passed: the swimmer
    // pressed at the edge drifts across the cell's side and would turn back.
    if (field && sh::swim_exiting(*field, me_at, exit_node) && exit_node >= 0) {
      f.swim_exit = exit_node;
      f.swim_exit_until = t + kSwimExit_s;
    }
    if (f.swim_exit >= 0 && t > f.swim_exit_until) f.swim_exit = -1;
    exit_node = f.swim_exit;
    if (exit_node >= 0) {
      f.landing = exit_node;  // once out, walked onto before the route takes over
      f.landing_until = t + kLanding_s;
      sh::MoveState ms;
      ms.origin = me_at;
      ms.waterlevel = s.world.entities[static_cast<size_t>(me)].waterlevel;
      const sh::MoveCmd out =
          sh::swim_out(ms, f.bot.pilot.pilot.g->nodes[static_cast<size_t>(exit_node)].origin, msec);
      cmd.angles.x = out.angles.x;
      cmd.angles.y = out.angles.y;
      f.bot.set_by = "swimexit";
      cmd.buttons = static_cast<uint8_t>((cmd.buttons & ~sh::kButtonJump) | out.buttons);
    } else if (fights && f.features[sh::Feature::swimaim]) {
      // Fighting in the water: the view stays on him, as a person's does, and the way is swum
      // with the keys under it: forward and side along the view's axes, what they leave of the
      // rise pressed straight up.
      swim_under_view(cmd, aim_view, dx, dy, dz);
      f.bot.set_by = "swimaim";
      f.stats.swim_fights++;
    }
  } else if (!dead && fighter_landing(f, s, me, t, cmd)) {
    // Just climbed out: onto its exit node first (footwork.hpp).
  } else if (!dead && f.lost_to >= 0) {
    const sh::Vec3f to = f.bot.pilot.pilot.g->nodes[static_cast<size_t>(f.lost_to)].origin;
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    const double rel = ge::core::libm::atan2(to.y - me_at.y, to.x - me_at.x) -
                       cmd.angles.y * (ge::core::kPi / 180.0);
    cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
    cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
    if (self.waterlevel >= 2) {
      // Swimming: the view pitched at it (down is positive) and jump held to rise.
      const double flat =
          std::sqrt((to.x - me_at.x) * (to.x - me_at.x) + (to.y - me_at.y) * (to.y - me_at.y));
      const double up = to.z + 16 - me_at.z;
      cmd.angles.x = static_cast<float>(-ge::core::libm::atan2(up, flat) * (180.0 / ge::core::kPi));
      f.bot.set_by = "lost";
      cmd.buttons |= sh::kButtonJump;
    } else if ((f.stats.commands / 48) % 3 == 0) {
      cmd.buttons |= sh::kButtonJump;
    }
  }

  if (dead) {
    // Back in once the mod allows: attack pressed every other command (a press is an edge).
    cmd.buttons = (f.stats.commands & 1) ? sh::kButtonAttack : 0;
    return cmd;
  }
  if (f.want_weapon > 0) {
    cmd.impulse = static_cast<uint8_t>(f.want_weapon);
    f.want_weapon = 0;
    f.impulse_at = t;
    f.stats.switches++;
  }

  if (!dead && f.features[sh::Feature::spam]) {
    spam_fire(f, s, me, st, t, cmd);
    spam_news_fire(f, s, me, st, t, cmd);
  }
  if (!dead && f.features[sh::Feature::packs])
    packs_bore(f, s, me, st, t, cmd);  // its launcher to the quad
  if (!dead) button_poke_cmd(f, s, me, t, cmd);
  if (!dead && f.escort.leader >= 0) escort_command(f, s, me, t, msec, cmd);

  // The trigger: a contact in sight, the view on the aim within the weapon's width at the range.
  bool clicked = false;  // the finger stepped this command
  const sh::Contact* c = nullptr;
  for (const sh::Contact& k : f.bot.senses.contacts)
    if (k.ent == f.bot.target && k.visible) c = &k;
  if (held && c && f.bot.aiming) {
    const server::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
    const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
    const float d = dist(eye, at_of(c->seen));
    // Past the width, how loose the finger is, degrees: a novice sprays at a view well off him,
    // an expert fires on him (the held weapon's aim trait, traits.hpp). The arc's beam is held
    // down longer and swept onto him, as a person holds it through the fight.
    const bool beam = held->bit == f.arsenal->beam;
    const double slack =
        f.skill == sh::AimSkill::perfect ? 1.5
        : beam ? sh::scale(f.traits.v[static_cast<size_t>(f.aim_slot)], {14, 9, 6, 4})
               : sh::scale(f.traits.v[static_cast<size_t>(f.aim_slot)], {10, 7, 3, 2});
    const float tol = static_cast<float>(
        ge::core::libm::atan2(held->width, std::fmax(d, 1.f)) * (180.0 / ge::core::kPi) + slack);
    // Not before the reaction to him is over (shooter/bot.hpp), however the view lies.
    const bool reacting = sh::bot_reacting(f.bot, s.sv.time);
    // The view the command carries, which a swim or a button may have turned off the aim.
    const float off = view_off(f.bot, cmd);
    bool fire = !reacting && d <= held->range && off <= tol;
    // The finger clicks while the view is roughly on him (sweeping onto him), not only on him.
    bool click = !reacting && d <= held->range && off <= tol * kClickWide;
    f.stats.trigger_checks++;
    f.stats.trigger_far += d > held->range;
    f.stats.trigger_off += d <= held->range && off > tol;
    if (fire && (mate_in_line(f, s, eye, at_of(c->seen)) ||
                 (held->blast && mate_near(f, s, at_of(c->seen), kBlastClear)))) {
      fire = click = false;
      f.stats.held_fire++;
      if (f.team) f.team->stats.held_shots++;
    }
    if (fire && f.escort.leader >= 0 &&
        escort_player_in_line(f, s, eye, at_of(c->seen), kFireClearance)) {
      fire = click = false;
      f.stats.held_fire++;
      f.escort.stats.shots_held++;
    }
    // A companion's blast never goes off where it reaches a player: held, and another weapon
    // taken up at once.
    if (f.escort.leader >= 0 && held->blast && !escort_blast_clear(f, s, at_of(c->seen))) {
      if (fire) {
        f.stats.held_fire++;
        f.escort.stats.blasts_held++;
      }
      fire = click = false;
      if (f.want_weapon == 0 && t - f.impulse_at >= kImpulseGap_s)
        choose_weapon(f, st, d, t, nullptr, false);
    }
    // The finger (`mouse`): fire clicked again and again, faster as its health runs out; a
    // stream held. Without it, held down while on him.
    const bool hand = f.bot.mouse && f.bot.aim.mouse;
    const double stress = std::fmin(1.0, std::fmax(0.0, (100.0 - st.health) / 80.0));
    const bool down =
        hand ? f.bot.finger.step(msec / 1000.0, click, held->stream, stress, f.bot.shot_due_s)
             : fire;
    clicked = true;
    if (down) {
      cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
      f.stats.shots++;
    }
    if (why) {
      why->decide("trigger");
      why->fact("range", d);
      why->fact("off", off);
      why->fact("tolerance", tol);
      why->choose(fire ? "fire" : "hold", fire       ? "on the target within its width"
                                          : reacting ? "still reacting to him"
                                                     : "off it");
    }
  }
  if (!clicked) f.bot.finger.step(msec / 1000.0, false, false, 0);  // nothing to fire at: let go
  if (!dead) swim_view(f, s, me, msec, swim, cmd);  // under water the view turns at a hand's pace
  // A view set above (a button, a doorway, the swim's way out) reached by the hand, not jumped
  // to; no shot of its own until the hand is there (shooter/bot.hpp `bot_hand_override`).
  if (!dead) {
    const bool wet = s.world.entities[static_cast<size_t>(me)].waterlevel >= 2;
    if (sh::bot_hand_override(f.bot, cmd, msec, wet) > kHandThere)
      cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonAttack);
  }
  // THE TURN BACK TAKEN ROUND (`uturn`): on the ground at a run, a wish more than `kTurnBack`
  // off the velocity is pressed across it instead (`kTurnAcross` on the side the wish lies, or the
  // view leads), until the velocity has come round: a person turning back curves through it at
  // speed (the ground's acceleration across 300 u/s turns it ~570 degrees a second, a circle of
  // ~32 units) where a straight press back runs through a stop.
  if (!dead && f.features[sh::Feature::uturn]) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(me)];
    const sh::PilotMode pm = f.bot.pilot.pilot.mode;
    const bool feet = pm == sh::PilotMode::walk || pm == sh::PilotMode::arrived ||
                      pm == sh::PilotMode::idle || pm == sh::PilotMode::wait;
    if (feet && d.waterlevel == 0 && (d.flags & sh::kFlagOnGround) != 0 &&
        turn_back_round(cmd, d.origin, d.velocity, f.bot.pilot.world))
      f.stats.turned_round++;
  }
  // THE BRINK: with the feet free, on the ground or in the air between a stair's steps, a hop's
  // landings or a knock, no key carries it into lava or slime (in the air a wish against the
  // run still brakes at the full acceleration: the air's cap is on the speed gained, not lost).
  bool brinked = false;
  if (!dead && f.features[sh::Feature::brink]) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(me)];
    const sh::PilotMode pm = f.bot.pilot.pilot.mode;
    const bool free_feet = pm == sh::PilotMode::walk || pm == sh::PilotMode::strafe ||
                           pm == sh::PilotMode::hop || pm == sh::PilotMode::idle ||
                           pm == sh::PilotMode::arrived || pm == sh::PilotMode::wait;
    // A flight the pilot proved lands safe (a gap trick's, or a hop by one) is not braked.
    if (free_feet && d.waterlevel == 0 && !f.bot.pilot.pilot.brink_free &&
        sh::brink_guard(s.sv, *f.bot.pilot.pilot.g, me, d.origin, d.velocity, cmd).changed) {
      f.stats.brink++;
      brinked = true;
    }
  }
  // The keys: pressed by a hand on a keyboard (keyboard.hpp), or the side key turned round as a
  // person's hand turns it (footwork.hpp).
  if (f.features[sh::Feature::keys]) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(me)];
    fighter_keyboard(f, cmd, d.velocity, (d.flags & sh::kFlagOnGround) != 0, brinked);
  } else {
    fighter_keys(f, cmd);
  }
  return cmd;
}

}  // namespace shooters_edge::bots
