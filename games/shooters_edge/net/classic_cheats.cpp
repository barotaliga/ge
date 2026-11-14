// The classic server's cheat check (classic_cheats.hpp).
//
// ge:summary The classic server's cheat check: the speed check's cuts scored once a second
// ge:keywords anti-cheat speed-hack speed-check guard flagged cheat-watch
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_cheats_test.cpp
#include "shooters_edge/net/classic_cheats.hpp"

namespace gn = ge::net;
namespace sh = ge::shooter;

namespace shooters_edge::net {

gn::Guard cheat_guard() {
  gn::GuardRules rules;
  rules.tick_hz = 1;
  rules.window_s = 1e9;
  gn::Guard g;
  gn::guard_init(g, rules);
  return g;
}

void cheat_watch_init(CheatWatch& w) { w = CheatWatch{}; }

void cheat_watch_frame(CheatWatch& w, const sh::Server& sv) {
  for (size_t i = 0; i < sv.clients.size() && i < w.cut_seen.size(); i++) {
    const sh::ClientSlot& c = sv.clients[i];
    const bool on = c.active && !c.bot;
    // A slot that emptied, or a new client in it (the count starts again): judged afresh.
    if (on != w.active[i] || c.speed_cut_ms < w.cut_seen[i]) {
      gn::guard_reset(w.guard, static_cast<int>(i));
      w.cut_seen[i] = c.speed_cut_ms;
      w.cut_window[i] = 0;
      w.active[i] = on;
      continue;
    }
    if (!on) continue;
    w.cut_window[i] += c.speed_cut_ms - w.cut_seen[i];
    w.cut_seen[i] = c.speed_cut_ms;
  }
  if (w.window_start < 0 || sv.time < w.window_start) w.window_start = sv.time;
  if (sv.time - w.window_start < 1.0) return;
  w.window_start = sv.time;
  w.seconds++;
  for (size_t i = 0; i < w.cut_window.size(); i++) {
    if (w.active[i] && w.cut_window[i] > kSpeedSlackMs)
      gn::guard_flag(
          w.guard, static_cast<int>(i), gn::Cheat::flood, w.seconds,
          static_cast<int32_t>(w.cut_window[i] > INT32_MAX ? INT32_MAX : w.cut_window[i]));
    w.cut_window[i] = 0;
  }
  gn::guard_tick(w.guard, w.seconds);
}

bool cheat_watch_out(const CheatWatch& w, int slot) { return gn::guard_out(w.guard, slot); }

std::string cheat_watch_verdict(const CheatWatch& w, int slot) {
  return gn::guard_verdict(w.guard, slot);
}

}  // namespace shooters_edge::net
