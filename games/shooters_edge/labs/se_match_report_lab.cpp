// LAB `match-report`: WHAT A FINISHED MATCH LEAVES (docs/accounts.md § The match report). Shooter's
// Edge's dedicated server plays bot matches to their time limit, offline, and the core keeps the
// report the game hands it when the map changes (engine/server's `ServerGame::match_report`,
// games/shooters_edge/net/classic_report.hpp): a free-for-all and a team match. It answers whether
// each match reports once, with every player and their frags, deaths, suicides and team kills as
// the rules played them out, the teams' scores summed, how it ended, and whether the record reads
// back as it was written and comes out the same on every run; each report is drawn as the
// scoreboard a hub would show.
// Then the reports go to an account master on the simulated network (account/service.hpp,
// account/matches.hpp), signed by a server the master's owner trusts and by one the owner has not
// listed, the first answer lost so the report goes twice: it answers whether each is taken once,
// counted only from the trusted server, and how an account's totals read whole, by mode, by map and
// at a good ping. The lab plays an account's part with the first bot of each match (a bot logs in
// to nothing); the totals are drawn as the hub's player page would sum them.
//
// ge:summary Lab: the report a bot match leaves on the dedicated server, drawn as its scoreboard
// ge:keywords lab match-report report scoreboard frags deaths suicides team-kills bot-match
// ge:skill net labs
// ge:test tests/shooters_edge_classic_report_test.cpp
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "account/matches.hpp"
#include "account/service.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "net/report.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_game.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;

struct Match {
  const char* label;
  const char* title;
  const char* what;
  const char* expect;
  bool teams;
  int bots;
};

struct Played {
  std::vector<gn::MatchReport> reports;
  std::vector<uint8_t> bytes;  // the last report as written
  bool reads_back = false;
  std::string error;
};

Played play(const Match& m, uint32_t seed, double minutes, int frames) {
  Played out;
  gs::ServerConfig c;
  c.hostname = "Report Night";
  c.map = "arena";
  c.maxclients = 8;
  c.bots = m.bots;
  c.seed = seed;
  char limit[32];
  std::snprintf(limit, sizeof limit, "%g", minutes);
  c.rules["timelimit"] = limit;
  if (m.teams) c.rules["teamplay"] = "1";
  gs::Server s;
  if (!gs::server_open(s, se::classic_game(), c, nullptr, nullptr, out.error)) return out;
  for (int f = 0; f < frames; f++) gs::server_frame(s);
  out.reports = s.reports;
  if (out.reports.empty()) return out;
  if (!gn::report_write(out.reports.back(), out.bytes, out.error)) return out;
  gn::MatchReport back;
  std::vector<uint8_t> again;
  out.reads_back = gn::report_read(out.bytes.data(), out.bytes.size(), back, out.error) &&
                   gn::report_write(back, again, out.error) && again == out.bytes;
  return out;
}

std::string num(int64_t v) { return std::to_string(v); }

std::string clock(int64_t ms) {
  char out[48];
  std::snprintf(out, sizeof out, "%lld:%02lld", static_cast<long long>(ms / 60000),
                static_cast<long long>(ms / 1000 % 60));
  return out;
}

