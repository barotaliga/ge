// The classic servers' results, defined (results.hpp): a match seen ending, kept; the page and
// JSON.
//
// ge:summary The hub's classic results defined: the end of a match read off status records
// ge:keywords hub results results_see results_answer match-end scoreboard
// ge:skill net
// ge:test tests/hub_results_test.cpp
#include "hub/results.hpp"

#include <algorithm>

#include "core/json_write.hpp"
#include "hub/page_parts.hpp"
#include "hub/pages.hpp"

namespace ge::hub {

namespace gn = ge::net;
using core::json_arr;
using core::json_num;
using core::json_obj;
using core::json_str;

namespace {

bool in_play(const gn::StatusPlayer& p) { return (p.flags & gn::kPlayerSpectator) == 0; }

// Players in play and how many of them have a score.
int scoring(const gn::Status& s, int& players) {
  int n = 0;
  players = 0;
  for (const gn::StatusPlayer& p : s.players) {
    if (!in_play(p)) continue;
    players++;
    if (p.score != 0) n++;
  }
  return n;
}

int64_t total(const gn::Status& s) {
  int64_t t = 0;
  for (const gn::StatusPlayer& p : s.players)
    if (in_play(p)) t += p.score;
  return t;
}

// The scores all lower than before: the match on this map started again.
bool restarted(const gn::Status& before, const gn::Status& after) {
  int players = 0;
  if (scoring(before, players) < 2) return false;
  bool any_lower = false;
  for (const gn::StatusPlayer& p : before.players) {
    if (!in_play(p) || p.score <= 0) continue;
    const auto it = std::find_if(after.players.begin(), after.players.end(),
                                 [&](const gn::StatusPlayer& q) { return q.name == p.name; });
    if (it == after.players.end()) continue;
    if (it->score >= p.score) return false;
    any_lower = true;
  }
  return any_lower && total(after) * 4 < total(before);
}

std::string ago_text(int64_t s) {
  if (s < 60) return "just now";
  if (s < 3600) return std::to_string(s / 60) + " min ago";
  if (s < 86400) return std::to_string(s / 3600) + " h ago";
  return std::to_string(s / 86400) + (s < int64_t{2} * 86400 ? " day ago" : " days ago");
}

std::string length_text(int64_t s) {
  if (s < 0) return "-";
  return std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
}

gn::Status as_status(const ClassicResult& c) {
  gn::Status s;
  s.name = c.name;
  s.map = c.map;
  s.mode = c.mode;
  s.players = c.players;
  s.teams = c.teams;
  return s;
}

}  // namespace

bool results_see(Results& r, const gn::Address& server, const gn::Status& before,
                 int64_t playing_since_us, const gn::Status& after, int64_t now_us,
                 int64_t unix_s) {
  if (before.phase != gn::StatusPhase::playing) return false;
  int players = 0;
  if (scoring(before, players) < 2 || players < 2) return false;
  const bool same_map = after.map == before.map;
  const bool to_intermission = same_map && after.phase == gn::StatusPhase::intermission;
  const bool to_warmup =
      after.phase == gn::StatusPhase::lobby || after.phase == gn::StatusPhase::countdown;
  const bool moved_on = !same_map;
  const bool again =
      same_map && after.phase == gn::StatusPhase::playing && restarted(before, after);
  if (!to_intermission && !to_warmup && !moved_on && !again) return false;
  // At an intermission the server still shows the final scores; otherwise the last playing ones.
  const gn::Status& last = to_intermission ? after : before;
  ClassicResult c;
  c.id = r.next_id++;
  c.server = server;
  c.name = before.name;
  c.map = before.map;
  c.mode = before.mode;
  c.end_s = unix_s;
  c.length_s = playing_since_us >= 0 ? (now_us - playing_since_us) / 1000000 : -1;
  for (const gn::StatusPlayer& p : last.players)
    if (in_play(p)) c.players.push_back(p);
  std::stable_sort(
      c.players.begin(), c.players.end(),
      [](const gn::StatusPlayer& a, const gn::StatusPlayer& b) { return a.score > b.score; });
  c.teams = last.teams;
  r.kept.push_back(std::move(c));
  while (r.kept.size() > kResultsKept) r.kept.pop_front();
  r.seen++;
  return true;
}

bool results_answer(const HubSpec& spec, const Results* r, std::string_view target, int64_t unix_s,
                    Answer& out) {
  const std::string_view path = target.substr(0, target.find('?'));
  const bool api = path == "/v1/results";
  if (!api && path != "/results") return false;
  const std::string server = query_param(target, "server");
  std::vector<const ClassicResult*> shown;
  if (r) {
    for (auto it = r->kept.rbegin(); it != r->kept.rend(); ++it) {
      if (static_cast<int>(shown.size()) >= kResultsShown) break;
      if (!server.empty() && gn::to_string(it->server) != server) continue;
      shown.push_back(&*it);
    }
  }
  if (api) {
    Answer a;
    a.max_age_s = 30;
    if (!r) {
      a.code = 404;
      a.max_age_s = 0;
      a.body = json_obj({{"error", json_str("This hub reads no classic servers.")}});
      out = std::move(a);
      return true;
    }
    std::vector<std::string> v;
    v.reserve(shown.size());
    for (const ClassicResult* c : shown) {
      std::vector<std::string> ps;
      ps.reserve(c->players.size());
      for (const gn::StatusPlayer& p : c->players)
        ps.push_back(json_obj({{"name", json_str(p.name)},
                               {"team", json_str(p.team)},
                               {"score", json_num(p.score)},
                               {"ping", json_num(p.ping_ms)}}));
      std::vector<std::string> ts;
      ts.reserve(c->teams.size());
      for (const gn::StatusTeam& t : c->teams)
        ts.push_back(json_obj({{"name", json_str(t.name)}, {"score", json_num(t.score)}}));
      v.push_back(json_obj({{"id", json_num(static_cast<long long>(c->id))},
                            {"server", json_str(gn::to_string(c->server))},
                            {"name", json_str(c->name)},
                            {"map", json_str(c->map)},
                            {"mode", json_str(c->mode)},
                            {"end", json_num(c->end_s)},
                            {"length", json_num(c->length_s)},
                            {"teams", json_arr(ts)},
                            {"players", json_arr(ps)}}));
    }
    a.body = json_obj({{"results", json_arr(v)}});
    out = std::move(a);
    return true;
  }
  std::string m = "<h2>Results</h2>";
  if (!r) {
    m += "<p class=\"none\">This hub reads no classic servers, so it has no results of theirs.</p>";
  } else {
    m += "<p class=\"sum\">The matches the classic servers were seen to finish, newest first. "
         "They are read off what each server shows, so a match counts in no player's stats.</p>";
    if (shown.empty()) m += "<p class=\"none\">No match has been seen to finish yet.</p>";
    for (const ClassicResult* c : shown) {
      m += "<article class=\"result\"><h3>" + html_escape(c->map) + " <small>" +
           html_escape(c->mode.empty() ? "-" : c->mode) + "</small></h3><p class=\"meta\">" +
           "<a href=\"/results?server=" + html_escape(gn::to_string(c->server)) + "\">" +
           html_escape(c->name) + "</a> · " + ago_text(unix_s - c->end_s) + " · " +
           length_text(c->length_s) + "</p>" + scoreboard_html(spec, as_status(*c)) + "</article>";
    }
  }
  out = html(r ? 200 : 404,
             head(spec, "Results", false) + nav(spec, "Results") + "<main class=\"stats\">" + m +
                 "</main>" + foot(),
             30);
  return true;
}

}  // namespace ge::hub
