// The game CLI's servers (commands.hpp): the browser's list (net/server_list.hpp on a socket of
// the CLI's own, the wall clock driving it until every reply is in or `--wait` passes), one
// server's info and players, and the remote console's request and reply. The game's protocol is
// its Spec's (`query`, `rcon`, `read_rcon`); where to look comes from the player's own settings
// (`sb_lan`, `sb_masters`, `sb_servers`, when the game has them) and the flags.
//
// ge:summary The game CLI's servers: the browser's list, one server's players, the remote console
// ge:keywords gamecli servers server-browser server-info players roster rcon remote-console
// ge:keywords status-query masters lan sb_lan sb_masters sb_servers rcon_password ping
// ge:skill agent-interface net
// ge:test tests/gamecli_cli_test.cpp
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "core/json_write.hpp"
#include "forward/routes.hpp"
#include "gamecli/commands.hpp"
#include "net/transport.hpp"
#include "net/udp_socket.hpp"

namespace ge::gamecli {

namespace fs = std::filesystem;
namespace gn = ge::net;
using core::json_arr;
using core::json_bool;
using core::json_num;
using core::json_obj;
using core::json_str;

namespace {

using Clock = std::chrono::steady_clock;

int64_t since_us(Clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
}

std::vector<std::string> split(const std::string& text, const char* seps) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = text.find_first_of(seps, at);
    const std::string w = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
    if (!w.empty()) out.push_back(w);
    if (end == std::string::npos) break;
    at = end + 1;
  }
  return out;
}

// A setting from the player's config.cfg (`seta name "value"`), read as text: the browser's own
// sources. "" when the config or the setting is not there.
std::string player_setting(Ctx& ctx, const std::string& name, const std::string& fallback) {
  if (!try_places(ctx)) return fallback;
  std::ifstream f(fs::path(ctx.places.settings) / "config.cfg");
  std::string line;
  while (f && std::getline(f, line)) {
    std::istringstream words(line);
    std::string a, b;
    words >> a >> b;
    if ((a == "seta" || a == "set") && b == name) {
      std::string rest;
      std::getline(words, rest);
      const size_t q0 = rest.find('"'), q1 = rest.rfind('"');
      if (q0 != std::string::npos && q1 > q0) return rest.substr(q0 + 1, q1 - q0 - 1);
      return split(rest, " \t").empty() ? "" : split(rest, " \t")[0];
    }
  }
  return fallback;
}

double wait_s(const Ctx& ctx, double fallback) {
  const std::string w = ctx.flag("wait");
  if (w.empty()) return fallback;
  const double v = std::strtod(w.c_str(), nullptr);
  return v > 0 ? v : fallback;
}

bool resolve(const std::string& text, uint16_t port, gn::Address& out, std::string& error) {
  if (gn::parse_address(text, out)) return true;
  return gn::resolve_ipv4(text, port, out, &error);
}

std::string from_word(uint8_t from) {
  if ((from & gn::kServerFromNamed) != 0) return "named";
  if ((from & gn::kServerFromLan) != 0) return "lan";
  return "master";
}

std::string roster_json(const gn::ServerInfo& info) {
  std::vector<std::string> players;
  players.reserve(info.roster.size());
  for (const gn::ServerPlayer& p : info.roster)
    players.push_back(json_obj({{"name", json_str(p.name)},
                                {"team", json_str(p.team)},
                                {"frags", json_num(p.frags)},
                                {"ping_ms", json_num(p.ping_ms)},
                                {"spectator", json_bool(p.spectator)}}));
  return json_arr(players);
}

