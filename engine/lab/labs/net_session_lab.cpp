// LAB `net-session`: a dedicated host's lobby filled over the simulated network, once under each
// condition preset, and put through a session's whole life on a script: every client joins, asks
// for a team and says it is ready; each sends a line to everyone, one to its team and one to the
// next player alone, and one floods the chat; the leader kicks the last player, whose return is
// refused; another's link is cut for six seconds and it comes back to its seat by its token; the
// leader sets the game's settings, starts, and returns everyone to the lobby. It answers whether
// the session keeps its word on every link (every chat copy delivered, every roster the host's,
// the kick and the ban held, the seat kept) and what the lobby looks like from each preset: how
// long it took to fill and the pings everyone's player list shows. The seats are charted over the
// run and the pings drawn as boxes. Everything runs on a virtual clock off one seed, so the lab
// is a determinism test too.
//
// ge:summary Lab: a 32-player lobby over each preset: joins, teams, chat, kick, reconnect, start
// ge:keywords lab net session lobby team chat kick ban reconnect token leader start return ping
// ge:keywords player-list roster preset dedicated-server multiplayer
// ge:skill net labs
// ge:test tests/net_session_test.cpp
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/session.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;

constexpr int64_t kTick = 10000;  // every node updates at 100 Hz
constexpr int kTeams = 4;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

// FNV-1a, a value's eight bytes at a time or a text's.
uint64_t fnv(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) h = (h ^ ((v >> (8 * i)) & 0xff)) * 0x100000001b3ull;
  return h;
}
uint64_t fnv_text(uint64_t h, const char* text, size_t n) {
  for (size_t i = 0; i < n; i++) h = (h ^ static_cast<uint8_t>(text[i])) * 0x100000001b3ull;
  return h;
}

uint64_t fold(uint64_t h, const gn::SessionEvent& e, const std::vector<uint8_t>& bytes) {
  const int fields[6] = {static_cast<int>(e.type),
                         e.player,
                         e.other,
                         static_cast<int>(e.scope),
                         static_cast<int>(e.why),
                         static_cast<int>(e.ask)};
  for (const int f : fields) h = fnv(h, static_cast<uint64_t>(static_cast<int64_t>(f)));
  if (e.type == gn::SessionEventType::chat) h = fnv_text(h, gn::session_text(bytes, e), e.size);
  return h;
}

gn::NodeConfig node_config(bool host, uint64_t seed) {
  gn::NodeConfig c;
  c.protocol = gn::protocol_id("net-session", 1);
  c.accept = host;
  c.max_peers = host ? gn::kMaxPeers : 1;
  c.channels.count = 1;
  c.channels.delivery[0] = gn::Delivery::reliable;
  c.timeout_us = 2000000;
  c.seed = seed;
  return c;
}

struct Client {
  gn::Transport t;
  gn::Node n;
  gn::SessionClient s;
  uint64_t hash = 0xcbf29ce484222325ull;
  int chats = 0, refused_join_banned = 0, phase_events = 0;
};

// One preset's session, its numbers and its series.
struct Outcome {
  int clients = 0, peak = 0;
  double fill_ms = -1;
  uint64_t lines = 0, refused = 0, copies = 0, received = 0;
  int roster_bad = 0;
  bool kick_ok = false, ban_ok = false, back_ok = false;
  int phases_seen = 0, seated_end = 0;
  std::vector<double> pings;
  std::vector<double> t_s, seated;
  uint64_t hash = 0;
  gn::SessionStats stats;
};

