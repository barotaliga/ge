// The hub's Player, Match and Leaders pages and their JSON (stats_page.hpp).
//
// ge:summary The hub's Player, Match and Leaders pages defined: filters, tables, the JSON
// ge:keywords hub player-page match-page leaders weapon-accuracy stats_answer
// ge:skill net
// ge:test tests/hub_stats_page_test.cpp
#include "hub/stats_page.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "account/store.hpp"
#include "core/json_write.hpp"
#include "hub/ladder_page.hpp"
#include "hub/page_parts.hpp"
#include "hub/pages.hpp"

namespace ge::hub {

namespace ga = ge::account;
namespace gn = ge::net;
using core::json_arr;
using core::json_bool;
using core::json_num;
using core::json_obj;
using core::json_str;

namespace {

constexpr int64_t kDay_s = 86400;

// ---- the ask ----

int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string decode(std::string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') {
      out += ' ';
    } else if (s[i] == '%' && i + 2 < s.size() && nibble(s[i + 1]) >= 0 && nibble(s[i + 2]) >= 0) {
      out += static_cast<char>(nibble(s[i + 1]) * 16 + nibble(s[i + 2]));
      i += 2;
    } else {
      out += s[i];
    }
  }
  return out;
}

// Every value of `key` in the query, a comma-separated value split, empty ones left out.
std::vector<std::string> params(std::string_view target, std::string_view key) {
  std::vector<std::string> out;
  const size_t q = target.find('?');
  if (q == std::string_view::npos) return out;
  std::string_view rest = target.substr(q + 1);
  while (!rest.empty()) {
    const size_t amp = rest.find('&');
    const std::string_view pair = rest.substr(0, amp);
    rest = amp == std::string_view::npos ? std::string_view() : rest.substr(amp + 1);
    const size_t eq = pair.find('=');
    if (eq == std::string_view::npos || pair.substr(0, eq) != key) continue;
    const std::string v = decode(pair.substr(eq + 1));
    size_t at = 0;
    while (at <= v.size()) {
      const size_t comma = std::min(v.find(',', at), v.size());
      if (comma > at) out.push_back(v.substr(at, comma - at));
      at = comma + 1;
    }
  }
  return out;
}

std::string param(std::string_view target, std::string_view key) {
  const std::vector<std::string> v = params(target, key);
  return v.empty() ? std::string() : v.front();
}

long long number(const std::string& s, long long fallback) {
  if (s.empty() || s.size() > 9) return fallback;
  long long n = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return fallback;
    n = n * 10 + (c - '0');
  }
  return n;
}

ga::TotalsFilter filter_of(std::string_view target) {
  ga::TotalsFilter f;
  f.game = param(target, "game");
  f.map = param(target, "map");
  f.modes = params(target, "mode");
  f.without = params(target, "without");
  const long long ping = number(param(target, "ping"), 0);
  for (int b : ga::kPingBands)
    if (ping == b) f.ping_under = b;
  return f;
}

// The filter as a query (`&`-led), so a link keeps it.
std::string filter_query(const ga::TotalsFilter& f) {
  std::string q;
  const auto add = [&](const char* k, const std::string& v) {
    if (!v.empty()) q += std::string("&") + k + "=" + url_text(v);
  };
  add("game", f.game);
  add("map", f.map);
  for (const std::string& m : f.modes) add("mode", m);
  for (const std::string& m : f.without) add("without", m);
  if (f.ping_under) add("ping", std::to_string(f.ping_under));
  return q;
}

bool report_keeps(const ga::TotalsFilter& f, const gn::MatchReport& r, const gn::ReportPlayer* p) {
  return ga::totals_keeps(f, {0, r.game, r.mode, r.map, p ? ga::ping_band(p->ping_avg) : 0}) &&
         (p || f.ping_under == 0);
}

// ---- the numbers ----

std::string fixed1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

std::string pct(int64_t part, int64_t whole) {
  return whole > 0 ? fixed1(100.0 * static_cast<double>(part) / static_cast<double>(whole)) + "%"
                   : "-";
}

std::string efficiency(int64_t frags, int64_t deaths) { return pct(frags, frags + deaths); }

std::string clock_text(int64_t seconds) {
  char b[32];
  if (seconds >= 3600)
    std::snprintf(b, sizeof b, "%lldh %02lldm", static_cast<long long>(seconds / 3600),
                  static_cast<long long>(seconds / 60 % 60));
  else
    std::snprintf(b, sizeof b, "%lld:%02lld", static_cast<long long>(seconds / 60),
                  static_cast<long long>(seconds % 60));
  return b;
}

std::string id_hex(uint64_t id) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(id));
  return b;
}

