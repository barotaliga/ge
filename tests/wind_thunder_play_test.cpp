// engine/wind/thunder_play.hpp: a strike's thunder, played through the synth, starts within a
// block of its first sound's arrival at 1, 4 and 12 km and is quieter farther off; it is panned
// toward the channel; a flash is queued once however often the storm is listened to; a clap
// whose first sound went by while nobody played is let go; and a recording is freed once its
// voice has ended.
//
// ge:summary Test: thunder heard: claps start on time, pan, queue once, drop late, free at the end
// ge:keywords test thunder-play hear_storm play_thunder arrival onset pan synth
// ge:skill water testing

#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>

#include "audio/synth.hpp"
#include "check.hpp"
#include "wind/lightning.hpp"
#include "wind/thunder.hpp"
#include "wind/thunder_play.hpp"

namespace gw = ge::wind;
namespace ga = ge::audio;

namespace {

constexpr int kRate = 48000;
constexpr int kBlock = 480;  // 10 ms

gw::Storm severe() {
  gw::StormSpec spec;
  spec.seed = 38;
  spec.severity = 0.8;
  return gw::lay_storm(spec);
}

// The first ground flash of `storm` after `from`.
gw::Flash first_ground(const gw::Storm& storm, double from) {
  gw::Flash fl[128];
  for (double t = from; t < from + 3600; t += 60) {
    const int n = gw::flashes_between(storm, t, t + 60, fl, 128);
    for (int i = 0; i < n && i < 128; ++i)
      if (fl[i].kind == gw::FlashKind::ground && fl[i].t0 >= t) return fl[i];
  }
  return gw::Flash{};
}

// The arrival the thunder model gives for `f` at `ear`.
double arrival(const gw::Flash& f, const gw::Ear& ear) {
  auto ch = std::make_unique<gw::Channel>();
  auto th = std::make_unique<gw::Thunder>();
  gw::build_channel(f, *ch);
  gw::hear_thunder(f, *ch, ear.x, ear.y, ear.z, gw::sound_speed(ear.celsius), *th);
  return f.t0 + th->first;
}

struct Heard {
  double onset = -1;  // s, the first frame over the threshold
  double peak = 0;
  double left = 0, right = 0;  // energy a side
};

// Listen to `storm` at `ear` from just before `f` for `seconds`, through a synth, offline; only
// the flash `f` is in the window the player is first asked about.
Heard listen(const gw::Storm& storm, const gw::Flash& f, const gw::Ear& ear, double seconds) {
  gw::ThunderPlayer player;
  player.started = true;
  player.last = f.t0 - 1e-6;
  ga::Synth synth;
  std::vector<float> out(2 * kBlock);
  Heard h;
  const double t0 = f.t0;
  for (int b = 0; b * kBlock < seconds * kRate; ++b) {
    const double t = t0 + static_cast<double>(b) * kBlock / kRate;
    // Only `f` is queued: the player is asked once, just after it started.
    if (b == 0) gw::hear_storm(player, storm, f.t0 + 1e-3, ear);
    gw::play_thunder(player, synth, t, static_cast<double>(kBlock) / kRate);
    synth.render(out.data(), kBlock);
    for (int i = 0; i < kBlock; ++i) {
      const double l = out[2 * static_cast<size_t>(i)], r = out[2 * static_cast<size_t>(i) + 1];
      const double a = std::max(std::fabs(l), std::fabs(r));
      if (h.onset < 0 && a > 1e-4) h.onset = t + static_cast<double>(i) / kRate;
      h.peak = std::max(h.peak, a);
      h.left += l * l;
      h.right += r * r;
    }
  }
  return h;
}

}  // namespace

TEST(a_strike_is_heard_on_time_and_quieter_farther_off) {
  const gw::Storm storm = severe();
  const gw::Flash f = first_ground(storm, 600);
  CHECK(f.kind == gw::FlashKind::ground);
  double last_peak = 1e9;
  for (const double km : {1.0, 4.0, 12.0}) {
    gw::Ear ear;
    ear.x = f.ex;
    ear.y = f.ey;
    ear.z = f.ez + km * 1000;
    const double want = arrival(f, ear);
    const Heard h = listen(storm, f, ear, want - f.t0 + 2.0);
    CHECK_MSG(h.onset >= 0, "no thunder heard at " + std::to_string(km) + " km");
    CHECK_MSG(std::fabs(h.onset - want) < 0.02,
              "the thunder at " + std::to_string(km) + " km began at " +
                  std::to_string(h.onset - f.t0) + " s, its first sound is due at " +
                  std::to_string(want - f.t0) + " s");
    CHECK(h.peak < last_peak);
    last_peak = h.peak;
  }
}

TEST(a_strike_on_the_right_is_heard_on_the_right) {
  const gw::Storm storm = severe();
  const gw::Flash f = first_ground(storm, 600);
  gw::Ear ear;
  ear.x = f.ex - 2000;  // facing +z, the strike 2 km to the east: on the right
  ear.y = f.ey;
  ear.z = f.ez;
  const Heard h = listen(storm, f, ear, arrival(f, ear) - f.t0 + 2.0);
  CHECK_MSG(h.right > 2 * h.left, "the right's energy " + std::to_string(h.right) +
                                      " is not well over the left's " + std::to_string(h.left));
}

TEST(a_flash_is_queued_once_and_a_late_one_is_let_go) {
  const gw::Storm storm = severe();
  const gw::Flash f = first_ground(storm, 600);
  gw::Ear ear;
  ear.x = f.ex;
  ear.z = f.ez + 3000;
  gw::ThunderPlayer player;
  player.started = true;
  player.last = f.t0 - 1e-6;
  gw::hear_storm(player, storm, f.t0 + 1e-3, ear);
  const size_t queued = player.waiting.size();
  CHECK(queued >= 1);
  // Asked again over the same moment, and again later: nothing new for `f`.
  player.last = f.t0 - 1e-6;
  gw::hear_storm(player, storm, f.t0 + 1e-3, ear);
  CHECK(player.waiting.size() == queued);
  // Played only after its first sound went by: let go, never started.
  ga::Synth synth;
  gw::play_thunder(player, synth, arrival(f, ear) + 1.0, 0.01);
  CHECK(player.sounding.empty());
}

TEST(a_clap_is_freed_when_its_voice_ends) {
  const gw::Storm storm = severe();
  const gw::Flash f = first_ground(storm, 600);
  gw::Ear ear;
  ear.x = f.ex;
  ear.z = f.ez + 1000;
  gw::ThunderPlayer player;
  player.started = true;
  player.last = f.t0 - 1e-6;
  gw::hear_storm(player, storm, f.t0 + 1e-3, ear);
  CHECK(!player.waiting.empty());
  const double at = player.waiting.front().at;
  const double seconds = player.waiting.front().pcm->seconds();
  ga::Synth synth;
  std::vector<float> out(2 * kBlock);
  bool sounded = false;
  for (double t = at - 0.05; t < at + seconds + 1.0; t += static_cast<double>(kBlock) / kRate) {
    gw::play_thunder(player, synth, t, static_cast<double>(kBlock) / kRate);
    sounded = sounded || !player.sounding.empty();
    synth.render(out.data(), kBlock);
  }
  gw::play_thunder(player, synth, at + seconds + 1.0, 0.01);
  CHECK(sounded);
  CHECK(player.sounding.empty());
}
