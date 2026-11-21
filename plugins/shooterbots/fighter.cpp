// The fighter (fighter.hpp): its stats, the map's items timed off what is sent, the mode, the goal,
// the weapon and the trigger.
//
// ge:summary The fighter: stats, item timing, mode, goal, weapon and trigger on the baseline mod
// ge:keywords fighter fighter_init fighter_command read_stats item-spot weapon-table tolerance
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooterbots/fighter.hpp"
#include "shooterbots/floor_trap.hpp"

#include <array>
#include <cmath>
#include <cstring>

#include "ai/choose.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"
#include "shooterbots/brink.hpp"
#include "shooterbots/buttons.hpp"
#include "shooterbots/ears.hpp"
#include "shooterbots/fighter_parts.hpp"
#include "shooterbots/foe_ledger.hpp"
#include "shooterbots/footwork.hpp"
#include "shooterbots/hop_start_book.hpp"
#include "shooterbots/keyboard.hpp"
#include "shooterbots/knowledge.hpp"
#include "shooterbots/launcher_fire.hpp"
#include "shooterbots/map_plan.hpp"
#include "shooterbots/packs.hpp"
#include "shooterbots/spam.hpp"
#include "shooterbots/team.hpp"
#include "shooterbots/watch.hpp"
#include "shooterbots/water.hpp"

namespace sb = ge::shooterbots;