// The report as a hub's scoreboard: the match's line, the teams, then every player.
gl::Picture scoreboard(const gn::MatchReport& r, int w, int h) {
  const gl::Rgba bg{18, 20, 26}, ink{226, 228, 232}, dim{140, 146, 158}, band{34, 38, 48};
  const gl::Rgba gold{238, 190, 80}, red{226, 92, 80}, blue{96, 150, 236};
  gl::Picture p = gl::create_picture(w, h, bg);
  const int s = w >= 900 ? 3 : 2;
  const int row = gl::kGlyphH * s + 8 * s / 2;
  int y = 4 * s;
  gl::draw_text(
      p, 6 * s, y,
      r.map + "  " + r.mode + "  " + clock(r.duration_ms) + "  " + gn::match_end_name(r.ended),
      gold, s);
  y += row;
  gl::draw_text(p, 6 * s, y, r.server + "  " + r.game + " " + r.version, dim, s);
  y += row + 2 * s;
  for (const gn::ReportTeam& t : r.teams) {
    gl::draw_text(p, 6 * s, y, "team " + t.name, t.name == "red" ? red : blue, s);
    gl::draw_text(p, w - 60.0 * s, y, num(t.score), ink, s);
    y += row;
  }
  if (!r.teams.empty()) y += s * 2;
  const char* heads[] = {"player", "team", "frags", "deaths", "suic", "tk", "secs"};
  const double cols[] = {6, 0.34, 0.48, 0.60, 0.72, 0.81, 0.89};
  auto col = [&](int i) { return i == 0 ? cols[0] * s : cols[i] * w; };
  fill_rect(p, 0, y - s * 2, w, row, band);
  for (int i = 0; i < 7; i++) gl::draw_text(p, col(i), y, heads[i], dim, s);
  y += row;
  for (const gn::ReportPlayer& pl : r.players) {
    if (y + row > h) break;
    const gl::Rgba c = pl.team == "red" ? red : pl.team == "blue" ? blue : ink;
    const std::string cells[] = {pl.name + (pl.bot ? " (bot)" : ""),
                                 pl.team.empty() ? "-" : pl.team,
                                 num(pl.frags),
                                 num(pl.deaths),
                                 num(pl.suicides),
                                 num(pl.team_kills),
                                 num(pl.seconds)};
    for (int i = 0; i < 7; i++) gl::draw_text(p, col(i), y, cells[i], i == 0 ? c : ink, s);
    y += row;
  }
  return p;
}

// THE MASTER'S HALF: the reports sent to a service on the simulated network.
namespace ga = ge::account;

constexpr uint16_t kServerPort = 27500;

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

gn::Key key_of(uint8_t b) {
  gn::Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(size_t{b} * 7 + i);
  return k;
}

struct Master {
  gn::SimNetwork net = gn::make_sim_network(3, gn::LinkConditions{});
  gn::Transport service_port, server_port;
  ga::Service svc;
  uint64_t fresh_n = 1;
  int64_t now = 1794500000ll * 1000000;
  Master() {
    gn::open_sim_transport(service_port, net, gn::sim_address(1, ga::kAccountPort));
    gn::open_sim_transport(server_port, net, gn::sim_address(2, kServerPort));
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::ServiceConfig c;
    c.seed = key_of(1);
    ga::service_init(svc, c, std::move(store), service_port, counter_fresh, &fresh_n);
  }
  // A report sent until answered (the first answer lost when `lose_first`): the sends it took.
  int send(const gn::SignKeys& server, const gn::MatchReport& r, bool lose_first,
           ga::Taken& taken) {
    ga::ReportAsk a;
    if (!ga::report_ask_start(a, gn::sim_address(1, ga::kAccountPort), ga::service_key(svc), server,
                              r, now))
      return 0;
    for (int i = 0; i < 400 && !a.done; i++) {
      ga::report_ask_update(a, server_port, now);
      now += 20000;
      gn::sim_advance(net, now);
      ga::service_update(svc, now);
      gn::sim_advance(net, now);
      uint8_t buf[gn::kMaxDatagram];
      gn::Address from;
      int n = 0;
      while ((n = gn::transport_receive(server_port, buf, sizeof buf, from, now)) >= 0) {
        if (lose_first) {
          lose_first = false;
          continue;
        }
        ga::report_ask_read(a, buf, static_cast<size_t>(n), from);
      }
    }
    taken = a.taken;
    return a.answered ? a.tries : 0;
  }
};

const char* taken_name(ga::Taken t) {
  return t == ga::Taken::counted ? "counted" : t == ga::Taken::shown ? "shown only" : "again";
}