bool ends_with(const std::string& s, std::string_view tail) {
  return s.size() > tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

const char* const kWeaponParts[] = {"_attacks", "_hits", "_kills", "_deaths"};

struct Weapon {
  std::string name;
  int64_t attacks = 0, hits = 0, kills = 0, deaths = 0;
};

int64_t get(const std::map<std::string, int64_t>& c, const std::string& k) {
  const auto it = c.find(k);
  return it == c.end() ? 0 : it->second;
}

// The weapons the columns name (`<w>_attacks` and `<w>_hits`), by kills, then name.
std::vector<Weapon> weapons_of(const std::map<std::string, int64_t>& c) {
  std::vector<Weapon> out;
  for (const auto& [name, v] : c) {
    if (!ends_with(name, "_attacks")) continue;
    Weapon w;
    w.name = name.substr(0, name.size() - 8);
    if (!c.count(w.name + "_hits")) continue;
    w.attacks = v;
    w.hits = get(c, w.name + "_hits");
    w.kills = get(c, w.name + "_kills");
    w.deaths = get(c, w.name + "_deaths");
    out.push_back(w);
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const Weapon& a, const Weapon& b) { return a.kills > b.kills; });
  return out;
}

bool weapon_column(const std::map<std::string, int64_t>& c, const std::string& name) {
  for (const char* part : kWeaponParts)
    if (ends_with(name, part)) {
      const std::string w = name.substr(0, name.size() - std::string_view(part).size());
      return c.count(w + "_attacks") && c.count(w + "_hits");
    }
  return false;
}

std::string label(const std::string& column) {
  std::string out = ends_with(column, "_max") ? column.substr(0, column.size() - 4) : column;
  std::replace(out.begin(), out.end(), '_', ' ');
  return ends_with(column, "_max") ? out + " (best)" : out;
}

std::map<std::string, int64_t> columns_of(const gn::MatchReport& r, const gn::ReportPlayer& p) {
  std::map<std::string, int64_t> c;
  for (size_t i = 0; i < r.columns.size() && i < p.columns.size(); i++)
    c[r.columns[i]] = p.columns[i];
  return c;
}

const ga::Account* account_named(const ga::Service& svc, const std::string& name) {
  const ga::Account* a = ga::find_by_name(svc.store, name);
  return a && a->removed_s == 0 ? a : nullptr;
}

std::string player_link(const ga::Service& svc, const gn::ReportPlayer& p) {
  const ga::Account* a = p.account ? ga::find(svc.store, p.account) : nullptr;
  if (!a || a->removed_s != 0 || p.bot)
    return html_escape(p.name) + (p.bot ? " <small class=\"bot\">bot</small>" : "");
  return "<a href=\"/player/" + url_text(a->name) + "\">" + html_escape(p.name) + "</a>";
}

const gn::MatchReport* match_by_id(const ga::Service& svc, std::string_view hex, bool* counted) {
  if (hex.size() != 16) return nullptr;
  uint64_t id = 0;
  for (char c : hex) {
    if (nibble(c) < 0) return nullptr;
    id = id * 16 + static_cast<uint64_t>(nibble(c));
  }
  for (auto it = svc.matches.kept.rbegin(); it != svc.matches.kept.rend(); ++it)
    if (it->report.id == id) {
      if (counted) *counted = it->counted;
      return &it->report;
    }
  return nullptr;
}

const gn::ReportPlayer* seat_of(const gn::MatchReport& r, uint64_t account) {
  for (const gn::ReportPlayer& p : r.players)
    if (p.account == account && !p.bot) return &p;
  return nullptr;
}

// ---- HTML pieces ----

std::string page(const HubSpec& spec, const std::string& title, const char* on,
                 const std::string& main) {
  return head(spec, title, false) + nav(spec, on) + "<main class=\"stats\">" + main + "</main>" +
         foot();
}

std::string options(const std::vector<std::string>& values, const std::string& on,
                    const char* every) {
  std::string h = std::string("<option value=\"\">") + every + "</option>";
  for (const std::string& v : values)
    h += "<option" + std::string(v == on ? " selected" : "") + ">" + html_escape(v) + "</option>";
  return h;
}