std::string entry_json(const gn::ServerEntry& e, bool with_roster) {
  std::vector<std::pair<std::string, std::string>> m = {
      {"address", json_str(gn::to_string(e.address))},
      {"name", json_str(e.info.name)},
      {"map", json_str(e.info.map)},
      {"mode", json_str(e.info.mode)},
      {"game", json_str(e.info.game)},
      {"state", json_str(e.info.state)},
      {"players", json_num(e.info.players)},
      {"max_players", json_num(e.info.max_players)},
      {"spectators", json_num(e.info.spectators)},
      {"ping_ms", json_num(e.ping_ms)},
      {"locked", json_bool(e.info.locked)},
      {"from", json_str(from_word(e.from))}};
  if (with_roster) {
    m.push_back({"roster", roster_json(e.info)});
    std::vector<std::pair<std::string, std::string>> keys;
    keys.reserve(e.info.keys.size());
    for (const auto& [k, v] : e.info.keys) keys.push_back({k, json_str(v)});
    m.push_back({"keys", json_obj(keys)});
  }
  return json_obj(m);
}

std::string entry_text(const gn::ServerEntry& e) {
  return gn::to_string(e.address) + "  " + (e.info.name.empty() ? "(no name)" : e.info.name) +
         "  " + e.info.map + "  " + e.info.mode + "  " + std::to_string(e.info.players) + "/" +
         std::to_string(e.info.max_players) + "  " + std::to_string(e.ping_ms) + " ms" +
         (e.info.locked ? "  password" : "") + "\n";
}

// The list refreshed on `config` until every reply is in or `wait` passes; false with a refusal.
bool browse(Ctx& ctx, const gn::ServerListConfig& config, double wait, gn::ServerList& list,
            gn::Transport& t) {
  if (ctx.spec.query.status == nullptr || ctx.spec.query.read_status == nullptr) {
    refuse(ctx, 1, std::string(ctx.spec.title) + " names no server protocol",
           std::string(ctx.spec.cli) + " help");
    return false;
  }
  std::string error;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error)) {
    refuse(ctx, 1, "the browser's socket did not open: " + error,
           "check that this machine has a network");
    return false;
  }
  gn::server_list_init(list, config, ctx.spec.query, t);
  const Clock::time_point t0 = Clock::now();
  gn::server_list_refresh(list, 0);
  const int64_t until = static_cast<int64_t>(wait * 1e6);
  for (int64_t now = since_us(t0); now < until; now = since_us(t0)) {
    gn::server_list_update(list, now);
    if (gn::server_list_done(list, now)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return true;
}

// One server asked; its entry, or null with a refusal.
const gn::ServerEntry* ask_one(Ctx& ctx, gn::ServerList& list, gn::Transport& t, const char* usage,
                               int& rc) {
  rc = 1;
  if (ctx.words.size() != 1) {
    rc = refuse(ctx, 2, std::string(usage) + " takes one address, host[:port]",
                std::string(ctx.spec.cli) + " " + usage +
                    " 192.168.1.20:" + std::to_string(ctx.spec.default_port));
    return nullptr;
  }
  gn::ServerListConfig config;
  gn::Address a;
  std::string error;
  if (!resolve(ctx.words[0], ctx.spec.default_port, a, error)) {
    refuse(ctx, 1, error,
           "check the address, or find it with " + std::string(ctx.spec.cli) + " servers");
    return nullptr;
  }
  config.servers.push_back(a);
  if (!browse(ctx, config, wait_s(ctx, 2), list, t)) return nullptr;
  for (const gn::ServerEntry& e : list.entries)
    if (e.address == a && e.answered) return &e;
  refuse(ctx, 1, gn::to_string(a) + " did not answer in " + ctx.flag("wait", "2") + " s",
         "check the address and the port; a server behind a router needs its port forwarded");
  return nullptr;
}

}  // namespace

