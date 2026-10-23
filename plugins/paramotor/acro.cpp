// The paramotor's acro (acro.hpp).
//
// ge:summary The paramotor's acro: the kept way, the risers, the rears' hold, the swing, the tangle
// ge:keywords paramotor acro kept_way riser_rig riser_lift bite_rears hold_pitch count_swing
// ge:keywords fallen_in read_load step_tangled loop tangle
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#include "paramotor/acro.hpp"

#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "paramotor/air.hpp"

namespace ge::paramotor {

namespace {

namespace lm = core::libm;
namespace num = core::num;
namespace A = spec::acro;
using core::Vec3;

// How far the rears bite at the speed `v` m/s: none at `kFrom`, all of it at `kFull`.
double bite_at(double v) {
  return core::clamp((v - A::swing::kFrom) / (A::swing::kFull - A::swing::kFrom), 0.0, 1.0);
}

}  // namespace

bool kept_way(Vec3 a, double down, Vec3 u, Vec3 last, Vec3& out) {
  // The last way, turned square to the lines as they stand now.
  const double along = last.x * u.x + last.y * u.y + last.z * u.z;
  Vec3 pw{last.x - along * u.x, last.y - along * u.y, last.z - along * u.z};
  const double pl = core::hypot3(pw.x, pw.y, pw.z);
  if (pl > 1e-6) pw = Vec3{pw.x / pl, pw.y / pl, pw.z / pl};
  Vec3 fw{a.x - down * u.x, a.y - down * u.y, a.z - down * u.z};
  const double fl = core::hypot3(fw.x, fw.y, fw.z);
  if (fl > 0.5) {
    fw = Vec3{fw.x / fl, fw.y / fl, fw.z / fl};
    const double meets = fw.x * pw.x + fw.y * pw.y + fw.z * pw.z;
    if (pl <= 1e-6 || meets >= 0) {
      out = fw;
      return false;
    }
    out = pw;
    return meets < A::kBackflown && fl > A::kBackSpeed;
  }
  out = pw;
  return false;
}

double riser_rig(const ParaState& p, double bar) {
  return p.flying && bar > 0 ? A::dive::kRig * bar : 0;
}

double riser_lift(const ParaState& p, double bar) {
  return p.flying && bar < 0 ? -bar * A::swing::kLift * p.rears : 0;
}

void bite_rears(ParaState& p, double bar, double v) {
  if (!p.flying || bar > -0.3) {
    p.rears = 0;
    return;
  }
  p.rears = num::max(p.rears, bite_at(v));
}

double reflex_of(const ParaState& p, double bar) {
  return bar > 0 ? A::dive::kReflex * bar : -bar * A::swing::kReflex * p.rears;
}

double held_of(const ParaState& p, double bar) {
  return bar < 0 ? -bar * A::swing::kHeld * p.rears : 0;
}

double held_share(const ParaState& p, double bar, double v) {
  if (!p.flying || bar >= 0 || p.rears <= 0) return 0;
  return -bar * bite_at(v);
}

Vec3 hold_pitch(ParaState& p, Vec3 at, Vec3& vel, double live, Vec3 c, Vec3 u, double m,
                double dt) {
  const double vc = core::hypot3(c.x, c.y, c.z);
  if (live <= 0 || vc < 1) return Vec3{0, 0, 0};
  const Vec3 v{c.x / vc, c.y / vc, c.z / vc};
  // Square to his way, on the side the wing stands.
  const double along = u.x * v.x + u.y * v.y + u.z * v.z;
  Vec3 n{u.x - along * v.x, u.y - along * v.y, u.z - along * v.z};
  const double nl = core::hypot3(n.x, n.y, n.z);
  if (nl < 1e-6) return Vec3{0, 0, 0};
  n = Vec3{n.x / nl, n.y / nl, n.z / nl};
  const double cb = lm::cos(A::swing::kBack);
  const double sb = lm::sin(A::swing::kBack);
  const double lines = spec::wing::kLines;
  const Vec3 t{at.x + (n.x * cb - v.x * sb) * lines, at.y + (n.y * cb - v.y * sb) * lines,
               at.z + (n.z * cb - v.z * sb) * lines};
  // The spot's own speed, off where it stood last step (his own, the first).
  const bool fresh = std::fabs(p.hold_at - (p.t - dt)) > dt / 2;
  const Vec3 sv =
      fresh ? vel : Vec3{(t.x - p.hold.x) / dt, (t.y - p.hold.y) / dt, (t.z - p.hold.z) / dt};
  p.hold = t;
  p.hold_at = p.t;
  const double k = A::swing::kSpring * live;
  const double d = A::swing::kDamp * live;
  const Vec3 f{k * (t.x - p.x) - d * (p.vx - sv.x), k * (t.y - p.y) - d * (p.vy - sv.y),
               k * (t.z - p.z) - d * (p.vz - sv.z)};
  vel.x -= f.x / m * dt;
  vel.y -= f.y / m * dt;
  vel.z -= f.z / m * dt;
  return f;
}

bool count_swing(ParaState& p, Vec3 u) {
  const Vec3 w = p.way;
  double rx = u.y * w.z - u.z * w.y;
  double rz = u.x * w.y - u.y * w.x;
  const double rl = core::hypot(rx, rz);
  if (rl < 1e-6) return false;
  rx /= rl;
  rz /= rl;
  // Ahead, level: right x up.
  const double at = lm::atan2(u.x * -rz + u.z * rx, u.y);
  double d = at - lm::atan2(lm::sin(p.swing), lm::cos(p.swing));
  d = lm::atan2(lm::sin(d), lm::cos(d));
  p.swing += d;
  // Whole turns: the unwrapped swing's size over a turn, truncated (it is not negative).
  const auto turns = static_cast<int32_t>(std::fabs(p.swing) / core::kTau);
  if (turns > p.loops) {
    p.loops = turns;
    return true;
  }
  return false;
}

bool fallen_in(ParaState& p, double tension, double m, Vec3 u, double apart, double dt) {
  const bool slack = p.rears > 0 && tension < A::kSlack * m * spec::kGravity;
  if (slack && u.y < A::kOver) {
    p.slack += dt;
  } else {
    p.slack = 0;
  }
  return p.slack >= A::kSlackFor || (slack && apart < A::kClose * spec::wing::kLines);
}

void read_load(ParaState& p, double tension, double m, double dt) {
  const double g = tension / (m * spec::kGravity);
  p.g += (g - p.g) * (1 - lm::exp(-dt / A::kGLag));
}

void step_tangled(ParaState& p, skier::SkierState& s, const ParaWorld& w, double m, double dt) {
  const ParaAir air = para_air_at(*w.level, *w.field, w.wind, w.now, s.x, s.y, s.z);
  const Vec3 a{s.vx - air.x, s.vy - air.y, s.vz - air.z};
  const double v = core::hypot3(a.x, a.y, a.z);
  const double k = 0.5 * spec::kAirDensity * A::tangle::kDrag * v / m;
  s.vx -= a.x * k * dt;
  s.vy -= a.y * k * dt;
  s.vz -= a.z * k * dt;
  s.speed = core::hypot3(s.vx, s.vy, s.vz);
  // Turned round by the cloth about him while he is off the snow.
  if (!s.on_snow) {
    s.heading = core::angle_diff(0, s.heading + A::tangle::kSpin * dt);
    s.yaw_rate = A::tangle::kSpin;
  }
  // The cloth trailing up the air from him, a bundle on short lines.
  const double trail = v > 0.5 ? 2.5 / v : 0;
  p.x = s.x - a.x * trail;
  p.y = s.y + spec::kHarness - a.y * trail + (v > 0.5 ? 0 : 1.5);
  p.z = s.z - a.z * trail;
  p.vx = s.vx, p.vy = s.vy, p.vz = s.vz;
  p.tension = 0;
  p.g += (0 - p.g) * (1 - lm::exp(-dt / A::kGLag));
  p.airspeed = v;
  p.fold = 1;
  p.fold_side = 0;
  p.stalled = true;
}

}  // namespace ge::paramotor