// The filter form: game, map, mode, the modes left out, the ping; `extra` more fields first.
std::string filter_form(const char* action, const ga::TotalsFilter& f,
                        const std::set<std::string>& games, const std::set<std::string>& maps,
                        const std::set<std::string>& modes, const std::string& extra) {
  std::string h = std::string("<form class=\"search\" method=\"get\" action=\"") + action + "\">";
  h += extra;
  if (games.size() > 1)
    h += "<select name=\"game\">" + options({games.begin(), games.end()}, f.game, "every game") +
         "</select>";
  h += "<select name=\"map\">" + options({maps.begin(), maps.end()}, f.map, "every map") +
       "</select>";
  h += "<select name=\"mode\">" +
       options({modes.begin(), modes.end()}, f.modes.empty() ? "" : f.modes.front(), "every mode") +
       "</select>";
  h += "<select name=\"ping\"><option value=\"\">any ping</option>";
  for (int b : ga::kPingBands)
    h += "<option value=\"" + std::to_string(b) + "\"" + (f.ping_under == b ? " selected" : "") +
         ">under " + std::to_string(b) + " ms</option>";
  h += "</select>";
  for (const std::string& m : modes) {
    const bool on = std::find(f.without.begin(), f.without.end(), m) != f.without.end();
    h += "<label class=\"without\"><input type=\"checkbox\" name=\"without\" value=\"" +
         html_escape(m) + "\"" + (on ? " checked" : "") + "> no " + html_escape(m) + "</label>";
  }
  return h + "<button>Show</button></form>";
}

std::string tile(const std::string& value, const char* what) {
  return "<div class=\"tile\"><b>" + value + "</b><small>" + what + "</small></div>";
}

std::string tiles(const ga::Totals& t) {
  return "<div class=\"tiles\">" + tile(std::to_string(t.matches), "matches") +
         tile(std::to_string(t.wins), "wins") + tile(std::to_string(t.frags), "frags") +
         tile(std::to_string(t.deaths), "deaths") +
         tile(efficiency(t.frags, t.deaths), "efficiency") +
         tile(t.matches ? fixed1(static_cast<double>(t.frags) / static_cast<double>(t.matches))
                        : "-",
              "frags a match") +
         tile(clock_text(t.seconds), "played") + "</div>";
}

std::string weapons_table(const std::vector<Weapon>& ws) {
  if (ws.empty()) return "";
  std::string h =
      "<h3>Weapons</h3><table class=\"num\"><tr><th>weapon</th><th>accuracy</th>"
      "<th>hits</th><th>attacks</th><th>kills</th><th>deaths by it</th></tr>";
  for (const Weapon& w : ws)
    h += "<tr><td>" + html_escape(w.name) + "</td><td>" + pct(w.hits, w.attacks) + "</td><td>" +
         std::to_string(w.hits) + "</td><td>" + std::to_string(w.attacks) + "</td><td>" +
         std::to_string(w.kills) + "</td><td>" + std::to_string(w.deaths) + "</td></tr>";
  return h + "</table>";
}

std::string others_table(const std::map<std::string, int64_t>& c) {
  std::string rows;
  for (const auto& [name, v] : c)
    if (!weapon_column(c, name))
      rows +=
          "<tr><td>" + html_escape(label(name)) + "</td><td>" + std::to_string(v) + "</td></tr>";
  return rows.empty() ? "" : "<h3>More</h3><table class=\"num kv\">" + rows + "</table>";
}

// The score's own column, unless the game's score is its frags (then the frags column says it).
bool score_column(const HubSpec& spec) { return spec.score != "frags"; }

std::string match_row(const HubSpec& spec, const gn::MatchReport& r, const gn::ReportPlayer* p) {
  std::string h = "<tr><td><a href=\"/match/" + id_hex(r.id) + "\">" + utc_text(r.start_s) +
                  "</a></td><td>" + html_escape(r.mode) + "</td><td>" + html_escape(r.map) +
                  "</td><td>" + html_escape(r.server) + "</td>";
  if (p)
    h += (score_column(spec) ? "<td>" + std::to_string(p->score) + "</td>" : std::string()) +
         "<td>" + std::to_string(p->frags) + " / " + std::to_string(p->deaths) + "</td><td>" +
         std::to_string(p->ping_avg) + "</td>";
  else
    h += "<td>" + std::to_string(r.players.size()) + "</td>";
  return h + "</tr>";
}

// ---- the Player page ----

struct PlayerView {
  const ga::Account* account = nullptr;
  ga::TotalsFilter filter;
  ga::Totals sum;
  std::map<std::pair<std::string, std::string>, ga::Totals> by_mode;  // game, mode
  std::set<std::string> games, maps, modes;  // everything the account played, for the form
  std::vector<const ga::KeptMatch*> recent;  // newest first
};