namespace {

// Where a refresh looks: the LAN (the player's sb_lan or --lan), the masters and the servers named
// (the player's settings, the flags and the places). Names that did not resolve go in
// `unresolved`; true when the LAN is asked.
bool servers_config(Ctx& ctx, gn::ServerListConfig& config, std::vector<std::string>& unresolved) {
  const bool lan =
      ctx.has("lan") ? ctx.flag("lan") != "0" : player_setting(ctx, "sb_lan", "1") != "0";
  if (lan)
    for (uint32_t p = ctx.spec.lan_first; p <= ctx.spec.lan_last; p++)
      config.lan.push_back(gn::ipv4(255, 255, 255, 255, static_cast<uint16_t>(p)));
  const auto add = [&](const std::string& text, uint16_t port, std::vector<gn::Address>& into) {
    gn::Address a;
    std::string error;
    if (!resolve(text, port, a, error)) {
      unresolved.push_back(text);
      return;
    }
    for (const gn::Address& b : into)
      if (b == a) return;
    into.push_back(a);
  };
  std::vector<std::string> masters = split(player_setting(ctx, "sb_masters", ""), " ,");
  for (const std::string& m : split(ctx.flag("master"), ",")) masters.push_back(m);
  if (try_places(ctx))
    for (const std::string& m : ctx.places.masters) masters.push_back(m);
  for (const std::string& m : masters) add(m, ctx.spec.master_port, config.masters);
  std::vector<std::string> named = split(player_setting(ctx, "sb_servers", ""), " ,");
  for (const std::string& s : split(ctx.flag("server"), ",")) named.push_back(s);
  for (const std::string& s : named) add(s, ctx.spec.default_port, config.servers);

  return lan;
}

}  // namespace

int cmd_servers(Ctx& ctx) {
  gn::ServerListConfig config;
  std::vector<std::string> unresolved;
  const bool lan = servers_config(ctx, config, unresolved);
  gn::ServerFilter filter;
  filter.name_has = ctx.flag("has");
  filter.mode = ctx.flag("mode");
  filter.hide_empty = ctx.has("not-empty");
  filter.hide_full = ctx.has("not-full");
  gn::ServerSort sort = gn::ServerSort::ping;
  const std::string by = ctx.flag("sort", "ping");
  bool sorted = false;
  for (const gn::ServerSort s : {gn::ServerSort::ping, gn::ServerSort::players,
                                 gn::ServerSort::name, gn::ServerSort::map, gn::ServerSort::mode})
    if (by == gn::server_sort_name(s)) sort = s, sorted = true;
  if (!sorted)
    return refuse(ctx, 2, "--sort=" + by + " is not ping, players, name, map or mode",
                  std::string(ctx.spec.cli) + " help servers");

  gn::ServerList list;
  gn::Transport t;
  if (!browse(ctx, config, wait_s(ctx, 3), list, t)) return 1;
  std::vector<int> rows;
  gn::server_list_view(list, filter, sort, rows);
  std::vector<std::string> out;
  std::string text;
  for (const int i : rows) {
    out.push_back(entry_json(list.entries[static_cast<size_t>(i)], false));
    text += entry_text(list.entries[static_cast<size_t>(i)]);
  }
  const int known = static_cast<int>(list.entries.size());
  gn::close_transport(t);
  std::string next;
  if (rows.empty())
    next = config.masters.empty()
               ? "none answered on the LAN; name a master (--master=host:port, or settings set "
                 "sb_masters) or a server (--server=host:port)"
               : "none answered; widen the filter or wait longer (--wait=6)";
  else
    next = std::string(ctx.spec.cli) + " players <address> says who is on one; " + ctx.spec.cli +
           " join <address> plays there";
  return answer(ctx,
                json_obj({{"servers", json_arr(out)},
                          {"asked", json_num(known)},
                          {"lan", json_bool(lan)},
                          {"masters", json_num(static_cast<long long>(config.masters.size()))},
                          {"unresolved", core::json_strs(unresolved)}}),
                text,
                std::to_string(rows.size()) + " servers shown of " + std::to_string(known) +
                    " asked" + (unresolved.empty() ? "" : "; did not resolve: " + unresolved[0]),
                next);
}

