// The hub's JSON API, defined: the routes, the filters, the records written as JSON.
//
// ge:summary The hub's JSON API defined: routes, filters, records and streams as JSON
// ge:keywords hub api json route filter percent-decode record stream node master
// ge:skill net
// ge:test tests/hub_api_test.cpp
#include "hub/api.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <utility>
#include <vector>

#include "core/json_write.hpp"

namespace ge::hub {

namespace gn = ge::net;
using ge::core::json_arr;
using ge::core::json_bool;
using ge::core::json_num;
using ge::core::json_obj;
using ge::core::json_str;

namespace {

using Members = std::vector<std::pair<std::string, std::string>>;

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string decode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size()) {
      const int a = hex_digit(s[i + 1]), b = hex_digit(s[i + 2]);
      if (a >= 0 && b >= 0) {
        out.push_back(static_cast<char>(a * 16 + b));
        i += 2;
        continue;
      }
    }
    out.push_back(s[i] == '+' ? ' ' : s[i]);
  }
  return out;
}

std::string colour(uint32_t rgb) {
  char out[8];
  std::snprintf(out, sizeof out, "#%06x", rgb & 0xffffffu);
  return out;
}

long long age_s(int64_t now_us, int64_t then_us) {
  return then_us < 0 ? -1 : static_cast<long long>((now_us - then_us) / 1000000);
}

Answer json_answer(int code, std::string body, int max_age_s) {
  Answer a;
  a.code = code;
  a.body = std::move(body);
  a.max_age_s = max_age_s;
  return a;
}

const char* const kPaths[] = {
    "/v1",
    "/v1/servers",
    "/v1/servers/{address}",
    "/v1/servers/{address}/events",
    "/v1/streams",
    "/v1/nodes",
    "/v1/masters",
    "/v1/demos",
    "/v1/demos/{sha256}",
    "/v1/players/{name}",
    "/v1/matches",
    "/v1/matches/{id}",
    "/v1/leaders",
};

std::string paths_json() {
  std::vector<std::string> v;
  v.reserve(std::size(kPaths));
  for (const char* p : kPaths) v.push_back(json_str(p));
  return json_arr(v);
}

Answer not_found(std::string_view what) {
  return json_answer(404,
                     json_obj({{"error", json_str(what)},
                               {"next", json_str("GET /v1 lists every path this hub answers")},
                               {"paths", paths_json()}}),
                     0);
}

int kinds_of(const std::string& text, uint8_t fallback) {
  if (text.empty()) return fallback;
  if (text == "all") return gn::kAllKinds;
  uint8_t kinds = 0;
  size_t at = 0;
  while (at <= text.size()) {
    const size_t comma = text.find(',', at);
    const std::string part =
        text.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    gn::NodeKind k;
    if (!gn::node_kind_of(part, k)) return -1;
    kinds = static_cast<uint8_t>(kinds | gn::kind_bit(k));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return kinds;
}

int cache_s(const Poller& p) { return static_cast<int>(p.config.busy_us / 1000000); }

}  // namespace

std::string target_path(std::string_view target) {
  const size_t q = target.find('?');
  return decode(target.substr(0, q));
}

std::string query_param(std::string_view target, std::string_view key) {
  const size_t q = target.find('?');
  if (q == std::string_view::npos) return "";
  std::string_view rest = target.substr(q + 1);
  while (!rest.empty()) {
    const size_t amp = rest.find('&');
    const std::string_view pair = rest.substr(0, amp);
    const size_t eq = pair.find('=');
    if (decode(pair.substr(0, eq)) == key) {
      return eq == std::string_view::npos ? "" : decode(pair.substr(eq + 1));
    }
    if (amp == std::string_view::npos) break;
    rest = rest.substr(amp + 1);
  }
  return "";
}

