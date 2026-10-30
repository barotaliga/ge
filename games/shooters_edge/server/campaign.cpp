// The campaign host (campaign.hpp states the rules).
//
// ge:summary The campaign host: a map's start, the level changes, the parms and flags carried
// ge:keywords campaign_new_game campaign_join campaign_frame campaign_change campaign_stats
// ge:keywords start_level skill-round settle carried-parms
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_test.cpp
#include "shooters_edge/server/campaign.hpp"

#include <algorithm>

#include "core/num.hpp"
#include "shooter/frame.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::server {

namespace {

namespace con = ge::console;

// Every setting the old server had changed from its default, kept for the next one.
void keep_settings(Campaign& c) {
  if (!c.server) return;
  c.settings.clear();
  for (const con::Cvar* v : c.server->cvars.sorted())
    if (!v->is_default()) c.settings.emplace_back(v->name, v->value);
}

void set(QcServer& s, const std::string& name, const std::string& value) {
  s.cvars.set(name, value, con::Source::local, true);
}

// `skill` to the nearest whole number in 0..3, written back.
void round_skill(QcServer& s) {
  const con::Cvar* v = s.cvars.find("skill");
  const double r = ge::core::num::round(v->number);
  const int skill = static_cast<int>(std::clamp(r, 0.0, 3.0));
  set(s, "skill", std::to_string(skill));
}

// The map `name` started on a new server; the players entered with `change`'s parms.
bool start_level(Campaign& c, const std::string& name, MapChange change, std::string& error) {
  keep_settings(c);
  c.server.reset();  // it points into the old map: it goes before the loader is asked again
  c.map = MapSpec{};
  if (c.opt.load == nullptr) {
    error = "campaign: no map loader; next: set CampaignOptions::load";
    return false;
  }
  if (!c.opt.load(c.opt.load_ctx, name, c.map, error)) return false;
  c.map.name = name;

  QcServerOptions so = c.opt.server;
  so.loop.single_player_clock = true;
  so.loop.mintic = kCampaignStep;
  so.loop.start_time = 1.0;
  c.server = make_qc_server(*c.progs, so, error);
  if (!c.server) return false;
  QcServer& s = *c.server;
  for (const auto& [k, v] : c.settings) set(s, k, v);
  set(s, "registered", c.opt.registered ? "1" : "0");
  if (s.cvars.find("coop")->on()) set(s, "deathmatch", "0");
  round_skill(s);
  apply_settings(s);
  s.serverflags = c.serverflags;
  c.entered_serverflags = c.serverflags;
  if (!load_map(s, c.map, error)) return false;
  sh::settle_frames(s.sv, kSettleFrames, kSettleSeconds);

  const char* kind = change == MapChange::new_game       ? "new "
                     : change == MapChange::change_level ? "change "
                                                         : "restart ";
  c.history.push_back(kind + name);
  for (int i = 0; i < sh::kMaxClients; i++) {
    CampaignPlayer& p = c.players[i];
    if (!p.active) continue;
    connect(s, i, p.name, false, false, p.carried ? p.parms : nullptr);
    std::copy(std::begin(s.clients[i].spawn_parms), std::end(s.clients[i].spawn_parms),
              std::begin(p.entered));
  }
  return true;
}

}  // namespace

bool campaign_new_game(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                       const std::string& map, std::string& error) {
  c.server.reset();
  c.opt = opt;
  c.progs = &progs;
  c.settings.clear();
  c.history.clear();
  for (CampaignPlayer& p : c.players) p = CampaignPlayer{};
  // The settings lines run on a scratch server so they are read as the console reads them.
  if (!opt.settings.empty()) {
    QcServerOptions so = opt.server;
    std::unique_ptr<QcServer> scratch = make_qc_server(progs, so, error);
    if (!scratch) return false;
    for (const std::string& line : opt.settings) server_command(*scratch, line);
    c.server = std::move(scratch);
  }
  return campaign_change(c, MapChange::new_game, map, error);
}

void campaign_join(Campaign& c, int slot, const std::string& name) {
  CampaignPlayer& p = c.players[slot];
  p = CampaignPlayer{};
  p.active = true;
  p.name = name;
  if (!c.server) return;
  QcServer& s = *c.server;
  connect(s, slot, name, false, false);
  std::copy(std::begin(s.clients[slot].spawn_parms), std::end(s.clients[slot].spawn_parms),
            std::begin(p.entered));
}

bool campaign_change(Campaign& c, MapChange change, const std::string& map, std::string& error) {
  switch (change) {
    case MapChange::new_game:
      c.serverflags = 0;
      for (CampaignPlayer& p : c.players) p.carried = false;
      break;
    case MapChange::change_level:
      if (c.server) {
        QcServer& s = *c.server;
        save_change_parms(s);
        for (int i = 0; i < sh::kMaxClients; i++) {
          CampaignPlayer& p = c.players[i];
          if (!p.active) continue;
          std::copy(std::begin(s.clients[i].spawn_parms), std::end(s.clients[i].spawn_parms),
                    std::begin(p.parms));
          p.carried = true;
        }
        const int at = s.q.layout->g.serverflags;
        if (at >= 0) c.serverflags = pg::global_float(s.q.vm, at);
      }
      break;
    case MapChange::restart:
      c.serverflags = c.entered_serverflags;
      for (CampaignPlayer& p : c.players) {
        if (!p.active) continue;
        std::copy(std::begin(p.entered), std::end(p.entered), std::begin(p.parms));
        p.carried = true;
      }
      break;
  }
  return start_level(c, map, change, error);
}

bool campaign_frame(Campaign& c, const sh::Packet* packets, int count, std::string& error) {
  if (!c.server) {
    error = "campaign: no map is running; next: start a new game";
    return false;
  }
  QcServer& s = *c.server;
  qc_frame(s, packets, count);
  if (s.pending_map.empty()) return true;
  const std::string map = s.pending_map;
  const MapChange change = s.pending_change;
  return campaign_change(c, change, map, error);
}

CampaignStats campaign_stats(const QcServer& s) {
  CampaignStats st;
  const pg::LayoutGlobals& g = s.q.layout->g;
  const pg::Vm& vm = s.q.vm;
  auto read = [&](int at) { return at < 0 ? 0.0f : pg::global_float(vm, at); };
  st.total_secrets = read(g.total_secrets);
  st.total_monsters = read(g.total_monsters);
  st.found_secrets = read(g.found_secrets);
  st.killed_monsters = read(g.killed_monsters);
  return st;
}

}  // namespace shooters_edge::server