int cmd_server_info(Ctx& ctx) {
  gn::ServerList list;
  gn::Transport t;
  int rc = 0;
  const gn::ServerEntry* e = ask_one(ctx, list, t, "server info", rc);
  if (e == nullptr) return rc;
  std::string text = entry_text(*e);
  for (const auto& [k, v] : e->info.keys) {
    text += "  ";
    text += k;
    text += " = ";
    text += v;
    text += "\n";
  }
  for (const gn::ServerPlayer& p : e->info.roster)
    text += "  " + p.name + (p.spectator ? " (spectating)" : "  " + std::to_string(p.frags)) +
            (p.team.empty() ? "" : "  " + p.team) + "  " + std::to_string(p.ping_ms) + " ms\n";
  const std::string result = entry_json(*e, true);
  return answer(ctx, result, text, e->info.name + " on " + e->info.map,
                std::string(ctx.spec.cli) + " join " + gn::to_string(e->address));
}

int cmd_players(Ctx& ctx) {
  gn::ServerList list;
  gn::Transport t;
  int rc = 0;
  const gn::ServerEntry* e = ask_one(ctx, list, t, "players", rc);
  if (e == nullptr) return rc;
  std::string text;
  for (const gn::ServerPlayer& p : e->info.roster)
    text += p.name + (p.spectator ? "  (spectating)" : "  " + std::to_string(p.frags) + " frags") +
            (p.team.empty() ? "" : "  team " + p.team) + "  " + std::to_string(p.ping_ms) + " ms\n";
  return answer(ctx,
                json_obj({{"address", json_str(gn::to_string(e->address))},
                          {"name", json_str(e->info.name)},
                          {"map", json_str(e->info.map)},
                          {"players", roster_json(e->info)}}),
                text,
                std::to_string(e->info.roster.size()) + " on " +
                    (e->info.name.empty() ? gn::to_string(e->address) : e->info.name) + " (" +
                    e->info.map + ")");
}

int cmd_rcon(Ctx& ctx) {
  if (ctx.spec.rcon == nullptr || ctx.spec.read_rcon == nullptr)
    return refuse(ctx, 1, std::string(ctx.spec.title) + " has no remote console",
                  std::string(ctx.spec.cli) + " help");
  if (ctx.words.size() < 2)
    return refuse(ctx, 2, "rcon takes an address and the command",
                  std::string(ctx.spec.cli) + " rcon 192.168.1.20 --password=<p> -- status");
  std::string command;
  for (size_t i = 1; i < ctx.words.size(); i++) command += (i > 1 ? " " : "") + ctx.words[i];
  const std::string password =
      ctx.has("password") ? ctx.flag("password") : player_setting(ctx, "rcon_password", "");
  if (password.empty())
    return refuse(ctx, 1, "no remote console password",
                  "pass --password=<the server's>, or settings set rcon_password <it>");
  gn::Address a;
  std::string error;
  if (!resolve(ctx.words[0], ctx.spec.default_port, a, error))
    return refuse(ctx, 1, error, "check the address");
  gn::Transport t;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error))
    return refuse(ctx, 1, "the socket did not open: " + error,
                  "check that this machine has a network");
  uint8_t packet[1400];
  const size_t n = ctx.spec.rcon(password, command, packet, sizeof packet);
  if (n == 0) {
    gn::close_transport(t);
    return refuse(ctx, 2, "the command is too long for one packet", "send a shorter one");
  }
  const Clock::time_point t0 = Clock::now();
  gn::transport_send(t, a, packet, n, 0);
  std::string reply;
  int64_t last = -1;
  const int64_t until = static_cast<int64_t>(wait_s(ctx, 2) * 1e6);
  uint8_t buf[gn::kMaxDatagram];
  for (int64_t now = since_us(t0); now < until; now = since_us(t0)) {
    gn::Address from;
    const int got = gn::transport_receive(t, buf, sizeof buf, from, now);
    if (got > 0 && from == a) {
      std::string text;
      if (ctx.spec.read_rcon(buf, static_cast<size_t>(got), text)) {
        reply += text;
        last = now;
      }
      continue;
    }
    if (last >= 0 && now - last > 300000) break;  // a reply in several packets has ended
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  gn::close_transport(t);
  if (last < 0)
    return refuse(ctx, 1, gn::to_string(a) + " did not answer the remote console",
                  "check the address and port; the server may not offer a remote console");
  return answer(ctx,
                json_obj({{"address", json_str(gn::to_string(a))},
                          {"command", json_str(command)},
                          {"reply", json_str(reply)}}),
                reply, "");
}

