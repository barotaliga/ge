// The hub's browse, defined: the game's server list stepped, its answers merged into the index.
//
// ge:summary The hub's browse defined: the server list's answers as records in the index
// ge:keywords hub browse merge status_of server-list refresh drop
// ge:skill net
// ge:test tests/hub_browse_test.cpp
#include "hub/browse.hpp"

#include <algorithm>

namespace ge::hub {

namespace gn = ge::net;

namespace {

gn::StatusPhase phase_of(const std::string& state) {
  std::string s = state;
  for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  if (s.find("countdown") != std::string::npos) return gn::StatusPhase::countdown;
  if (s.find("standby") != std::string::npos || s.find("warmup") != std::string::npos) {
    return gn::StatusPhase::lobby;
  }
  if (s.find("intermission") != std::string::npos || s.find("ended") != std::string::npos) {
    return gn::StatusPhase::intermission;
  }
  return gn::StatusPhase::playing;
}

}  // namespace

gn::Status status_of(const gn::ServerInfo& info) {
  gn::Status s;
  s.name = info.name;
  s.map = info.map;
  s.mode = info.mode;
  s.phase = phase_of(info.state);
  s.max_players = info.max_players;
  s.max_spectators = info.max_spectators;
  s.locked = info.locked;
  s.players.reserve(info.roster.size());
  for (const gn::ServerPlayer& p : info.roster) {
    gn::StatusPlayer sp;
    sp.name = p.name;
    sp.team = p.team;
    sp.score = p.frags;
    sp.ping_ms = static_cast<uint16_t>(std::clamp(p.ping_ms, 0, 65535));
    sp.flags = p.spectator ? gn::kPlayerSpectator : 0;
    s.players.push_back(sp);
    if (p.spectator || p.team.empty()) continue;
    auto t = std::find_if(s.teams.begin(), s.teams.end(),
                          [&](const gn::StatusTeam& x) { return x.name == p.team; });
    if (t == s.teams.end()) t = s.teams.insert(s.teams.end(), gn::StatusTeam{p.team, 0, 0});
    t->score += p.frags;
  }
  if (s.teams.size() < 2) s.teams.clear();  // one team named is no team game
  std::sort(s.teams.begin(), s.teams.end(),
            [](const gn::StatusTeam& a, const gn::StatusTeam& b) { return a.score > b.score; });
  for (const auto& [k, v] : info.keys) {
    if (s.keys.size() >= 24) break;
    s.keys.emplace_back(k, v);
  }
  return s;
}

void browse_init(Browse& b, const BrowseConfig& config, const gn::ServerQuery& query,
                 gn::Transport& socket, int64_t now_us) {
  b.config = config;
  gn::ServerListConfig lc;
  lc.masters = config.masters;
  lc.servers = config.servers;
  gn::server_list_init(b.list, lc, query, socket);
  b.next_refresh_us = now_us;
  b.stats = {};
  b.on = !config.masters.empty() || !config.servers.empty();
}

void browse_update(Browse& b, Poller& p, int64_t now_us, int64_t unix_s) {
  if (!b.on) return;
  if (now_us >= b.next_refresh_us) {
    gn::server_list_refresh(b.list, now_us);
    b.next_refresh_us = now_us + b.config.refresh_us;
    b.stats.refreshes++;
  }
  gn::server_list_update(b.list, now_us);
  for (const gn::ServerEntry& e : b.list.entries) {
    if (!e.answered) continue;
    auto it = std::find_if(p.nodes.begin(), p.nodes.end(),
                           [&](const HubNode& n) { return n.address == e.address; });
    if (it == p.nodes.end()) {
      if (static_cast<int>(p.nodes.size()) >= p.config.max_nodes) continue;
      HubNode n;
      n.address = e.address;
      n.kind = gn::NodeKind::game;
      n.first_us = now_us;
      n.browsed = true;
      p.nodes.push_back(std::move(n));
      it = p.nodes.end() - 1;
      b.stats.added++;
      p.serial++;
    }
    if (!it->browsed) continue;  // the engine's own master lists it: its own record wins
    if (it->heard_us >= b.list.refreshed_us) continue;  // already taken this refresh
    gn::Status s = status_of(e.info);
    const uint64_t h = record_hash(s);
    if (!it->have || h != it->record_hash) {
      it->changes++;
      b.stats.changes++;
      p.serial++;
    }
    bool ended = false;
    if (it->have) {
      // The match's start is known only when the hub saw its phase begin.
      const int64_t since = it->phase_us > it->first_us ? it->phase_us : -1;
      ended = results_see(b.results, it->address, it->status, since, s, now_us, unix_s);
    }
    if (!it->have || s.phase != it->status.phase || ended) it->phase_us = now_us;
    it->record_hash = h;
    it->status = std::move(s);
    it->have = true;
    it->heard_us = now_us;
    b.stats.merged++;
  }
  const size_t before = p.nodes.size();
  p.nodes.erase(std::remove_if(p.nodes.begin(), p.nodes.end(),
                               [&](const HubNode& n) {
                                 const int64_t last = n.heard_us >= 0 ? n.heard_us : n.first_us;
                                 return n.browsed && now_us - last > b.config.drop_us;
                               }),
                p.nodes.end());
  if (p.nodes.size() != before) {
    b.stats.dropped += before - p.nodes.size();
    p.serial++;
  }
}

}  // namespace ge::hub
