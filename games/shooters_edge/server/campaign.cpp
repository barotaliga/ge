// The campaign host (campaign.hpp states the rules).
//
// ge:summary The campaign host: a map's start, the level changes, the parms and flags carried
// ge:keywords campaign_new_game campaign_join campaign_frame campaign_change campaign_stats
// ge:keywords start_level skill-round settle carried-parms
// ge:skill progs shooter
// ge:test tests/shooters_edge_campaign_test.cpp
#include "shooters_edge/server/campaign.hpp"

#include <algorithm>
#include <string_view>

#include "core/num.hpp"
#include "progs/vm.hpp"
#include "shooter/frame.hpp"
#include "shooters_edge/server/campaign_episodes.hpp"
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

// The episodes done (campaign_episodes.hpp): the runes the game carried in added to the server's
// `sv_episodes_done`; on the start map every done episode's rune in `serverflags`.
uint32_t episodes_done(const QcServer& s) {
  const con::Cvar* v = s.cvars.find("sv_episodes_done");
  return v == nullptr ? 0u : static_cast<uint32_t>(v->integer()) & kEpisodeBits;
}

void keep_episodes(Campaign& c, QcServer& s) {
  if (c.opt.episodes.start_map.empty()) return;
  const uint32_t carried =
      static_cast<uint32_t>(ge::core::num::to_int32(c.serverflags)) & kEpisodeBits;
  const uint32_t done = episodes_done(s) | carried;
  if (done != episodes_done(s)) set(s, "sv_episodes_done", std::to_string(done));
  if (c.map.name == c.opt.episodes.start_map)
    c.serverflags =
        static_cast<float>(static_cast<uint32_t>(ge::core::num::to_int32(c.serverflags)) | done);
}

// The map `name` started on a new server, `kind` its line in the history; with `join`, the
// players entered with the parms the change left them.
bool start_level(Campaign& c, const std::string& name, const char* kind, bool join,
                 std::string& error) {
  keep_settings(c);
  c.server = {};  // it points into the old map: it goes before the loader is asked again
  c.map = MapSpec{};
  if (c.opt.load == nullptr) {
    error = "the game is built wrong: the campaign has no map loader";
    return false;
  }
  if (!c.opt.load(c.opt.load_ctx, name, c.map, error)) return false;
  c.map.name = name;

  QcServerOptions so = c.opt.server;
  so.loop.single_player_clock = true;
  so.loop.mintic = kCampaignStep;
  so.loop.start_time = 1.0;
  std::unique_ptr<QcServer> made = make_qc_server(*c.progs, so, error);
  if (!made) return false;
  QcServer& s = *made;
  c.server = qc_runner(std::move(made));
  for (const auto& [k, v] : c.settings) set(s, k, v);
  set(s, "registered", c.opt.registered ? "1" : "0");
  if (s.cvars.find("coop")->on()) set(s, "deathmatch", "0");
  round_skill(s);
  apply_settings(s);
  keep_episodes(c, s);
  if (!shut_episode_bars(c.map, c.opt.episodes, episodes_done(s), c.opt.registered, c.bars, error))
    return false;
  s.serverflags = c.serverflags;
  c.entered_serverflags = c.serverflags;
  if (!c.server.start(c.map, error)) return false;
  // The map's server is made by the start: its touch watch is set on it now, before it settles.
  if (s.cvars.find("coop")->on()) {
    c.coop_weapons = CoopWeapons{&s, 0};
    s.sv.touch_watch = &coop_weapon_touch;
    s.sv.touch_watch_ctx = &c.coop_weapons;
  }
  sh::settle_frames(s.sv, kSettleFrames, kSettleSeconds);

  c.history.push_back(kind + name);
  if (!join) return true;
  for (int i = 0; i < sh::kMaxClients; i++) {
    CampaignPlayer& p = c.players[i];
    if (!p.active) continue;
    c.server.connect(i, p.name, false, false, p.carried ? p.parms : nullptr);
    std::copy(std::begin(s.clients[i].spawn_parms), std::end(s.clients[i].spawn_parms),
              std::begin(p.entered));
  }
  return true;
}

}  // namespace

void coop_weapon_touch(void* ctx, sh::Server& sv, int trigger, int toucher, bool after) {
  CoopWeapons& w = *static_cast<CoopWeapons*>(ctx);
  if (w.server == nullptr || toucher < 1 || toucher > sv.set.maxclients) return;
  QcServer& s = *w.server;
  pg::Vm& vm = s.q.vm;
  const auto& f = s.q.layout->f;
  const char* cn =
      pg::vm_string(vm, static_cast<int32_t>(*pg::entity_field(vm, trigger, f.classname)));
  if (cn == nullptr || std::string_view(cn).substr(0, 7) != "weapon_") return;
  const float items = pg::as_float(*pg::entity_field(vm, toucher, f.items));
  if (!after) {
    w.items_before = items;
    return;
  }
  // Nothing new given (he had it already), or the program took it away itself.
  if (items == w.items_before || !model_shown(s, trigger)) return;
  *pg::entity_field(vm, trigger, f.model) = 0;  // the empty text: neither drawn nor sent
  *pg::entity_field(vm, trigger, f.solid) = pg::as_word(static_cast<float>(sh::kSolidNot));
  sv.edicts[static_cast<size_t>(trigger)].solid = sh::kSolidNot;
}

bool campaign_new_game(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                       const std::string& map, std::string& error) {
  c.server = {};
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
    c.server = qc_runner(std::move(scratch));
    for (const std::string& line : opt.settings) c.server.console(line);
  }
  return campaign_change(c, MapChange::new_game, map, error);
}

void campaign_join(Campaign& c, int slot, const std::string& name) {
  CampaignPlayer& p = c.players[slot];
  p = CampaignPlayer{};
  p.active = true;
  p.name = name;
  if (!c.server) return;
  c.server.connect(slot, name, false, false);
  const RulesCommon& s = *c.server;
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
        c.server.carry();
        const RulesCommon& s = *c.server;
        for (int i = 0; i < sh::kMaxClients; i++) {
          CampaignPlayer& p = c.players[i];
          if (!p.active) continue;
          std::copy(std::begin(s.clients[i].spawn_parms), std::end(s.clients[i].spawn_parms),
                    std::begin(p.parms));
          p.carried = true;
        }
        c.serverflags = s.world.serverflags;
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
  const char* kind = change == MapChange::new_game       ? "new "
                     : change == MapChange::change_level ? "change "
                                                         : "restart ";
  return start_level(c, map, kind, true, error);
}

bool campaign_reopen(Campaign& c, const pg::Progs& progs, const CampaignOptions& opt,
                     const std::string& map, std::string& error) {
  c.server = {};  // the settings are the caller's (`c.settings`), never the old server's
  c.opt = opt;
  c.progs = &progs;
  return start_level(c, map, "load ", false, error);
}

bool campaign_frame(Campaign& c, const sh::Packet* packets, int count, std::string& error) {
  if (!c.server) {
    error = "No game is running.";
    return false;
  }
  c.server.frame(packets, count);
  const RulesCommon& s = *c.server;
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
