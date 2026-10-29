// Thunder's definitions: the channel's segments binned by when they are heard, and the sound made
// of the bins (filtered hashed noise and a lingering rumble).
//
// ge:summary Thunder: arrivals binned off the channel, the envelope rendered as filtered noise
// ge:keywords thunder bins arrival cutoff rumble noise low-pass hear_thunder render_thunder
// ge:skill water audio
// ge:test tests/wind_lightning_test.cpp
#include "wind/thunder.hpp"

#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::wind {

namespace {

namespace lm = core::libm;

constexpr double kTau = 6.283185307179586;
constexpr double kBent = 12000;      // m, where the sound starts to be bent up over a listener
constexpr double kNearest = 50;      // m, the closest a segment counts as
constexpr double kRumbleHz = 70;     // the lingering rumble's cut-off
constexpr double kRumbleLag = 0.15;  // s, how long the rumble lingers on the envelope

double cutoff_at(double d) { return 5000 / (1 + d / 300); }

double fade_at(double d) {
  const double u = (d - kBent) / (kSilent - kBent);
  if (u <= 0) return 1;
  if (u >= 1) return 0;
  return 1 - u * u * (3 - 2 * u);
}

double distance(const Segment& s, double x, double y, double z) {
  const double dx = 0.5 * (s.x0 + s.x1) - x, dy = 0.5 * (s.y0 + s.y1) - y,
               dz = 0.5 * (s.z0 + s.z1) - z;
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  return d < kNearest ? kNearest : d;
}

double point(double px, double py, double pz, double x, double y, double z) {
  const double d = std::sqrt((px - x) * (px - x) + (py - y) * (py - y) + (pz - z) * (pz - z));
  return d < kNearest ? kNearest : d;
}

double seg_len(const Segment& s) {
  const double dx = s.x1 - s.x0, dy = s.y1 - s.y0, dz = s.z1 - s.z0;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The share of white noise's power a one-pole low-pass of coefficient `k` keeps, square-rooted
// and inverted, so filtered noise keeps about the level it came in at.
double makeup(double k) { return std::sqrt((2 - k) / k); }

}  // namespace

double sound_speed(double celsius) { return 331.3 + 0.606 * celsius; }

void hear_thunder(const Flash& flash, const Channel& channel, double x, double y, double z,
                  double speed, Thunder& out) {
  out.first = 0;
  out.length = 0;
  out.peak = 0;
  out.bins = 0;
  out.nearest = 1e30;
  for (int i = 0; i < channel.count; ++i) {
    const Segment& g = channel.seg[i];
    const double d = std::fmin(point(g.x0, g.y0, g.z0, x, y, z), point(g.x1, g.y1, g.z1, x, y, z));
    if (d < out.nearest) out.nearest = d;
  }
  if (channel.count == 0 || flash.strokes == 0 || out.nearest >= kSilent) return;
  out.first = flash.stroke[0].t + out.nearest / speed;
  double weighted[kThunderBins];
  for (int b = 0; b < kThunderBins; ++b) {
    out.loud[b] = 0;
    weighted[b] = 0;
  }
  int last = -1;
  for (int k = 0; k < flash.strokes; ++k) {
    const Stroke& s = flash.stroke[k];
    for (int i = 0; i < channel.count; ++i) {
      const Segment& g = channel.seg[i];
      if (g.order > 0 && k > 0) continue;
      const double d = distance(g, x, y, z);
      const double fade = fade_at(d);
      if (fade <= 0) continue;
      // The segment is heard from its near end to its far end: its sound spread over the bins
      // between, so a channel rumbles on rather than ticking segment by segment.
      const double n0 = point(g.x0, g.y0, g.z0, x, y, z), n1 = point(g.x1, g.y1, g.z1, x, y, z);
      const double a0 = (s.t + std::fmin(n0, n1) / speed - out.first) / kThunderBin;
      const double a1 = (s.t + std::fmax(n0, n1) / speed - out.first) / kThunderBin;
      const auto b0 = static_cast<int>(a0), b1 = static_cast<int>(a1);
      if (b0 < 0 || b1 >= kThunderBins) continue;
      const double loud = s.energy * g.glow * seg_len(g) * 1000 / (d * d) * fade / (b1 - b0 + 1);
      const double cut = cutoff_at(d);
      for (int b = b0; b <= b1; ++b) {
        out.loud[b] += static_cast<float>(loud);
        weighted[b] += loud * cut;
      }
      if (b1 > last) last = b1;
    }
  }
  out.bins = last + 1;
  out.length = out.bins * kThunderBin;
  for (int b = 0; b < out.bins; ++b) {
    out.cutoff[b] = out.loud[b] > 0 ? static_cast<float>(weighted[b] / out.loud[b]) : 60.0f;
    if (out.loud[b] > out.peak) out.peak = out.loud[b];
  }
}

void render_thunder(const Thunder& thunder, double rate, uint32_t seed, double gain,
                    std::span<float> out) {
  const double rk = 1 - lm::exp(-kTau * kRumbleHz / rate);
  const double lag = 1 - lm::exp(-1 / (kRumbleLag * rate));
  double a1 = 0, a2 = 0, r1 = 0, r2 = 0, slow = 0, k = 0.5;
  int bin = -1;
  for (size_t i = 0; i < out.size(); ++i) {
    const double t = static_cast<double>(i) / rate;
    const double at = t / kThunderBin;
    const auto b = static_cast<int>(at);
    double loud = 0;
    if (b < thunder.bins) {
      const double next = b + 1 < thunder.bins ? thunder.loud[b + 1] : 0;
      const double u = at - b;
      loud = thunder.loud[b] * (1 - u) + next * u;
      if (b != bin) {
        bin = b;
        k = 1 - lm::exp(-kTau * thunder.cutoff[b] / rate);
      }
    }
    slow += (loud - slow) * lag;
    const auto n = static_cast<uint32_t>(i);
    const double w1 = 2 * storm_hash(seed, 20, n) - 1, w2 = 2 * storm_hash(seed, 21, n) - 1;
    a1 += (w1 - a1) * k;
    a2 += (a1 - a2) * k;
    r1 += (w2 - r1) * rk;
    r2 += (r1 - r2) * rk;
    const double v =
        gain * (std::sqrt(loud) * a2 * makeup(k) * 0.5 + std::sqrt(slow) * r2 * makeup(rk) * 0.35);
    out[i] = static_cast<float>(lm::tanh(v));
  }
}

}  // namespace ge::wind
