// The archive's paths, defined: the search and a demo's summary as JSON, the file itself.
//
// ge:summary The hub archive's paths: /v1/demos search, a demo's summary, the file by hash
// ge:keywords hub archive api demos json download sha256 immutable cache
// ge:skill net
// ge:test tests/hub_archive_test.cpp
#include <filesystem>
#include <fstream>
#include <iterator>

#include "core/json_write.hpp"
#include "hub/archive.hpp"

namespace ge::hub {

namespace fs = std::filesystem;
using ge::core::json_arr;
using ge::core::json_bool;
using ge::core::json_num;
using ge::core::json_obj;
using ge::core::json_str;
using ge::core::json_strs;

namespace {

constexpr int kYearS = 31536000;

Answer json_answer(int code, std::string body, int max_age_s) {
  Answer a;
  a.code = code;
  a.body = std::move(body);
  a.max_age_s = max_age_s;
  return a;
}

Answer missing(const std::string& what, const std::string& next) {
  return json_answer(404, json_obj({{"error", json_str(what)}, {"next", json_str(next)}}), 0);
}

bool is_sha(std::string_view s) {
  if (s.size() != 64) return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

std::string row_json(const DemoRow& r, bool scores) {
  std::vector<std::string> players, teams;
  players.reserve(r.players.size());
  teams.reserve(r.teams.size());
  for (const DemoPlayer& p : r.players) {
    std::vector<std::pair<std::string, std::string>> m = {{"name", json_str(p.name)},
                                                          {"team", json_str(p.team)}};
    if (scores && p.scored) m.emplace_back("score", json_num(p.score));
    players.push_back(json_obj(m));
  }
  for (const DemoTeam& t : r.teams) {
    std::vector<std::pair<std::string, std::string>> m = {{"name", json_str(t.name)}};
    if (scores && t.scored) m.emplace_back("score", json_num(t.score));
    teams.push_back(json_obj(m));
  }
  return json_obj({{"sha256", json_str(r.sha)},
                   {"game", json_str(r.game)},
                   {"game_version", json_num(r.game_version)},
                   {"map", json_str(r.map)},
                   {"mode", json_str(r.mode)},
                   {"server", json_str(r.server)},
                   {"title", json_str(r.title)},
                   {"start", json_num(r.start_s)},
                   {"duration_ms", json_num(r.duration_ms)},
                   {"complete", json_bool(r.duration_ms >= 0)},
                   {"size", json_num(r.size)},
                   {"added", json_num(r.added_s)},
                   {"tags", json_strs(r.tags)},
                   {"players", json_arr(players)},
                   {"teams", json_arr(teams)},
                   {"file", json_str("/" + archive_path(r.sha))}});
}

}  // namespace

std::string demo_json(const DemoRow& r) { return row_json(r, true); }

bool archive_answer(const Archive* a, std::string_view target, int64_t now_s, Answer& out) {
  std::string path = target_path(target);
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  const bool api = path == "/v1/demos" || path.rfind("/v1/demos/", 0) == 0;
  const bool file = path.rfind("/d/", 0) == 0;
  if (!api && !file) return false;
  if (!a) {
    out = missing("this hub keeps no demo archive",
                  "start the hub with --archive=<directory> to keep and serve demos");
    return true;
  }
  if (path == "/v1/demos") {
    const DemoPage page = archive_search(*a, demo_query(target), now_s);
    std::vector<std::string> rows;
    rows.reserve(page.rows.size());
    // Scores are left out of the list, so a reader is not told the result before watching.
    for (const DemoRow* r : page.rows) rows.push_back(row_json(*r, false));
    out = json_answer(200,
                      json_obj({{"total", json_num(page.total)},
                                {"page", json_num(page.page)},
                                {"pages", json_num(page.pages)},
                                {"per_page", json_num(kDemosPerPage)},
                                {"demos", json_arr(rows)}}),
                      30);
    return true;
  }
  if (api) {
    const std::string sha = path.substr(std::string("/v1/demos/").size());
    const DemoRow* r = is_sha(sha) ? archive_find(*a, sha) : nullptr;
    out = r ? json_answer(200, demo_json(*r), kYearS)
            : missing("no demo " + sha + " in this archive",
                      "GET /v1/demos lists the demos; a demo is named by its SHA-256");
    return true;
  }
  // /d/{abc}/{sha}.ger or .json
  const size_t slash = path.find('/', 3);
  const size_t dot = path.rfind('.');
  const std::string sha = slash != std::string::npos && dot != std::string::npos && dot > slash
                              ? path.substr(slash + 1, dot - slash - 1)
                              : "";
  const std::string ext = dot != std::string::npos ? path.substr(dot) : "";
  const DemoRow* r = is_sha(sha) && path == "/" + archive_path(sha).substr(0, 6) + sha + ext
                         ? archive_find(*a, sha)
                         : nullptr;
  if (!r || (ext != ".ger" && ext != ".json")) {
    out = missing("no file " + path + " in this archive",
                  "GET /v1/demos lists the demos and each one's file");
    return true;
  }
  if (ext == ".json") {
    out = json_answer(200, demo_json(*r), kYearS);
    return true;
  }
  std::ifstream f(fs::path(a->dir) / archive_path(sha), std::ios::binary);
  if (!f) {
    out = missing("the file of demo " + sha + " is gone from the disk",
                  "the operator restores it under the archive's d/ or restarts the hub");
    return true;
  }
  out = Answer{};
  out.code = 200;
  out.type = "application/octet-stream";
  out.body.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  out.max_age_s = kYearS;
  return true;
}

}  // namespace ge::hub