Outcome play(const gn::LinkConditions& link, int n, uint32_t seed) {
  Outcome o;
  o.clients = n;
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport host_t;
  gn::open_sim_transport(host_t, net, gn::sim_address(0, 1));
  auto host_n = std::make_unique<gn::Node>();
  gn::node_init(*host_n, node_config(true, uint64_t{seed} * 7 + 1), host_t);
  auto host = std::make_unique<gn::SessionHost>();
  gn::SessionConfig sc;
  sc.max_players = n;
  sc.team_count = kTeams;
  sc.reconnect_us = 20000000;
  sc.seed = uint64_t{seed} * 13 + 5;
  gn::session_host_init(*host, sc, *host_n, 0);
  std::vector<std::unique_ptr<Client>> cs;
  for (int i = 0; i < n; i++) {
    auto c = std::make_unique<Client>();
    gn::open_sim_transport(c->t, net, gn::sim_address(static_cast<uint32_t>(i + 1), 1));
    gn::node_init(c->n, node_config(false, uint64_t{seed} * 1000 + static_cast<uint64_t>(i)), c->t);
    gn::session_client_init(c->s, c->n, 0);
    c->s.reconnect_us = sc.reconnect_us;
    cs.push_back(std::move(c));
  }
  const int kicked = n - 1, cut = n - 2, spammer = 1;
  int cut_seat = gn::kNoPlayer;
  uint64_t host_hash = 0xcbf29ce484222325ull;
  const gn::Address host_at = gn::sim_address(0, 1);
  // The leader is whoever was seated first, which the links decide: ask its own mirror.
  const auto leader = [&]() -> gn::SessionClient& {
    for (auto& c : cs) {
      if (c->s.me != gn::kNoPlayer && c->s.players[c->s.me].leader) return c->s;
    }
    return cs[0]->s;
  };
  int64_t now = 0;
  const int64_t end = 22000000;
  for (int64_t step = 0; now < end; step++) {
    now += kTick;
    const int64_t ms = now / 1000;
    // The script, acted at the top of the tick.
    for (int i = 0; i < n; i++) {
      if (ms == 50 * i + 10) {
        char name[16];
        std::snprintf(name, sizeof name, "player %02d", i);
        gn::session_client_join(cs[i]->s, host_at, name, 0, now);
      }
    }
    if (ms == 4000) {
      for (auto& c : cs) gn::session_client_team(c->s, c->s.me % kTeams);
    }
    if (ms == 4500 || ms == 15000) {
      for (auto& c : cs) gn::session_client_ready(c->s, true);
    }
    if (ms == 5000) {
      for (int i = 0; i < n; i++) {
        gn::SessionClient& s = cs[i]->s;
        char line[48];
        const int k = std::snprintf(line, sizeof line, "hello from %s", s.name);
        gn::session_client_chat(s, gn::ChatScope::all, gn::kNoPlayer, line, static_cast<size_t>(k));
        gn::session_client_chat(s, gn::ChatScope::team, gn::kNoPlayer, "to my team", 10);
        gn::session_client_chat(s, gn::ChatScope::direct, cs[(i + 1) % n]->s.me, "just you", 8);
      }
    }
    if (ms == 5200) {
      for (int k = 0; k < 12; k++)
        gn::session_client_chat(cs[spammer]->s, gn::ChatScope::all, gn::kNoPlayer, "spam", 4);
    }
    if (ms == 6000) gn::session_client_kick(leader(), cs[kicked]->s.me);
    if (ms == 7000) {
      cut_seat = cs[cut]->s.me;
      gn::LinkConditions dead;
      dead.loss = 1;
      gn::sim_set_path(net, 0, static_cast<uint32_t>(cut + 1), dead);
    }
    if (ms == 9500) gn::session_client_join(cs[kicked]->s, host_at, "again", 0, now);
    if (ms == 13000) gn::sim_set_path(net, 0, static_cast<uint32_t>(cut + 1), link);
    if (ms == 15500) {
      std::vector<uint8_t> settings(200);
      for (size_t k = 0; k < settings.size(); k++) settings[k] = static_cast<uint8_t>(k * 7);
      gn::session_client_settings(leader(), settings.data(), settings.size());
    }
    if (ms == 16000) gn::session_client_start(leader());
    if (ms == 19000) gn::session_client_return(leader());

    gn::sim_advance(net, now);
    gn::node_update(*host_n, now);
    gn::session_host_update(*host, now);
    for (const gn::SessionEvent& e : host->events)
      host_hash = fold(host_hash, e, host->event_bytes);
    for (auto& c : cs) {
      gn::node_update(c->n, now);
      gn::session_client_update(c->s, now);
      for (const gn::SessionEvent& e : c->s.events) {
        c->hash = fold(c->hash, e, c->s.event_bytes);
        if (e.type == gn::SessionEventType::chat) c->chats++;
        if (e.type == gn::SessionEventType::phase) c->phase_events++;
        if (e.type == gn::SessionEventType::refused && e.ask == gn::SessionAsk::join &&
            e.why == gn::SessionWhy::banned)
          c->refused_join_banned++;
      }
    }
    gn::node_flush(*host_n, now);
    for (auto& c : cs) gn::node_flush(c->n, now);
    const int seated = gn::session_player_count(*host, true);
    o.peak = std::max(o.peak, seated);
    if (o.fill_ms < 0 && seated == n) o.fill_ms = static_cast<double>(now) / 1000.0;
    if (step % 10 == 0) {
      o.t_s.push_back(static_cast<double>(now) / 1e6);
      o.seated.push_back(seated);
    }
  }
  o.stats = host->stats;
  o.lines = host->stats.chat_relayed;
  o.refused = host->stats.chat_refused;
  o.copies = host->stats.chat_copies;
  for (auto& c : cs) o.received += static_cast<uint64_t>(c->chats);
  for (int i = 0; i < n; i++) {
    const gn::SessionClient& s = cs[i]->s;
    if (s.state != gn::ClientState::seated) continue;
    o.seated_end++;
    o.phases_seen += cs[i]->phase_events >= 2;
    for (int seat = 0; seat < gn::kMaxPlayers; seat++) {
      const gn::Player& a = s.players[seat];
      const gn::Player& b = host->players[seat];
      if (a.used != b.used || a.present != b.present || a.team != b.team || a.ready != b.ready ||
          a.leader != b.leader || std::string(a.name) != b.name) {
        o.roster_bad++;
        break;
      }
    }
    if (s.me != gn::kNoPlayer) o.pings.push_back(s.players[s.me].ping_ms);
  }
  const gn::SessionClient& k = cs[kicked]->s;
  o.kick_ok = k.state == gn::ClientState::lost && host->stats.kicks == 1;
  o.ban_ok = cs[kicked]->refused_join_banned == 1;
  o.back_ok = cs[cut]->s.state == gn::ClientState::seated && cs[cut]->s.me == cut_seat &&
              host->stats.rejoins >= 1;
  uint64_t h = host_hash;
  for (auto& c : cs) h = fnv(h, c->hash);
  o.hash = h;
  return o;
}

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void run(gl::Lab& lab) {
  const int n = static_cast<int>(lab.arg_num("clients"));
  if (n < 4 || n > gn::kMaxPlayers) {
    lab.fail("--clients=" + lab.arg("clients") + " is out of range",
             "Pass --clients=4 to 32 (the script kicks one player and cuts another); skill: net.");
    return;
  }
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  gl::LabTable table{"a " + std::to_string(n) + "-player lobby on a dedicated host, each preset",
                     {"preset", "seated", "fill ms", "lines", "refused", "copies", "received",
                      "rosters", "kick", "ban", "back", "phases", "ping p50", "ping max", "log"},
                     {}};
  gl::LineChart seated_chart{
      {"PLAYERS SEATED AND PRESENT, EACH PRESET", "the host's count", "t (s)", "players"},
      {},
      {{6, "kick"}, {7, "cut"}, {13, "restored"}, {16, "start"}, {19, "return"}},
      {},
      {},
      false,
      false,
      false};
  std::vector<gl::Sample> pings;
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    const Outcome o = play(link, n, seed);
    const gl::Summary p = gl::summarize(o.pings);
    char hash[20];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(o.hash));
    table.rows.push_back(
        {name, std::to_string(o.peak) + "/" + std::to_string(n),
         o.fill_ms < 0 ? "-" : fmt(o.fill_ms, 0), std::to_string(o.lines),
         std::to_string(o.refused), std::to_string(o.copies), std::to_string(o.received),
         std::to_string(o.seated_end - o.roster_bad) + "/" + std::to_string(o.seated_end),
         o.kick_ok ? "ok" : "FAIL", o.ban_ok ? "ok" : "FAIL", o.back_ok ? "ok" : "FAIL",
         std::to_string(o.phases_seen) + "/" + std::to_string(o.seated_end), fmt(p.p50, 0),
         fmt(p.max, 0), hash});
    const std::string k = name + "-";
    lab.metric(k + "fill-ms", o.fill_ms, "ms");
    lab.metric(k + "chat-copies", static_cast<double>(o.copies), "count");
    lab.metric(k + "chat-received", static_cast<double>(o.received), "count");
    lab.metric(k + "chat-refused", static_cast<double>(o.refused), "count");
    lab.metric(k + "rosters-wrong", o.roster_bad, "count");
    lab.metric(k + "ping-p50-ms", p.p50, "ms");
    seated_chart.traces.push_back({name, o.t_s, o.seated});
    pings.push_back({name, o.pings});
    if (o.peak != n)
      lab.fail(name + ": only " + std::to_string(o.peak) + " of " + std::to_string(n) +
                   " players were ever seated at once",
               "Read the hello and the seats in engine/net/session_host.cpp, then run "
               "tests/net_session_test.cpp; skill: net.");
    if (o.received != o.copies)
      lab.fail(name + ": " + std::to_string(o.received) + " chat copies arrived of " +
                   std::to_string(o.copies) + " the host sent",
               "Read the chat relay in engine/net/session_host.cpp and the reliable channel in "
               "engine/net/connection.cpp; skill: net.");
    if (o.roster_bad)
      lab.fail(name + ": " + std::to_string(o.roster_bad) +
                   " clients' player lists differ from the host's at the end",
               "Compare the player messages the host sends (session_host.cpp's announce) with "
               "on_player in session_client.cpp; skill: net.");
    if (!o.kick_ok || !o.ban_ok)
      lab.fail(name + ": the kick or its ban did not hold",
               "Read session_host_kick and the ban checks in engine/net/session_host.cpp, then run "
               "tests/net_session_test.cpp; skill: net.");
    if (!o.back_ok)
      lab.fail(name + ": the cut player did not come back to its seat",
               "Read the token's return in on_hello (engine/net/session_host.cpp) and the retry "
               "in session_client_update; skill: net.");
    if (o.phases_seen != o.seated_end)
      lab.fail(name + ": " + std::to_string(o.seated_end - o.phases_seen) +
                   " seated clients missed the start or the return",
               "Read session_host_start and session_host_return in engine/net/session_host.cpp; "
               "skill: net.");
  }
  lab.table(std::move(table));
  lab.note(
      "seated: the most players present at once; fill: when the last joined (the joins are 50 ms "
      "apart). lines: chat lines the host relayed, refused: lines it turned away (the flood, past "
      "the bucket); copies: what it sent, received: what the clients got, which must match. "
      "rosters: seated clients whose player list is the host's at the end. kick: the kicked "
      "client was told and lost; ban: its return was refused; back: the client cut for six "
      "seconds came back to the same seat by its token. phases: clients that saw the start and "
      "the return. ping: each seated client's own round trip as its player list shows it. log: a "
      "hash of every event every end saw.");
  const auto shoot = [&](const std::string& id, const gl::Chart& chart, const std::string& caption,
                         const std::string& look_for) {
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot({id,
              chart.title,
              caption,
              look_for,
              {{"presets", lab.arg("presets")},
               {"clients", lab.arg("clients")},
               {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot));
  };
  if (!seated_chart.traces.empty()) {
    shoot("seated", gl::line_chart(seated_chart),
          "Players seated and present over the run, one line a preset, with the script's moments "
          "marked.",
          "every line climbs to the full lobby within a few seconds, drops by one at the kick and "
          "one more when the cut player goes away, and rises by one when it is restored.");
    gl::ChartFrame frame{"PINGS ON THE PLAYER LIST, EACH PRESET", "every seated client's own",
                         "ping (ms)", ""};
    shoot("pings", gl::box_chart(frame, pings),
          "The round trip each client's player list shows for itself at the end, one box a "
          "preset: p25 to p75, the median, whiskers p5 to p95.",
          "each box sits near its preset's round trip (twice the latency, plus a tick each way).");
  }
}

const gl::LabSpec kSpec{
    "net-session",
    gl::LabKind::sim,
    "engine",
    "engine/net (session_host, session_client)",
    "net",
    "On each network condition, does a full lobby keep its word: everyone seated, every chat copy "
    "delivered, every player list the host's, the kick and its ban held, a dropped player back in "
    "its seat?",
    {
        {"presets", "lan,wifi,bad", "condition presets, comma-separated"},
        {"clients", "32", "players joining the lobby (4 to 32)"},
        {"seed", "1", "the network's, the nodes' and the tokens' stream"},
    },
    "--clients=8 --presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
