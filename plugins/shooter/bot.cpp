// ge:summary The bot: senses, the pilot's command, the view on a contact, the axes under the view
// ge:keywords bot bot_init bot_source bot_next aim hold release
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooter/bot.hpp"

#include <array>

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
    const double ahead = ai::age(c->seen, sv.time) + b.aim.lead_s;
    const Vec3f at{static_cast<float>(c->seen.x + c->velocity.x * ahead),
                   static_cast<float>(c->seen.y + c->velocity.y * ahead),
                   static_cast<float>(c->seen.z + c->velocity.z * ahead)};
    float yaw = 0, pitch = 0;
    aim_angles(eye, at, yaw, pitch);
    if (!b.aiming) {
      // Acquired: the hands start from where the view is, so the turn waits their reaction.
      const std::array<double, 2> from{b.yaw, b.pitch};
      b.aim_hands.reset(from);
      b.strafe = b.pilot.pilot.set.strafe;
      b.pilot.pilot.set.strafe = false;
    }
    b.aiming = true;
    b.target = c->ent;
    want[0] = unwrap(yaw, b.yaw);
    want[1] = pitch;
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
    if (b.aiming) {
      const float off = wrap(b.yaw - pilot_yaw);
      if (off < kRelease && off > -kRelease && b.pitch < kRelease && b.pitch > -kRelease) {
        b.aiming = false;
        b.target = -1;
        b.pilot.pilot.set.strafe = b.strafe;
        if (why) {
          why->decide("aim");
          why->choose("release", "no contact: the view back on the pilot's way");
        }
      }
    }
  }

  if (b.aiming) {
    static constexpr const char* kNames[2] = {"aim_yaw", "aim_pitch"};
    ai::record_intent(why, kNames, want);
    double out[2];
    b.aim_hands.step(want, out);
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
  const std::vector<ai::AxisHands> h = pilot_hands(hands);
  PilotSettings ps = set;
  ps.lead_s = pilot_lead_s(hands);
  pilot_init(b.pilot.pilot, g, ps, h, hz, seed);
  b.aim = aim_profile(skill);
  b.aim_hands = ai::Hands(b.aim.hands, hz, seed ^ 0x5bd1e995u);
  b.strafe = ps.strafe;
}

CmdSource bot_source(Bot& b) { return {&b, bot_next}; }

}  // namespace ge::shooter
