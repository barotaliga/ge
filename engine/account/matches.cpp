// The matches a master keeps (account/matches.hpp).
//
// ge:summary Reports taken once by server and id, kept, and added to trusted accounts' totals
// ge:keywords match-report totals trusted-servers reports-log
// ge:skill net
// ge:test tests/account_matches_test.cpp
#include "account/matches.hpp"

#include <algorithm>
#include <vector>

namespace ge::account {

namespace {

constexpr char kHex[] = "0123456789abcdef";

std::string hex_of(const uint8_t* data, size_t size) {
  std::string out;
  out.reserve(size * 2);
  for (size_t i = 0; i < size; i++) {
    out += kHex[data[i] >> 4];
    out += kHex[data[i] & 15];
  }
  return out;
}

int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool bytes_of(std::string_view hex, std::vector<uint8_t>& out) {
  if (hex.size() % 2 != 0) return false;
  out.resize(hex.size() / 2);
  for (size_t i = 0; i < out.size(); i++) {
    const int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  return true;
}

// A column whose name ends "_max" keeps its largest value; the rest are summed.
bool is_max(const std::string& name) {
  return name.size() >= 4 && name.compare(name.size() - 4, 4, "_max") == 0;
}

void add_column(std::map<std::string, int64_t>& to, const std::string& name, int64_t v) {
  auto [it, made] = to.try_emplace(name, v);
  if (made) return;
  it->second = is_max(name) ? std::max(it->second, v) : it->second + v;
}

// The best score and the best team's name: who won.
void add_totals(std::map<TotalsKey, Totals>& totals, const net::MatchReport& r) {
  int64_t best = 0;
  bool any = false;
  for (const net::ReportPlayer& p : r.players)
    if (!any || p.score > best) {
      best = p.score;
      any = true;
    }
  std::string best_team;
  int64_t team_best = 0;
  bool tied = false;
  for (const net::ReportTeam& t : r.teams) {
    if (best_team.empty() || t.score > team_best) {
      best_team = t.name;
      team_best = t.score;
      tied = false;
    } else if (t.score == team_best) {
      tied = true;
    }
  }
  if (tied) best_team.clear();
  for (const net::ReportPlayer& p : r.players) {
    if (p.account == 0 || p.bot) continue;
    Totals& t = totals[{p.account, r.game, r.mode, r.map, ping_band(p.ping_avg)}];
    t.matches++;
    const bool won = r.teams.empty() ? p.score == best : !best_team.empty() && p.team == best_team;
    if (won) t.wins++;
    t.score += p.score;
    t.frags += p.frags;
    t.deaths += p.deaths;
    t.suicides += p.suicides;
    t.team_kills += p.team_kills;
    t.seconds += p.seconds;
    for (size_t i = 0; i < r.columns.size() && i < p.columns.size(); i++)
      add_column(t.columns, r.columns[i], p.columns[i]);
  }
}

Taken take(Matches& m, const net::Key& server, const net::MatchReport& r) {
  if (!m.seen.insert({server, r.id}).second) {
    m.again++;
    return Taken::again;
  }
  m.taken++;
  const bool counted = m.trusted.count(server) != 0;
  if (counted) {
    add_totals(m.totals, r);
    ladder_add(m.ladder, r);
    m.counted++;
  }
  m.kept.push_back({server, counted, r});
  if (m.kept.size() > kMatchesKept) m.kept.pop_front();
  return counted ? Taken::counted : Taken::shown;
}

}  // namespace

int ping_band(int64_t ping_ms) {
  int b = 0;
  while (b < kPingBandCount - 1 && ping_ms >= kPingBands[b]) b++;
  return b;
}

void matches_trust(Matches& m, const net::Key& server) { m.trusted.insert(server); }

Taken matches_add(Matches& m, const net::Key& server, const net::MatchReport& r) {
  const Taken t = take(m, server, r);
  if (t == Taken::again) return t;
  std::vector<uint8_t> bytes;
  std::string error;
  if (net::report_write(r, bytes, error))
    m.log += "report " + hex_of(server.data(), server.size()) + " " +
             hex_of(bytes.data(), bytes.size()) + "\n";
  return t;
}

DuoAnswer matches_duo_ask(Matches& m, uint64_t asker, uint64_t partner, std::string_view name) {
  const DuoAnswer a = ladder_duo_ask(m.ladder, asker, partner, name);
  m.log += m.ladder.log;
  m.ladder.log.clear();
  return a;
}

bool matches_line(Matches& m, const std::string& line, std::string& error) {
  if (line.rfind("duo ", 0) == 0) return ladder_line(m.ladder, line, error);
  const size_t a = line.find(' '), b = line.find(' ', a + 1);
  if (line.compare(0, a, "report") != 0 || a == std::string::npos || b == std::string::npos) {
    error = "not a report line";
    return false;
  }
  std::vector<uint8_t> key_bytes, bytes;
  if (!bytes_of(std::string_view(line).substr(a + 1, b - a - 1), key_bytes) ||
      key_bytes.size() != 32) {
    error = "the server's key is not 64 hex digits";
    return false;
  }
  if (!bytes_of(std::string_view(line).substr(b + 1), bytes)) {
    error = "the report is not hex";
    return false;
  }
  net::MatchReport r;
  if (!net::report_read(bytes.data(), bytes.size(), r, error)) return false;
  net::Key server{};
  std::copy(key_bytes.begin(), key_bytes.end(), server.begin());
  take(m, server, r);
  return true;
}

bool totals_keeps(const TotalsFilter& f, const TotalsKey& key) {
  const auto has = [](const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
  };
  const auto& [id, game, mode, map, band] = key;
  if (f.ping_under > 0 && (band >= kPingBandCount - 1 || kPingBands[band] > f.ping_under))
    return false;
  if (!f.game.empty() && game != f.game) return false;
  if (!f.map.empty() && map != f.map) return false;
  return !f.modes.empty() ? has(f.modes, mode) : !has(f.without, mode);
}

void totals_add(Totals& to, const Totals& t) {
  to.matches += t.matches;
  to.wins += t.wins;
  to.score += t.score;
  to.frags += t.frags;
  to.deaths += t.deaths;
  to.suicides += t.suicides;
  to.team_kills += t.team_kills;
  to.seconds += t.seconds;
  for (const auto& [name, v] : t.columns) add_column(to.columns, name, v);
}

Totals totals_sum(const Matches& m, uint64_t account, const TotalsFilter& f) {
  Totals sum;
  for (auto it = m.totals.lower_bound({account, "", "", "", 0});
       it != m.totals.end() && std::get<0>(it->first) == account; ++it)
    if (totals_keeps(f, it->first)) totals_add(sum, it->second);
  return sum;
}

std::map<uint64_t, Totals> totals_by_account(const Matches& m, const TotalsFilter& f,
                                             int64_t since_s) {
  std::map<TotalsKey, Totals> recent;
  if (since_s > 0)
    for (const KeptMatch& k : m.kept)
      if (k.counted && k.report.start_s >= since_s) add_totals(recent, k.report);
  std::map<uint64_t, Totals> out;
  for (const auto& [key, t] : since_s > 0 ? recent : m.totals)
    if (totals_keeps(f, key)) totals_add(out[std::get<0>(key)], t);
  return out;
}

bool matches_link_demo(Matches& m, uint64_t id, int64_t start_s, std::string_view server,
                       std::string_view sha) {
  if (id == 0 || sha.size() != 64) return false;  // no summary, or not a demo's name
  for (KeptMatch& k : m.kept) {
    if (k.report.id != id || k.report.start_s != start_s || k.report.server != server) continue;
    k.report.demo_sha256 = std::string(sha);
    return true;
  }
  return false;
}

}  // namespace ge::account