PlayerView player_view(const ga::Service& svc, const ga::Account& a, std::string_view target) {
  PlayerView v;
  v.account = &a;
  v.filter = filter_of(target);
  v.sum = ga::totals_sum(svc.matches, a.id, v.filter);
  for (auto it = svc.matches.totals.lower_bound({a.id, "", "", "", 0});
       it != svc.matches.totals.end() && std::get<0>(it->first) == a.id; ++it) {
    const auto& [id, game, mode, map, band] = it->first;
    v.games.insert(game);
    v.maps.insert(map);
    v.modes.insert(mode);
    if (ga::totals_keeps(v.filter, it->first)) ga::totals_add(v.by_mode[{game, mode}], it->second);
  }
  for (auto it = svc.matches.kept.rbegin();
       it != svc.matches.kept.rend() && v.recent.size() < static_cast<size_t>(kRecentShown); ++it) {
    const gn::ReportPlayer* p = seat_of(it->report, a.id);
    if (p && report_keeps(v.filter, it->report, p)) v.recent.push_back(&*it);
  }
  return v;
}

std::string player_html(const HubSpec& spec, const PlayerView& v) {
  const ga::Account& a = *v.account;
  std::string m = "<h2>" + html_escape(a.name) + "</h2><p class=\"sum\">Playing since " +
                  utc_text(a.created_s).substr(0, 10) + ". Counted from the servers this hub's " +
                  "owner trusts.</p>";
  m += filter_form(("/player/" + url_text(a.name)).c_str(), v.filter, v.games, v.maps, v.modes, "");
  if (v.sum.matches == 0) {
    m += "<p class=\"none\">No matches counted under this filter yet.</p>";
  } else {
    m += tiles(v.sum);
    m += "<h3>By mode</h3><table class=\"num\"><tr>" +
         std::string(v.games.size() > 1 ? "<th>game</th>" : "") +
         "<th>mode</th><th>matches</th><th>wins</th><th>frags</th><th>deaths</th>"
         "<th>efficiency</th></tr>";
    for (const auto& [key, t] : v.by_mode)
      m += "<tr>" + (v.games.size() > 1 ? "<td>" + html_escape(key.first) + "</td>" : "") + "<td>" +
           html_escape(key.second) + "</td><td>" + std::to_string(t.matches) + "</td><td>" +
           std::to_string(t.wins) + "</td><td>" + std::to_string(t.frags) + "</td><td>" +
           std::to_string(t.deaths) + "</td><td>" + efficiency(t.frags, t.deaths) + "</td></tr>";
    m += "</table>";
    m += weapons_table(weapons_of(v.sum.columns));
    m += others_table(v.sum.columns);
  }
  m += "<h3>Recent matches</h3>";
  if (v.recent.empty()) {
    m += "<p class=\"none\">None this hub still holds.</p>";
  } else {
    m += "<div class=\"wide\"><table class=\"num\"><tr><th>when (UTC)</th><th>mode</th><th>map</th>"
         "<th>server</th>" +
         (score_column(spec) ? "<th>" + html_escape(spec.score) + "</th>" : std::string()) +
         "<th>frags / deaths</th><th>ping</th></tr>";
    for (const ga::KeptMatch* k : v.recent)
      m += match_row(spec, k->report, seat_of(k->report, a.id));
    m += "</table></div>";
  }
  return page(spec, a.name, "", m);
}

std::string totals_json(const ga::Totals& t) {
  std::vector<std::pair<std::string, std::string>> cols;
  cols.reserve(t.columns.size());
  for (const auto& [name, v] : t.columns) cols.emplace_back(name, json_num(v));
  return json_obj({{"matches", json_num(t.matches)},
                   {"wins", json_num(t.wins)},
                   {"score", json_num(t.score)},
                   {"frags", json_num(t.frags)},
                   {"deaths", json_num(t.deaths)},
                   {"suicides", json_num(t.suicides)},
                   {"team_kills", json_num(t.team_kills)},
                   {"seconds", json_num(t.seconds)},
                   {"columns", json_obj(cols)}});
}

std::string match_summary_json(const gn::MatchReport& r, bool counted) {
  return json_obj({{"id", json_str(id_hex(r.id))},
                   {"game", json_str(r.game)},
                   {"mode", json_str(r.mode)},
                   {"map", json_str(r.map)},
                   {"server", json_str(r.server)},
                   {"start", json_num(r.start_s)},
                   {"duration_ms", json_num(r.duration_ms)},
                   {"ended", json_str(gn::match_end_name(r.ended))},
                   {"counted", json_bool(counted)},
                   {"players", json_num(static_cast<long long>(r.players.size()))}});
}

std::string player_json(const PlayerView& v) {
  std::vector<std::string> modes, recent;
  for (const auto& [key, t] : v.by_mode)
    modes.push_back(json_obj({{"game", json_str(key.first)},
                              {"mode", json_str(key.second)},
                              {"totals", totals_json(t)}}));
  for (const ga::KeptMatch* k : v.recent)
    recent.push_back(match_summary_json(k->report, k->counted));
  return json_obj({{"name", json_str(v.account->name)},
                   {"created", json_num(v.account->created_s)},
                   {"totals", totals_json(v.sum)},
                   {"by_mode", json_arr(modes)},
                   {"recent", json_arr(recent)}});
}

