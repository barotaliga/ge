// The guard's checks (net/guard.hpp).
//
// ge:summary The guard's checks: fields clamped and snapped, the flood window, the score's decay
// ge:keywords net guard anti-cheat clamp snap flood score decay
// ge:skill net
// ge:test tests/net_guard_test.cpp
#include "net/guard.hpp"

#include <algorithm>
#include <cmath>

namespace ge::net {

namespace {

bool valid(const Guard& g, int client) {
  return client >= 0 && client < static_cast<int>(g.clients.size());
}

void raise(Guard& g, int client, Cheat cheat, uint32_t tick, int32_t value) {
  GuardClient& c = g.clients[static_cast<size_t>(client)];
  const auto k = static_cast<size_t>(cheat);
  c.counts[k]++;
  c.score = std::min(c.score + g.rules.weight[k], g.rules.out_score * 2);  // out stays out
  c.worst = std::max(c.worst, c.score);
  if (c.score > g.rules.out_score) c.out = true;
  if (g.flags.size() == kMaxFlags) g.flags.erase(g.flags.begin());
  g.flags.push_back({client, cheat, tick, value});
  g.raised++;
}

int32_t read_field(const uint8_t* in, const InputField& f) {
  uint32_t u = in[f.offset];
  if (f.bytes == 2) u |= static_cast<uint32_t>(in[f.offset + 1]) << 8;
  if (!f.is_signed) return static_cast<int32_t>(u);
  return f.bytes == 2 ? static_cast<int32_t>(static_cast<int16_t>(u))
                      : static_cast<int32_t>(static_cast<int8_t>(u));
}

void write_field(uint8_t* in, const InputField& f, int32_t v) {
  const auto u = static_cast<uint32_t>(v);
  in[f.offset] = static_cast<uint8_t>(u & 0xff);
  if (f.bytes == 2) in[f.offset + 1] = static_cast<uint8_t>((u >> 8) & 0xff);
}

int window_ticks(const GuardRules& r) {
  return std::max(1, static_cast<int>(r.window_s * r.tick_hz));
}

}  // namespace

const char* cheat_name(Cheat c) {
  switch (c) {
    case Cheat::off_range:
      return "off-range";
    case Cheat::off_grid:
      return "off-grid";
    case Cheat::flood:
      return "flood";
    case Cheat::future:
      return "future";
    case Cheat::motion:
      return "motion";
    case Cheat::count:
      break;
  }
  return "?";
}

void guard_init(Guard& g, const GuardRules& rules) {
  g = Guard{};
  g.rules = rules;
}

int guard_input(Guard& g, int client, uint8_t* input, uint32_t tick) {
  if (!valid(g, client)) return 0;
  g.clients[static_cast<size_t>(client)].window_inputs++;
  int raised = 0;
  for (const InputField& f : g.rules.fields) {
    const int32_t v = read_field(input, f);
    int32_t fixed = std::clamp(v, f.min, f.max);
    if (fixed != v) {
      raise(g, client, Cheat::off_range, tick, v);
      raised++;
    } else if (f.step > 1 && (v - f.min) % f.step != 0) {
      const int32_t below = f.min + (v - f.min) / f.step * f.step;
      fixed = (v - below) * 2 >= f.step ? std::min(below + f.step, f.max) : below;
      raise(g, client, Cheat::off_grid, tick, v);
      raised++;
    }
    if (fixed != v) write_field(input, f, fixed);
  }
  return raised;
}

uint32_t guard_view(Guard& g, int client, uint32_t view_x256, uint32_t tick) {
  if (!valid(g, client)) return view_x256;
  const uint64_t most = (uint64_t{tick} + static_cast<uint64_t>(g.rules.future_slack)) << 8;
  if (view_x256 <= most) return view_x256;
  raise(g, client, Cheat::future, tick, static_cast<int32_t>((view_x256 >> 8) - tick));
  return tick << 8;
}

void guard_tick(Guard& g, uint32_t tick) {
  const int ticks = window_ticks(g.rules);
  const double decay = g.rules.decay / std::max(1, g.rules.tick_hz);
  for (size_t i = 0; i < g.clients.size(); i++) {
    GuardClient& c = g.clients[i];
    c.score = std::max(0.0, c.score - decay);
    if (++c.window_ticks < ticks) continue;
    if (c.window_inputs > ticks + g.rules.flood_slack)
      raise(g, static_cast<int>(i), Cheat::flood, tick, c.window_inputs);
    c.window_inputs = 0;
    c.window_ticks = 0;
  }
}

bool guard_motion(Guard& g, int client, const double* before, const double* after, uint32_t tick) {
  if (!valid(g, client) || g.rules.max_step <= 0) return true;
  const double dx = after[g.rules.motion_fields[0]] - before[g.rules.motion_fields[0]];
  const double dz = after[g.rules.motion_fields[1]] - before[g.rules.motion_fields[1]];
  const double most = g.rules.max_step;
  if (dx * dx + dz * dz <= most * most) return true;
  // The value kept is the step in tenths of the most a tick allows (10: exactly the most).
  const double d2 = (dx * dx + dz * dz) / (most * most);
  int32_t tenths = 10;
  while (tenths < 100000 && static_cast<double>(tenths) * tenths < d2 * 100) tenths++;
  raise(g, client, Cheat::motion, tick, tenths);
  return false;
}

void guard_reset(Guard& g, int client) {
  if (valid(g, client)) g.clients[static_cast<size_t>(client)] = GuardClient{};
}

bool guard_out(const Guard& g, int client) {
  return valid(g, client) && g.clients[static_cast<size_t>(client)].out;
}

std::string guard_summary(const Guard& g, int client) {
  if (!valid(g, client)) return "no such client";
  const GuardClient& c = g.clients[static_cast<size_t>(client)];
  std::string out;
  for (size_t k = 0; k < c.counts.size(); k++) {
    if (c.counts[k] == 0) continue;
    if (!out.empty()) out += ", ";
    out += cheat_name(static_cast<Cheat>(k));
    out += " x" + std::to_string(c.counts[k]);
  }
  if (out.empty()) out = "nothing flagged";
  const auto tenth = static_cast<int64_t>(std::floor(c.worst * 10 + 0.5));
  out += "; worst score " + std::to_string(tenth / 10) + "." + std::to_string(tenth % 10);
  if (c.out) out += "; out";
  return out;
}

}  // namespace ge::net
