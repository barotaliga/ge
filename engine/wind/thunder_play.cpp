// Thunder heard: see thunder_play.hpp.
//
// ge:summary The thunder player: flashes scheduled at their arrival, rendered, started, freed
// ge:keywords thunder-play hear_storm play_thunder clap schedule
// ge:skill water audio
// ge:test tests/wind_thunder_play_test.cpp

#include "wind/thunder_play.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "wind/lightning.hpp"
#include "wind/thunder.hpp"

namespace ge::wind {

namespace lm = core::libm;

namespace {

inline constexpr int kLook = 256;  // flashes looked at a call

// The bearing from the ear to the channel's nearest point, as a pan: -1 hard left, 1 right.
float pan_to(const Channel& ch, const Ear& ear) {
  double best = -1, px = 0, pz = 0;
  for (int i = 0; i < ch.count; ++i) {
    const Segment& s = ch.seg[i];
    const double mx = 0.5 * (s.x0 + s.x1), my = 0.5 * (s.y0 + s.y1), mz = 0.5 * (s.z0 + s.z1);
    const double d = core::hypot(core::hypot(mx - ear.x, my - ear.y), mz - ear.z);
    if (best < 0 || d < best) {
      best = d;
      px = mx - ear.x;
      pz = mz - ear.z;
    }
  }
  if (best < 0 || core::hypot(px, pz) < 1) return 0;
  // Heading 0 is +z, clockwise from above: the bearing of (px, pz) is atan2(px, pz).
  return static_cast<float>(lm::sin(lm::atan2(px, pz) - ear.heading));
}

void let_go_quietest(std::vector<ThunderClap>& waiting) {
  auto q = std::min_element(waiting.begin(), waiting.end(),
                            [](const ThunderClap& a, const ThunderClap& b) {
                              return a.gain / (1 + a.distance) < b.gain / (1 + b.distance);
                            });
  waiting.erase(q);
}

}  // namespace

uint64_t thunder_key(int cell, double t0) {
  const auto slot = static_cast<int64_t>(std::floor(t0 / kSlot));
  return (static_cast<uint64_t>(static_cast<uint32_t>(cell)) << 40) ^ static_cast<uint64_t>(slot);
}

void hear_storm(ThunderPlayer& player, const Storm& storm, double t, const Ear& ear) {
  const double speed = sound_speed(ear.celsius);
  // The first call looks back as far as thunder carries; later ones from the last.
  const double from = player.started ? player.last : t - kSilent / speed - kMaxFlash;
  player.started = true;
  player.last = t;
  if (t <= from) return;
  std::vector<Flash> flashes(kLook);
  const int n = std::min(flashes_between(storm, from, t, flashes.data(), kLook), kLook);
  auto ch = std::make_unique<Channel>();
  auto th = std::make_unique<Thunder>();
  for (int i = 0; i < n; ++i) {
    const Flash& f = flashes[static_cast<size_t>(i)];
    if (f.t0 < from || f.t0 >= t) continue;  // alive in the window, started before it
    const uint64_t key = thunder_key(f.cell, f.t0);
    if (std::find(player.heard.begin(), player.heard.end(), key) != player.heard.end()) continue;
    player.heard.push_back(key);
    build_channel(f, *ch);
    hear_thunder(f, *ch, ear.x, ear.y, ear.z, speed, *th);
    if (th->bins <= 0 || th->peak <= 0) continue;  // too far: seen, never heard
    const double at = f.t0 + th->first;
    if (at < t - kThunderLate) continue;  // its first sound already went by
    ThunderClap clap;
    clap.at = at;
    clap.key = key;
    clap.distance = th->nearest;
    clap.pan = pan_to(*ch, ear);
    clap.pcm = std::make_unique<audio::Pcm>();
    clap.pcm->rate = player.rate;
    // Its whole length and the rumble's tail after it.
    const auto frames = static_cast<size_t>((th->length + 3.0) * player.rate);
    clap.pcm->samples.assign(frames, 0.0f);
    render_thunder(*th, player.rate, player.seed ^ static_cast<uint32_t>(key), player.gain,
                   clap.pcm->samples);
    if (player.waiting.size() + player.sounding.size() >= kThunderClaps) {
      if (player.waiting.empty()) continue;
      let_go_quietest(player.waiting);
    }
    auto at_place = std::upper_bound(player.waiting.begin(), player.waiting.end(), at,
                                     [](double a, const ThunderClap& c) { return a < c.at; });
    player.waiting.insert(at_place, std::move(clap));
  }
  // A flash too old to come again needs no key.
  if (player.heard.size() > 4 * kLook)
    player.heard.erase(player.heard.begin(), player.heard.begin() + kLook);
}

void play_thunder(ThunderPlayer& player, audio::Synth& synth, double t, double block_s, int bus) {
  std::erase_if(player.sounding, [&](const ThunderClap& c) { return !synth.sample_live(c.id); });
  while (!player.waiting.empty() && player.waiting.front().at < t + block_s) {
    ThunderClap clap = std::move(player.waiting.front());
    player.waiting.erase(player.waiting.begin());
    if (clap.at < t - kThunderLate) continue;
    audio::SamplePlay play;
    play.gain = clap.gain;
    play.pan = clap.pan;
    play.loop = audio::Loop::file;
    play.delay_ms = static_cast<float>(std::max(0.0, clap.at - t) * 1000.0);
    clap.id = synth.sample(*clap.pcm, play, bus);
    if (clap.id.valid()) player.sounding.push_back(std::move(clap));
  }
}

}  // namespace ge::wind
