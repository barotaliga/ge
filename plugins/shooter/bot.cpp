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

MoveCmd bot_next(void* ctx, const Server& sv, int slot, int msec) {
  Bot& b = *static_cast<Bot*>(ctx);
  const int me = slot + 1;
  const Edict& self = sv.edicts[static_cast<size_t>(me)];
  ai::Why* why = b.pilot.why ? b.pilot.why->follow(slot, b.pilot.command) : nullptr;

  client_view(sv, me, b.view);
  sense(b.senses, sv, b.view, me, {b.pitch, b.yaw, 0}, sv.time, why);

  // The pilot's command, with its own trace (pilot_next advances the command count).
  MoveCmd cmd = pilot_source(b.pilot).next(&b.pilot, sv, slot, msec);
  const float pilot_yaw = b.pilot.pilot.yaw;
  // Swimming (`pools`), the pilot's wish is pressed again under another view in three dimensions:
  // its pitch dives and rises.
  const Vec3f pilot_view = cmd.angles;
  const bool swim3d = b.pools && self.waterlevel >= 2;
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
    // Lost from sight, his course is followed a moment only (`corner`): no wall is seen through.
    const double age = ai::age(c->seen, sv.time);
    const double head =
        (b.corner && !c->visible ? std::fmin(age, kLostCarry_s) : age) + b.aim.lead_s;
    // The network's part, in the air only: a player's arc does not change in a round trip, but
    // on the ground a strafer's steps do, and a lead of the round trip on them (measured on top
    // of the hands' own) swings the aim wider than it gains (se-team at 200 ms: none on the
    // ground keeps a strong aim at its LAN rate).
    const double net = b.shot_speed > 0 || !b.rewound ? b.lag_s : 0;
    Mover m;
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
    const Vec3f look_from = b.selfaim ? eye + self.velocity * b.aim.lead_s : eye;
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
      b.freeze_until = sv.time + r - b.aim.hands[0].delay_s;
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

  if (b.aiming || b.looking) {
    static constexpr const char* kNames[2] = {"aim_yaw", "aim_pitch"};
    ai::record_intent(why, kNames, want);
    double out[2];
    b.aim_hands.step(want, out);
    if (!b.aiming) b.stats.looking++;
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
    if (b.calm && walking_mode(b.pilot.pilot.mode)) {
      // The calm view (bot.hpp): still in the band, turned at a hand's pace past it, drifting.
      const double dt = msec / 1000.0;
      b.calm_t += dt;
      // Where it looks: the route's point about `kCalmAhead` units on, so the view turns before a
      // corner to see round it and holds still on a straight; else the way it walks.
      float look_yaw = pilot_yaw;
      {
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
      if (b.glance) {
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
            // Held a moment past the turn there (at the calm view's pace).
            b.glance_off = pick;
            b.glance_until = sv.time + kGlanceHold_s + (pick > 90 || pick < -90 ? 0.6 : 0.42);
          }
          b.glance_next = sv.time + kGlanceMin_s + kGlanceSpread_s * core::next(b.glance_rng);
        }
        if (sv.time < b.glance_until) {
          look_yaw += b.glance_off;
          band = kGlanceBand;
        }
      }
      const double off = wrap(look_yaw - b.yaw);
      const double rate = (off > 120 || off < -120) ? kCalmAbout : kCalmTurn;
      double step = 0;
      if (off > band || off < -band) {
        const double want_step = off > 0 ? off - band * 0.5 : off + band * 0.5;
        step = std::fmax(-rate * dt, std::fmin(rate * dt, want_step));
      }
      const double drift = kCalmDrift * core::libm::sin(b.calm_t * 0.7) * dt;
      b.yaw = static_cast<float>(b.yaw + step + drift);
      b.pitch = static_cast<float>(b.pitch * (1 - dt) + 2 * core::libm::sin(b.calm_t * 0.4) * dt);
      if (swim3d)
        turn_axes_swim(cmd.forward, cmd.side, cmd.up, pilot_view, {b.pitch, b.yaw, 0});
      else
        turn_axes(cmd.forward, cmd.side, pilot_yaw, b.yaw);
      cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
    } else {
      b.yaw = pilot_yaw;
      b.pitch = 0;
    }
    b.wanted[0] = b.pressed[0] = b.yaw;
    b.wanted[1] = b.pressed[1] = b.pitch;
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
  b.react_rng = core::make_rng(seed ^ 0x2c1b3c6du);
  b.glance_rng = core::make_rng(seed ^ 0x6a09e667u);
  b.strafe = ps.strafe;
  b.hop = ps.hop;
}

CmdSource bot_source(Bot& b) { return {&b, bot_next}; }

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