std::string node_json(const HubSpec& spec, const HubNode& n, int64_t now_us) {
  Members m = {
      {"address", json_str(gn::to_string(n.address))},
      {"kind", json_str(gn::node_kind_name(n.kind))},
      {"version", json_num(n.version)},
      {"listed", json_bool(n.listed)},
      {"answered", json_bool(n.have)},
      {"age_s", json_num(age_s(now_us, n.heard_us))},
      {"changes", json_num(static_cast<long long>(n.changes))},
  };
  if (!n.have) return json_obj(m);
  const gn::Status& s = n.status;
  int bots = 0, spectators = 0, viewers = 0;
  std::vector<std::string> players;
  players.reserve(s.players.size());
  for (const gn::StatusPlayer& p : s.players) {
    bots += (p.flags & gn::kPlayerBot) ? 1 : 0;
    spectators += (p.flags & gn::kPlayerSpectator) ? 1 : 0;
    viewers += (p.flags & gn::kPlayerViewer) ? 1 : 0;
    players.push_back(json_obj({{"name", json_str(p.name)},
                                {"team", json_str(p.team)},
                                {spec.score, json_num(p.score)},
                                {"ping_ms", json_num(p.ping_ms)},
                                {"minutes", json_num(p.minutes)},
                                {"bot", json_bool(p.flags & gn::kPlayerBot)},
                                {"spectator", json_bool(p.flags & gn::kPlayerSpectator)},
                                {"viewer", json_bool(p.flags & gn::kPlayerViewer)},
                                {"colour", json_str(colour(p.colour))}}));
  }
  std::vector<std::string> teams;
  teams.reserve(s.teams.size());
  for (const gn::StatusTeam& t : s.teams) {
    teams.push_back(json_obj({{"name", json_str(t.name)},
                              {spec.score, json_num(t.score)},
                              {"colour", json_str(colour(t.colour))}}));
  }
  std::vector<std::string> streams;
  streams.reserve(s.streams.size());
  for (const gn::StatusStream& st : s.streams) {
    streams.push_back(json_obj({{"node", json_str(gn::to_string(st.node))},
                                {"stream", json_num(st.stream)},
                                {"delay_s", json_num(st.delay_s)},
                                {"viewers", json_num(st.viewers)},
                                {"locked", json_bool(st.flags & gn::kStreamLocked)}}));
  }
  Members keys;
  keys.reserve(s.keys.size());
  for (const auto& [k, v] : s.keys) keys.emplace_back(k, json_str(v));
  m.insert(m.end(), {
                        {"name", json_str(s.name)},
                        {"map", json_str(s.map)},
                        {"mode", json_str(s.mode)},
                        {"phase", json_str(gn::status_phase_name(s.phase))},
                        {"time_left_s", json_num(s.time_left_s)},
                        {"humans", json_num(humans(s))},
                        {"bots", json_num(bots)},
                        {"spectators", json_num(spectators)},
                        {"viewers", json_num(viewers)},
                        {"max_players", json_num(s.max_players)},
                        {"max_spectators", json_num(s.max_spectators)},
                        {"country", json_str(s.country)},
                        {"locked", json_bool(s.locked)},
                        {"truncated", json_bool(s.truncated)},
                        {"interest", json_num(interest(spec, n, now_us))},
                        {"players", json_arr(players)},
                        {"teams", json_arr(teams)},
                        {"streams", json_arr(streams)},
                        {"keys", json_obj(keys)},
                    });
  return json_obj(m);
}

