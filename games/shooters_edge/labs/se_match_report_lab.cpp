// LAB `match-report`: WHAT A FINISHED MATCH LEAVES (docs/accounts.md § The match report). Shooter's
// Edge's dedicated server plays bot matches to their time limit, offline, and the core keeps the
// report the game hands it when the map changes (engine/server's `ServerGame::match_report`,
// games/shooters_edge/net/classic_report.hpp): a free-for-all and a team match. It answers whether
// each match reports once, with every player and their frags, deaths, suicides and team kills as
// the rules played them out, the teams' scores summed, how it ended, and whether the record reads
// back as it was written and comes out the same on every run; each report is drawn as the
// scoreboard a hub would show.
//
// ge:summary Lab: the report a bot match leaves on the dedicated server, drawn as its scoreboard
// ge:keywords lab match-report report scoreboard frags deaths suicides team-kills bot-match
// ge:skill net labs
// ge:test tests/shooters_edge_classic_report_test.cpp
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "net/report.hpp"
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
  }
  lab.metric("matches_reported_once", once, "matches");
  lab.metric("reports_read_back", read_back, "reports");
  lab.metric("same_report_again", same, "reports");
  lab.metric("deaths", deaths, "deaths");
  lab.table(std::move(table));
}

const gl::LabSpec kSpec{
    "match-report",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_report and engine/net/report (what a finished match leaves)",
    "net",
    "Does a match on our server report once when it ends, with every player's frags, deaths, "
    "suicides and team kills as the rules played them, the teams' scores and how it ended, in a "
    "record that reads back as written and is the same on every run?",
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
