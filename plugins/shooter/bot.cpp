// ge:summary The bot: senses, the pilot's command, the view on a contact, the axes under the view
// ge:keywords bot bot_init bot_source bot_next aim hold release look watch pre-aim
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooter/bot.hpp"

#include <array>
#include <cmath>

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
  b.stats.commands++;

  const Vec3f eye{self.origin.x, self.origin.y, self.origin.z + b.senses.set.view_height};
  const Contact* c = freshest(b.senses, eye);
  const bool fresh =
      c && ai::known(c->seen) && (c->visible || ai::age(c->seen, sv.time) <= b.aim.hold_s);
  double want[2];
  if (fresh) {
    double ahead = ai::age(c->seen, sv.time) + b.aim.lead_s;
    if (b.shot_speed > 0 || !b.rewound) ahead += b.lag_s;
    if (b.shot_speed > 0) {
      const double dx = c->seen.x - eye.x, dy = c->seen.y - eye.y, dz = c->seen.z - eye.z;
      ahead += std::sqrt(dx * dx + dy * dy + dz * dz) / b.shot_speed;
    }
    const Vec3f at{static_cast<float>(c->seen.x + c->velocity.x * ahead),
                   static_cast<float>(c->seen.y + c->velocity.y * ahead),
                   static_cast<float>(c->seen.z + c->velocity.z * ahead + b.aim_dz)};
    float yaw = 0, pitch = 0;
    aim_angles(eye, at, yaw, pitch);
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
    const float off = wrap(b.yaw - pilot_yaw);
    const bool on_way =
        off < kRelease && off > -kRelease && b.pitch < kRelease && b.pitch > -kRelease;
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
    turn_axes(cmd.forward, cmd.side, pilot_yaw, b.yaw);
    cmd.angles = {quantize_angle(b.pitch), quantize_angle(wrap(b.yaw)), 0};
  } else {
    b.yaw = pilot_yaw;
    b.pitch = 0;
    b.wanted[0] = b.pressed[0] = b.yaw;
    b.wanted[1] = b.pressed[1] = 0;
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