Answer api_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us) {
  std::string path = target_path(target);
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  const std::string game = json_str(spec.game);

  if (path == "/v1" || path == "/") {
    int players = 0;
    for (const HubNode& n : p.nodes) players += n.have ? humans(n.status) : 0;
    return json_answer(
        200,
        json_obj({{"game", game},
                  {"id", json_str(spec.id)},
                  {"protocol", json_num(spec.protocol)},
                  {"servers", json_num(poller_count(p, gn::kind_bit(gn::NodeKind::game)))},
                  {"broadcasts", json_num(poller_count(p, gn::kind_bit(gn::NodeKind::tv)))},
                  {"players", json_num(players)},
                  {"score", json_str(spec.score)},
                  {"modes",
                   [&] {
                     std::vector<std::string> v;
                     v.reserve(spec.modes.size());
                     for (const HubMode& m : spec.modes) v.push_back(json_str(m.name));
                     return json_arr(v);
                   }()},
                  {"paths", paths_json()}}),
        cache_s(p));
  }

  if (path == "/v1/servers") {
    const std::string mode = query_param(target, "mode");
    const int min_players = std::atoi(query_param(target, "min_players").c_str());
    const bool has_players = query_param(target, "has_players") == "1";
    const int kinds = kinds_of(query_param(target, "kind"), gn::kind_bit(gn::NodeKind::game));
    if (kinds < 0) {
      return json_answer(
          400,
          json_obj({{"error", json_str("kind is not one of game, tv, forward, all")},
                    {"next", json_str("pass kind=game, kind=tv,forward or kind=all")}}),
          0);
    }
    std::vector<std::pair<int64_t, const HubNode*>> rows;
    for (const HubNode& n : p.nodes) {
      if (!n.have || (gn::kind_bit(n.kind) & kinds) == 0) continue;
      if (!mode.empty() && n.status.mode != mode) continue;
      if (has_players && humans(n.status) == 0) continue;
      int people = 0;
      for (const gn::StatusPlayer& pl : n.status.players) {
        people += (pl.flags & (gn::kPlayerSpectator | gn::kPlayerViewer)) ? 0 : 1;
      }
      if (people < min_players) continue;
      rows.emplace_back(interest(spec, n, now_us), &n);
    }
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
      if (a.first != b.first) return a.first > b.first;
      return gn::address_hash(a.second->address) < gn::address_hash(b.second->address);
    });
    std::vector<std::string> v;
    v.reserve(rows.size());
    for (const auto& r : rows) v.push_back(node_json(spec, *r.second, now_us));
    return json_answer(200,
                       json_obj({{"game", game},
                                 {"count", json_num(static_cast<long long>(v.size()))},
                                 {"servers", json_arr(v)}}),
                       cache_s(p));
  }

  const std::string kServers = "/v1/servers/";
  if (path.rfind(kServers, 0) == 0) {
    std::string rest = path.substr(kServers.size());
    bool events = false;
    const std::string kEvents = "/events";
    if (rest.size() > kEvents.size() &&
        rest.compare(rest.size() - kEvents.size(), kEvents.size(), kEvents) == 0) {
      rest.resize(rest.size() - kEvents.size());
      events = true;
    }
    gn::Address a;
    if (!gn::parse_address(rest, a)) return not_found("'" + rest + "' is not an address");
    const HubNode* n = poller_find(p, a);
    if (!n) return not_found("no node at " + rest + " is known to this hub");
    Answer ans = json_answer(200, node_json(spec, *n, now_us), cache_s(p));
    ans.events = events;
    ans.node = a;
    if (events) ans.type = "text/event-stream";
    return ans;
  }

  if (path == "/v1/streams") {
    std::vector<std::string> v;
    for (const HubNode& n : p.nodes) {
      if (!n.have) continue;
      for (const gn::StatusStream& st : n.status.streams) {
        v.push_back(json_obj({{"node", json_str(gn::to_string(st.node))},
                              {"stream", json_num(st.stream)},
                              {"server", json_str(gn::to_string(n.address))},
                              {"server_name", json_str(n.status.name)},
                              {"delay_s", json_num(st.delay_s)},
                              {"viewers", json_num(st.viewers)},
                              {"locked", json_bool(st.flags & gn::kStreamLocked)}}));
      }
    }
    return json_answer(200,
                       json_obj({{"game", game},
                                 {"count", json_num(static_cast<long long>(v.size()))},
                                 {"streams", json_arr(v)}}),
                       cache_s(p));
  }

  if (path == "/v1/nodes") {
    const int kinds = kinds_of(query_param(target, "kind"), gn::kAllKinds);
    if (kinds < 0) return not_found("kind is not one of game, tv, forward, all");
    std::vector<std::string> v;
    for (const HubNode& n : p.nodes) {
      if ((gn::kind_bit(n.kind) & kinds) == 0) continue;
      v.push_back(json_obj({{"address", json_str(gn::to_string(n.address))},
                            {"kind", json_str(gn::node_kind_name(n.kind))},
                            {"version", json_num(n.version)},
                            {"listed", json_bool(n.listed)},
                            {"answered", json_bool(n.have)},
                            {"name", json_str(n.have ? n.status.name : "")},
                            {"age_s", json_num(age_s(now_us, n.heard_us))}}));
    }
    return json_answer(200,
                       json_obj({{"game", game},
                                 {"count", json_num(static_cast<long long>(v.size()))},
                                 {"nodes", json_arr(v)}}),
                       cache_s(p));
  }

  if (path == "/v1/masters") {
    std::vector<std::string> v;
    v.reserve(p.config.masters.size());
    for (const gn::Address& m : p.config.masters) {
      v.push_back(json_obj({{"address", json_str(gn::to_string(m))},
                            {"list_age_s", json_num(age_s(now_us, p.listed_us))}}));
    }
    return json_answer(200,
                       json_obj({{"game", game},
                                 {"listed", json_num(static_cast<long long>(p.nodes.size()))},
                                 {"masters", json_arr(v)}}),
                       cache_s(p));
  }

  return not_found("no such path: " + path);
}

}  // namespace ge::hub
