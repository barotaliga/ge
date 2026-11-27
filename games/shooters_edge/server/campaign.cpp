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

// A player's team, as the campaign's programs read it: 1 (the default bottom colour plus one),
// never 0. A monster's team is 0, so a player left on 0 is on the monsters' team: with teamplay on
// (a co-op game's), a program that guards friendly fire by an equal team alone (Dissolution of
// Eternity's) let no shot of his hurt a monster.
void set_player_team(Campaign& c, int slot) {
  QcServer* s = c.server ? qc_of(c.server) : nullptr;
  if (s == nullptr || s->progs == nullptr) return;
  const pg::Def* d = pg::find_field(*s->progs, "team");
  if (d == nullptr) return;
  *pg::entity_field(s->q.vm, slot + 1, d->offset) = pg::as_word(1.0f);
}

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
  // Every brush model a unit wider on each side (CampaignOptions::wide_models).
  for (ge::shooter::BrushModel& m : c.map.models) {
    if (!c.opt.wide_models) break;
    for (int k = 0; k < 3; k++) {
      m.mins[k] -= 1;
      m.maxs[k] += 1;
    }
  }

  QcServerOptions so = c.opt.server;
  so.loop.single_player_clock = true;
  so.loop.mintic = kCampaignStep;
  so.loop.start_time = 1.0;
  std::unique_ptr<QcServer> made = make_qc_server(*c.progs, so, error);
  if (!made) return false;
  QcServer& s = *made;
  c.server = qc_runner(std::move(made));
  for (const auto& [k, v] : c.settings) set(s, k, v);
  for (const std::string& line : c.opt.every_map) c.server.console(line);
  set(s, "registered", c.opt.registered ? "1" : "0");
  if (s.cvars.find("coop")->on()) set(s, "deathmatch", "0");
  round_skill(s);
  apply_settings(s);
  // The episodes in order are a setting (sv_episodes_in_order); by default every episode stands
  // open and a new game carries no runes, as in the original game.
  c.bars = {};
  if (const con::Cvar* order = s.cvars.find("sv_episodes_in_order");
      order != nullptr && order->on()) {
    keep_episodes(c, s);
    if (!shut_episode_bars(c.map, c.opt.episodes, episodes_done(s), c.opt.registered, c.bars,
                           error))
      return false;
  }
  s.serverflags = c.serverflags;
  c.entered_serverflags = c.serverflags;
  if (!c.server.start(c.map, error)) return false;
  sh::settle_frames(s.sv, kSettleFrames, kSettleSeconds);

  c.history.push_back(kind + name);
  if (!join) return true;
  for (int i = 0; i < sh::kMaxClients; i++) {
    CampaignPlayer& p = c.players[i];
    if (!p.active) continue;
    c.server.connect(i, p.name, false, false, p.carried ? p.parms : nullptr);
    set_player_team(c, i);
    std::copy(std::begin(s.clients[i].spawn_parms), std::end(s.clients[i].spawn_parms),
              std::begin(p.entered));
  }
  return true;
}

}  // namespace

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

void campaign_set_coop(Campaign& c, bool on) {
  QcServer* s = c.server ? qc_of(c.server) : nullptr;
  if (s == nullptr) return;
  set(*s, "coop", on ? "1" : "0");
  if (on) set(*s, "deathmatch", "0");
  write_game_kind(*s);
}

void campaign_join(Campaign& c, int slot, const std::string& name) {
  CampaignPlayer& p = c.players[slot];
  p = CampaignPlayer{};
  p.active = true;
  p.name = name;
  if (!c.server) return;
  c.server.connect(slot, name, false, false);
  set_player_team(c, slot);
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