int cmd_route(Ctx& ctx) {
  if (ctx.spec.routes.ask == nullptr)
    return refuse(ctx, 1, std::string(ctx.spec.title) + " has no forwarders to route through",
                  std::string(ctx.spec.cli) + " join <address> connects straight");
  if (ctx.words.size() != 1)
    return refuse(
        ctx, 2, "route takes one address, host[:port]",
        std::string(ctx.spec.cli) + " route 192.168.1.20:" + std::to_string(ctx.spec.default_port));
  gn::Address server;
  std::string error;
  if (!resolve(ctx.words[0], ctx.spec.default_port, server, error))
    return refuse(ctx, 1, error,
                  "check the address, or find it with " + std::string(ctx.spec.cli) + " servers");
  gn::ServerListConfig config;
  std::vector<std::string> unresolved;
  servers_config(ctx, config, unresolved);
  bool named = false;
  for (const gn::Address& a : config.servers) named = named || a == server;
  if (!named) config.servers.push_back(server);
  gn::ServerList list;
  gn::Transport t;
  if (!browse(ctx, config, wait_s(ctx, 3), list, t)) return 1;
  forward::RouteFinder rf;
  const Clock::time_point t0 = Clock::now();
  forward::route_finder_start(rf, ctx.spec.routes, t, list, 0);
  const int64_t until = static_cast<int64_t>(wait_s(ctx, 3) * 1e6);
  for (int64_t now = since_us(t0); now < until; now = since_us(t0)) {
    forward::route_finder_update(rf, now);
    if (forward::route_finder_done(rf, now)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  gn::close_transport(t);
  const forward::Route r = forward::best_route(forward::route_finder_table(rf, list), server);
  std::vector<std::string> hops;
  hops.reserve(r.forwarders.size());
  for (const gn::Address& a : r.forwarders) hops.push_back(gn::to_string(a));
  const std::string chain = forward::route_chain(r, server);
  std::string text;
  std::string summary;
  std::string next;
  if (!r.found) {
    summary = gn::to_string(server) + " did not answer, and no forwarder reaches it";
    next = "check the address, or name a master that lists forwarders (--master=host:port)";
  } else if (r.forwarders.empty()) {
    summary = "straight is best: " + std::to_string(r.ping_ms) + " ms";
    text = gn::to_string(server) + "  " + std::to_string(r.ping_ms) + " ms straight\n";
    next = std::string(ctx.spec.cli) + " join " + gn::to_string(server);
  } else {
    summary = "through " + std::to_string(r.forwarders.size()) + " forwarder" +
              (r.forwarders.size() == 1 ? "" : "s") + ": " + std::to_string(r.ping_ms) + " ms" +
              (r.direct_ms >= 0 ? " (straight " + std::to_string(r.direct_ms) + " ms)"
                                : " (no straight answer)");
    for (const std::string& h : hops) text += h + "  forwarder\n";
    text += gn::to_string(server) + "  " + std::to_string(r.ping_ms) + " ms in all\n";
    next = "connect to " + hops[0] + " with the route after it as the game's route setting (" +
           chain.substr(hops[0].size() + 1) + ")";
  }
  return answer(ctx,
                json_obj({{"server", json_str(gn::to_string(server))},
                          {"found", json_bool(r.found)},
                          {"forwarders", core::json_strs(hops)},
                          {"chain", json_str(chain)},
                          {"ping_ms", json_num(r.ping_ms)},
                          {"direct_ms", json_num(r.direct_ms)},
                          {"asked", json_num(static_cast<long long>(rf.asked.size()))},
                          {"answered", json_num(static_cast<long long>(rf.replies))}}),
                text, summary, next);
}

}  // namespace ge::gamecli
