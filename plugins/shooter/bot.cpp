// ge:summary The bot: senses, the pilot's command, the view on a contact, the axes under the view
// ge:keywords bot bot_init bot_source bot_next aim hold release look watch pre-aim
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooter/bot.hpp"

#include <array>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/intercept.hpp"
#include "shooter/trace.hpp"
#include "shooter/usercmd.hpp"

namespace ge::shooter {

namespace {

float unwrap(float deg, float near) {
  float d = deg - near;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return near + d;
}

float wrap(float deg) { return unwrap(deg, 0); }

// A new contact this near the view (degrees) was pre-aimed at.
constexpr float kPreAim = 5;
// The one in hand out of sight this long is a new sighting when he shows again, s.
constexpr double kResight_s = 0.3;
// An enemy lost from sight is followed along his course this long (`corner`): a person's eye
// carries on a reaction's worth before it sees him gone, then waits where he went.
constexpr double kLostCarry_s = 0.2;
// The one in hand, in sight, is kept over another in sight unless that one is this much nearer
// (his distance over the other's): a player stays on whom he is shooting, and a wide view brings
// others into it all the time.
constexpr float kSwitchNearer = 2;

constexpr double kCalmBand = 25;    // degrees the view may lie off the way it walks, still
constexpr double kCalmTurn = 180;   // degrees a second it turns past the band
constexpr double kCalmAbout = 360;  // and for an about-face
// Nothing in sight any more, the view goes back to the way (or over to a spot watched) this many
// degrees a second: a hand's pace, not the aim's flick. Measured on e1m2 (se-people on the bots'
// recordings): the view turns 103 degrees a second on average, 7 % of it past 360 (people 107 and
// 6 %; the aim's flick back gave 162 and 14 %); at 180 the bots fought less (frags -0.9 a minute).
constexpr double kBackTurn = 360;

bool walking_mode(PilotMode pm) {
  return pm == PilotMode::walk || pm == PilotMode::idle || pm == PilotMode::wait ||
         pm == PilotMode::ride || pm == PilotMode::arrived;
}
constexpr double kCalmDrift = 3;   // degrees a second of the hand's drift, at most
constexpr float kCalmAhead = 320;  // units on along the route the calm view looks at
// The glance (bot.hpp `glance`): how often, how far round, what counts as open, how long held.
constexpr double kGlanceMin_s = 1.2, kGlanceSpread_s = 1.3;
constexpr float kGlanceSide = 75, kGlanceBack = 120;
constexpr float kGlanceReach = 2048, kGlanceOpen = 400;
constexpr double kGlanceHold_s = 0.4;
constexpr double kGlanceBand = 5;  // degrees: the glance looks right at its side
// The sound's swipe (bot.hpp `listen`): a sound this fresh is swiped at, unless its maker was seen
// this recently this near it, or the view is already within `kSoundAhead` degrees of it.
constexpr double kSoundFresh_s = 0.25, kSoundKnown_s = 3;
constexpr float kSoundSame = 300;
constexpr double kSoundAhead = 30;

// The calm view's strokes take this many times a turn meant's (THE MOUSE HAND, bot.hpp): people
// look round at ease while they walk and jump, and turn fast onto an enemy or about.
constexpr double kCalmPace = 1.15;
constexpr double kCalmPitchTol = 6;  // the calm view's pitch is put right past this, degrees
constexpr double kSwimBand = 20;     // a swimmer's view may lie this far off the swim, degrees
// Off the aim (the calm view, a hop's sweeps, a swim), a stroke the other way waits until the one
// under way is this far done: slowed to a quarter of its mean, the view never snaps round (a hop's
// side switch reversed it at speed; people's view reverses 0.09 times a second).
constexpr double kTurnBackAtEase = 0.9;
constexpr double kGameTol = 3;  // a view the game set is near enough this close, degrees
// A rocket jump's shot (pilot.hpp `shot_pitch`): the hand's stroke down onto its pitch starts
// its Fitts time onto a target this wide (degrees) and a little more (s) before the shot, so it
// lands as the jump goes; the shot itself fires at the jump's pitch exactly.
constexpr double kShotWidth = 4, kShotSpare_s = 0.04;
// And the shot waits until the view is this near the jump's, degrees (yaw, pitch).
constexpr double kShotReadyYaw = 12, kShotReadyPitch = 6;

// The view a step on through the mouse hand toward `want` (yaw, pitch; yaw unwrapped near the
// view): close enough within `tol_x`, `tol_y` degrees, the target `width` degrees across, a shot
// due in `due` s, strokes at `pace` of a turn meant, leading the target when `lead`.
void hand_step(Bot& b, const double want[2], double dt, double tol_x, double tol_y, double width,
               double due, double pace, bool lead, double out[2]) {
  // The hand follows a view something else turned (a jump's air control, the swim's view).
  if (std::fabs(b.hand.x() + b.yaw) > 0.01 || std::fabs(b.hand.y() + b.pitch) > 0.01)
    b.hand.reset(-b.yaw, -b.pitch);
  ai::MouseWant w;
  w.x = -want[0];
  w.y = -want[1];
  w.tol_x = tol_x;
  w.tol_y = tol_y;
  w.width = width;
  w.due_s = due;
  w.pace = pace;
  w.lead = lead;
  if (!b.aiming) w.turn_back = kTurnBackAtEase;
  double x = 0, y = 0;
  b.hand.step(dt, w, x, y);
  out[0] = -x;
  out[1] = std::fmax(-70.0, std::fmin(80.0, -y));
}

MoveCmd bot_next(void* ctx, const Server& sv, int slot, int msec) {
  Bot& b = *static_cast<Bot*>(ctx);
  const int me = slot + 1;
  const Edict& self = sv.edicts[static_cast<size_t>(me)];
  ai::Why* why = b.pilot.why ? b.pilot.why->follow(slot, b.pilot.command) : nullptr;

  client_view(sv, me, b.view);
  sense(b.senses, sv, b.view, me, {b.pitch, b.yaw, 0}, sv.time, why);

  // The pilot's command, with its own trace (pilot_next advances the command count). Under the
  // mouse hand a rocket jump's shot waits for the hand's view (pilot.hpp `shot_hand`).
  b.pilot.pilot.shot_hand = b.mouse && b.aim.mouse;
  MoveCmd cmd = pilot_source(b.pilot).next(&b.pilot, sv, slot, msec);
  const float pilot_yaw = b.pilot.pilot.yaw;
  // Swimming (`pools`), the pilot's wish is pressed again under another view in three dimensions:
  // its pitch dives and rises; not while it backs out of a pit (escape.hpp: its keys are the
  // proof's, under the view it shoots with).
  const Vec3f pilot_view = cmd.angles;
  const bool swim3d = b.pools && self.waterlevel >= 2 && !b.pilot.pilot.escaping;
  b.stats.commands++;

  const Vec3f eye{self.origin.x, self.origin.y, self.origin.z + b.senses.set.view_height};
  const Contact* c = freshest(b.senses, eye);
  if (b.keep && b.aiming && c && c->ent != b.target && c->visible) {
    for (const Contact& k : b.senses.contacts) {
      if (k.ent != b.target || !k.visible) continue;
      const auto from_eye = [&](const ai::Memory& m) {
        return Vec3f{static_cast<float>(m.x) - eye.x, static_cast<float>(m.y) - eye.y,
                     static_cast<float>(m.z) - eye.z};
      };
      const Vec3f to_k = from_eye(k.seen), to_c = from_eye(c->seen);
      if (dot(to_k, to_k) <= kSwitchNearer * kSwitchNearer * dot(to_c, to_c)) {
        c = &k;
        b.stats.kept++;
      }
    }
  }
  const bool fresh =
      c && ai::known(c->seen) && (c->visible || ai::age(c->seen, sv.time) <= b.aim.hold_s);
  double want[2];
  if (fresh) {
    // The lead for the hands' lag (`aim.lead_s`): the mouse hand leads its own strokes by the
    // target's motion over each stroke's time (ai/mouse.hpp), so none is added for it.
    const float hand_lead = b.mouse && b.aim.mouse ? 0.f : b.aim.lead_s;
    // Lost from sight, his course is followed a moment only (`corner`): no wall is seen through.
    const double age = ai::age(c->seen, sv.time);
    const double head = (b.corner && !c->visible ? std::fmin(age, kLostCarry_s) : age) + hand_lead;
    {
      const Vec3f to = Vec3f{static_cast<float>(c->seen.x), static_cast<float>(c->seen.y),
                             static_cast<float>(c->seen.z)} -
                       eye;
      b.aim_dist = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
    }
    // The network's part, in the air only: a player's arc does not change in a round trip, but
    // on the ground a strafer's steps do, and a lead of the round trip on them (measured on top
    // of the hands' own) swings the aim wider than it gains (se-team at 200 ms: none on the
    // ground keeps a strong aim at its LAN rate).
    const double net = b.shot_speed > 0 || !b.rewound ? b.lag_s : 0;
    MovingTarget m;
    m.at = {static_cast<float>(c->seen.x), static_cast<float>(c->seen.y),
            static_cast<float>(c->seen.z)};
    m.velocity = c->course;
    if (b.target_arc && c->ent >= 1 &&
        c->ent <= sv.set.maxclients) {  // a player falls; a monster flies
      const Vec3f down{m.at.x, m.at.y, m.at.z - 4096};
      const HullTrace tr = trace_move(sv, m.at, {}, {}, down, kTraceNoMonsters, c->ent);
      m.floor_z = tr.endpos.z + 24;
      m.airborne = m.at.z - m.floor_z > 4;
    }
    const double lead = head + (m.airborne ? net : 0);
    // From where its own eye will be when the hands have caught up (`selfaim`).
    const Vec3f look_from = b.selfaim ? eye + self.velocity * hand_lead : eye;
    float yaw = 0, pitch = 0;
    Vec3f at = mover_at(m, lead);
    if (b.shot_speed > 0) {
      Shot shot;
      shot.speed = b.shot_speed;
      shot.lob = b.lob;
      const Vec3f muzzle = b.lob ? self.origin : eye;
      const Intercept ic = intercept(muzzle, m, lead, shot, b.intercepts);
      at = ic.at;
      at.z += b.aim_dz;
      aim_angles(look_from, at, yaw, pitch);
      if (ic.lobbed) {
        double flight = 0;
        lob_pitch(muzzle, at, shot, pitch, flight);  // the arc's pitch onto the aimed point
      }
    } else {
      at.z += b.aim_dz;
      aim_angles(look_from, at, yaw, pitch);
    }
    if (!b.aiming) {
      // Acquired: the hands start from where the view is, so the turn waits their reaction.
      const std::array<double, 2> from{b.yaw, b.pitch};
      b.aim_hands.reset(from);
      b.strafe = b.pilot.pilot.set.strafe;
      b.hop = b.pilot.pilot.set.hop;
      b.pilot.pilot.set.strafe = false;
      b.pilot.pilot.set.hop = false;
    }
    const bool resight = c->ent == b.target && sv.time - b.target_seen_at > kResight_s;
    if (c->visible && (!b.aiming || c->ent != b.target || resight) && b.aim.react_med > 0) {
      // A new sighting (another enemy, or this one back after a while out of sight): the
      // reaction drawn for it, of which the hands' lag is the last part. One the view was already
      // on (pre-aimed at a doorway, say) is taken in half the time.
      double r = draw_reaction(b.aim, core::next(b.react_rng));
      const float dy = wrap(yaw - b.yaw), dp = pitch - b.pitch;
      const bool pre = dy * dy + dp * dp <= kPreAim * kPreAim;
      if (pre) {
        r *= 0.5;
        b.stats.pre_aimed++;
      }
      b.stats.reactions++;
      b.stats.react_s += r;
      b.react_until = sv.time + r;
      // The hands' own lag makes up the reaction's last part; the mouse hand has none (its
      // stroke's time is on top, as a person's is).
      b.freeze_until = sv.time + r - (b.mouse && b.aim.mouse ? 0 : b.aim.hands[0].delay_s);
      b.freeze_view[0] = b.yaw;
      b.freeze_view[1] = b.pitch;
      if (why) {
        why->decide("react");
        why->fact("reaction_s", r);
        why->choose(pre ? "pre-aimed" : "react",
                    pre ? "the view already on him" : "a new sighting");
      }
    }
    b.aiming = true;
    b.target = c->ent;
    if (c->visible) b.target_seen_at = sv.time;
    want[0] = unwrap(yaw, b.yaw);
    want[1] = pitch;
    if (sv.time < b.freeze_until) {
      want[0] = b.freeze_view[0];
      want[1] = b.freeze_view[1];
    }
    if (c->visible)
      b.stats.aiming++;
    else
      b.stats.holding++;
    if (why) {
      why->decide("aim");
      why->fact("ent", c->ent);
      why->fact("age", ai::age(c->seen, sv.time));
      why->choose(c->visible ? "track" : "hold",
                  c->visible ? "a contact in sight" : "where it was, carried along");
    }
  } else {
    want[0] = unwrap(pilot_yaw, b.yaw);
    want[1] = 0;
    if (b.look) {
      float yaw = 0, pitch = 0;
      aim_angles(eye, b.look_at, yaw, pitch);
      want[0] = unwrap(yaw, b.yaw);
      want[1] = pitch;
    }
    // A calm view goes back onto the way, or over to a spot watched, at a hand's pace
    // (`kBackTurn`), not the aim's flick: nothing is in sight to snap onto.
    if (b.calm && b.paced) {
      const double dt = msec / 1000.0;
      const double dy = want[0] - b.yaw, dp = want[1] - b.pitch;
      want[0] = b.yaw + std::fmax(-kBackTurn * dt, std::fmin(kBackTurn * dt, dy));
      want[1] = b.pitch + std::fmax(-kBackTurn * 0.5 * dt, std::fmin(kBackTurn * 0.5 * dt, dp));
    }
    const float off = wrap(b.yaw - pilot_yaw);
    // A calm walker hands back once within the calm band, which holds the view still from there.
    const bool calm_walk = b.calm && walking_mode(b.pilot.pilot.mode);
    const float rel = calm_walk ? static_cast<float>(kCalmBand) : kRelease;
    const bool on_way = off < rel && off > -rel && b.pitch < rel && b.pitch > -rel;
    if (b.aiming && (on_way || b.look)) {
      // Let go: back onto the pilot's way, or straight on to the spot watched (the hands kept).
      b.aiming = false;
      b.target = -1;
      b.pilot.pilot.set.strafe = b.strafe;
      b.pilot.pilot.set.hop = b.hop;
      b.looking = b.look;
      if (why) {
        why->decide("aim");
        why->choose("release", b.look ? "no contact: the view on the spot watched"
                                      : "no contact: the view back on the pilot's way");
      }
    } else if (!b.aiming && b.look && !b.looking) {
      // Took up watching: the hands start from where the view is.
      const std::array<double, 2> from{b.yaw, b.pitch};
      b.aim_hands.reset(from);
      b.looking = true;
    } else if (b.looking && !b.look && on_way) {
      b.looking = false;  // back on the pilot's way
    }
  }

  // A rocket jump's shot this command: its view is the jump's, whatever the aim was on. And one
  // coming: under the hand, the stroke onto the jump's view starts its Fitts time before the
  // shot, so it lands as the jump goes (the aim and a look give way to it).
  Pilot& jp = b.pilot.pilot;
  const bool blast_shot = jp.shot_now;
  const double shot_dx = wrap(jp.shot_yaw - b.yaw), shot_dy = jp.shot_pitch - b.pitch;
  const bool shot_due =
      b.mouse && b.aim.mouse && !swim3d && jp.shot_pitch != 0 && !blast_shot &&
      jp.shot_in_s <=
          b.hand.fitts(std::sqrt(shot_dx * shot_dx + shot_dy * shot_dy), kShotWidth) + kShotSpare_s;
  // People's hop start under way (pilot.hpp `recipes`): its view is the recording's, a person's
  // own mouse already, so it is played as it is, with no watch over it (an aim still comes first).
  const bool recipe_on = jp.recipe >= 0 && jp.mode == PilotMode::hop;
  if ((b.aiming || (b.looking && !recipe_on)) && !blast_shot && !shot_due) {
    static constexpr const char* kNames[2] = {"aim_yaw", "aim_pitch"};
    ai::record_intent(why, kNames, want);
    double out[2];
    if (b.mouse && b.aim.mouse) {
      // The hand: onto the enemy within his width (his middle's half width, 16 units, at his
      // range), a shot weapon's stroke timed to its readiness, a beam traced; a lost one or a
      // spot watched loosely.
      const bool sighted = b.aiming && c && c->visible;
      const double half = core::libm::atan2(16.0, std::fmax(1.0, static_cast<double>(b.aim_dist))) *
                          (180 / core::kPi);
      const double tol = sighted ? std::fmax(0.4, half * 0.7) : 3;
      const double due = sighted && !b.beam ? b.shot_due_s : 0;
      hand_step(b, want, msec / 1000.0, tol, tol, std::fmax(1.0, half * 2), due, b.aiming ? 1 : 1.3,
                sighted, out);
    } else {
      b.aim_hands.step(want, out);
    }
    if (!b.aiming) b.stats.looking++;
    b.view_by = b.aiming ? "aim" : "look";
    b.wanted[0] = want[0];
    b.wanted[1] = want[1];
    b.pressed[0] = out[0];
    b.pressed[1] = out[1];
    b.yaw = static_cast<float>(out[0]);
    b.pitch = static_cast<float>(out[1]);
    if (swim3d)
      turn_axes_swim(cmd.forward, cmd.side, cmd.up, pilot_view, {b.pitch, b.yaw, 0});
    else
      turn_axes(cmd.forward, cmd.side, pilot_yaw, b.yaw);
    cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
  } else {
    if (recipe_on) {
      b.view_by = "recipe";
      b.yaw = pilot_yaw;
      cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
    } else if (b.calm && walking_mode(b.pilot.pilot.mode)) {
      // The calm view (bot.hpp): still in the band, turned at a hand's pace past it, drifting.
      const double dt = msec / 1000.0;
      b.calm_t += dt;
      // Where it looks: the route's point about `kCalmAhead` units on, so the view turns before a
      // corner to see round it and holds still on a straight; else the way it walks.
      float look_yaw = pilot_yaw;
      if (b.lookway) {
        // The way in sight (bot.hpp `lookway`): the furthest point on that the eye still sees.
        const Edict& me_ent = sv.edicts[static_cast<size_t>(me)];
        std::vector<uint8_t> breaks;
        const std::vector<Vec3f> path = pilot_path(b.pilot.pilot, me_ent.origin, &breaks);
        const float vh = b.senses.set.view_height;
        const Vec3f eye_at{me_ent.origin.x, me_ent.origin.y, me_ent.origin.z + vh};
        float run = 0;
        size_t best = 0;
        for (size_t k = 1; k < path.size() && k < breaks.size() && !breaks[k]; k++) {
          const Vec3f d = path[k] - path[k - 1];
          run += std::sqrt(d.x * d.x + d.y * d.y);
          const Vec3f to_eye{path[k].x, path[k].y, path[k].z + vh};
          if (trace_move(sv, eye_at, {}, {}, to_eye, kTraceNoMonsters, me).fraction < 1) break;
          best = k;
          if (run >= kCalmAhead) break;
        }
        const Vec3f to = path[best] - me_ent.origin;
        if (best > 0 && to.x * to.x + to.y * to.y > 64 * 64) {
          float py = 0, pp = 0;
          aim_angles(me_ent.origin, path[best], py, pp);
          // A point behind the walk (a node just passed): the walk's heading instead.
          if (std::fabs(wrap(py - pilot_yaw)) <= 90) look_yaw = py;
        }
      } else {
        const Edict& me_ent = sv.edicts[static_cast<size_t>(me)];
        const std::vector<Vec3f> path = pilot_path(b.pilot.pilot, me_ent.origin, nullptr);
        float run = 0;
        for (size_t k = 1; k < path.size(); k++) {
          const Vec3f d = path[k] - path[k - 1];
          run += std::sqrt(d.x * d.x + d.y * d.y);
          if (run >= kCalmAhead || k + 1 == path.size()) {
            const Vec3f to = path[k] - me_ent.origin;
            if (to.x * to.x + to.y * to.y > 64 * 64) {
              float py = 0, pp = 0;
              aim_angles(me_ent.origin, path[k], py, pp);
              look_yaw = py;
            }
            break;
          }
        }
      }
      double band = kCalmBand;
      if (b.listen) {
        // THE SOUND'S SWIPE (bot.hpp): an enemy heard where none was known, looked at.
        const Vec3f o_at = sv.edicts[static_cast<size_t>(me)].origin;
        const Vec3f eye_at{o_at.x, o_at.y, o_at.z + b.senses.set.view_height};
        if (sv.time >= b.sound_back_until) {
          for (const Contact& k : b.senses.contacts) {
            if (!ai::known(k.heard) || k.heard.at <= b.sound_heard_at ||
                ai::age(k.heard, sv.time) > kSoundFresh_s)
              continue;
            b.sound_heard_at = k.heard.at;
            const Vec3f heard{static_cast<float>(k.heard.x), static_cast<float>(k.heard.y),
                              static_cast<float>(k.heard.z)};
            if (ai::known(k.seen) && ai::age(k.seen, sv.time) < kSoundKnown_s) {
              const Vec3f seen{static_cast<float>(k.seen.x), static_cast<float>(k.seen.y),
                               static_cast<float>(k.seen.z)};
              const Vec3f d = heard - seen;
              if (dot(d, d) < kSoundSame * kSoundSame) continue;  // known there
            }
            float sy = 0, sp = 0;
            aim_angles(eye_at, heard, sy, sp);
            if (std::fabs(wrap(sy - b.yaw)) <= kSoundAhead) continue;  // in view already
            b.sound_yaw = sy;
            b.sound_landed = false;
            b.sound_until = sv.time + 1.0;
            b.sound_back_until = b.sound_until + 0.6;
            b.stats.sound_swipes++;
            if (why) {
              why->decide("view");
              why->fact("sound_yaw", sy);
              why->choose("swipe", "an enemy heard where none was known");
            }
            break;
          }
        }
        if (sv.time < b.sound_until) {
          look_yaw = b.sound_yaw;
          band = kGlanceBand;
          if (!b.sound_landed && std::fabs(wrap(look_yaw - b.yaw)) <= 2 * kGlanceBand) {
            b.sound_landed = true;
            b.sound_until = sv.time + 2.0 * b.aim.react_med;
          }
          b.sound_back_until = b.sound_until + 0.6;
          b.glance_until = -1;  // no glance over it
        }
      }
      if (b.glance && sv.time >= b.sound_until) {
        // THE GLANCE (bot.hpp): to the side with the longest open line of sight, now and then.
        if (sv.time >= b.glance_next && b.glance_until < sv.time) {
          const Vec3f o_at = sv.edicts[static_cast<size_t>(me)].origin;
          const Vec3f eye_at{o_at.x, o_at.y, o_at.z + b.senses.set.view_height};
          float best = kGlanceOpen, pick = 0;
          for (float o : {-kGlanceBack, -kGlanceSide, kGlanceSide, kGlanceBack}) {
            const double a = (look_yaw + o) * (core::kPi / 180.0);
            const Vec3f to{eye_at.x + static_cast<float>(core::libm::cos(a)) * kGlanceReach,
                           eye_at.y + static_cast<float>(core::libm::sin(a)) * kGlanceReach,
                           eye_at.z};
            const HullTrace tr = trace_move(sv, eye_at, {}, {}, to, kTraceNoMonsters, me);
            if (tr.fraction * kGlanceReach > best) {
              best = tr.fraction * kGlanceReach;
              pick = o;
            }
          }
          if (pick != 0) {
            // A swipe there at a turn meant's pace, held `kGlanceHold_s` from when it lands (the
            // turn's time at most a second), and a swipe back.
            b.glance_off = pick;
            b.glance_landed = false;
            b.glance_until = sv.time + 1.0;
          }
          b.glance_next = sv.time + kGlanceMin_s + kGlanceSpread_s * core::next(b.glance_rng);
        }
        if (sv.time < b.glance_until) {
          look_yaw += b.glance_off;
          band = kGlanceBand;
          if (!b.glance_landed && std::fabs(wrap(look_yaw - b.yaw)) <= 2 * kGlanceBand) {
            b.glance_landed = true;
            b.glance_until = sv.time + kGlanceHold_s;
          }
          b.glance_back_until = b.glance_until + 0.6;
        }
      }
      b.walk_yaw = pilot_yaw;
      b.look_yaw = look_yaw;
      const double off = wrap(look_yaw - b.yaw);
      // A swipe there and back at a turn meant's pace (the glance, the sound's).
      const bool swipe =
          (b.glance && sv.time < b.glance_back_until) || (b.listen && sv.time < b.sound_back_until);
      b.view_by = !swipe ? "calm" : (b.listen && sv.time < b.sound_back_until) ? "sound" : "glance";
      if (b.mouse && b.aim.mouse) {
        // The hand's strokes onto the look, at ease (an about-face at a turn meant's pace); the
        // pitch let lie within kCalmPitchTol of level.
        const double want_view[2] = {b.yaw + off, 0};
        double out[2];
        const double pace = (off > 120 || off < -120 || swipe) ? 1.0 : kCalmPace;
        hand_step(b, want_view, dt, band, kCalmPitchTol, band, 0, pace, false, out);
        b.yaw = static_cast<float>(out[0]);
        b.pitch = static_cast<float>(out[1]);
      } else {
        const double rate = (off > 120 || off < -120) ? kCalmAbout : kCalmTurn;
        double step = 0;
        if (off > band || off < -band) {
          const double want_step = off > 0 ? off - band * 0.5 : off + band * 0.5;
          step = std::fmax(-rate * dt, std::fmin(rate * dt, want_step));
        }
        const double drift = kCalmDrift * core::libm::sin(b.calm_t * 0.7) * dt;
        b.yaw = static_cast<float>(b.yaw + step + drift);
        b.pitch = static_cast<float>(b.pitch * (1 - dt) + 2 * core::libm::sin(b.calm_t * 0.4) * dt);
      }
      if (swim3d)
        turn_axes_swim(cmd.forward, cmd.side, cmd.up, pilot_view, {b.pitch, b.yaw, 0});
      else
        turn_axes(cmd.forward, cmd.side, pilot_yaw, b.yaw);
      cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
    } else if (b.mouse && b.aim.mouse && b.pilot.pilot.mode != PilotMode::grenade &&
               (swim3d || (pilot_view.x < 1 && pilot_view.x > -1))) {
      // A jump, a hop, a strafe or a swim: the pilot's heading followed by the hand (its wish
      // turned under the view, in three dimensions under water, so the air control and the swim
      // are the same); a blast jump's pitched view stays the pilot's. Under water it looks along
      // the swim within kSwimBand, at ease.
      double want_view[2] = {b.yaw + wrap(pilot_yaw - b.yaw), 0};
      double out[2];
      b.view_by = swim3d ? "swim" : "air";
      if (shot_due) {  // the stroke onto the jump's view, timed to land with its shot
        want_view[0] = b.yaw + shot_dx;
        want_view[1] = jp.shot_pitch;
        b.view_by = "shot";
      }
      if (swim3d) {
        hand_step(b, want_view, msec / 1000.0, kSwimBand, kCalmPitchTol, kSwimBand, 0, kCalmPace,
                  false, out);
      } else {
        hand_step(b, want_view, msec / 1000.0, 2, shot_due ? 2 : kCalmPitchTol, 4, 0, 1, !shot_due,
                  out);
      }
      b.yaw = static_cast<float>(out[0]);
      b.pitch = static_cast<float>(out[1]);
      if (swim3d)
        turn_axes_swim(cmd.forward, cmd.side, cmd.up, pilot_view, {b.pitch, b.yaw, 0});
      else
        turn_axes(cmd.forward, cmd.side, pilot_yaw, b.yaw);
      cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
    } else if (b.mouse && b.aim.mouse) {
      // The pilot's pitched view (a grenade jump's throw, a rocket jump's shot): the hand's
      // strokes there; the command that fires takes the view exactly (the stroke timed to land
      // with it, above, leaves only its error to put right).
      const double want_view[2] = {b.yaw + wrap(pilot_yaw - b.yaw), pilot_view.x};
      double out[2];
      hand_step(b, want_view, msec / 1000.0, 2, 2, 4, 0, 1, false, out);
      if (cmd.buttons & kButtonAttack) {
        out[0] = want_view[0];
        out[1] = want_view[1];
      }
      b.view_by = blast_shot ? "shot" : "pilot";
      b.yaw = static_cast<float>(out[0]);
      b.pitch = static_cast<float>(out[1]);
      turn_axes(cmd.forward, cmd.side, pilot_yaw, b.yaw);
      cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
    } else {
      b.view_by = "pilot";
      b.yaw = pilot_yaw;
      b.pitch = blast_shot ? pilot_view.x : 0;
    }
    b.wanted[0] = b.pressed[0] = b.yaw;
    b.wanted[1] = b.pressed[1] = b.pitch;
  }
  // The view near enough the coming shot's for it to go (read by the pilot's next command).
  {
    const double dx = wrap(jp.shot_yaw - b.yaw), dy = jp.shot_pitch - b.pitch;
    jp.shot_ready =
        jp.shot_pitch == 0 || (std::fabs(dx) < kShotReadyYaw && std::fabs(dy) < kShotReadyPitch);
  }
  return cmd;
}

}  // namespace

void bot_init(Bot& b, const NavGraph& g, const PilotSettings& set, PilotHands hands, AimSkill skill,
              double hz, uint32_t seed) {
  bot_init(b, g, set, hands, aim_profile(skill), hz, seed);
}

void bot_init(Bot& b, const NavGraph& g, const PilotSettings& set, PilotHands hands,
              const AimProfile& aim, double hz, uint32_t seed) {
  const std::vector<ai::AxisHands> h = pilot_hands(hands);
  PilotSettings ps = set;
  ps.lead_s = pilot_lead_s(hands);
  pilot_init(b.pilot.pilot, g, ps, h, hz, seed);
  b.aim = aim;
  b.aim_hands = ai::Hands(b.aim.hands, hz, seed ^ 0x5bd1e995u);
  {
    // One player in ten is left-handed: his drag right runs up.
    ai::MouseHand hand = aim.hand;
    core::Rng hr = core::make_rng(seed ^ 0x3c6ef372u);
    if (core::next(hr) < 0.1) hand.tilt = -hand.tilt;
    b.hand = ai::Mouse(hand, seed ^ 0x9b05688cu);
    b.finger = ai::Finger(aim.finger, seed ^ 0x1f83d9abu);
  }
  b.react_rng = core::make_rng(seed ^ 0x2c1b3c6du);
  b.glance_rng = core::make_rng(seed ^ 0x6a09e667u);
  b.strafe = ps.strafe;
  b.hop = ps.hop;
}

CmdSource bot_source(Bot& b) { return {&b, bot_next}; }

float bot_hand_override(Bot& b, MoveCmd& cmd, int msec, bool wet) {
  const char* by = b.set_by;
  b.set_by = nullptr;
  if (!(b.mouse && b.aim.mouse)) return 0;
  const float dy = wrap(cmd.angles.y - b.yaw), dp = cmd.angles.x - b.pitch;
  if (dy < 0.05f && dy > -0.05f && dp < 0.05f && dp > -0.05f) return 0;  // the hand's own
  const Vec3f set_view = cmd.angles;
  const double want[2] = {b.yaw + dy, set_view.x};
  double out[2];
  hand_step(b, want, msec / 1000.0, kGameTol, kGameTol, 4, 0, 1, false, out);
  b.yaw = static_cast<float>(out[0]);
  b.pitch = static_cast<float>(out[1]);
  b.view_by = by ? by : "game";
  if (wet)
    turn_axes_swim(cmd.forward, cmd.side, cmd.up, set_view, {b.pitch, b.yaw, 0});
  else
    turn_axes(cmd.forward, cmd.side, set_view.y, b.yaw);
  cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
  const float ry = wrap(set_view.y - b.yaw), rp = set_view.x - b.pitch;
  return std::sqrt(ry * ry + rp * rp);
}

void bot_goal(Bot& b, int32_t goal) {
  b.pilot.goals.assign(1, goal);
  b.pilot.next_goal = 0;
  b.pilot.pilot.mode = PilotMode::idle;
}

void bot_aim(Bot& b, const AimProfile& aim) {
  b.aim = aim;
  for (size_t k = 0; k < aim.hands.size(); k++) b.aim_hands.retune(k, aim.hands[k]);
}

float bot_aim_off(const Bot& b) {
  if (!b.aiming) return 0;
  const float dy = wrap(static_cast<float>(b.wanted[0] - b.pressed[0]));
  const float dp = static_cast<float>(b.wanted[1] - b.pressed[1]);
  return std::sqrt(dy * dy + dp * dp);
}

bool bot_reacting(const Bot& b, double t) { return b.aiming && t < b.react_until; }

}  // namespace ge::shooter