// ---- the Match page ----

std::string match_html(const HubSpec& spec, const ga::Service& svc, const gn::MatchReport& r,
                       bool counted) {
  std::string m = "<h2>" + html_escape(r.mode) + " on " + html_escape(r.map) + "</h2>";
  m += "<p class=\"sum\">" + html_escape(r.server) + " · " + utc_text(r.start_s) + " UTC · " +
       clock_text(r.duration_ms / 1000) + " · " +
       (r.ended == gn::MatchEnd::limit     ? "played to the limit"
        : r.ended == gn::MatchEnd::forfeit ? "ended by a forfeit"
                                           : "broken off") +
       "</p>";
  if (!counted)
    m += "<p class=\"warn\">From a server this hub's owner has not listed: shown here, counted in "
         "no player's stats.</p>";
  if (!r.demo_sha256.empty())
    m += "<p><a class=\"btn\" href=\"/demo/" + html_escape(r.demo_sha256) + "\">The demo</a></p>";
  if (!r.teams.empty()) {
    m += "<div class=\"tiles\">";
    for (const gn::ReportTeam& t : r.teams)
      m += tile(std::to_string(t.score), html_escape(t.name).c_str());
    m += "</div>";
  }
  std::vector<const gn::ReportPlayer*> seats;
  seats.reserve(r.players.size());
  for (const gn::ReportPlayer& p : r.players) seats.push_back(&p);
  const auto team_score = [&](const std::string& team) {
    for (const gn::ReportTeam& t : r.teams)
      if (t.name == team) return t.score;
    return int64_t{0};
  };
  std::stable_sort(seats.begin(), seats.end(), [&](const auto* a, const auto* b) {
    if (a->team != b->team) {
      const int64_t ta = team_score(a->team), tb = team_score(b->team);
      return ta != tb ? ta > tb : a->team < b->team;
    }
    return a->score > b->score;
  });
  m += "<h3>Scoreboard</h3><table class=\"num\"><tr><th>player</th>" +
       std::string(r.teams.empty() ? "" : "<th>team</th>") +
       (score_column(spec) ? "<th>" + html_escape(spec.score) + "</th>" : std::string()) +
       "<th>frags</th><th>deaths</th><th>efficiency</th><th>ping</th><th>time</th></tr>";
  for (const gn::ReportPlayer* p : seats)
    m += "<tr><td>" + player_link(svc, *p) + "</td>" +
         (r.teams.empty() ? "" : "<td>" + html_escape(p->team) + "</td>") +
         (score_column(spec) ? "<td>" + std::to_string(p->score) + "</td>" : std::string()) +
         "<td>" + std::to_string(p->frags) + "</td><td>" + std::to_string(p->deaths) + "</td><td>" +
         efficiency(p->frags, p->deaths) + "</td><td>" + std::to_string(p->ping_avg) + "</td><td>" +
         clock_text(p->seconds) + "</td></tr>";
  m += "</table>";
  // The weapons any player attacked with, by everyone's kills; a cell is a player's accuracy.
  std::map<std::string, int64_t> all;
  for (const gn::ReportPlayer* p : seats)
    for (const auto& [name, v] : columns_of(r, *p)) all[name] += v;
  std::vector<Weapon> ws = weapons_of(all);
  ws.erase(std::remove_if(ws.begin(), ws.end(), [](const Weapon& w) { return w.attacks == 0; }),
           ws.end());
  if (!ws.empty()) {
    m += "<h3>Accuracy</h3><div class=\"wide\"><table class=\"num\"><tr><th>player</th>";
    for (const Weapon& w : ws) m += "<th>" + html_escape(w.name) + "</th>";
    m += "</tr>";
    for (const gn::ReportPlayer* p : seats) {
      const std::map<std::string, int64_t> c = columns_of(r, *p);
      m += "<tr><td>" + player_link(svc, *p) + "</td>";
      for (const Weapon& w : ws) {
        const int64_t a = get(c, w.name + "_attacks"), h = get(c, w.name + "_hits");
        const int64_t k = get(c, w.name + "_kills");
        m +=
            "<td>" +
            (a ? pct(h, a) + (k ? " <small>" + std::to_string(k) + " kills</small>" : std::string())
               : std::string("-")) +
            "</td>";
      }
      m += "</tr>";
    }
    m += "</table></div><p class=\"sum\">Each cell: the hits over the attacks, and the kills.</p>";
  }
  std::vector<std::string> others;
  for (const auto& [name, v] : all)
    if (!weapon_column(all, name)) others.push_back(name);
  if (!others.empty()) {
    m += "<h3>More</h3><div class=\"wide\"><table class=\"num\"><tr><th>player</th>";
    for (const std::string& o : others) m += "<th>" + html_escape(label(o)) + "</th>";
    m += "</tr>";
    for (const gn::ReportPlayer* p : seats) {
      const std::map<std::string, int64_t> c = columns_of(r, *p);
      m += "<tr><td>" + player_link(svc, *p) + "</td>";
      for (const std::string& o : others) m += "<td>" + std::to_string(get(c, o)) + "</td>";
      m += "</tr>";
    }
    m += "</table></div>";
  }
  return page(spec, r.mode + " on " + r.map, "", m);
}

