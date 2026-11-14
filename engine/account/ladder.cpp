// The ranked ladders (ladder.hpp): Elo per game and mode, duos, seasons and their champions.
//
// ge:summary The ranked ladders defined: the rated match, Elo in tenths, duos, seasons closed
// ge:keywords ladder elo rating duo season champions ladder_add
// ge:skill net
// ge:test tests/account_ladder_test.cpp
#include "account/ladder.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::account {

namespace {

constexpr int64_t kDay_s = 86400;
constexpr char kHex[] = "0123456789abcdef";

int64_t floor_div(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

std::string fold(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

bool name_ok(std::string_view s) {
  if (s.empty() || s.size() > kDuoName || s.front() == ' ' || s.back() == ' ') return false;
  for (char c : s) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == ' ' || c == '-' || c == '.' || c == '_';
    if (!ok) return false;
  }
  return true;
}

int64_t round_half_away(double x) { return static_cast<int64_t>(x >= 0 ? x + 0.5 : x - 0.5); }

// One side's move, tenths: K (by the entry's games) times (result less expected), in tenths.
int64_t elo_move(const Standing& self, int64_t theirs, double result) {
  const double expected =
      1.0 / (1.0 + core::libm::pow(10.0, static_cast<double>(theirs - self.rating) / 4000.0));
  const int64_t k = self.games < kKNewGames ? kKNew : kKSettled;
  return round_half_away(static_cast<double>(k) * 10.0 * (result - expected));
}

void rate(std::map<uint64_t, Standing>& table, uint64_t a, uint64_t b, double result_a,
          int64_t start_s) {
  Standing& sa = table[a];
  Standing& sb = table[b];
  const int64_t ra = sa.rating, rb = sb.rating;
  const int64_t da = elo_move(sa, rb, result_a), db = elo_move(sb, ra, 1.0 - result_a);
  for (auto [s, d, res] : {std::tuple{&sa, da, result_a}, std::tuple{&sb, db, 1.0 - result_a}}) {
    s->rating += d;
    s->games++;
    if (res > 0.75)
      s->wins++;
    else if (res < 0.25)
      s->losses++;
    else
      s->draws++;
    s->last_s = start_s;
  }
}

void close_season(Ladder& l) {
  if (l.season < 0) return;
  for (auto& [key, t] : l.tables) {
    if (t.season.empty()) continue;
    SeasonRecord rec;
    rec.game = std::get<0>(key);
    rec.mode = std::get<1>(key);
    rec.kind = std::get<2>(key);
    rec.season = l.season;
    rec.start_s = season_start(l, l.season);
    rec.end_s = season_start(l, l.season + 1);
    for (const auto& [who, s] : season_order(t)) {
      if (s.games < kSeasonMinGames || rec.champions.size() >= kChampionsKept) break;
      rec.champions.push_back({who, s.rating, s.games, s.wins});
    }
    if (!rec.champions.empty()) l.history.push_back(std::move(rec));
    t.season.clear();
  }
}

// Move the season on to `season` when it is later, closing the ones behind.
void reach_season(Ladder& l, int64_t season) {
  if (season <= l.season) return;
  close_season(l);
  l.season = season;
}

std::string hex_of(std::string_view s) {
  std::string out;
  for (unsigned char c : s) {
    out += kHex[c >> 4];
    out += kHex[c & 15];
  }
  return out;
}

int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool unsigned_of(std::string_view s, uint64_t& out) {
  if (s.empty() || s.size() > 19) return false;
  out = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
    out = out * 10 + static_cast<uint64_t>(c - '0');
  }
  return true;
}

DuoAnswer ask(Ladder& l, uint64_t asker, uint64_t partner, std::string_view name, bool logged) {
  if (asker == partner || asker == 0 || partner == 0) return DuoAnswer::self;
  if (!name_ok(name)) return DuoAnswer::bad_name;
  const std::string key = fold(name);
  for (const Duo& d : l.duos)
    if (fold(d.name) == key)
      return (d.a == std::min(asker, partner) && d.b == std::max(asker, partner))
                 ? DuoAnswer::already
                 : DuoAnswer::taken;
  if (!logged)
    l.log +=
        "duo " + std::to_string(asker) + " " + std::to_string(partner) + " " + hex_of(name) + "\n";
  const auto back = l.asks.find({partner, asker});
  if (back != l.asks.end() && fold(back->second) == key) {
    Duo d;
    d.id = l.duos.size() + 1;
    d.a = std::min(asker, partner);
    d.b = std::max(asker, partner);
    d.name = back->second;  // as the first of the two wrote it
    l.duos.push_back(d);
    l.asks.erase(back);
    l.asks.erase({asker, partner});
    return DuoAnswer::made;
  }
  l.asks[{asker, partner}] = std::string(name);
  return DuoAnswer::asked;
}

}  // namespace

const char* ladder_kind_name(LadderKind k) { return k == LadderKind::duel ? "duel" : "pair"; }

int64_t season_of(const Ladder& l, int64_t unix_s) {
  return floor_div(unix_s - kSeasonEpoch_s, std::max(1, l.season_days) * kDay_s);
}

int64_t season_start(const Ladder& l, int64_t season) {
  return kSeasonEpoch_s + season * std::max(1, l.season_days) * kDay_s;
}

