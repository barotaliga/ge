// The hub's Ladder page and its JSON (ladder_page.hpp).
//
// ge:summary The hub's Ladder page defined: the ladder links, standings, champions, the JSON
// ge:keywords hub ladder-page standings champions ladder_answer
// ge:skill net
// ge:test tests/hub_ladder_page_test.cpp
#include "hub/ladder_page.hpp"

#include <string>
#include <vector>

#include "account/ladder.hpp"
#include "account/store.hpp"
#include "core/json_write.hpp"
#include "hub/page_parts.hpp"
#include "hub/pages.hpp"
#include "hub/stats_page.hpp"

namespace ge::hub {

namespace ga = ge::account;
using core::json_arr;
using core::json_num;
using core::json_obj;
using core::json_str;

namespace {

constexpr int64_t kDay_s = 86400;

std::string whole(int64_t tenths) { return std::to_string((tenths + (tenths >= 0 ? 5 : -5)) / 10); }

std::string record(const ga::Standing& s) {
  return std::to_string(s.wins) + "-" + std::to_string(s.losses) + "-" + std::to_string(s.draws);
}

std::string account_name(const ga::Service& svc, uint64_t id) {
  const ga::Account* a = ga::find(svc.store, id);
  return a && a->removed_s == 0 ? a->name : "(gone)";
}

std::string player_link(const ga::Service& svc, uint64_t id) {
  const std::string n = account_name(svc, id);
  return "<a href=\"/player/" + url_text(n) + "\">" + html_escape(n) + "</a>";
}

// An entry's name in a page: an account's link, or a duo's name with its two players' links.
std::string who_html(const ga::Service& svc, ga::LadderKind k, uint64_t who) {
  if (k == ga::LadderKind::duel) return player_link(svc, who);
  const ga::Duo* d = ga::duo_by_id(svc.matches.ladder, who);
  if (!d) return "(gone)";
  return "<b>" + html_escape(d->name) + "</b> <small>" + player_link(svc, d->a) + " &amp; " +
         player_link(svc, d->b) + "</small>";
}

std::string who_text(const ga::Service& svc, ga::LadderKind k, uint64_t who) {
  if (k == ga::LadderKind::duel) return account_name(svc, who);
  const ga::Duo* d = ga::duo_by_id(svc.matches.ladder, who);
  return d ? d->name : "(gone)";
}

std::string page(const HubSpec& spec, const std::string& title, const std::string& main) {
  return head(spec, title, false) + nav(spec, "Ladder") + "<main class=\"stats\">" + main +
         "</main>" + foot();
}

std::string key_query(const ga::LadderKey& k) {
  return "game=" + url_text(std::get<0>(k)) + "&mode=" + url_text(std::get<1>(k));
}

std::string day_text(int64_t unix_s) { return utc_text(unix_s).substr(0, 10); }

}  // namespace

Answer ladder_answer(const HubSpec& spec, const account::Service& svc, std::string_view target,
                     int64_t unix_s, bool api) {
  const ga::Ladder& l = svc.matches.ladder;
  const std::string game = query_param(target, "game"), mode = query_param(target, "mode");
  const ga::LadderKey* key = nullptr;
  const ga::LadderTable* table = nullptr;
  for (const auto& [k, t] : l.tables)
    if ((game.empty() || std::get<0>(k) == game) && (mode.empty() || std::get<1>(k) == mode)) {
      key = &k;
      table = &t;
      break;
    }
  const int64_t season = ga::season_of(l, unix_s);
  const int64_t ends_s = ga::season_start(l, season + 1);
  // The season standings show only when the ladder's season is the one it is now.
  const bool current = l.season == season;
  if (api) {
    Answer a;
    a.max_age_s = 30;
    if (mode.empty() && game.empty()) {
      std::vector<std::string> v;
      v.reserve(l.tables.size());
      for (const auto& [k, t] : l.tables)
        v.push_back(json_obj({{"game", json_str(std::get<0>(k))},
                              {"mode", json_str(std::get<1>(k))},
                              {"kind", json_str(ga::ladder_kind_name(std::get<2>(k)))},
                              {"entries", json_num(static_cast<long long>(t.all_time.size()))}}));
      a.body = json_obj({{"season", json_num(season)},
                         {"season_ends", json_num(ends_s)},
                         {"ladders", json_arr(v)}});
      return a;
    }
    if (!table) {
      a.code = 404;
      a.max_age_s = 0;
      a.body = json_obj({{"error", json_str("No ladder for that game and mode on this hub.")}});
      return a;
    }
    const ga::LadderKind kind = std::get<2>(*key);
    const auto rows = [&](const std::vector<std::pair<uint64_t, ga::Standing>>& order) {
      std::vector<std::string> v;
      v.reserve(order.size());
      for (const auto& [who, s] : order)
        v.push_back(json_obj({{"name", json_str(who_text(svc, kind, who))},
                              {"rating", json_num(s.rating)},  // tenths
                              {"games", json_num(s.games)},
                              {"wins", json_num(s.wins)},
                              {"losses", json_num(s.losses)},
                              {"draws", json_num(s.draws)}}));
      return json_arr(v);
    };
    std::vector<std::string> past;
    for (const ga::SeasonRecord& r : l.history) {
      if (r.game != std::get<0>(*key) || r.mode != std::get<1>(*key) || r.kind != kind) continue;
      std::vector<std::string> champs;
      champs.reserve(r.champions.size());
      for (const ga::Champion& c : r.champions)
        champs.push_back(json_obj({{"name", json_str(who_text(svc, kind, c.who))},
                                   {"rating", json_num(c.rating)},
                                   {"games", json_num(c.games)}}));
      past.push_back(json_obj({{"season", json_num(r.season)},
                               {"start", json_num(r.start_s)},
                               {"end", json_num(r.end_s)},
                               {"champions", json_arr(champs)}}));
    }
    a.body = json_obj(
        {{"game", json_str(std::get<0>(*key))},
         {"mode", json_str(std::get<1>(*key))},
         {"kind", json_str(ga::ladder_kind_name(kind))},
         {"season", json_num(season)},
         {"season_ends", json_num(ends_s)},
         {"min_games", json_num(ga::kSeasonMinGames)},
         {"season_standings", current ? rows(ga::season_order(*table)) : std::string("[]")},
         {"all_time", rows(ga::all_time_order(*table))},
         {"past_seasons", json_arr(past)}});
    return a;
  }

  std::string m = "<h2>Ladder</h2>";
  if (!table) {
    m += "<p class=\"none\">No rated matches yet. A match is rated when it is one against one, or "
         "two registered duos against each other, every player logged in. Register a duo on "
         "<a href=\"/account\">your account</a>.</p>";
    return html(200, page(spec, "Ladder", m), 30);
  }
  const ga::LadderKind kind = std::get<2>(*key);
  m += "<div class=\"filters\">";
  for (const auto& [k, t] : l.tables)
    m += "<a href=\"/ladder?" + key_query(k) + "\"" + (&k == key ? " class=\"on\"" : "") + ">" +
         html_escape(std::get<1>(k)) + "</a>";
  m += "</div>";
  const int64_t days_left = (ends_s - unix_s + kDay_s - 1) / kDay_s;
  m += "<h3>Season " + std::to_string(season) + "</h3><p class=\"sum\">" +
       day_text(ga::season_start(l, season)) + " to " + day_text(ends_s - 1) + ", " +
       std::to_string(days_left) + (days_left == 1 ? " day" : " days") +
       " left. Every season the ratings start again at 1500; the best three with " +
       std::to_string(ga::kSeasonMinGames) + " matches or more are its champions.</p>";
  const auto table_html = [&](const std::vector<std::pair<uint64_t, ga::Standing>>& order,
                              size_t shown, bool season_table) {
    std::string h = "<table class=\"num rank\"><tr><th>#</th><th>" +
                    std::string(kind == ga::LadderKind::duel ? "player" : "duo") +
                    "</th><th>rating</th><th>won-lost-drawn</th><th>matches</th></tr>";
    size_t rank = 0;
    for (const auto& [who, s] : order) {
      if (rank >= shown) break;
      const bool ranked = !season_table || s.games >= ga::kSeasonMinGames;
      rank++;
      h += std::string("<tr") + (ranked ? "" : " class=\"unranked\"") + "><td>" +
           (ranked ? std::to_string(rank) : std::string("-")) + "</td><td>" +
           who_html(svc, kind, who) + "</td><td>" + whole(s.rating) + "</td><td>" + record(s) +
           "</td><td>" + std::to_string(s.games) +
           (ranked ? std::string()
                   : " <small>" + std::to_string(ga::kSeasonMinGames - s.games) +
                         " more to rank</small>") +
           "</td></tr>";
    }
    return h + "</table>";
  };
  if (current && !table->season.empty())
    m += table_html(ga::season_order(*table), 200, true);
  else
    m += "<p class=\"none\">No rated matches this season yet.</p>";
  m += "<h3>All time</h3>" + table_html(ga::all_time_order(*table), kAllTimeShown, false);
  m += "<h3>Past champions</h3>";
  std::string past;
  for (auto it = l.history.rbegin(); it != l.history.rend(); ++it) {
    const ga::SeasonRecord& r = *it;
    if (r.game != std::get<0>(*key) || r.mode != std::get<1>(*key) || r.kind != kind) continue;
    past += "<tr><td>" + std::to_string(r.season) + "</td><td>" + day_text(r.start_s) + " to " +
            day_text(r.end_s - 1) + "</td>";
    for (size_t i = 0; i < ga::kChampionsKept; i++)
      past += "<td>" +
              (i < r.champions.size() ? who_html(svc, kind, r.champions[i].who) + " <small>" +
                                            whole(r.champions[i].rating) + "</small>"
                                      : std::string("-")) +
              "</td>";
    past += "</tr>";
  }
  m += past.empty() ? "<p class=\"none\">The first season has not ended yet.</p>"
                    : "<table class=\"num champions\"><tr><th>season</th><th>dates</th>"
                      "<th>first</th><th>second</th><th>third</th></tr>" +
                          past + "</table>";
  return html(200, page(spec, "Ladder: " + std::get<1>(*key), m), 30);
}

}  // namespace ge::hub