std::string match_json(const gn::MatchReport& r, bool counted) {
  std::vector<std::string> teams, players;
  for (const gn::ReportTeam& t : r.teams)
    teams.push_back(json_obj({{"name", json_str(t.name)}, {"score", json_num(t.score)}}));
  for (const gn::ReportPlayer& p : r.players) {
    std::vector<std::pair<std::string, std::string>> cols;
    for (const auto& [name, v] : columns_of(r, p)) cols.emplace_back(name, json_num(v));
    players.push_back(json_obj({{"name", json_str(p.name)},
                                {"team", json_str(p.team)},
                                {"account", json_bool(p.account != 0)},
                                {"bot", json_bool(p.bot)},
                                {"score", json_num(p.score)},
                                {"frags", json_num(p.frags)},
                                {"deaths", json_num(p.deaths)},
                                {"suicides", json_num(p.suicides)},
                                {"team_kills", json_num(p.team_kills)},
                                {"seconds", json_num(p.seconds)},
                                {"ping_min", json_num(p.ping_min)},
                                {"ping_avg", json_num(p.ping_avg)},
                                {"ping_max", json_num(p.ping_max)},
                                {"columns", json_obj(cols)}}));
  }
  return json_obj({{"match", match_summary_json(r, counted)},
                   {"demo_sha256", json_str(r.demo_sha256)},
                   {"teams", json_arr(teams)},
                   {"players", json_arr(players)}});
}

// ---- the Leaders page ----

struct Leader {
  const ga::Account* account = nullptr;
  double value = 0;
  bool ratio = false;
  int64_t matches = 0;
};

// The stat's value for `t`, false when `t` does not take part (a ratio over too few matches).
bool stat_value(const std::string& stat, const ga::Totals& t, double& v, bool& ratio) {
  const auto d = [](int64_t n) { return static_cast<double>(n); };
  ratio =
      stat == "efficiency" || stat == "kd" || stat == "frags_per_match" || ends_with(stat, "_acc");
  if (ratio && t.matches < kLeadersMinMatches) return false;
  if (stat == "frags")
    v = d(t.frags);
  else if (stat == "wins")
    v = d(t.wins);
  else if (stat == "matches")
    v = d(t.matches);
  else if (stat == "score")
    v = d(t.score);
  else if (stat == "efficiency") {
    if (t.frags + t.deaths == 0) return false;
    v = 100.0 * d(t.frags) / d(t.frags + t.deaths);
  } else if (stat == "kd")
    v = d(t.frags) / d(std::max<int64_t>(1, t.deaths));
  else if (stat == "frags_per_match")
    v = d(t.frags) / d(t.matches);
  else if (ends_with(stat, "_acc")) {
    const std::string w = stat.substr(0, stat.size() - 4);
    const int64_t a = get(t.columns, w + "_attacks");
    if (a == 0) return false;
    v = 100.0 * d(get(t.columns, w + "_hits")) / d(a);
  } else {
    const auto it = t.columns.find(stat);
    if (it == t.columns.end()) return false;
    v = d(it->second);
  }
  return true;
}

std::string value_text(const Leader& l, const std::string& stat) {
  if (!l.ratio) return std::to_string(static_cast<long long>(l.value));
  return stat == "kd" ? fixed1(l.value) : fixed1(l.value) + (stat == "frags_per_match" ? "" : "%");
}

struct LeadersView {
  std::string stat;
  long long days = 0;
  ga::TotalsFilter filter;
  std::vector<Leader> rows;
  std::vector<std::string> stats;  // the stats the form offers
  std::set<std::string> games, maps, modes;
};

