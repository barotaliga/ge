// The fate of each packet on a conditioned link (the model is stated in conditions.hpp), and the
// named presets.
//
// ge:summary Each packet's fate on a conditioned link; the named condition presets
// ge:keywords net conditions decide fate preset gilbert-elliott queue bandwidth
// ge:skill net
// ge:test tests/net_conditions_test.cpp
#include "net/conditions.hpp"

#include <cmath>
#include <cstdio>

namespace ge::net {

namespace {

int64_t us(double ms) { return static_cast<int64_t>(std::floor(ms * 1000.0)); }

struct Preset {
  const char* name;
  LinkConditions c;
};

// Typical one-way numbers, from a cable across a room to a phone on a moving train.
const Preset kPresets[] = {
    {"lan", {0.25, 0.1, 0, 1, 0, 0, 0, 0, 250}},
    {"broadband", {15, 3, 0.002, 1, 0, 0, 0, 20000, 250}},
    {"wifi", {25, 12, 0.01, 2, 0.001, 0.002, 8, 10000, 250}},
    {"mobile", {45, 20, 0.02, 2, 0.002, 0.01, 15, 2000, 400}},
    {"bad", {90, 40, 0.05, 3, 0.01, 0.02, 30, 1000, 500}},
    {"awful", {180, 80, 0.15, 4, 0.02, 0.05, 60, 384, 800}},
};

}  // namespace

double stream_resend_ms(const LinkConditions& c) {
  const double t = 3 * c.latency_ms + 2 * c.jitter_ms;
  return t > 200 ? t : 200;
}

Fate decide(const LinkConditions& c, LinkState& s, core::Rng& rng, int64_t now_us, size_t bytes) {
  // Five draws a packet, always, in this order (conditions.hpp).
  const double u_loss = core::next(rng);
  const double u_chain = core::next(rng);
  const double u_jitter = core::next(rng);
  const double u_dup = core::next(rng);
  const double u_reorder = core::next(rng);

  Fate f;
  bool lost;
  if (c.burst <= 1 || c.loss <= 0 || c.loss >= 1) {
    lost = u_loss < c.loss;
  } else {
    // Gilbert-Elliott: leave the bad state with r = 1/burst, enter it with p chosen so the
    // long-run share in it, p / (p + r), is `loss`.
    const double r = 1.0 / c.burst;
    const double p = c.loss * r / (1.0 - c.loss);
    s.bad = s.bad ? !(u_chain < r) : u_chain < p;
    lost = s.bad;
  }
  if (c.drop_every > 0) {
    const auto at = static_cast<int>(s.sent % static_cast<uint64_t>(c.drop_every));
    if (at >= c.drop_every - c.drop_run) lost = true;
  }
  s.sent++;

  const int64_t start = s.busy_until_us > now_us ? s.busy_until_us : now_us;
  if (!c.stream && start - now_us > us(c.queue_ms)) {
    f.queue_drop = true;
    return f;
  }
  int64_t out = start;
  if (c.bandwidth_kbps > 0) {
    // kbit/s is bits a millisecond: the bits over the rate is the time on the line, ms.
    out += us(static_cast<double>((bytes + kWireOverhead) * 8) / c.bandwidth_kbps);
  }
  s.busy_until_us = out;  // a packet lost on the line still took its time on it
  int64_t arrive = out + us(c.latency_ms) + us(c.jitter_ms * u_jitter);
  if (c.stream) {
    // The stream sends a lost segment again after its timeout; everything behind waits for it.
    if (lost) arrive += us(stream_resend_ms(c));
    if (arrive < s.last_arrive_us) arrive = s.last_arrive_us;
    s.last_arrive_us = arrive;
    f.copies = 1;
    f.arrive_us[0] = arrive;
    return f;
  }
  if (lost) return f;
  if (u_reorder < c.reorder) {
    arrive += us(c.reorder_ms);
  } else {
    if (arrive < s.last_arrive_us) arrive = s.last_arrive_us;
    s.last_arrive_us = arrive;
  }
  f.copies = 1;
  f.arrive_us[0] = arrive;
  if (u_dup < c.duplicate) {
    f.copies = 2;
    f.arrive_us[1] = arrive + 1;
  }
  return f;
}

const std::vector<std::string>& preset_names() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> v;
    for (const Preset& p : kPresets) v.emplace_back(p.name);
    return v;
  }();
  return names;
}

bool conditions_preset(std::string_view name, LinkConditions& out) {
  for (const Preset& p : kPresets) {
    if (name == p.name) {
      out = p.c;
      return true;
    }
  }
  return false;
}

std::string describe_conditions(const LinkConditions& c) {
  char b[256];
  int n = std::snprintf(b, sizeof b, "%g ms", c.latency_ms);
  const auto add = [&](const char* fmt, auto... v) {
    if (n < static_cast<int>(sizeof b)) n += std::snprintf(b + n, sizeof b - n, fmt, v...);
  };
  if (c.jitter_ms > 0) add(" +0..%g", c.jitter_ms);
  const char* drop = c.stream ? "resent" : "loss";
  if (c.loss > 0) {
    add(c.burst > 1 ? ", %g%% %s (bursts of %g)" : ", %g%% %s", c.loss * 100, drop, c.burst);
  }
  if (c.duplicate > 0 && !c.stream) add(", %g%% doubled", c.duplicate * 100);
  if (c.reorder > 0 && !c.stream) add(", %g%% held %g ms", c.reorder * 100, c.reorder_ms);
  if (c.bandwidth_kbps > 0) add(", %g kbit/s", c.bandwidth_kbps);
  if (c.drop_every > 0) add(", %d of every %d dropped", c.drop_run, c.drop_every);
  if (c.stream) add("%s", ", as a stream");
  return b;
}

}  // namespace ge::net