namespace ge::shooterbots {

namespace {

// The move's keys as the trace's `keys` column compares them: who changed them last.
struct KeysSeen {
  int16_t forward = 0, side = 0;
  bool jump = false;
};
KeysSeen keys_seen(const sh::MoveCmd& c) {
  return {c.forward, c.side, (c.buttons & sh::kButtonJump) != 0};
}
void keys_mark(Fighter& f, const sh::MoveCmd& c, KeysSeen& was, const char* by) {
  const KeysSeen now = keys_seen(c);
  if (now.forward != was.forward || now.side != was.side || now.jump != was.jump) f.keys_by = by;
  was = now;
}

namespace ai = ge::ai;

using namespace fighter_parts;

constexpr float kRetreatBelow = 50;        // health plus armour's protection: weak under this
constexpr float kStrongAgain = 80;         // and strong again at this
constexpr double kChase_s = 3;             // a contact lost this long ago is still chased
constexpr double kModeBeat_s = 0.5;        // a person's beat: a mode held this long (`beat`)
constexpr double kShotWait_s = 0.15;       // a rocket jump's shot waits this long for the jump key
constexpr float kUnlockShot = 900;         // a goal item's shot button fired at from within this
constexpr double kUnlockLookHold_s = 0.6;  // (`unlocklook`) its look held this long out of sight
constexpr float kUnlockOn = 3;             // (`unlocklook`) the view this near it (degrees): a shot
constexpr double kGoalTire_s = 30;         // one goal item held this long in vain is let go of
constexpr double kHealTire_s = 8;          // and sooner by a weak player, who cannot wait for it
constexpr double kTired_s = 20;            // for this long
constexpr float kRetakeStrong = 120;  // strength (a launcher counted) to go and take an area back
constexpr double kRetake_s = 20;      // a key area believed lost this long ago is still retaken
constexpr double kHeardChase_s = 2;   // and one heard this long ago is gone after
constexpr float kCornered = 350;      // units: an enemy in sight this near is fought (the double's
                                      // range: a back turned on him this close is shot)
constexpr float kCowardCornered = 192;  // a coward still runs from one farther than this
constexpr double kImpulseGap_s = 0.5;
// (`rlback`) a weapon asked for lands within this (the launcher's refire and the raise), s
constexpr double kAskLands_s = 1.0;
constexpr float kAimChest = 16;
constexpr float kHoldLook = 700;  // (`holdra`) a holder this near its armour keeps its view on it
                                  // // (`aimclear`) his chest over his origin, units
// (18b's ledger) a held burst with the view this near the aim's point, degrees: the point itself
// runs into the world, not the hand still on its way to it
constexpr float kBurstOnAim = 3;
constexpr double kDecide_s = 4;        // with no news, its item goal is chosen again this often
constexpr double kItemDueSpare_s = 5;  // (`itemfirst`) a key item back this soon after its run
constexpr double kFoeKey_s = 10;       // an enemy heard firing a launcher this lately carries one
constexpr float kKeyArms = 2;          // the arms a launcher's or the arc's shot is heard as
// (`chasekill`) An enemy believed this low (health and armour's protection) is worth following,
// by a fighter at least kChaseStrong itself.
constexpr float kChaseLow = 60;
constexpr float kChaseStrong = 100;
// The turn back taken round (`uturn`): at this speed or more on the ground, a wish this far off the
// velocity is pressed this far across it instead (degrees); past `kTurnStraight` (straight back)
// the view's side is the side.
constexpr double kTurnBackSpeed = 30, kTurnBack = 110, kTurnAcross = 95, kTurnStraight = 170;
constexpr double kTurnRoom =
    48;  // and the room it needs across, units (the circle's ~32 and a box)
constexpr double kCallAgain_s = 8;   // an enemy in sight is called out again after this
constexpr float kRocketNear = 400;   // a rocket at a man nearer than this is aimed at his middle
constexpr double kSwimExit_s = 3;    // a climb out of the water kept this long at most
constexpr double kSwimWayFlat = 48;  // a swim's way nearer than this across is risen or sunk to
constexpr double kLanding_s = 2;  // out of the water, its exit node walked onto this long at most
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
constexpr double kShortHold_s = 3;  // a short goal is given up after this
// Swimming, the view turns this many degrees a second past a band this wide off the swim's way
// (an about-face faster), as the calm view walks (shooterbots/bot.cpp).
constexpr double kSwimTurn = 180;
constexpr double kSwimAbout = 360;
constexpr float kSwimBand = 20;
// The finger clicks while the view is within this many times the trigger's tolerance of him.
constexpr float kClickWide = 2.5f;
// A view set for a shot (a button, a doorway) is fired at once the hand is this near it, degrees.
constexpr float kHandThere = 3;

// How far, in degrees, the view command `cmd` carries is off the aim `b` wants.
float view_off(const sb::Bot& b, const sh::MoveCmd& cmd) {
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
void swim_view(Fighter& f, const ge::shooterbots::BotView& s, int me, int msec, bool swim,
               sh::MoveCmd& cmd) {
  const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  bool seen = false;
  if (f.bot.aiming)
    for (const sb::Contact& k : f.bot.senses.contacts) seen |= k.ent == f.bot.target && k.visible;
  const bool wading =
      self.waterlevel >= 1 && (swim || f.bot.pilot.pilot.mode == sb::PilotMode::swim);
  // The mouse hand turns a swimmer's view itself (shooterbots/bot.hpp).
  const bool hand = f.bot.mouse && f.bot.aim.mouse && f.features[sb::Feature::pools];
  if ((self.waterlevel < 2 && !wading) || seen || f.swim_exit >= 0 ||
      !f.features[sb::Feature::pace] || hand) {
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

bool alive_slot(const ge::shooterbots::BotView& s, int slot) {
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
    return sb::walk_clear(*world, origin, to);
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

// The role's goal (team.hpp), true when it set one; false leaves the goal to the items.
// (`holdpoint`) A node a holder stands on beside item `i`: on its floor, kStandLo..kStandHi from
// it in a clear line, the nearest to kStandAt; -1 none. Sought once.
constexpr float kStandLo = 128, kStandHi = 320, kStandAt = 200;
int32_t stand_node(Fighter& f, const ge::shooterbots::BotView& s, int32_t i) {
  ItemSpot& it = f.items[static_cast<size_t>(i)];
  if (it.stand_node != -2) return it.stand_node;
  it.stand_node = -1;
  const sb::NavGraph& g = *f.bot.pilot.pilot.g;
  float best = 1e9f;
  const sh::Vec3f spot{it.at.x, it.at.y, it.at.z + 16};
  for (size_t n = 0; n < g.nodes.size(); n++) {
    const sh::Vec3f o = g.nodes[n].origin;
    const float d = dist(o, it.at);
    if (d < kStandLo || d > kStandHi || std::fabs(o.z - 24 - it.at.z) > 40) continue;
    if (std::fabs(d - kStandAt) >= best) continue;
    if (sh::trace_move(s.sv, o, {}, {}, spot, sh::kTraceNoMonsters, f.slot + 1).fraction < 1)
      continue;
    best = std::fabs(d - kStandAt);
    it.stand_node = static_cast<int32_t>(n);
  }
  return it.stand_node;
}

// (`rlhunt`) Where the enemy launcher holder to go for is: an enemy believed to carry the launcher
// (heard firing it: foe_has_key), seen or heard within kHuntNews_s, within kHuntFar, with a mate
// believed near him too; the nearest. False when this fighter carries the launcher itself, is not
// strong enough, or there is none.
bool rl_hunt_at(const Fighter& f, const Stats& st, sh::Vec3f me, double t, sh::Vec3f& at) {
  constexpr double kHuntNews_s = 3;      // his place this fresh
  constexpr float kHuntFar = 1500;       // and this near, units
  constexpr float kHuntStrong = 170;     // health and armour's protection (a yellow armour on)
  constexpr float kHuntMateNear = 1200;  // a mate believed this near him
  if ((st.items & f.arsenal->launcher) != 0 || strength(st) < kHuntStrong || !f.team) return false;
  float best = kHuntFar;
  bool found = false;
  for (const sb::Contact& k : f.bot.senses.contacts) {
    if (k.ent < 1 || !foe_has_key(f, k.ent, t)) continue;
    const bool seen = ai::known(k.seen) && ai::age(k.seen, t) <= kHuntNews_s;
    const bool heard = ai::known(k.heard) && ai::age(k.heard, t) <= kHuntNews_s;
    if (!seen && !heard) continue;
    const sh::Vec3f p = at_of(seen ? k.seen : k.heard);
    const float d = dist(p, me);
    if (d >= best) continue;
    bool mate = false;
    for (const Mate& m : f.mind.mates)
      mate |= t - m.at_t < kExpertMateFresh_s && dist(m.at, p) < kHuntMateNear;
    if (!mate) continue;
    best = d;
    at = p;
    found = true;
  }
  return found;
}

bool team_goal(Fighter& f, const ge::shooterbots::BotView& s, const Stats& st, sh::Vec3f me,
               double t, const sb::Contact* c, ai::Why* why) {
  const sb::NavGraph& g = *f.bot.pilot.pilot.g;
  // A backpack close by is had first, unless retreating with someone about.
  if (f.mode != FightMode::retreat) {
    const int32_t pack = pack_near(f, s, st, me, 400);
    if (pack >= 0) {
      if (pack != f.pack_ent) f.stats.packs++;
      f.pack_ent = pack;
      go_to(f, sb::nav_nearest(g, s.world.entities[static_cast<size_t>(pack)].origin));
      return true;
    }
    f.pack_ent = -1;  // none in sight: the next is chosen by sight again
    // A pack it knows a death left (packs.hpp): one from a fight with rockets in it from far,
    // any other near and with nothing else on.
    if (f.features[sb::Feature::packinfer]) {
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
  // The team's taker for its armour (mind.hpp, `fetch`) backing off still goes for it, the armour
  // being what it backs off for, unless the enemy is nearer it.
  if (f.team && f.mode == FightMode::retreat && f.mind.fetch >= 0 &&
      f.reach.reached(f.items[static_cast<size_t>(f.mind.fetch)].node)) {
    const sh::Vec3f there = f.items[static_cast<size_t>(f.mind.fetch)].at;
    if (c == nullptr || !ai::known(c->seen) || dist(at_of(c->seen), there) >= dist(me, there)) {
      go_item(f, s, f.mind.fetch);
      if (why) {
        why->decide("role_goal");
        why->choose("fetch", "the team's taker for its armour, backing off to it");
      }
      return true;
    }
    f.team->stats.fetch_end[4]++;
  }
  if (!f.team || f.mode == FightMode::retreat) return false;
  // A pack a mate called, when this player is the one to fetch it (packs.hpp).
  if (const int32_t pn = f.features[sb::Feature::packs] ? packs_goal(f, me, st, t) : -1; pn >= 0) {
    go_to(f, pn);
    return true;
  }
  const Mind& mn = f.mind;
  const auto node_of = [&](int32_t i) { return f.items[static_cast<size_t>(i)].node; };
  const auto can_reach = [&](int32_t i) {
    return i >= 0 && node_reached(f, node_of(i)) && !route_lost(f, i, t);
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
  // The team's taker for its armour (mind.hpp, `fetch`): the trip held, in a fight too.
  if (mn.fetch >= 0 && can_reach(mn.fetch)) {
    go_item(f, s, mn.fetch);
    note("fetch", "the team's taker for its armour");
    return true;
  }
  // (`rlfetch`) The team's taker for a launcher (mind.hpp): waiting on it as it comes due.
  if (mn.rl_fetch >= 0 && can_reach(mn.rl_fetch)) {
    go_item(f, s, mn.rl_fetch);
    note("rlfetch", "the team's taker for a launcher");
    return true;
  }
  // (`rlhunt`) An enemy launcher's holder near, a mate near him too: gone for, from its own way.
  if (sh::Vec3f hunt; f.features[sb::Feature::rlhunt] && f.role != Role::rush &&
                      f.role != Role::carry && rl_hunt_at(f, st, me, t, hunt)) {
    f.stats.rl_hunts++;
    go_to(f, sb::nav_nearest(g, hunt));
    note("hunt", "an enemy launcher's holder near, a mate near him too: overwhelmed");
    return true;
  }
  switch (f.role) {
    case Role::hold: {
      const int32_t area = mn.claim;
      if (!can_reach(area) || strength(st) < kHoldStrong) return false;
      // He chases out of his area no further than the leash; else he stands on it.
      if (f.mode == FightMode::chase && c && dist(at_of(c->seen), at(area)) < kLeash) {
        go_to(f, sb::nav_nearest(g, at_of(c->seen)));
        note("hold", "chasing within the leash");
        return true;
      }
      // The item gone and not due for a while: out to fire at the ways in (spam.hpp).
      if (spam_plan(f, s, st, t)) {
        go_to(f, f.spam_node);
        note("hold", "out to fire at a way in while the item is away");
        return true;
      }
      // (`holdpoint`) An item it leaves to a mate: stood off, beside it, while it is there or due.
      const ItemSpot& held_it = f.items[static_cast<size_t>(area)];
      const bool up = held_it.present || held_it.back_at - t < 10;
      f.team->stats.hold_plans++;
      const bool leaves = (f.features[sb::Feature::holdpoint] || f.features[sb::Feature::holdra]) &&
                          up && gives_up(f, s, f.slot + 1, area, st, t);
      if (up) (leaves ? f.team->stats.hold_left : f.team->stats.hold_taken)++;
      if (leaves) {
        const int32_t n = stand_node(f, s, area);
        if (n >= 0 && (f.reach_at < 0 || f.reach.reached(n))) {
          go_to(f, n);
          note("hold", "beside the item it leaves to a mate");
          return true;
        }
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
    case Role::rush: {
      if (!can_reach(quad)) return false;
      // (`rushdetour`) a key item due on the way first, when the quad's clock allows
      const int32_t first =
          f.features[sb::Feature::rushdetour] ? rush_detour(f, st, t, quad) : int32_t{-1};
      if (first >= 0) {
        go_item(f, s, first);
        note("rush", "a key item due first: taken, then on to the quad by its time");
        return true;
      }
      go_item(f, s, quad);
      note("rush", "the quad is due: waiting on it");
      return true;
    }
    case Role::carry: {
      if (c && ai::known(c->seen) && ai::age(c->seen, t) < 3) {
        go_to(f, sb::nav_nearest(g, at_of(c->seen)));
        note("carry", "the quad at the enemy in sight");
        return true;
      }
      // The key areas cleared in turn: the one it is not at.
      int32_t sweep = -1;
      for (int32_t i : {armour, launcher})
        if (can_reach(i) && dist(at(i), me) > 250 && (sweep < 0 || i == f.goal_item)) sweep = i;
      if (sweep < 0 && mn.enemy_t > t - 6) {
        go_to(f, sb::nav_nearest(g, mn.enemy_at));
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
        go_to(f, sb::nav_nearest(g, at_of(c->seen)));
        note("push", "chasing in the base");
        return true;
      }
      if (mn.push_go_t < 0) {
        go_to(f, sb::nav_nearest(g, mn.push_at));
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
        go_to(f, sb::nav_nearest(g, mn.help_at));
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
        go_to(f, sb::nav_nearest(g, mn.enemy_at));
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
                   bool blast_ok = true, bool now = false) {
  const auto score = [&](int32_t i) {
    const BotWeapon& w = f.arsenal->weapons[static_cast<size_t>(i)];
    if (!blast_ok && w.blast) return -1.0;
    // (`blastsafe`) a blast at an enemy this near would burst at its shooter
    if (w.blast && range < kBlastNear && f.features[sb::Feature::blastsafe])
      return loaded(st, w) ? 1.0 : -1.0;
    return loaded(st, w) ? weapon_score(w, range) : -1.0;
  };
  const std::span<const BotWeapon> weapons = f.arsenal->weapons;
  const auto name = [&](int32_t i) { return weapons[static_cast<size_t>(i)].name; };
  const int32_t w = ai::pick_best(static_cast<int32_t>(weapons.size()), score, why, "weapon", name,
                                  "the best loaded at the range");
  if (w >= 0) {
    const BotWeapon& best = weapons[static_cast<size_t>(w)];
    // (`rlback`) The launcher, best with him past the blast's reach: asked for at once, and a
    // lesser weapon asked for and not yet in hand (it lands when the launcher's refire is over)
    // asked back, unless the launcher was the last ask.
    const bool back = f.features[sb::Feature::rlback] && best.bit == f.arsenal->launcher &&
                      range >= kBlastNear && f.asked_weapon != best.number;
    const bool pending = back && best.bit == st.weapon && t - f.impulse_at < kAskLands_s;
    if ((best.bit != st.weapon || pending) && loaded(st, best) &&
        (now || back || t - f.impulse_at >= kImpulseGap_s)) {
      f.want_weapon = best.number;
      f.hid = false;
    }
  }
}

// The world's line from `a` to `b` clear (no player stops it).
bool clear_line(const ge::shooterbots::BotView& s, sh::Vec3f a, sh::Vec3f b) {
  return sh::trace_move(s.sv, a, {}, {}, b, sh::kTraceNoMonsters, -1).fraction >= 1;
}

// The pilot on a scripted way (`NavKind::script`, a trick's route) this step.
bool on_script(const sb::Pilot& p) {
  if (p.g == nullptr || p.k + 1 >= p.route.size()) return false;
  const sb::NavNode& a = p.g->nodes[static_cast<size_t>(p.route[p.k])];
  for (int32_t q = 0; q < a.links; q++) {
    const sb::NavLink& l = p.g->links[static_cast<size_t>(a.first_link) + static_cast<size_t>(q)];
    if (l.to == p.route[p.k + 1]) return l.kind == sb::NavKind::script;
  }
  return false;
}

// THE HIDE (`hide`, M18r.md 0k2): a pack dropped at a death holds the weapon in hand, so a
// player who has the launcher holds the scattergun while no one is in sight and takes the
// launcher up for the fight (at once: the mod's select is instant once the refire is over, and
// waits it out before). People keep the plain scattergun in hand at 25 % (dm2) to 39 % (e1m2) of
// the launcher owners' deaths with rockets left; a novice keeps the launcher up.
// The plainest loaded spread a hide takes up (the scattergun): its weapon bit, 0 for none.
int32_t hide_row(const Fighter& f, const Stats& st) {
  const BotWeapon* sg = nullptr;
  for (const BotWeapon& w : f.arsenal->weapons)
    if (w.curve == WeaponCurve::scatter && w.ammo >= 0 && loaded(st, w) &&
        (sg == nullptr || w.number < sg->number))
      sg = &w;
  return sg != nullptr ? sg->bit : 0;
}

// (`hide`) Not until no enemy has been seen this long, s: a fight's peeks keep the launcher up.
constexpr double kHideCalm_s = 1.5;

void hide_launcher(Fighter& f, const Stats& st, bool seen_now, double t, ai::Why* why) {
  if (!f.features[sb::Feature::hide] || f.skill == sb::AimSkill::novice || seen_now ||
      t - f.mind.sighted_t < kHideCalm_s || t - f.show_raise_t < kRaiseHold_s)
    return;
  const BotArsenal& a = *f.arsenal;
  const BotWeapon* rl = weapon_row(a, a.launcher);
  if (rl == nullptr || st.weapon != rl->bit) return;
  const BotWeapon* sg = weapon_row(a, hide_row(f, st));
  if (sg == nullptr || t - f.impulse_at < kImpulseGap_s) return;
  f.want_weapon = sg->number;
  f.hid = true;
  f.stats.hides++;
  if (why) {
    why->decide("hide");
    why->choose(sg->name, "no one in sight: the launcher kept out of a dropped pack");
  }
}

void think(Fighter& f, const ge::shooterbots::BotView& s, int me, const Stats& st, ai::Why* why) {
  const double t = s.sv.time;
  const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  floor_trap_think(f, s, me, t);  // a floor trap's button pressed on an enemy on it (floortrap)
  // (`noroute`) The node the pilot found no route to on its last steps (-1: it had one) and the
  // item it was going for, read before this think's goal hands the pilot a plan again.
  const bool no_route = f.bot.pilot.pilot.mode == sb::PilotMode::idle &&
                        f.bot.pilot.pilot.route.empty() && f.goal_node >= 0;
  const int32_t no_route_to = no_route ? f.goal_node : -1;
  const int32_t no_route_item = no_route ? f.goal_item : -1;
  watch_bridges(f, s, me);
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};

  // Where it can get to from here, worked out once a second (a teleporter or a drop is one way).
  if (f.reach_at < 0 || t - f.reach_at >= 1) {
    const sb::NavGraph& g = *f.bot.pilot.pilot.g;
    // Rocket jumps only while it can afford one: the launcher and a rocket, the health for the
    // blast (about half a rocket's 120), a player who moves well enough to try (nav.hpp). Grenade
    // jumps only when it must: the grenade launcher and no rocket launcher (dm2's quad with no
    // launcher on the team), on the same terms.
    sb::NavJumper& jumper = f.bot.pilot.pilot.set.jumper;
    const float moves = f.traits[sb::Trait::moves];
    const BotArsenal& arsenal = *f.arsenal;
    const float rockets = ammo_of(st, arsenal.launcher_ammo);
    const bool afford = rockets >= 1 && st.health >= kRocketJumpHealth && f.escort.leader < 0 &&
                        moves >= kRocketJumpMoves;
    jumper.rockets = (st.items & arsenal.launcher) != 0 && afford && f.features[sb::Feature::rj];
    jumper.grenades = (st.items & arsenal.lob) != 0 && (st.items & arsenal.launcher) == 0 &&
                      afford && f.features[sb::Feature::gj];
    jumper.level = moves >= kRocketLevelMoves[2] ? 2 : moves >= kRocketLevelMoves[1] ? 1 : 0;
    jumper.extra_s = kRocketLevelExtra_s[jumper.level];
    jumper.cuts_all = f.features[sb::Feature::jumpcuts] ||
                      f.features[sb::Feature::giveup];  // jumps missed twice kept off
    jumper.scripts = f.features[sb::Feature::routes];   // the map's scripted ways to key items
    // The map's gap tricks (shooterbots/nav_gaps.hpp): a hopper's, out of its hop, and planned only
    // while it already moves near a gap's speed (a slow one would walk up to it and give it up).
    const float across_speed =
        std::sqrt(self.velocity.x * self.velocity.x + self.velocity.y * self.velocity.y);
    jumper.gaps = f.bot.pilot.pilot.set.hop && f.features[sb::Feature::gapjump] &&
                  across_speed >= kGapPlanSpeed;
    // Slower, one far enough along the route to hop up to its speed first (`gapbuild`).
    jumper.gap_build_s = f.bot.pilot.pilot.set.hop && f.features[sb::Feature::gapjump] &&
                                 f.features[sb::Feature::gapbuild]
                             ? kGapBuild_s
                             : -1;
    // Out of a pit by rocket (shooterbots/escape.hpp): the launcher and a rocket; in water the
    // health for the blast too (lava and slime kill whoever stays, so any health tries it there).
    int liquid = sh::kContentsEmpty;
    if (self.waterlevel >= 1 && f.bot.pilot.world != nullptr)
      sh::water_level(*f.bot.pilot.world, self.origin, &liquid);
    const bool burns = liquid == sh::kContentsLava || liquid == sh::kContentsSlime;
    jumper.escapes = (st.items & arsenal.launcher) != 0 && rockets >= 1 &&
                     (burns || st.health >= kRocketJumpHealth) && f.features[sb::Feature::escape];
    // The doors its own errands keep off the routes kept off the reach too (door_errand).
    std::span<const int32_t> shut = locked_errands(f)
                                        ? std::span<const int32_t>(f.bot.pilot.pilot.shut)
                                        : std::span<const int32_t>();
    // (`errandreach`) The door an errand goes round to its button is a way through all the same:
    // off the routes (the pilot's `shut`), not off the reach.
    std::array<int32_t, 16> reach_shut{};
    if (f.features[sb::Feature::errandreach] && f.door_ent >= 0 &&
        shut.size() <= reach_shut.size()) {
      size_t n = 0;
      for (const int32_t d : shut)
        if (d != f.door_ent) reach_shut[n++] = d;
      shut = std::span<const int32_t>(reach_shut.data(), n);
    }
    // (`bridgeseen`) Nor onto a bridge still in (watch_bridges' shut links).
    const std::span<const sb::NavExtraLink> shut_links =
        f.bot.pilot.pilot.set.shut_every
            ? std::span<const sb::NavExtraLink>(f.bot.pilot.pilot.shut_links)
            : std::span<const sb::NavExtraLink>();
    const int32_t from = sb::nav_nearest(g, self.origin);
    f.reach_n = sb::nav_reach(g, from, f.reach, f.bot.pilot.pilot.set.jumper, shut, shut_links);
    f.reach_at = t;
    f.reach_from = from;
    if (f.reach_n > 1) f.field_node = from;  // (`fields`) where its ways are read from
    // (`errandreach`) A reach of its own node at most (in the air over a shaft, no node near) is
    // not known yet: worked out again at the next think, not read as nowhere to go.
    if (f.features[sb::Feature::errandreach] && f.reach_n <= 1) f.reach_at = -1;
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
    const sb::NavGraph& g = *f.bot.pilot.pilot.g;
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
    // With the map's traps known, a small reach is a pool only on a trap node: a map with much
    // of it behind doors not yet open (base1, 2525 of 10469 nodes reached) is no pool, and the
    // swim field's way out there pulled him away from a route up a slope out of a shallow river.
    const bool trapped = f.trap == nullptr ||
                         (f.reach_from >= 0 && static_cast<size_t>(f.reach_from) < f.trap->size() &&
                          (*f.trap)[static_cast<size_t>(f.reach_from)]);
    const bool pool = !was_diving && !wanted_here && f.reach_at >= 0 &&
                      static_cast<size_t>(f.reach_n) * 4 < g.nodes.size() && trapped;
    const bool swim = self.waterlevel >= 2;
    sh::Vec3f way;
    f.swimming = !was_diving && f.swim && (self.waterlevel >= 1 || pool) &&
                 (pool || sb::nav_nearest(g, self.origin) < 0) &&
                 sb::swim_next(*f.swim, g, self.origin, way);
    if (f.swimming) {
      f.lost++;
    } else {
      // Dry in a pocket whose way out is under water: into the water at its nearest way out, by a
      // route when one leads there.
      const auto wet_node = [&](size_t n) {
        sh::Vec3f to;
        return sb::swim_next(*f.swim, g, g.nodes[n].origin, to);
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
      if (!escaping && !was_diving &&
          ((pool && (f.dry_pockets || swim)) || sb::nav_nearest(g, self.origin) < 0)) {
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
          // (`lostdry`) On dry land, never back onto a node in lava or slime or on a mover (dm2's
          // south pit: the nearest nodes in a clear line stood on its trains, over the lava).
          if (f.features[sb::Feature::lostdry] && !pool && !swim &&
              ((!g.harm.empty() && g.harm[n] != 0) || g.nodes[n].mover >= 0))
            continue;
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
  // (`itemglance`) a key spot passed off the screen: offered to the calm view to glance at.
  item_glance_offer(f, s, me, eye, t, why);
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

  // (`quadseen`) An enemy in sight with the quad's glow: the quad is gone and back no sooner than
  // its time after; the team told ("enemy quad").
  if (f.features[sb::Feature::quadseen] && f.team) {
    bool glow = false;
    sh::Vec3f where{};
    for (const sb::Contact& c : f.bot.senses.contacts)
      if (c.visible && c.ent >= 1 && static_cast<size_t>(c.ent) < s.world.entities.size() &&
          (s.world.entities[static_cast<size_t>(c.ent)].effects & kGlowQuad) != 0) {
        glow = true;
        where = at_of(c.seen);
      }
    if (glow && quad_glow_seen(f, t)) {
      Said said;
      said.kind = SayKind::enemy_power;
      said.item = "quad";
      mind_say(f, t, said, where);
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
    const sb::Contact* foe = sb::freshest(f.bot.senses, eye);
    // No blast where it would reach a player (the companion itself among them): another weapon.
    const bool blast_ok = !foe || escort_blast_clear(f, s, at_of(foe->seen));
    choose_weapon(f, st, foe ? dist(eye, at_of(foe->seen)) : 400.f, t, why, blast_ok);
    return;
  }

  const sb::Contact* c = sb::freshest(f.bot.senses, eye);
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
  const float mine = sb::might(own_might(st));
  const float theirs = c ? foe_might(f, c->ent, t) : 0;
  // An enemy in sight this close is fought whatever the odds: turning away only shows him a back.
  const bool cornered =
      seen_now &&
      dist(eye, at_of(c->seen)) < (f.temper == sb::Temper::coward ? kCowardCornered : kCornered);
  const bool engage = c && (cornered || sb::takes_fight(f.temper, f.edge, mine, theirs));
  // (`itemfirst`) A key item of its own to have: its goal a key item there, or back within its run
  // and kItemDueSpare_s, and the enemy not heard firing the launcher or the arc (a key item he
  // would drop is worth the fight). Experts leave a fight to time an item: no chase, and the
  // fight's feet keep the route (footwork.cpp), the aim on him.
  f.item_first = false;
  if (f.features[sb::Feature::itemfirst] && f.goal_item >= 0 && (f.reach_at >= 0 || fields_on(f))) {
    const ItemSpot& gi = f.items[static_cast<size_t>(f.goal_item)];
    if (key_item(gi) && gi.node >= 0 && node_run_s(f, gi.node) < ai::kUnreached) {
      const double run = node_run_s(f, gi.node);
      const bool due = gi.present || gi.back_at - t <= run + kItemDueSpare_s;
      const bool his_key = c && foe_has_key(f, c->ent, t);
      f.item_first = due && !his_key;
    }
  }
  if (f.item_first) f.stats.item_first++;
  // (`chasekill`, M18r.md 6f) THE CHASE IS A CHOICE: one lost from sight or heard is followed only
  // when he is believed killable (his body on the ledger low, this player strong) or he holds the
  // launcher or the arc and this player does not; else it goes on with its goal.
  bool chase_ok = true;
  if (f.features[sb::Feature::chasekill] && c && !seen_now) {
    bool killable = false, his_key = false;
    if (c->ent >= 1 && static_cast<size_t>(c->ent) < f.foes.size()) {
      const FoeRead& fr = f.foes[static_cast<size_t>(c->ent)];
      killable = t - fr.t <= kFoeFade_s && foe_body(f, c->ent, t) <= kChaseLow &&
                 strength(st) >= kChaseStrong;
      his_key = foe_has_key(f, c->ent, t);
    }
    bool my_key = false;
    for (const BotWeapon& w : st.arsenal->weapons)
      if (w.arms >= kKeyArms && loaded(st, w)) my_key = true;
    chase_ok = killable || (his_key && !my_key);
    if (!chase_ok) f.stats.chase_held++;
    if (why) {
      why->decide("chase");
      why->fact("killable", killable);
      why->fact("his_key", his_key);
      why->fact("my_key", my_key);
      why->choose(chase_ok ? "follow" : "own_way", killable ? "believed killable"
                                                   : chase_ok
                                                       ? "his launcher, none of mine"
                                                       : "neither killable nor armed better");
    }
  }
  const bool recent =
      !f.item_first && chase_ok && c &&
      ((ai::known(c->seen) && ai::age(c->seen, t) <= kChase_s && engage) ||
       (by_ear && ai::age(c->heard, t) <= kHeardChase_s && ai::age(c->heard, t) >= f.hear_react_s &&
        sb::chases_sound(f.temper, f.edge, f.rash, mine, theirs)));
  const sh::Vec3f enemy =
      c ? at_of(by_ear || !ai::known(c->seen) ? c->heard : c->seen) : sh::Vec3f{};
  const float range = c ? dist(eye, enemy) : 400;

  // The mode.
  const float strong = strength(st);
  const float below = sb::retreat_below(f.temper);
  const bool weak = f.mode == FightMode::retreat
                        ? strong < std::fmin(below + (kStrongAgain - kRetreatBelow), 100.f)
                        : strong < below;
  const int32_t heal_at =
      c && weak ? best_item(f, s, st, self.origin, t, true, &enemy, nullptr) : -1;
  // Help is called over a key area being lost (shooterbots/mind.hpp `wants_help`).
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
      (c && weak && f.temper == sb::Temper::coward);
  // A PERSON'S BEAT (`beat`): a fight is held through a peek (the contact out of sight no longer
  // than a beat, and nothing calls for a retreat), and any mode a beat after it is entered unless
  // a sighting calls for a fight. Without it a contact lost for a frame turned a fight into a
  // chase and back: dm2, 3 ten-minute seeds, the mode's changes 0.76 -> 0.46 a bot-second, its
  // median hold 0.42 -> 1.04 s (under 0.2 s: 20 -> 5 %); 12 seeds head to head +1.5 frags a
  // match, speed even.
  const bool beat = f.features[sb::Feature::beat];
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
  // A SCRIPTED WAY TAKEN IS FINISHED (`tripkeep`, M18r.md 0c1): on a trick's way to its goal
  // (e1m2's stair jumps to the YA) the goal is held, unless it backs off: 9 of 28 climbs toward
  // the YA were dropped halfway for shells, the quad, the mega or health (12 got there).
  const bool trip = f.features[sb::Feature::tripkeep] && !escaping && committed < 0 &&
                    f.goal_item >= 0 && f.mode != FightMode::retreat &&
                    f.items[static_cast<size_t>(f.goal_item)].present &&
                    on_script(f.bot.pilot.pilot);
  if (trip) f.stats.trips_kept++;
  int32_t near = -1;
  const char* unasked = nullptr;  // why no short goal was looked for (the log's `item_passed`)
  if (!trip && !escaping && committed < 0 && f.objective < 0 && f.diving < 0 &&
      f.mode != FightMode::retreat && f.features[sb::Feature::shortgoals] && f.mind.fetch < 0 &&
      f.mind.rl_fetch < 0) {
    const bool held = f.short_item >= 0 && t < f.short_until &&
                      f.items[static_cast<size_t>(f.short_item)].present;
    if (held) {
      near = f.short_item;
    } else if (f.goal_item < 0 ||
               dist(f.items[static_cast<size_t>(f.goal_item)].at, self.origin) > kOnTheWay ||
               (f.features[sb::Feature::nearwait] &&
                !f.items[static_cast<size_t>(f.goal_item)].present)) {
      near = on_the_way(f, s, st, self.origin, self.velocity, t);
      if (near >= 0) {
        f.short_until = t + kShortHold_s;
        f.stats.on_the_way++;
      }
    } else {
      unasked = "its goal near";
    }
  } else {
    unasked = trip                                        ? "a scripted way held"
              : escaping                                  ? "escaping"
              : committed >= 0 || f.diving >= 0           ? "diving"
              : f.objective >= 0                          ? "the map's way"
              : f.mode == FightMode::retreat              ? "retreating"
              : f.mind.fetch >= 0 || f.mind.rl_fetch >= 0 ? "fetching"
                                                          : "off";
  }
  f.short_item = near;
  item_passed(f, s, st, self.origin, self.velocity, t, near, unasked, why);

  // A shut door on the way: its button first (door_errand).
  if (locked_errands(f) && !escaping && committed < 0 && f.objective < 0) {
    door_errand(f, s, t, why);
    field_errand(f, s, t, why);
  }
  if (!escaping && committed < 0 && f.objective < 0) bridge_errand(f, t);

  // The goal: the role's first, then the items.
  if (!escaping && f.trap_flee >= 0 && f.trap_exit >= 0) {
    // On a floor trap's floor as it goes: off it first (`trapflee`, floor_trap.hpp).
    go_to(f, f.trap_exit);
  } else if (trip) {
    // the scripted way held to its goal
  } else if (committed >= 0) {
    // Kept on the dive's field whatever the reach from the water says (the nearest node under
    // water may stand in the pocket already).
    f.diving = f.last_dive = committed;
  } else if (!escaping && f.objective >= 0 && f.mode != FightMode::retreat) {
    go_to(f, f.objective);  // the way through the map (way_through.hpp) before any item
    f.goal_item = -1;       // and no item's errand (a shot at its button) kept from before
    f.unlocking = false;
  } else if (!escaping && (f.trap_press >= 0 || f.trap_stay >= 0)) {
    // A floor trap's button, an enemy on its floor, or held at it while the floor is away
    // (floor_trap.hpp).
    const int32_t k = f.trap_press >= 0 ? f.trap_press : f.trap_stay;
    go_to(f,
          f.button_spots[static_cast<size_t>(f.floor_traps[static_cast<size_t>(k)].button)].node);
  } else if (!escaping && f.bridge_button >= 0) {
    // (`noroute`) a bridge in on the way to the goal: its button first (noroute_watch)
    go_to(f, f.button_spots[static_cast<size_t>(f.bridge_button)].node);
  } else if (!escaping && f.door_button >= 0) {
    go_to(f, f.button_spots[static_cast<size_t>(f.door_button)].node);  // a shut door's button
  } else if (near >= 0) {
    // On to the long goal through it (`chains`), when that is there to take.
    const int32_t long_goal = f.goal_item != near ? f.goal_item : f.chain_item;
    const int32_t after = long_goal >= 0 && long_goal != near &&
                                  f.items[static_cast<size_t>(long_goal)].present &&
                                  f.features[sb::Feature::chains]
                              ? long_goal
                              : -1;
    go_item(f, s, near, after);
    if (why) {
      why->decide("on_the_way");
      why->fact("item", near);
      why->choose(f.items[static_cast<size_t>(near)].label.c_str(), "worth having, by its way");
    }
  } else if (!escaping && !team_goal(f, s, st, self.origin, t, c, why)) {
    // A foe is never chased into a trap (a pit no way on foot leads back out of): he lets it go
    // unless he is in that trap already.
    const sb::NavGraph& cg = *f.bot.pilot.pilot.g;
    const int32_t chase_to = f.mode == FightMode::chase && c ? sb::nav_nearest(cg, enemy) : -1;
    const auto trapped = [&](int32_t n) {
      return n >= 0 && f.trap && static_cast<size_t>(n) < f.trap->size() &&
             (*f.trap)[static_cast<size_t>(n)] != 0;
    };
    const bool chase_there =
        chase_to >= 0 && (!trapped(chase_to) || trapped(sb::nav_nearest(cg, self.origin)));
    if (chase_there) {
      go_to(f, chase_to);
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
      const bool kept = f.features[sb::Feature::commit] && f.goal_item >= 0 &&
                        now_news == f.decided_news && t - f.decided_at < kDecide_s &&
                        f.items[static_cast<size_t>(f.goal_item)].present && !heal;
      int32_t i = f.goal_item;
      if (!kept) {
        i = best_item(f, s, st, self.origin, t, heal, c ? &enemy : nullptr, why);
        f.decided_news = now_news;
        f.decided_at = t;
      }
      const int32_t mate = f.outmatched ? nearest_mate_node(f, self.origin, t) : -1;
      // (`nogoal`, M18r.md 3) Nothing worth taking: no item goal kept from before (a holder in a
      // full armour stood on the armour it walked back to as its claim, the old goal kept: the
      // goal's tiring read it, and the lying count took it for a taker).
      if (i < 0 && f.features[sb::Feature::nogoal]) f.goal_item = -1;
      if (i < 0 && mate >= 0) {
        go_to(f, mate);  // nothing to take: to the team
      } else if (i >= 0) {
        go_item(f, s, i, chain_after(f, s, st, i, t, heal, c ? &enemy : nullptr));
      } else if (f.sense == MapSense::novice && (!f.team || f.mind.claim < 0)) {
        // Nothing in mind: a novice walks the map to learn it.
        go_to(f, explore_node(f, self.origin));
      } else if (f.team && f.mind.claim >= 0 &&
                 f.reach.reached(f.items[static_cast<size_t>(f.mind.claim)].node) &&
                 !route_lost(f, f.mind.claim, t)) {
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
      if (it.node < 0 || !f.reach.reached(it.node) || !item_in_mind(f, static_cast<int32_t>(i)) ||
          route_lost(f, static_cast<int32_t>(i), t))
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

  // (`noroute`) The goal the pilot finds no route to: a bridge's button first, or set aside.
  if (f.features[sb::Feature::noroute] && !escaping && f.objective < 0 && committed < 0)
    noroute_watch(f, self.origin, t, no_route_to, no_route_item, why);
  if (f.diving < 0) f.last_dive = -1;
  // (`hide`) Held while no one is in sight; a sighting takes the launcher up at once.
  if (f.hid && st.weapon != hide_row(f, st)) f.hid = false;
  // (`earlob`) a throw under way keeps the lob up
  if (!(f.hid && !seen_now) && !f.lob_on) choose_weapon(f, st, range, t, why, true, f.hid);
  hide_launcher(f, st, seen_now, t, why);
  // A blast jump on the way next: its launcher in hand for it (a rocket jump's, a grenade
  // jump's).
  const sb::Pilot& pl = f.bot.pilot.pilot;
  if ((pl.set.jumper.rockets || pl.set.jumper.grenades || pl.set.jumper.escapes) &&
      t - f.impulse_at >= kImpulseGap_s)
    for (size_t k = pl.k; k + 1 < pl.route.size() && k < pl.k + 3; k++) {
      const sb::NavNode& from = pl.g->nodes[static_cast<size_t>(pl.route[k])];
      int number = 0;  // the launcher's
      for (int32_t q = 0; q < from.links; q++) {
        const sb::NavLink& l =
            pl.g->links[static_cast<size_t>(from.first_link) + static_cast<size_t>(q)];
        if (l.to != pl.route[k + 1]) continue;
        const BotArsenal& arsenal = *f.arsenal;
        const BotWeapon* rl = weapon_row(arsenal, arsenal.launcher);
        const BotWeapon* gl = weapon_row(arsenal, arsenal.lob);
        if (l.kind == sb::NavKind::rocket && rl && st.weapon != rl->bit) number = rl->number;
        if (l.kind == sb::NavKind::grenade && gl && st.weapon != gl->bit) number = gl->number;
      }
      if (number > 0) {
        f.want_weapon = number;
        break;
      }
    }
}

// A living teammate within kFireClearance of the line from `eye` to `target`, nearer than it.
bool mate_in_line(const Fighter& f, const ge::shooterbots::BotView& s, sh::Vec3f eye,
                  sh::Vec3f target) {
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

}  // namespace

// A living teammate within `reach` of `at` (a blast's aim: its splash would hurt him).
bool mate_near(const Fighter& f, const ge::shooterbots::BotView& s, sh::Vec3f at, float reach) {
  if (!f.team) return false;
  for (const Fighter* m : f.team->members) {
    if (m == &f || m->slot < 0 || !alive_slot(s, m->slot)) continue;
    if (dist(s.world.entities[static_cast<size_t>(m->slot) + 1].origin, at) < reach) return true;
  }
  return false;
}

bool foe_has_key(const Fighter& f, int ent, double t) {
  if (ent < 1 || static_cast<size_t>(ent) >= f.foes.size()) return false;
  const FoeRead& fr = f.foes[static_cast<size_t>(ent)];
  if (f.features[sb::Feature::rlknown] && t - fr.key_t <= kKeyKept_s) return true;
  return t - fr.t <= kFoeKey_s && fr.arms >= kKeyArms;
}

void fighter_go(Fighter& f, int32_t node) { go_to(f, node); }

float fighter_strength(const Stats& st) {
  return st.health + protection(st.armortype, st.armor) +
         ((st.items & st.arsenal->launcher) ? 50.f : 0.f);
}

bool blast_bursts_near(const Fighter& f, const ge::shooterbots::BotView& s, int me,
                       sh::Vec3f angles, float reach) {
  if (!f.features[sb::Feature::blastsafe]) return false;
  const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  const sh::Vec3f fwd = sh::angle_vectors(angles).forward;
  const sh::Vec3f mid{self.origin.x, self.origin.y, self.origin.z + 4};
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + 22};
  // From where the shot leaves too (16 over the origin, 8 ahead): a lip the eye clears may
  // still take the rocket.
  const sh::Vec3f muzzle{self.origin.x + fwd.x * 8, self.origin.y + fwd.y * 8,
                         self.origin.z + 16 + fwd.z * 8};
  for (const sh::Vec3f from : {eye, muzzle}) {
    const sh::HullTrace tr =
        sh::trace_move(s.sv, from, {}, {}, from + fwd * reach, sh::kTraceMissile, me);
    if (dist(tr.endpos, mid) < kBlastNear) return true;
  }
  return false;
}

sh::MoveCmd fighter_command(Fighter& f, const ge::shooterbots::BotView& s, int slot, int msec) {
  const int me = slot + 1;
  const Stats st = read_stats(s, me, *f.arsenal);
  const double t = s.sv.time;
  f.bot.pilot.grenade_ready = st.weapon == f.arsenal->lob;  // a grenade jump fires with it in hand
  // A rocket out of a pit with this; under `rjready` ready to fire too (its refire over).
  f.bot.pilot.rocket_ready =
      st.weapon == f.arsenal->launcher && (!f.features[sb::Feature::rjready] || t >= f.ready_at);
  f.bot.pilot.aimed = f.bot.aiming;  // (`aimland`) the last command's view a fight's
  // A companion's view is its own (it faces the leader).
  f.bot.calm = f.escort.leader < 0 && f.features[sb::Feature::calm];
  if (f.features[sb::Feature::packs] || f.features[sb::Feature::packinfer])
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
    const sb::Pilot& pl = f.bot.pilot.pilot;
    const sb::NavGraph* g = pl.g;
    w[5] += g && f.goal_node >= 0 && g->nodes[static_cast<size_t>(f.goal_node)].wet;
    w[6] += e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y < 50 * 50;
    const int how = f.bot.aiming ? 0 : pl.mode == sb::PilotMode::swim ? 1 : 2;
    const double across = std::sqrt(static_cast<double>(e.velocity.x) * e.velocity.x +
                                    static_cast<double>(e.velocity.y) * e.velocity.y);
    f.stats.wet_speed[how] += across;
    f.stats.wet_n[how]++;
    f.stats.wet_still[how] += across < 50;
    w[7] += f.bot.pilot.pilot.mode == sb::PilotMode::swim;
    w[8] += f.bot.pilot.pilot.mode == sb::PilotMode::walk;
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
    const auto pm = f.bot.aiming ? static_cast<size_t>(sb::kPilotModes)
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
  mega_watch(f, st.health, t);      // the mega it took, timed from its own health (knowledge.hpp)
  mind_plan(f, s, me, st, t, why);  // its role and its calls, off its own beliefs (mind.hpp)
  if (!dead && t >= f.next_think) {
    f.next_think = t + kThink_s;
    think(f, s, me, st, why);
  }
  if (!dead && f.bot.pilot.pilot.mode == sb::PilotMode::arrived) f.next_think = 0;

  const BotWeapon* held = weapon_row(*f.arsenal, st.weapon);
  // Another kind of weapon taken up is held as its aim trait holds it.
  const int slot_of_aim = !held               ? f.aim_slot
                          : held->number <= 3 ? 0
                          : held->number <= 5 ? 1
                                              : held->number - 4;
  if (slot_of_aim != f.aim_slot && f.skill != sb::AimSkill::perfect) {
    f.aim_slot = slot_of_aim;
    sb::bot_aim(f.bot, sb::aim_profile(f.traits.v[static_cast<size_t>(slot_of_aim)],
                                       f.traits[sb::Trait::reaction]));
  }
  f.bot.shot_speed = held ? held->shot_speed : 0;
  // A novice fires at the plain distance; the rest at where he will be, on his arc in the air.
  f.bot.lob = held && held->bit == f.arsenal->lob;
  const bool leads = f.skill != sb::AimSkill::novice && f.features[sb::Feature::lead];
  f.bot.intercepts = leads ? 3 : 1;
  f.bot.target_arc = leads;
  f.bot.aim_dz = held ? held->aim_dz : 0;
  // A rocket near at hand is aimed at his middle for the direct blow; farther off at his feet,
  // where the blast reaches him whichever way he steps.
  if (held && held->bit == f.arsenal->launcher) {
    for (const sb::Contact& k : f.bot.senses.contacts)
      if (k.ent == f.bot.target && ai::known(k.seen) &&
          dist(s.world.entities[static_cast<size_t>(me)].origin, at_of(k.seen)) < kRocketNear)
        f.bot.aim_dz = 0;
    // (`aimclear`) The height aimed at whose line bursts within the blast's reach of the shooter
    // (a lip under its feet before his, a door frame by it): his middle, then his chest, the first
    // that clears; the burst check (`blastsafe`) held the shot on it before.
    if (f.features[sb::Feature::aimclear] && !dead)
      for (const sb::Contact& k : f.bot.senses.contacts) {
        if (k.ent != f.bot.target || !k.visible) continue;
        const sh::Vec3f o = s.world.entities[static_cast<size_t>(me)].origin;
        const sh::Vec3f eye{o.x, o.y, o.z + f.bot.senses.set.view_height};
        const sh::Vec3f him = at_of(k.seen);
        const auto bursts_near = [&](float dz) {
          const sh::Vec3f to{him.x, him.y, him.z + dz};
          return dist(sh::trace_move(s.sv, eye, {}, {}, to, sh::kTraceMissile, me).endpos, o) <
                 kBlastNear;
        };
        if (!bursts_near(f.bot.aim_dz)) break;
        for (const float dz : {0.f, kAimChest})
          if (dz > f.bot.aim_dz && !bursts_near(dz)) {
            f.bot.aim_dz = dz;
            f.stats.aim_cleared++;
            break;
          }
        break;
      }
  }

  if (f.escort.leader >= 0) escort_sense(f, s, me, t);
  if (f.hunts) hunt_sense(f, s);
  if (f.features[sb::Feature::watch])
    watch_look(f, s, me, dead, t, why);  // the ways in watched (watch.hpp)
  // (`holdra`) A holder by its armour, the armour lying there: the view on it, where the enemy
  // comes to take it.
  if (!dead && f.features[sb::Feature::holdra] && f.role == Role::hold && f.mind.claim >= 0 &&
      !f.bot.aiming) {
    const ItemSpot& hi = f.items[static_cast<size_t>(f.mind.claim)];
    const bool armour = hi.kind == ItemKind::armor2 || hi.kind == ItemKind::armor3;
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    if (armour && hi.present && dist(hi.at, me_at) < kHoldLook) {
      f.bot.look = true;
      f.bot.look_at = {hi.at.x, hi.at.y, hi.at.z + 16};
      f.bot.look_by = "held";
      f.stats.held_looks++;
    }
  }
  // Or one an enemy comes by, a rocket ready (spam.hpp).
  if (f.features[sb::Feature::spam]) spam_news_look(f, s, me, st, t);
  if (!dead) lob_look(f, s, me, st, t);  // (`earlob`) a throw round a corner
  if (!dead && f.features[sb::Feature::pokelook]) button_poke_look(f, s, me, t);
  // (`unlocklook`) The goal item's shot button: the bot's look, so the step's hand turns the view.
  f.unlock_looked = false;
  if (!dead && f.features[sb::Feature::unlocklook] && f.unlocking && f.goal_item >= 0 &&
      !f.bot.aiming) {
    const ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
    const sh::Vec3f o = s.world.entities[static_cast<size_t>(me)].origin;
    const sh::Vec3f eye{o.x, o.y, o.z + 22};
    const sh::Vec3f d = it.button_at - eye;
    if (it.button_shoot &&
        std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y) < kUnlockShot) {
      const sh::HullTrace tr =
          sh::trace_move(s.sv, eye, {}, {}, it.button_at, sh::kTraceNoMonsters, me);
      if (tr.fraction >= 1 || tr.ent == it.button_ent) f.unlock_seen_t = t;
      if (t - f.unlock_seen_t <= kUnlockLookHold_s) {
        f.bot.look = true;
        f.bot.look_at = it.button_at;
        f.bot.look_by = "unlock";
        f.unlock_looked = true;
      }
    }
  }
  foes_seen(f, t);  // (`ledgerforget`) the enemies in sight noted
  // The weapon's rhythm as its holder knows it: a shot is its ammunition going down, the next
  // ready the weapon's refire after it; the hand times its stroke to that (bot.hpp THE MOUSE
  // HAND), and only the arc's beam is traced.
  f.bot.mouse = f.features[sb::Feature::mouse];
  if (held) {
    const float ammo = ammo_of(st, held->ammo);
    const bool shot = st.weapon == f.shot_weapon && ammo < f.shot_ammo;
    if (shot) f.ready_at = t + held->refire_s;
    f.shot_weapon = st.weapon;
    f.shot_ammo = ammo;
    // (`ledgerhits`) its own rockets followed to their bursts, counted on the enemies seen there
    ledger_own_shots(f, s, me, shot && held->bit == f.arsenal->launcher, t);
  } else {
    ledger_own_shots(f, s, me, false, t);
  }
  f.bot.shot_due_s = std::fmax(0.0, f.ready_at - t);
  f.bot.beam = held && held->bit == f.arsenal->beam;
  f.bot.aim_front_only = f.features[sb::Feature::noback] && f.mode == FightMode::roam;
  f.bot.hold_front_only = f.features[sb::Feature::letgo];
  f.bot.run_front_only = f.features[sb::Feature::retreatrun] && f.mode == FightMode::retreat;
  sh::MoveCmd cmd = sb::bot_source(f.bot).next(&f.bot, s.sv, slot, msec);
  KeysSeen keys_was;
  f.keys_by = "-";
  keys_mark(f, cmd, keys_was, "bot");
  f.stats.mode_commands[static_cast<int>(f.mode)]++;
  if (held && held->number >= 0 && held->number <= kMostWeaponNumber)
    f.stats.weapon_commands[held->number]++;
  f.held_number = held ? held->number : 0;

  // The last step onto an item: the nav's node beside it is not the item, so once the pilot has
  // arrived the fighter walks the rest of the way straight at it (the axes under the view).
  if (!dead && f.bot.pilot.pilot.mode == sb::PilotMode::arrived && f.goal_item >= 0) {
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
  keys_mark(f, cmd, keys_was, "item");

  // A button opened by a shot: from where the line meets it, the view on it and the trigger.
  if (!dead && f.unlocking && f.goal_item >= 0 && !f.bot.aiming) {
    const ItemSpot& it = f.items[static_cast<size_t>(f.goal_item)];
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    const sh::Vec3f eye{me_at.x, me_at.y, me_at.z + 22};
    const sh::Vec3f d = it.button_at - eye;
    const double flat = std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y);
    if (f.features[sb::Feature::unlocklook]) {
      // (`unlocklook`) The view is the step's, on the look: stood still, the shot once it is there.
      if (f.unlock_looked && f.unlock_seen_t == t) {  // in sight: out of it, walked on
        const float yaw =
            static_cast<float>(ge::core::libm::atan2(d.y, d.x) * (180.0 / ge::core::kPi));
        const float pitch =
            static_cast<float>(-ge::core::libm::atan2(d.z, flat) * (180.0 / ge::core::kPi));
        float dy = yaw - cmd.angles.y;
        while (dy > 180) dy -= 360;
        while (dy < -180) dy += 360;
        const float dp = pitch - cmd.angles.x;
        cmd.forward = 0;
        cmd.side = 0;
        if ((f.stats.commands & 1) && dy * dy + dp * dp <= kUnlockOn * kUnlockOn)
          cmd.buttons |= sh::kButtonAttack;
      }
    } else if (it.button_shoot && flat < kUnlockShot) {
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

  keys_mark(f, cmd, keys_was, "button");
  if (!dead && f.features[sb::Feature::dodge])
    fighter_dodge(f, s, me, t, why, cmd);  // the side steps (footwork.hpp)
  keys_mark(f, cmd, keys_was, "dodge");

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
      for (const sb::Contact& k : f.bot.senses.contacts)
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
    // In an exit cell, the last leg out as the field proved it (shooterbots/swim.hpp `swim_out`):
    // chest deep and level, so the move's water jump lifts it over the edge.
    const sb::SwimField* field = f.diving >= 0 && f.items[static_cast<size_t>(f.diving)].dive
                                     ? f.items[static_cast<size_t>(f.diving)].dive
                                     : f.swim;
    int32_t exit_node = -1;
    // Kept once it is in an exit cell, until it is out or kSwimExit_s has passed: the swimmer
    // pressed at the edge drifts across the cell's side and would turn back.
    if (field && sb::swim_exiting(*field, me_at, exit_node) && exit_node >= 0) {
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
          sb::swim_out(ms, f.bot.pilot.pilot.g->nodes[static_cast<size_t>(exit_node)].origin, msec);
      cmd.angles.x = out.angles.x;
      cmd.angles.y = out.angles.y;
      f.bot.set_by = "swimexit";
      cmd.buttons = static_cast<uint8_t>((cmd.buttons & ~sh::kButtonJump) | out.buttons);
    } else if (fights && f.features[sb::Feature::swimaim]) {
      // Fighting in the water: the view stays on him, as a person's does, and the way is swum
      // with the keys under it: forward and side along the view's axes, what they leave of the
      // rise pressed straight up.
      swim_under_view(cmd, aim_view, dx, dy, dz);
      f.bot.set_by = "swimaim";
      f.stats.swim_fights++;
    }
  } else if (!dead && fighter_landing(f, s, me, t, cmd)) {
    // Just climbed out: onto its exit node first (footwork.hpp).
    keys_mark(f, cmd, keys_was, "landing");
  } else if (!dead && f.lost_to >= 0) {
    const sh::Vec3f to = f.bot.pilot.pilot.g->nodes[static_cast<size_t>(f.lost_to)].origin;
    const sh::Vec3f me_at = s.world.entities[static_cast<size_t>(me)].origin;
    const double rel = ge::core::libm::atan2(to.y - me_at.y, to.x - me_at.x) -
                       cmd.angles.y * (ge::core::kPi / 180.0);
    cmd.forward = sh::quantize_axis(static_cast<float>(ge::core::libm::cos(rel) * 320));
    cmd.side = sh::quantize_axis(static_cast<float>(-ge::core::libm::sin(rel) * 320));
    const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
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
    keys_mark(f, cmd, keys_was, "lost");
  }
  keys_mark(f, cmd, keys_was, "swim");  // the swim's branches above (the rest marked their own)

  if (dead) {
    // Back in once the mod allows: attack pressed every other command (a press is an edge).
    cmd.buttons = (f.stats.commands & 1) ? sh::kButtonAttack : 0;
    return cmd;
  }
  if (f.want_weapon > 0) {
    cmd.impulse = static_cast<uint8_t>(f.want_weapon);
    f.asked_weapon = f.want_weapon;
    f.want_weapon = 0;
    f.impulse_at = t;
    f.stats.switches++;
  }

  if (!dead && f.features[sb::Feature::spam]) {
    spam_fire(f, s, me, st, t, cmd);
    spam_news_fire(f, s, me, st, t, cmd);
  }
  keys_mark(f, cmd, keys_was, "spam");
  if (!dead && f.features[sb::Feature::packs])
    packs_bore(f, s, me, st, t, cmd);  // its launcher to the quad
  keys_mark(f, cmd, keys_was, "packs");
  if (!dead) button_poke_cmd(f, s, me, t, cmd);
  keys_mark(f, cmd, keys_was, "poke");
  if (!dead) floor_trap_cmd(f, s, me, cmd);
  keys_mark(f, cmd, keys_was, "trap");
  if (!dead && f.escort.leader >= 0) escort_command(f, s, me, t, msec, cmd);
  keys_mark(f, cmd, keys_was, "escort");

  // The trigger: a contact in sight, the view on the aim within the weapon's width at the range.
  bool clicked = false;  // the finger stepped this command
  const sb::Contact* c = nullptr;
  for (const sb::Contact& k : f.bot.senses.contacts)
    if (k.ent == f.bot.target && k.visible) c = &k;
  // Why the finger holds (18b's ledger, counted below with the launcher up): its first reason.
  FingerWhy finger_why = f.bot.target < 0 ? FingerWhy::none
                         : !c             ? FingerWhy::unseen
                                          : FingerWhy::not_aiming;
  if (held && c && f.bot.aiming) {
    const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
    const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
    const float d = dist(eye, at_of(c->seen));
    // Past the width, how loose the finger is, degrees: a novice sprays at a view well off him,
    // an expert fires on him (the held weapon's aim trait, traits.hpp). The arc's beam is held
    // down longer and swept onto him, as a person holds it through the fight.
    const bool beam = held->bit == f.arsenal->beam;
    const double slack =
        f.skill == sb::AimSkill::perfect ? 1.5
        : beam ? sb::scale(f.traits.v[static_cast<size_t>(f.aim_slot)], {14, 9, 6, 4})
               : sb::scale(f.traits.v[static_cast<size_t>(f.aim_slot)], {10, 7, 3, 2});
    const float tol = static_cast<float>(
        ge::core::libm::atan2(held->width, std::fmax(d, 1.f)) * (180.0 / ge::core::kPi) + slack);
    // Not before the reaction to him is over (shooterbots/bot.hpp), however the view lies.
    const bool reacting = sb::bot_reacting(f.bot, s.sv.time);
    // The view the command carries, which a swim or a button may have turned off the aim.
    const float off = view_off(f.bot, cmd);
    bool fire = !reacting && d <= held->range && off <= tol;
    // The finger clicks while the view is roughly on him (sweeping onto him), not only on him.
    bool click = !reacting && d <= held->range && off <= tol * kClickWide;
    f.stats.trigger_checks++;
    f.stats.trigger_far += d > held->range;
    f.stats.trigger_off += d <= held->range && off > tol;
    finger_why = reacting                 ? FingerWhy::reacting
                 : d > held->range        ? FingerWhy::far
                 : off > tol * kClickWide ? FingerWhy::off
                                          : FingerWhy::rhythm;
    if (fire && (mate_in_line(f, s, eye, at_of(c->seen)) ||
                 (held->blast && mate_near(f, s, at_of(c->seen), kBlastClear)))) {
      fire = click = false;
      finger_why = FingerWhy::mate;
      f.stats.held_fire++;
      if (f.team) f.team->stats.held_shots++;
    }
    if (fire && f.escort.leader >= 0 &&
        escort_player_in_line(f, s, eye, at_of(c->seen), kFireClearance)) {
      fire = click = false;
      finger_why = FingerWhy::mate;
      f.stats.held_fire++;
      f.escort.stats.shots_held++;
    }
    // (`blastsafe`) A blast never bursts at its shooter: the crosshair's first wall or player
    // (the burst) within kBlastNear of it holds the shot, and with the target that near another
    // weapon is taken up (a wall or floor that near: held until the view clears it).
    // The press itself is held too (the finger's stroke lands a click decided before).
    const bool burst_near =
        held->blast && blast_bursts_near(f, s, me, cmd.angles, std::fmin(d + 64.f, held->range));
    if (burst_near) {
      fire = click = false;
      finger_why = FingerWhy::burst;
      // (18b's ledger) past the near band: what the crosshair meets first, the world (0) or a
      // player (1), and a player behind the line to him seen clear (2: the lead off his body)
      if (d >= kFingerBands[0]) {
        const sh::Vec3f fwd = sh::angle_vectors(cmd.angles).forward;
        const sh::HullTrace tr =
            sh::trace_move(s.sv, eye, {}, {}, eye + fwd * (d + 64.f), sh::kTraceMissile, me);
        const bool body = tr.ent > 0 && tr.ent <= 32;
        f.stats.burst_by[body                                  ? 1
                         : !clear_line(s, eye, at_of(c->seen)) ? 0
                         : off <= kBurstOnAim                  ? 3
                                                               : 2]++;
      }
      f.stats.held_fire++;
      if (d < kBlastNear + 32 && f.want_weapon == 0 && t - f.impulse_at >= kImpulseGap_s)
        choose_weapon(f, st, d, t, nullptr, false);
    }
    // A companion's blast never goes off where it reaches a player: held, and another weapon
    // taken up at once.
    if (f.escort.leader >= 0 && held->blast && !escort_blast_clear(f, s, at_of(c->seen))) {
      if (fire) {
        f.stats.held_fire++;
        f.escort.stats.blasts_held++;
      }
      fire = click = false;
      finger_why = FingerWhy::mate;
      if (f.want_weapon == 0 && t - f.impulse_at >= kImpulseGap_s)
        choose_weapon(f, st, d, t, nullptr, false);
    }
    // The finger (`mouse`): fire clicked again and again, faster as its health runs out; a
    // stream held. Without it, held down while on him.
    const bool hand = f.bot.mouse && f.bot.aim.mouse;
    const double stress = std::fmin(1.0, std::fmax(0.0, (100.0 - st.health) / 80.0));
    const bool down =
        (hand ? f.bot.finger.step(msec / 1000.0, click, held->stream, stress, f.bot.shot_due_s)
              : fire) &&
        !burst_near;
    clicked = true;
    if (down) {
      cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
      f.stats.shots++;
      finger_why = FingerWhy::down;
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
  if (clicked) f.after_left = kAfterShots;  // in sight: a loss earns its rockets again
  if (!dead && !clicked && f.features[sb::Feature::afterfire])
    after_fire(f, s, me, st, held, t, cmd);
  if (!dead && !clicked && f.features[sb::Feature::earfire] &&
      (cmd.buttons & sh::kButtonAttack) == 0)
    ear_fire(f, s, me, st, held, t, cmd);
  if (!dead && (cmd.buttons & sh::kButtonAttack) == 0) lob_fire(f, s, me, st, t, cmd);
  if (!dead) swim_view(f, s, me, msec, swim, cmd);  // under water the view turns at a hand's pace
  // A view set above (a button, a doorway, the swim's way out) reached by the hand, not jumped
  // to; no shot of its own until the hand is there (shooterbots/bot.hpp `bot_hand_override`).
  if (!dead) {
    const bool wet = s.world.entities[static_cast<size_t>(me)].waterlevel >= 2;
    const bool pressed = (cmd.buttons & sh::kButtonAttack) != 0;
    if (sb::bot_hand_override(f.bot, cmd, msec, wet) > kHandThere) {
      cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonAttack);
      if (pressed) finger_why = FingerWhy::turning;
    }
    // 18b's ledger: the command's finger with the launcher up, by its reason.
    f.finger_last = -1;
    if (holds_launcher(st)) {
      const bool attack = (cmd.buttons & sh::kButtonAttack) != 0;
      if (attack && finger_why != FingerWhy::down) finger_why = FingerWhy::other;
      if (!attack && finger_why == FingerWhy::down) finger_why = FingerWhy::rhythm;
      f.stats.finger[static_cast<int>(finger_why)]++;
      f.finger_last = static_cast<int32_t>(finger_why);
      finger_band(f, s, me, c, finger_why);
    } else if (has_launcher(st)) {
      const FingerWhy why_not = launcher_rounds(st) < 1 ? FingerWhy::empty
                                : c != nullptr          ? FingerWhy::weapon_fight
                                                        : FingerWhy::weapon;
      f.stats.finger[static_cast<int>(why_not)]++;
      f.finger_last = static_cast<int32_t>(why_not);
      finger_band(f, s, me, c, why_not);
    }
  }
  // THE TURN BACK TAKEN ROUND (`uturn`): on the ground at a run, a wish more than `kTurnBack`
  // off the velocity is pressed across it instead (`kTurnAcross` on the side the wish lies, or the
  // view leads), until the velocity has come round: a person turning back curves through it at
  // speed (the ground's acceleration across 300 u/s turns it ~570 degrees a second, a circle of
  // ~32 units) where a straight press back runs through a stop.
  if (!dead && f.features[sb::Feature::uturn]) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(me)];
    const sb::PilotMode pm = f.bot.pilot.pilot.mode;
    const bool feet = pm == sb::PilotMode::walk || pm == sb::PilotMode::arrived ||
                      pm == sb::PilotMode::idle || pm == sb::PilotMode::wait;
    if (feet && d.waterlevel == 0 && (d.flags & sh::kFlagOnGround) != 0 &&
        turn_back_round(cmd, d.origin, d.velocity, f.bot.pilot.world))
      f.stats.turned_round++;
  }
  keys_mark(f, cmd, keys_was, "uturn");
  // THE BRINK: with the feet free, on the ground or in the air between a stair's steps, a hop's
  // landings or a knock, no key carries it into lava or slime (in the air a wish against the
  // run still brakes at the full acceleration: the air's cap is on the speed gained, not lost).
  bool brinked = false;
  f.brink_note = -1;
  if (!dead && f.features[sb::Feature::brink]) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(me)];
    const sb::PilotMode pm = f.bot.pilot.pilot.mode;
    const bool free_feet = pm == sb::PilotMode::walk || pm == sb::PilotMode::strafe ||
                           pm == sb::PilotMode::hop || pm == sb::PilotMode::idle ||
                           pm == sb::PilotMode::arrived || pm == sb::PilotMode::wait;
    // A flight the pilot proved lands safe (a gap trick's, or a hop by one) is not braked.
    if (free_feet && d.waterlevel == 0 && !f.bot.pilot.pilot.brink_free) {
      const sb::BrinkGuard bg =
          sb::brink_guard(s.sv, *f.bot.pilot.pilot.g, me, d.origin, d.velocity, cmd,
                          f.features[sb::Feature::brinkfall]);
      if (bg.ran)
        f.brink_note = bg.ways + (bg.fall_in ? 20 : bg.run_in ? 10 : 0) + (bg.changed ? 100 : 0);
      if (bg.changed) {
        f.stats.brink++;
        brinked = true;
      }
    }
  }
  keys_mark(f, cmd, keys_was, "brink");
  // The keys: pressed by a hand on a keyboard (keyboard.hpp), or the side key turned round as a
  // person's hand turns it (footwork.hpp).
  if (f.features[sb::Feature::keys]) {
    const sh::Edict& d = s.sv.edicts[static_cast<size_t>(me)];
    fighter_keyboard(f, cmd, d.velocity, (d.flags & sh::kFlagOnGround) != 0, brinked, d.origin);
    // (`shotjump`) A rocket jump's shot waits for the jump key: the mouse held still on the shot's
    // view, the trigger pressed on the command the key goes down (a shot before it bursts under a
    // player still on the ground: a flat run, no lift). Given up after `kShotWait_s`.
    if (f.features[sb::Feature::shotjump] && !dead) {
      if (f.bot.pilot.pilot.shot_now && (cmd.buttons & sh::kButtonAttack) != 0) {
        f.shot_wait_until = t + kShotWait_s;
        f.shot_view = cmd.angles;
      }
      if (t <= f.shot_wait_until) {
        cmd.angles.x = f.shot_view.x;
        cmd.angles.y = f.shot_view.y;
        f.bot.view_by = "shot";
        if ((cmd.buttons & sh::kButtonJump) != 0) {
          cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
          f.shot_wait_until = -1e9;
          f.stats.shot_waits++;
        } else {
          cmd.buttons = static_cast<uint8_t>(cmd.buttons & ~sh::kButtonAttack);
        }
      }
    }
  } else {
    fighter_keys(f, cmd);
  }
  return cmd;
}

}  // namespace ge::shooterbots