LeadersView leaders_view(const ga::Service& svc, std::string_view target, int64_t unix_s) {
  LeadersView v;
  v.filter = filter_of(target);
  v.stat = param(target, "stat");
  if (v.stat.empty()) v.stat = "frags";
  v.days = std::min<long long>(number(param(target, "days"), 0), 3650);
  const std::map<uint64_t, ga::Totals> all =
      ga::totals_by_account(svc.matches, v.filter, v.days > 0 ? unix_s - v.days * kDay_s : 0);
  std::set<std::string> columns, weapons;
  for (const auto& [key, t] : svc.matches.totals) {
    v.games.insert(std::get<1>(key));
    v.modes.insert(std::get<2>(key));
    v.maps.insert(std::get<3>(key));
    for (const auto& [name, n] : t.columns) {
      if (weapon_column(t.columns, name)) {
        if (ends_with(name, "_attacks")) weapons.insert(name.substr(0, name.size() - 8));
      } else {
        columns.insert(name);
      }
    }
  }
  v.stats = {"frags", "wins", "matches", "score", "efficiency", "kd", "frags_per_match"};
  for (const std::string& w : weapons) v.stats.push_back(w + "_acc");
  for (const std::string& w : weapons) v.stats.push_back(w + "_kills");
  for (const std::string& c : columns) v.stats.push_back(c);
  for (const auto& [id, t] : all) {
    const ga::Account* a = ga::find(svc.store, id);
    Leader l;
    if (!a || a->removed_s != 0 || !stat_value(v.stat, t, l.value, l.ratio)) continue;
    l.account = a;
    l.matches = t.matches;
    v.rows.push_back(l);
  }
  std::stable_sort(v.rows.begin(), v.rows.end(), [](const Leader& a, const Leader& b) {
    return a.value != b.value ? a.value > b.value : a.matches > b.matches;
  });
  if (v.rows.size() > static_cast<size_t>(kLeadersShown)) v.rows.resize(kLeadersShown);
  return v;
}

std::string stat_label(const std::string& s) {
  if (s == "kd") return "frags a death";
  if (s == "frags_per_match") return "frags a match";
  if (ends_with(s, "_acc")) return s.substr(0, s.size() - 4) + " accuracy";
  return label(s);
}

std::string leaders_html(const HubSpec& spec, const LeadersView& v) {
  std::string extra = "<select name=\"stat\">";
  for (const std::string& s : v.stats)
    extra += "<option value=\"" + html_escape(s) + "\"" + (s == v.stat ? " selected" : "") + ">" +
             html_escape(stat_label(s)) + "</option>";
  extra += "</select><select name=\"days\">";
  for (long long d : {0LL, 1LL, 7LL, 30LL})
    extra += "<option value=\"" + std::string(d ? std::to_string(d) : "") + "\"" +
             (d == v.days ? " selected" : "") + ">" +
             (d == 0 ? std::string("all time") : "the last " + std::to_string(d) + " d") +
             "</option>";
  extra += "</select>";
  std::string m = "<h2>Leaders: " + html_escape(stat_label(v.stat)) + "</h2>";
  m += filter_form("/leaders", v.filter, v.games, v.maps, v.modes, extra);
  if (v.rows.empty()) {
    m += "<p class=\"none\">Nobody yet under this filter.</p>";
  } else {
    m += "<table class=\"num rank\"><tr><th>#</th><th>player</th><th>" +
         html_escape(stat_label(v.stat)) + "</th><th>matches</th></tr>";
    const std::string q = filter_query(v.filter);
    for (size_t i = 0; i < v.rows.size(); i++)
      m += "<tr><td>" + std::to_string(i + 1) + "</td><td><a href=\"/player/" +
           url_text(v.rows[i].account->name) + (q.empty() ? "" : "?" + q.substr(1)) + "\">" +
           html_escape(v.rows[i].account->name) + "</a></td><td>" + value_text(v.rows[i], v.stat) +
           "</td><td>" + std::to_string(v.rows[i].matches) + "</td></tr>";
    m += "</table>";
  }
  if (v.stat == "efficiency" || v.stat == "kd" || v.stat == "frags_per_match" ||
      ends_with(v.stat, "_acc"))
    m += "<p class=\"sum\">Counting players with at least " + std::to_string(kLeadersMinMatches) +
         " matches under the filter.</p>";
  if (v.days > 0)
    m += "<p class=\"sum\">Over the last " + std::to_string(v.days) +
         " days of the matches this hub still holds.</p>";
  return page(spec, "Leaders", "Leaders", m);
}

std::string leaders_json(const LeadersView& v) {
  std::vector<std::string> rows;
  for (size_t i = 0; i < v.rows.size(); i++)
    rows.push_back(
        json_obj({{"rank", json_num(static_cast<long long>(i) + 1)},
                  {"name", json_str(v.rows[i].account->name)},
                  {"value", v.rows[i].ratio ? fixed1(v.rows[i].value)
                                            : json_num(static_cast<long long>(v.rows[i].value))},
                  {"matches", json_num(v.rows[i].matches)}}));
  return json_obj({{"stat", json_str(v.stat)},
                   {"days", json_num(v.days)},
                   {"min_matches", json_num(kLeadersMinMatches)},
                   {"leaders", json_arr(rows)}});
}

