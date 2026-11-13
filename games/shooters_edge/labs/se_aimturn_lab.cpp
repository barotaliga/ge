// LAB `se-aimturn`: HOW THE AIM HELP TURNS THE VIEW (plugins/shooter/aim_turn.hpp). A player 500
// units ahead strafes left and right; the helped player's own aim (his mouse) follows him late and
// a few degrees off, and he fires ten shots a second from `--from` to `--to` seconds. The server
// bends each shot by the view he sends (`aim_help` at the default leeway, 24 across and 48 up and
// down) and tells him when it did. The chart draws the yaw to the strafer, the player's own aim
// and the view he sends (his aim plus the turn): the view should close on the strafer within a few
// frames of the first bend, ride him while the attack is held (shots then meet him unbent), and
// come back to his own aim after it is let go. The metrics give the shots that hit, were bent or
// missed, the miss while held and the time back.
//
// ge:summary Lab: the aim help's view turn onto a strafer while firing and back after, charted
// ge:keywords lab se-aimturn aim-turn aim-help view inertia spring demo natural
// ge:skill shooter labs
// ge:test tests/shooter_aim_turn_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "arena_chart.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/aim.hpp"
#include "shooter/aim_help.hpp"
#include "shooter/aim_turn.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr float kDt = 1.0f / 60;
constexpr double kSeconds = 2.4;

// The strafer's place at `t`: 500 ahead, swaying 80 units left and right every 1.6 s.
sh::Vec3f strafer(double t) {
  return {500, static_cast<float>(80 * ge::core::libm::sin(t * ge::core::kTau / 1.6)), 0};
}

void run(gl::Lab& lab) {
  const auto from = lab.arg_num("from"), to = lab.arg_num("to");
  const auto lag = lab.arg_num("lag"), off = lab.arg_num("off");
  const sh::Vec3f eye{0, 0, 0};
  sh::AimTurn turn;
  sh::Vec3f view{0, 0, 0};
  float own = 0;  // the player's own aim's yaw (his mouse)
  gl::Trace target_tr{"to the strafer", {}, {}}, own_tr{"his own aim", {}, {}},
      view_tr{"the view sent", {}, {}};
  double next_shot = from, miss_sum = 0, back_at = -1;
  int held = 0, hit = 0, bent = 0, missed = 0;
  float most = 0;
  for (int i = 0; static_cast<double>(i) * kDt <= kSeconds; i++) {
    const double t = static_cast<double>(i) * kDt;
    const sh::Vec3f at = strafer(t);
    float yaw = 0, pitch = 0;
    sh::aim_angles(eye, at, yaw, pitch);
    // His mouse: where the strafer was `lag` seconds ago, `off` degrees to the side.
    float late = 0;
    sh::aim_angles(eye, strafer(t - lag), late, pitch);
    const float mouse = late + static_cast<float>(off);
    view.y += mouse - own;  // the mouse turns the view as always
    own = mouse;
    const bool attack = t >= from && t < to;
    if (attack && t >= next_shot) {  // a shot: the server bends it by the view sent
      next_shot += 0.1;
      const sh::AimTarget box{2, at + sh::Vec3f{-16, -16, -24}, at + sh::Vec3f{16, 16, 32}};
      const sh::Vec3f forward = sh::angle_vectors(view).forward;
      const sh::AimBend b = sh::aim_help(eye, forward, 2048, {&box, 1}, {24, 48});
      if (b.bent) {
        bent++;
        sh::aim_turn_bent(turn, 2);  // told to him: his view turns onto the strafer
      } else {
        // Unbent: it met his box or passed beyond the leeway.
        const float side = std::abs(forward.y * (at.x - eye.x) / forward.x - (at.y - eye.y));
        (side <= 16 ? hit : missed)++;
      }
    }
    sh::aim_turn_step(turn, view, eye, &at, attack, kDt);
    target_tr.x.push_back(t);
    target_tr.y.push_back(yaw);
    own_tr.x.push_back(t);
    own_tr.y.push_back(own);
    view_tr.x.push_back(t);
    view_tr.y.push_back(view.y);
    if (attack && t >= from + 0.2) {
      miss_sum += std::abs(view.y - yaw);
      held++;
    }
    most = std::max(most, std::abs(turn.yaw));
    if (!attack && t >= to && back_at < 0 && std::abs(view.y - own) < 0.1f) back_at = t - to;
  }
  const double miss = held > 0 ? miss_sum / held : 0;
  lab.metric("shots.hit_unbent", hit, "shots");
  lab.metric("shots.bent", bent, "shots");
  lab.metric("shots.missed", missed, "shots");
  lab.metric("turn.most_deg", most, "degrees");
  lab.metric("turn.held_miss_deg", miss, "degrees");
  lab.metric("turn.back_s", back_at, "s");
  gl::LineChart c;
  c.frame = {"THE AIM HELP TURNS THE VIEW",
             "a strafer 500 ahead; his own aim " + to_fixed(lag, 2) + " s late and " +
                 to_fixed(off, 1) + " degrees off; firing " + to_fixed(from, 1) + " to " +
                 to_fixed(to, 1) + " s",
             "t (s)", "yaw (degrees)"};
  c.spikes = false;
  c.traces = {std::move(target_tr), std::move(own_tr), std::move(view_tr)};
  c.bands.push_back({from, to, "firing"});
  la::shoot(lab, "turn", gl::line_chart(c),
            "The yaw to a strafing player, the helped player's own aim and the view he sends, "
            "while he fires (the band) and after.",
            "the view closing on the strafer soon after the band starts, riding him, and back on "
            "his own aim soon after it ends",
            {{"from", lab.arg("from")},
             {"to", lab.arg("to")},
             {"lag", lab.arg("lag")},
             {"off", lab.arg("off")}});
}

const gl::LabSpec kSpec{
    "se-aimturn",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/aim_turn (the view turned onto the aim help's bend)",
    "shooter",
    "Does the helped player's view close on the player his shots were bent onto, ride him while "
    "he fires, and come back to his own aim after?",
    {
        {"from", "0.4", "s: the attack pressed"},
        {"to", "1.6", "s: the attack let go"},
        {"lag", "0.1", "s his own aim follows the strafer late"},
        {"off", "2", "degrees his own aim is off to the side"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
