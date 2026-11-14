// The sequel's screens (sequel_screens.hpp states the rules; the facts are the clean-room note's,
// porting/shooters-edge/sequel-hud.md § 6).
//
// ge:summary The sequel's scoreboard and help computer: their programs, commands and refreshes
// ge:keywords sequel scoreboard help-computer layout sequel_scoreboard screens_command
// ge:skill shooter ui
// ge:test tests/shooters_edge_sequel_screens_test.cpp
#include "shooters_edge/server/sequel_screens.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "shooters_edge/server/sequel_campaign.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr int kRows = 12;     // the blocks shown
constexpr int kRefresh = 32;  // frames between a scoreboard's rewrites
constexpr int kMinuteFrames = 600;

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const SequelServer& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }

void show_scores(SequelServer& s, int slot, int killer) {
  SequelPlayer& p = s.players[slot];
  p.screen = kScreenScores;
  p.layout = sequel_scoreboard(s, slot, killer);
}

}  // namespace

std::string sequel_scoreboard(const SequelServer& s, int viewer, int killer) {
  std::vector<int> order;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && !s.clients[i].spectator) order.push_back(i);
  std::stable_sort(order.begin(), order.end(),
                   [&](int a, int b) { return s.players[a].frags > s.players[b].frags; });
  std::string out;
  for (int r = 0; r < static_cast<int>(order.size()) && r < kRows; r++) {
    const int i = order[static_cast<size_t>(r)];
    const int x = r >= 6 ? 160 : 0;
    const int y = 32 + 32 * (r % 6);
    char row[160];
    int n = 0;
    if (i == viewer || i == killer)
      n = std::snprintf(row, sizeof row, "xv %d yv %d picn %s ", x + 32, y,
                        i == viewer ? "tag1" : "tag2");
    const double in_game = std::max(0.0, s.level_time - s.clients[i].entered_at);
    const auto frames = static_cast<int64_t>(in_game * 10.0 + 1e-6);
    std::snprintf(row + n, sizeof row - size_t(n), "client %d %d %d %d %d %d ", x, y, i,
                  static_cast<int>(s.players[i].frags), std::min(s.clients[i].ping_ms, 9999),
                  static_cast<int>(frames / kMinuteFrames));
    if (out.size() + std::char_traits<char>::length(row) > kLayoutMost) break;
    out += row;
  }
  return out;
}

bool screens_command(SequelServer& s, int slot, const std::string& cmd) {
  SequelPlayer& p = s.players[slot];
  const bool help = cmd == "help" && !deathmatch(s);
  if (cmd == "putaway" || cmd == "inven") {
    p.screen = kScreenNone;
    return cmd == "putaway";
  }
  if (help) {
    if (p.screen == kScreenHelp && p.help_seen == s.help_changed) {
      p.screen = kScreenNone;
      return true;
    }
    p.screen = kScreenHelp;
    p.help_unread = 0;
    p.layout = campaign_help_screen(s);
    return true;
  }
  if (cmd != "score" && cmd != "help") return false;
  if (!deathmatch(s) && !coop(s)) {  // single player: only help closes
    p.screen = kScreenNone;
    return true;
  }
  if (p.screen == kScreenScores) {
    p.screen = kScreenNone;
    return true;
  }
  show_scores(s, slot, p.killer);
  return true;
}

void screens_frame(SequelServer& s) {
  if (s.frame_number % kRefresh != 0) return;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active && s.players[i].screen == kScreenScores)
      s.players[i].layout =
          sequel_scoreboard(s, i, s.intermission_at > 0 ? -1 : s.players[i].killer);
}

void screens_died(SequelServer& s, int slot) {
  if (!deathmatch(s)) return;
  SequelPlayer& p = s.players[slot];
  if (p.screen == kScreenScores)
    p.screen = kScreenNone;
  else
    show_scores(s, slot, p.killer);
}

void screens_spawned(SequelServer& s, int slot) { s.players[slot].screen = kScreenNone; }

void screens_intermission(SequelServer& s) {
  if (!deathmatch(s) && !coop(s)) return;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) show_scores(s, i, -1);
}

}  // namespace shooters_edge::server