// ---- /v1/matches ----

std::string matches_json(const ga::Service& svc, std::string_view target) {
  const ga::TotalsFilter f = filter_of(target);
  const std::string who = param(target, "player");
  const ga::Account* a = who.empty() ? nullptr : account_named(svc, who);
  const long long page_no = std::max<long long>(1, number(param(target, "page"), 1));
  std::vector<std::string> rows;
  long long seen = 0;
  for (auto it = svc.matches.kept.rbegin(); it != svc.matches.kept.rend(); ++it) {
    const gn::ReportPlayer* p = a ? seat_of(it->report, a->id) : nullptr;
    if ((!who.empty() && !p) || !report_keeps(f, it->report, p)) continue;
    if (seen++ / kRecentShown + 1 == page_no)
      rows.push_back(match_summary_json(it->report, it->counted));
  }
  return json_obj({{"page", json_num(page_no)},
                   {"pages", json_num((seen + kRecentShown - 1) / kRecentShown)},
                   {"matches", json_arr(rows)}});
}

// ---- answers ----

Answer json_answer(int code, std::string body) {
  Answer a;
  a.code = code;
  a.body = std::move(body);
  a.max_age_s = code == 200 ? 10 : 0;
  return a;
}

Answer json_missing(const std::string& what) {
  return json_answer(404, json_obj({{"error", json_str(what)}}));
}

Answer html_missing(const HubSpec& spec, const std::string& what) {
  return html(404,
              page(spec, "Not here", "",
                   "<h2>Not here</h2><p class=\"none\">" + html_escape(what) + "</p>"),
              0);
}

}  // namespace

std::string url_text(std::string_view s) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '.' || c == '_' || c == '~') {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 15];
    }
  }
  return out;
}

bool stats_path(std::string_view path) {
  const auto under = [&](std::string_view p) {
    return path == p ||
           (path.size() > p.size() && path.substr(0, p.size()) == p && path[p.size()] == '/');
  };
  return under("/player") || under("/match") || path == "/leaders" || under("/v1/players") ||
         under("/v1/matches") || path == "/v1/leaders" || path == "/ladder" || path == "/v1/ladder";
}

bool stats_answer(const HubSpec& spec, const account::Service* svc, std::string_view target,
                  int64_t unix_s, Answer& out) {
  const std::string path = target_path(target);
  if (!stats_path(path)) return false;
  const bool api = path.rfind("/v1/", 0) == 0;
  if (!svc) {
    const std::string why = "This hub keeps no player stats: its owner runs it without accounts.";
    out = api ? json_missing(why) : html_missing(spec, why);
    return true;
  }
  const auto tail = [&](std::string_view head) {
    return path.size() > head.size() + 1 ? path.substr(head.size() + 1) : std::string();
  };
  if (path.rfind("/player", 0) == 0 || path.rfind("/v1/players", 0) == 0) {
    const std::string name = tail(api ? "/v1/players" : "/player");
    const ga::Account* a = name.empty() ? nullptr : account_named(*svc, name);
    if (!a) {
      const std::string why = "No player called " + name + " has an account on this hub.";
      out = api ? json_missing(why) : html_missing(spec, why);
      return true;
    }
    const PlayerView v = player_view(*svc, *a, target);
    out = api ? json_answer(200, player_json(v)) : html(200, player_html(spec, v), 10);
    return true;
  }
  if (path == "/v1/matches") {
    out = json_answer(200, matches_json(*svc, target));
    return true;
  }
  if (path.rfind("/match", 0) == 0 || path.rfind("/v1/matches/", 0) == 0) {
    const std::string id = tail(api ? "/v1/matches" : "/match");
    bool counted = false;
    const gn::MatchReport* r = match_by_id(*svc, id, &counted);
    if (!r) {
      const std::string why = "This hub holds no match " + id + ": it keeps the last " +
                              std::to_string(ga::kMatchesKept) + ".";
      out = api ? json_missing(why) : html_missing(spec, why);
      return true;
    }
    out = api ? json_answer(200, match_json(*r, counted))
              : html(200, match_html(spec, *svc, *r, counted), 60);
    return true;
  }
  if (path == "/ladder" || path == "/v1/ladder") {
    out = ladder_answer(spec, *svc, target, unix_s, api);
    return true;
  }
  const LeadersView v = leaders_view(*svc, target, unix_s);
  out = api ? json_answer(200, leaders_json(v)) : html(200, leaders_html(spec, v), 30);
  return true;
}

}  // namespace ge::hub
