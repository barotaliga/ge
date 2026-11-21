// THE LAUNCHERS FIRED OUT OF SIGHT (launcher_fire.hpp): the rocket after one lost, at a sound, the
// lob's flight through the world and its throw, the finger's ledger by range.
//
// ge:summary A rocket after one lost, a rocket at a sound, the lob round a corner, the ledger
// ge:keywords afterfire earfire earlob lob_impact lob_lands lob_look lob_fire finger_band
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooterbots/launcher_fire.hpp"

#include <cmath>

#include "ai/memory.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/trace.hpp"
#include "shooterbots/intercept.hpp"

namespace ge::shooterbots {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;
namespace ai = ge::ai;

namespace {

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

sh::Vec3f at_of(const ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

// Where a grenade thrown from `from` under `angles` first meets the world or a player, flown in
// steps of kLobStep_s for its fuse at most (the classic throw: `speed` along the view, `up` along
// its up, the server's gravity).
constexpr double kLobStep_s = 0.05;
constexpr int kLobSteps = 50;  // the fuse's 2.5 s in steps
sh::Vec3f lob_impact(const ge::shooterbots::BotView& s, int me, sh::Vec3f from, sh::Vec3f angles) {
  const sh::ViewAxes ax = sh::angle_vectors(angles);
  const sb::Shot shot{600, true, 200, sb::kServerGravity};
  const sh::Vec3f v = ax.forward * shot.speed + ax.up * shot.up;
  sh::Vec3f at = from;
  for (int k = 1; k <= kLobSteps; k++) {
    const auto tf = static_cast<float>(k * kLobStep_s);
    const sh::Vec3f next{from.x + v.x * tf, from.y + v.y * tf,
                         from.z + v.z * tf - 0.5f * shot.gravity * tf * tf};
    const sh::HullTrace tr = sh::trace_move(s.sv, at, {}, {}, next, sh::kTraceMissile, me);
    if (tr.fraction < 1 || tr.startsolid) return tr.endpos;
    at = next;
  }
  return at;
}

// Whether a throw under `angles` from `from` bursts by `spot` and safely: its first impact within
// kLobReach of it, no nearer the shooter than the blast's reach, no mate by it.
bool lob_lands(const Fighter& f, const ge::shooterbots::BotView& s, int me, sh::Vec3f from,
               sh::Vec3f angles, sh::Vec3f spot) {
  const sh::Vec3f hit = lob_impact(s, me, from, angles);
  return dist(hit, spot) <= kLobReach && dist(hit, from) >= kBlastNear &&
         !mate_near(f, s, hit, kBlastClear);
}

}  // namespace

// THE ROCKET AFTER HIM (`afterfire`): the one aimed at gone from sight within kAfter_s, the
// aim holding where he is carried, the launcher up and its refire over: fired once the
// crosshair's burst lands within kAfterReach of where he is now believed, the burst no nearer the
// shooter than kBlastNear (`blastsafe`'s rule) and no mate by it; kAfterShots a loss.
void after_fire(Fighter& f, const ge::shooterbots::BotView& s, int me, const Stats& st,
                const BotWeapon* held, double t, sh::MoveCmd& cmd) {
  if (held == nullptr || held->bit != f.arsenal->launcher || !f.bot.aiming || f.after_left <= 0 ||
      t < f.ready_at || launcher_rounds(st) < 1)
    return;
  const sb::Contact* k = nullptr;
  for (const sb::Contact& q : f.bot.senses.contacts)
    if (q.ent == f.bot.target && !q.visible && ai::known(q.seen)) k = &q;
  if (k == nullptr) return;
  const double age = ai::age(k->seen, t);
  if (age > kAfter_s) return;
  const float a = static_cast<float>(age);
  const sh::Vec3f him{static_cast<float>(k->seen.x) + k->course.x * a,
                      static_cast<float>(k->seen.y) + k->course.y * a,
                      static_cast<float>(k->seen.z)};
  const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
  const sh::Vec3f fwd = sh::angle_vectors(cmd.angles).forward;
  const sh::HullTrace tr =
      sh::trace_move(s.sv, eye, {}, {}, eye + fwd * held->range, sh::kTraceMissile, me);
  const sh::Vec3f burst = tr.endpos;
  if (dist(burst, him) > kAfterReach || dist(burst, self.origin) < kBlastNear ||
      blast_bursts_near(f, s, me, cmd.angles, held->range) || mate_near(f, s, burst, kBlastClear))
    return;
  cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  f.after_left--;
  f.stats.after_shots++;
}

// THE ROCKET AT A SOUND (`earfire`): an enemy heard within kEar_s and not in sight, the launcher
// up and its refire over: fired once the crosshair's burst lands within kEarReach of where he was
// heard with a clear line to it, no nearer the shooter than kBlastNear nor by a mate; one a sound.
void ear_fire(Fighter& f, const ge::shooterbots::BotView& s, int me, const Stats& st,
              const BotWeapon* held, double t, sh::MoveCmd& cmd) {
  if (held == nullptr || held->bit != f.arsenal->launcher || t < f.ready_at ||
      launcher_rounds(st) < 1)
    return;
  const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
  const sh::Vec3f fwd = sh::angle_vectors(cmd.angles).forward;
  bool traced = false;
  sh::Vec3f burst{}, back{};
  for (const sb::Contact& k : f.bot.senses.contacts) {
    if (k.visible || !ai::known(k.heard) || ai::age(k.heard, t) > kEar_s ||
        k.heard.at <= f.ear_fired)
      continue;
    if (!traced) {
      burst =
          sh::trace_move(s.sv, eye, {}, {}, eye + fwd * held->range, sh::kTraceMissile, me).endpos;
      back = burst - fwd * 8.f;
      traced = true;
      if (dist(burst, self.origin) < kBlastNear || mate_near(f, s, burst, kBlastClear) ||
          blast_bursts_near(f, s, me, cmd.angles, held->range))
        return;
    }
    const sh::Vec3f heard = at_of(k.heard);
    if (dist(burst, heard) > kEarReach ||
        sh::trace_move(s.sv, back, {}, {}, heard, sh::kTraceNoMonsters, -1).fraction < 1)
      continue;
    cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
    f.ear_fired = k.heard.at;
    f.stats.ear_shots++;
    return;
  }
}

// 18b's ledger by range: with the one aimed at in sight, the finger pressing (or between its
// clicks), held for the burst, or another weapon up, in the band of his range (kFingerBands).
void finger_band(Fighter& f, const ge::shooterbots::BotView& s, int me, const sb::Contact* c,
                 FingerWhy why) {
  if (c == nullptr) return;
  const int k = why == FingerWhy::down || why == FingerWhy::rhythm ? 0
                : why == FingerWhy::burst                          ? 1
                : why == FingerWhy::weapon_fight                   ? 2
                                                                   : -1;
  if (k < 0) return;
  const sh::Vec3f at = s.world.entities[static_cast<size_t>(me)].origin;
  const float d = dist(at, at_of(c->seen));
  f.stats.finger_band[d < kFingerBands[0] ? 0 : d < kFingerBands[1] ? 1 : 2][k]++;
}

void lob_look(Fighter& f, const ge::shooterbots::BotView& s, int me, const Stats& st, double t) {
  const BotArsenal& a = *f.arsenal;
  const BotWeapon* gl = a.lob != 0 ? weapon_row(a, a.lob) : nullptr;
  const ge::shooter::WorldEntity& self = s.world.entities[static_cast<size_t>(me)];
  if (f.lob_on && (t > f.lob_until || f.bot.aiming || gl == nullptr || !loaded(st, *gl) ||
                   self.health <= 0 || self.waterlevel >= 2))
    f.lob_on = false;
  if (!f.features[sb::Feature::earlob] || gl == nullptr) return;
  if (!f.lob_on) {
    if (t < f.lob_next || f.bot.aiming || f.news_on || !loaded(st, *gl) || self.health <= 0 ||
        self.waterlevel >= 2)
      return;
    f.lob_next = t + kThink_s;
    const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
    for (const sb::Contact& k : f.bot.senses.contacts) {
      if (k.visible) continue;
      // The newer of where he was heard and where he was last seen.
      const bool by_ear = ai::known(k.heard) && (!ai::known(k.seen) || k.heard.at > k.seen.at);
      const ai::Memory& m = by_ear ? k.heard : k.seen;
      if (!ai::known(m) || ai::age(m, t) > kLob_s || m.at <= f.lob_heard) continue;
      const sh::Vec3f spot = at_of(m);
      const float d = dist(eye, spot);
      if (d < kLobNear || d > kLobFar) continue;
      // A straight line there is a rocket's (`afterfire`, `earfire`).
      if (sh::trace_move(s.sv, eye, {}, {}, spot, sh::kTraceNoMonsters, -1).fraction >= 1) continue;
      float pitch = 0;
      double flight = 0;
      if (!sb::lob_pitch(self.origin, spot, sb::Shot{600, true, 200, sb::kServerGravity}, pitch,
                         flight))
        continue;
      const float yaw =
          static_cast<float>(ge::core::libm::atan2(static_cast<double>(spot.y - self.origin.y),
                                                   static_cast<double>(spot.x - self.origin.x)) *
                             (180.0 / ge::core::kPi));
      const sh::Vec3f view{pitch, yaw, 0};
      if (!lob_lands(f, s, me, self.origin, view, spot)) continue;
      f.lob_on = true;
      f.lob_spot = spot;
      f.lob_view = view;
      f.lob_until = t + kLobHold_s;
      f.lob_heard = m.at;
      f.lob_asked = false;
      break;
    }
    if (!f.lob_on) return;
  }
  // The view there on the hand, the lob taken up.
  const sh::Vec3f eye{self.origin.x, self.origin.y, self.origin.z + f.bot.senses.set.view_height};
  f.bot.look = true;
  f.bot.look_at = eye + sh::angle_vectors(f.lob_view).forward * 256.f;
  f.bot.look_by = "lob";
  if (st.weapon != gl->bit && !f.lob_asked && f.want_weapon == 0) {
    f.want_weapon = gl->number;
    f.lob_asked = true;
  }
}

void lob_fire(Fighter& f, const ge::shooterbots::BotView& s, int me, const Stats& st, double t,
              ge::shooter::MoveCmd& cmd) {
  const BotArsenal& a = *f.arsenal;
  if (!f.lob_on || a.lob == 0 || st.weapon != a.lob || t < f.ready_at) return;
  float dy = cmd.angles.y - f.lob_view.y;
  while (dy > 180) dy -= 360;
  while (dy < -180) dy += 360;
  dy = std::fabs(dy);
  if (dy > kLobOn || std::fabs(cmd.angles.x - f.lob_view.x) > kLobOn) return;
  // Flown again from where it stands now, under the view it has.
  const sh::Vec3f from = s.world.entities[static_cast<size_t>(me)].origin;
  if (!lob_lands(f, s, me, from, cmd.angles, f.lob_spot)) return;
  cmd.buttons = static_cast<uint8_t>(cmd.buttons | sh::kButtonAttack);
  f.lob_on = false;
  f.stats.lob_shots++;
}

}  // namespace ge::shooterbots