// An account's totals under each filter, drawn as a player page's rows.
gl::Picture totals_picture(const ga::Matches& m, uint64_t account, int w, int h,
                           gl::LabTable& table) {
  const gl::Rgba bg{18, 20, 26}, ink{226, 228, 232}, dim{140, 146, 158}, band{34, 38, 48};
  const gl::Rgba gold{238, 190, 80};
  gl::Picture p = gl::create_picture(w, h, bg);
  const int s = w >= 900 ? 3 : 2;
  const int row = gl::kGlyphH * s + 4 * s;
  int y = 4 * s;
  gl::draw_text(p, 6 * s, y, "ANN'S STATS ON THE MASTER", gold, s);
  y += row + 2 * s;
  const char* heads[] = {"filter", "games", "wins", "frags", "deaths", "secs"};
  const double cols[] = {6, 0.42, 0.53, 0.64, 0.76, 0.88};
  auto col = [&](int i) { return i == 0 ? cols[0] * s : cols[i] * w; };
  fill_rect(p, 0, y - s * 2, w, row, band);
  for (int i = 0; i < 6; i++) gl::draw_text(p, col(i), y, heads[i], dim, s);
  y += row;
  struct Row {
    const char* name;
    ga::TotalsFilter f;
  };
  const Row rows[] = {
      {"all", {}},
      {"ffa only", {.modes = {"ffa"}}},
      {"without ffa", {.without = {"ffa"}}},
      {"arena", {.map = "arena"}},
      {"ping under 40", {.ping_under = 40}},
  };
  for (const Row& r : rows) {
    const ga::Totals t = ga::totals_sum(m, account, r.f);
    const std::string cells[] = {r.name,       num(t.matches), num(t.wins),
                                 num(t.frags), num(t.deaths),  num(t.seconds)};
    for (int i = 0; i < 6; i++) gl::draw_text(p, col(i), y, cells[i], ink, s);
    table.rows.push_back({cells[0], cells[1], cells[2], cells[3], cells[4], cells[5]});
    y += row;
  }
  return p;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double minutes = lab.arg_num("minutes");
  // The limit, the intermission the rules hold the scores up for, and a margin, at the 13 ms frame.
  const int frames = static_cast<int>((minutes * 60 + 12) / 0.013);
  const Match matches[] = {
      {"ffa", "A FREE-FOR-ALL'S REPORT",
       "Four bots play arena to its time limit on a dedicated server; when the map changes the "
       "game hands the core the match's report: each bot's frags, deaths, suicides and the "
       "seconds it played, best first.",
       "four bots, ended by the limit, deaths at least the frags", false, 4},
      {"teams", "A TEAM MATCH'S REPORT",
       "Four bots in two teams (teamplay 1): the report carries each team's score, the sum of "
       "its players' frags, and a frag a bot lost while alive is counted as a team kill.",
       "two teams, the scores the sums of their players', ended by the limit", true, 4},
  };
  gl::LabTable table{"each match's report",
                     {"match", "reports", "players", "ended", "frags", "deaths", "suicides",
                      "team kills", "bytes", "reads back", "same again"},
                     {}};
  int once = 0, read_back = 0, same = 0, deaths = 0;
  std::vector<gn::MatchReport> sent;
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  for (const Match& m : matches) {
    const Played a = play(m, seed, minutes, frames);
    const Played b = play(m, seed, minutes, frames);
    if (a.reports.empty()) {
      lab.fail(std::string("the ") + m.label + " match left no report" +
                   (a.error.empty() ? "" : ": " + a.error),
               "Check that the match ends inside the frames run (--minutes) and that "
               "games/shooters_edge/net/classic_game.cpp's change_if_asked calls end_match; "
               "skill: net.");
      return;
    }
    const gn::MatchReport& r = a.reports.back();
    int64_t frags = 0, died = 0, suicides = 0, tks = 0;
    for (const gn::ReportPlayer& p : r.players) {
      frags += p.frags;
      died += p.deaths;
      suicides += p.suicides;
      tks += p.team_kills;
    }
    once += a.reports.size() == 1 ? 1 : 0;
    read_back += a.reads_back ? 1 : 0;
    same += a.bytes == b.bytes ? 1 : 0;
    deaths += static_cast<int>(died);
    table.rows.push_back({m.label, num(static_cast<int64_t>(a.reports.size())),
                          num(static_cast<int64_t>(r.players.size())), gn::match_end_name(r.ended),
                          num(frags), num(died), num(suicides), num(tks),
                          num(static_cast<int64_t>(a.bytes.size())), a.reads_back ? "yes" : "no",
                          a.bytes == b.bytes ? "yes" : "no"});
    lab.shot({m.label, m.title, m.what, m.expect, {{"seed", lab.arg("seed")}}},
             scoreboard(r, w, h));
    sent.push_back(r);
  }
  // The reports to a master: from a trusted server (the first answer lost) and an unlisted one.
  Master master;
  const gn::SignKeys trusted = gn::sign_keys(key_of(7)), stranger = gn::sign_keys(key_of(8));
  ga::matches_trust(master.svc.matches, trusted.pub);
  gl::LabTable to_master{
      "the reports at the master", {"match", "server", "sends", "taken", "kept by the master"}, {}};
  int counted = 0, shown = 0;
  uint64_t sent_id = 1;
  for (gn::MatchReport r : sent) {
    if (!r.players.empty()) {  // the lab's stand-in for Ann, logged in
      r.players.front().account = 1;
      r.players.front().bot = false;
    }
    r.id = sent_id++;  // two lab servers, both with no clock: their own ids would be alike
    for (const bool is_trusted : {true, false}) {
      ga::Taken t = ga::Taken::shown;
      const int tries = master.send(is_trusted ? trusted : stranger, r, is_trusted, t);
      counted += t == ga::Taken::counted ? 1 : 0;
      shown += t == ga::Taken::shown ? 1 : 0;
      to_master.rows.push_back({r.mode + " on " + r.map, is_trusted ? "trusted" : "not listed",
                                num(tries), taken_name(t),
                                num(static_cast<int64_t>(master.svc.matches.kept.size()))});
    }
  }
  gl::LabTable totals{
      "Ann's totals at the master", {"filter", "games", "wins", "frags", "deaths", "secs"}, {}};
  lab.shot({"totals",
            "ONE ACCOUNT'S TOTALS, FILTERED",
            "Both reports go to the account master twice over: from a server its owner trusts (the "
            "first answer lost, so the report goes again) and from one the owner has not listed. "
            "Only the trusted server's count, once each; the first bot plays Ann's account. Her "
            "totals are kept per game, mode, map and ping band, and summed under each filter.",
            "two games in all, one without ffa, one in ffa only, both on arena at a good ping",
            {{"seed", lab.arg("seed")}}},
           totals_picture(master.svc.matches, 1, w, h, totals));
  lab.metric("reports_counted_at_the_master", counted, "reports");
  lab.metric("reports_shown_only", shown, "reports");
  lab.metric("reports_kept_by_the_master", static_cast<double>(master.svc.matches.kept.size()),
             "reports");
  lab.metric("matches_reported_once", once, "matches");
  lab.metric("reports_read_back", read_back, "reports");
  lab.metric("same_report_again", same, "reports");
  lab.metric("deaths", deaths, "deaths");
  lab.table(std::move(table));
  lab.table(std::move(to_master));
  lab.table(std::move(totals));
}

const gl::LabSpec kSpec{
    "match-report",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_report and engine/net/report (what a finished match leaves)",
    "net",
    "Does a match on our server report once when it ends, with every player's frags, deaths, "
    "suicides and team kills as the rules played them, the teams' scores and how it ended, in a "
    "record that reads back as written and is the same on every run; and does a master take each "
    "report once, count only a trusted server's, and sum an account's totals by mode, map and "
    "ping?",
    {
        {"seed", "5", "the bots' and the server's stream"},
        {"minutes", "0.34", "timelimit, minutes"},
        {"width", "960", "each scoreboard's width, px"},
        {"height", "540", "each scoreboard's height, px"},
    },
    "--width=640 --height=360 --minutes=0.2",
    run,
};
GE_LAB(kSpec);

}  // namespace