const char* duo_answer_words(DuoAnswer a) {
  switch (a) {
    case DuoAnswer::asked:
      return "Asked. The duo is made when your partner asks for you under the same name.";
    case DuoAnswer::made:
      return "Your duo is made. Its two-on-two matches now count on the ladder.";
    case DuoAnswer::already:
      return "You two already are that duo.";
    case DuoAnswer::taken:
      return "Another duo has that name. Choose another.";
    case DuoAnswer::bad_name:
      return "A duo's name is 1 to 24 letters, digits, spaces, dashes, dots or underscores.";
    case DuoAnswer::self:
      return "A duo needs a partner other than you.";
  }
  return "";
}

DuoAnswer ladder_duo_ask(Ladder& l, uint64_t asker, uint64_t partner, std::string_view name) {
  return ask(l, asker, partner, name, false);
}

const Duo* duo_of(const Ladder& l, uint64_t a, uint64_t b) {
  const uint64_t lo = std::min(a, b), hi = std::max(a, b);
  for (const Duo& d : l.duos)
    if (d.a == lo && d.b == hi) return &d;
  return nullptr;
}

const Duo* duo_by_id(const Ladder& l, uint64_t id) {
  return id >= 1 && id <= l.duos.size() ? &l.duos[id - 1] : nullptr;
}

bool ladder_add(Ladder& l, const net::MatchReport& r) {
  // The two sides: by team, or one player each when there are no teams.
  struct Side {
    std::vector<uint64_t> who;
    int64_t score = 0;
  };
  std::vector<Side> sides;
  for (const net::ReportPlayer& p : r.players)
    if (p.bot || p.account == 0) return false;
  if (r.teams.empty()) {
    if (r.players.size() != 2) return false;
    for (const net::ReportPlayer& p : r.players) sides.push_back({{p.account}, p.score});
  } else {
    if (r.teams.size() != 2) return false;
    for (const net::ReportTeam& t : r.teams) {
      Side s;
      s.score = t.score;
      for (const net::ReportPlayer& p : r.players)
        if (p.team == t.name) s.who.push_back(p.account);
      sides.push_back(s);
    }
    size_t on_sides = sides[0].who.size() + sides[1].who.size();
    if (on_sides != r.players.size()) return false;
  }
  const size_t n = sides[0].who.size();
  if (n != sides[1].who.size() || n < 1 || n > 2) return false;
  uint64_t a = 0, b = 0;
  LadderKind kind = LadderKind::duel;
  if (n == 1) {
    a = sides[0].who[0];
    b = sides[1].who[0];
    if (a == b) return false;
  } else {
    const Duo* da = duo_of(l, sides[0].who[0], sides[0].who[1]);
    const Duo* db = duo_of(l, sides[1].who[0], sides[1].who[1]);
    if (!da || !db || da == db) return false;
    a = da->id;
    b = db->id;
    kind = LadderKind::pair;
  }
  const double result = sides[0].score > sides[1].score   ? 1.0
                        : sides[0].score < sides[1].score ? 0.0
                                                          : 0.5;
  const int64_t season = season_of(l, r.start_s);
  reach_season(l, season);
  LadderTable& t = l.tables[{r.game, r.mode, kind}];
  rate(t.all_time, a, b, result, r.start_s);
  if (season == l.season) rate(t.season, a, b, result, r.start_s);
  l.rated++;
  return true;
}

void ladder_close_due(Ladder& l, int64_t unix_s) {
  if (l.season >= 0) reach_season(l, season_of(l, unix_s));
}

bool ladder_line(Ladder& l, const std::string& line, std::string& error) {
  const size_t a = line.find(' '), b = line.find(' ', a + 1), c = line.find(' ', b + 1);
  uint64_t asker = 0, partner = 0;
  if (line.compare(0, a, "duo") != 0 || c == std::string::npos ||
      !unsigned_of(std::string_view(line).substr(a + 1, b - a - 1), asker) ||
      !unsigned_of(std::string_view(line).substr(b + 1, c - b - 1), partner)) {
    error = "not a duo line ('duo <account> <partner> <name in hex>')";
    return false;
  }
  const std::string_view hex = std::string_view(line).substr(c + 1);
  std::string name;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    const int hi = nibble(hex[i]), lo = nibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      error = "the duo's name is not hex";
      return false;
    }
    name += static_cast<char>(hi * 16 + lo);
  }
  ask(l, asker, partner, name, true);
  return true;
}

namespace {

std::vector<std::pair<uint64_t, Standing>> ordered(const std::map<uint64_t, Standing>& m,
                                                   bool ranked_first) {
  std::vector<std::pair<uint64_t, Standing>> v(m.begin(), m.end());
  std::stable_sort(v.begin(), v.end(), [&](const auto& x, const auto& y) {
    if (ranked_first) {
      const bool rx = x.second.games >= kSeasonMinGames, ry = y.second.games >= kSeasonMinGames;
      if (rx != ry) return rx;
    }
    if (x.second.rating != y.second.rating) return x.second.rating > y.second.rating;
    return x.second.games > y.second.games;
  });
  return v;
}

}  // namespace

std::vector<std::pair<uint64_t, Standing>> season_order(const LadderTable& t) {
  return ordered(t.season, true);
}

std::vector<std::pair<uint64_t, Standing>> all_time_order(const LadderTable& t) {
  return ordered(t.all_time, false);
}

}  // namespace ge::account
