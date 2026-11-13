// Coop over the classic protocol (classic_coop.hpp).
//
// ge:summary Coop over the network: the endpoint on the campaign, seats carried by the campaign
// ge:keywords coop campaign-door classic_coop coop-join
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_coop_test.cpp
#include "shooters_edge/net/classic_coop.hpp"

#include <memory>
#include <string>

#include "shooters_edge/net/classic_login.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/campaign.hpp"

namespace shooters_edge::net {

namespace {

struct Door {
  sv::Campaign* c = nullptr;
  std::unique_ptr<ClassicLogin> login;  // the logins, when asked (`logins`)
  ClassicServer s;
};

Door& door(void* d) { return *static_cast<Door*>(d); }

// A client's `begin`: he enters the campaign, unless it carried him onto this map already.
void seat_client(void* ctx, int slot, const std::string& name, bool /*spectator*/) {
  sv::Campaign& c = *door(ctx).c;
  if (c.server && c.server->clients[slot].active) return;
  sv::campaign_join(c, slot, name);
}

// A client gone: the campaign lets him go and carries him no more.
void unseat_client(void* ctx, int slot) {
  sv::Campaign& c = *door(ctx).c;
  c.players[slot].active = false;
  if (c.server && c.server->clients[slot].active) c.server.drop(slot);
}

void* open(sv::Campaign& c, gn::Transport& t, const std::string& hostname, uint32_t seed, int most,
           void (*tap)(void* ctx, const sv::RulesCommon& game), void* ctx, std::string& error) {
  if (!c.server) {
    error = "No game is running. Type \"coopgame\" to start a coop game.";
    return nullptr;
  }
  auto d = std::make_unique<Door>();
  d->c = &c;
  ServerConfig cfg;
  cfg.hostname = hostname;
  cfg.seed = seed;
  cfg.most_players = most;
  if (!classic_start(d->s, c.server, t, cfg, error)) return nullptr;
  d->s.local_tap = tap;
  d->s.local_ctx = ctx;
  d->s.seat = seat_client;
  d->s.unseat = unseat_client;
  d->s.seat_ctx = d.get();
  return d.release();
}

// A client's line goes to the login first: a login line never reaches the campaign or chat.
bool coop_words(void* ctx, int slot, const std::string& line) {
  Door& o = door(ctx);
  return o.login && classic_login_line(*o.login, slot, line);
}

bool logins(void* d, const ge::server::ServerConfig& c, std::string& error) {
  Door& o = door(d);
  o.login = std::make_unique<ClassicLogin>();
  if (!classic_login_open(*o.login, o.s, c, error)) {
    o.login.reset();
    return false;
  }
  o.s.game_command = coop_words;
  o.s.game_ctx = &o;
  return true;
}

bool seat(void* d, int slot, const std::string& name, bool bot, std::string& error) {
  return classic_add_local(door(d).s, slot, name, error, bot);
}

void unseat(void* d, int slot) {
  ClassicServer& s = door(d).s;
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].local) return;
  s.clients[slot].rules_active = false;  // the window let the campaign's seat go already
  classic_drop(s, slot, "left the game");
}

void unbind(void* d) { classic_unbind(door(d).s); }

bool rebind(void* d, std::string& error) {
  Door& o = door(d);
  return classic_change(o.s, o.c->server, error);
}

bool frame(void* d, const sh::Packet* local, int count, int64_t now_us, std::string& error) {
  Door& o = door(d);
  o.s.local_moves.assign(local, local + count);
  classic_frame(o.s, now_us);
  if (o.login) classic_login_frame(*o.login);
  sv::Campaign& c = *o.c;
  const sv::RulesCommon& now = *c.server;
  if (now.pending_map.empty()) return true;
  const std::string map = now.pending_map;
  const sv::MapChange change = now.pending_change;
  classic_unbind(o.s);
  if (!sv::campaign_change(c, change, map, error)) return false;
  return classic_change(o.s, c.server, error);
}

int joined(const void* d) {
  const ClassicServer& s = static_cast<const Door*>(d)->s;
  int n = 0;
  for (const ClassicClient& c : s.clients)
    if (!c.local && c.state == ConnState::spawned) n++;
  return n;
}

void close(void* d) {
  std::unique_ptr<Door> o(static_cast<Door*>(d));
  ClassicServer& s = o->s;
  s.seat = nullptr;
  s.unseat = nullptr;
  s.game_command = nullptr;
  s.name_rule = nullptr;
  if (s.game == nullptr) return;  // a next map that failed: nothing is left to tell them
  for (int i = 0; i < sh::kMaxClients; i++) {  // the campaign lets its seats go itself
    if (s.clients[i].state == ConnState::free || s.clients[i].local) continue;
    s.clients[i].rules_active = false;
    classic_drop(s, i, "the game ended");
  }
  classic_unbind(s);
}

}  // namespace

const client::CampaignDoor& classic_coop() {
  static const client::CampaignDoor d{open,   seat,   unseat, frame, unbind,
                                      rebind, logins, joined, close};
  return d;
}

}  // namespace shooters_edge::net
