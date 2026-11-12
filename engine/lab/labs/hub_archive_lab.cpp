// LAB `hub-archive`: the hub's demo archive (docs/hub.md § The archive) with a month of demos. Five
// hundred replay files (version 2) are written the way a recorder writes them, with the hub's meta
// words: a map, a mode, a server, a start, the players and teams, and a trailer with the final
// scores (a few cut short, a few tagged for a league), some sent twice. The archive keeps them by
// hash, answers the Demos page's searches (by player, mode, map, team, tag, age, a page at a time)
// and prunes to two weeks. It answers whether every file is kept once, whether each search finds
// what the script says it should, and what a search costs; the Demos page is drawn by a headless
// Chromium when the machine has one (measured).
//
// ge:summary Lab: a month of demos through the hub's archive; dedup, searches checked and timed
// ge:keywords lab hub archive demos search player sha256 dedup prune timing chromium
// ge:skill net labs
// ge:test tests/hub_archive_test.cpp
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "hub/archive.hpp"
#include "hub/pages.hpp"
#include "lab/lab.hpp"
#include "lab/labs/hub_browser.hpp"
#include "replay/stream.hpp"

namespace {

namespace gl = ge::lab;
namespace gh = ge::hub;
namespace gr = ge::replay;
namespace fs = std::filesystem;

constexpr int64_t kNow = 1794450000;  // the evening the month ends on
const char* const kNames[] = {"vale",   "sable", "orrin",  "wren",    "kit",     "moss",
                              "ash",    "fen",   "quill",  "brand",   "pike",    "rook",
                              "lark",   "dune",  "nyx",    "oak",     "ivy",     "elm",
                              "tamsin", "cole",  "Marrow", "Thistle", "juniper", "Bram",
                              "Corin",  "Elske", "Hollis", "Isolde",  "Jory",    "Kestrel"};
const char* const kMaps[] = {"tower", "crypt", "bridges", "foundry", "aqueduct", "cellar"};
struct ModeDef {
  const char* name;
  int players;
  bool teams;
};
constexpr ModeDef kModes[] = {
    {"4on4", 8, true}, {"2on2", 4, true}, {"1on1", 2, false}, {"ctf", 8, true}, {"ffa", 6, false}};
const char* const kServers[] = {"Night Arena", "Duel Hall", "Flag Yard", "Open Pit", "Team Two"};

uint64_t mix(uint64_t x) {
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

struct Made {
  std::vector<uint8_t> bytes;
  std::vector<std::string> players;
  std::string mode, map;
  int64_t start = 0;
  bool tagged = false;
};

std::string tabbed(const std::string& a, const std::string& b) {
  std::string s = a;
  s += '\t';
  s += b;
  return s;
}

Made make_demo(uint64_t seed, int i) {
  const uint64_t r = mix(seed * 1000003u + static_cast<uint64_t>(i));
  const ModeDef& m = kModes[r % std::size(kModes)];
  Made d;
  d.mode = m.name;
  d.map = kMaps[(r >> 8) % std::size(kMaps)];
  d.start = kNow - static_cast<int64_t>((r >> 16) % (uint64_t{30} * 86400));
  d.tagged = (r >> 40) % 20 == 0;
  const bool cut = (r >> 45) % 33 == 0;
  gr::StreamHeader h;
  h.game = "shooters_edge";
  h.meta = {{"map", d.map},
            {"mode", d.mode},
            {"server", kServers[(r >> 50) % std::size(kServers)]},
            {"start", std::to_string(d.start)}};
  if (m.teams) {
    h.meta.emplace_back("team", "red");
    h.meta.emplace_back("team", "blue");
  }
  if (d.tagged) h.meta.emplace_back("tag", "league");
  const size_t first = (r >> 24) % std::size(kNames);
  for (int k = 0; k < m.players; k++) {
    const std::string name = kNames[(first + static_cast<size_t>(k) * 7) % std::size(kNames)];
    d.players.push_back(name);
    h.meta.emplace_back("player", m.teams ? tabbed(name, k % 2 ? "blue" : "red") : name);
  }
  h.tracks = {{1, gr::TrackKind::meta, gr::TrackCodec::stored, "meta"},
              {2, gr::TrackKind::messages, gr::TrackCodec::zstd, "messages"}};
  gr::StreamWriter w;
  gr::stream_begin(w, h, d.bytes);
  const int seconds = 60 + static_cast<int>((r >> 30) % 600);
  for (int s = 0; s < seconds; s += 10) {
    std::vector<uint8_t> chunk(64);
    for (size_t b = 0; b < chunk.size(); b++) {
      chunk[b] = static_cast<uint8_t>(mix(r + static_cast<uint64_t>(s) * 64 + b) & 0x0f);
    }
    gr::stream_add(w, 2, int64_t{s} * 1000, chunk);
    gr::stream_end_block(w, s % 30 == 0);
  }
  if (!cut) {
    gr::Meta fin = {{"duration_ms", std::to_string(int64_t{seconds} * 1000)}};
    for (size_t k = 0; k < d.players.size(); k++) {
      fin.emplace_back("score", tabbed(d.players[k], std::to_string(mix(r + k) % 50)));
    }
    gr::stream_finish(w, fin);
  }
  return d;
}

void run(gl::Lab& lab) {
  const int count = std::stoi(lab.arg("demos"));
  const uint64_t seed = std::stoull(lab.arg("seed"));
  gh::Archive a;  // in memory: the files' bytes are hashed, not written
  std::vector<Made> made;
  made.reserve(static_cast<size_t>(count));
  int sent = 0;
  for (int i = 0; i < count; i++) {
    made.push_back(make_demo(seed, i));
    std::string error;
    const gh::AddResult got = gh::archive_add(a, made.back().bytes, kNow, {}, error);
    if (got != gh::AddResult::added)
      lab.fail("demo " + std::to_string(i) + " not kept: " + error,
               "next: read archive_add's refusal (engine/hub/archive.cpp)");
    sent++;
    if (i % 25 == 0) {  // the same demo from a second TV node
      gh::archive_add(a, made.back().bytes, kNow, {}, error);
      sent++;
    }
  }
  lab.metric("demos_sent", sent, "files");
  lab.metric("demos_kept", static_cast<double>(a.rows.size()), "demos");
  lab.metric("duplicates", static_cast<double>(a.stats.duplicates), "files");
  lab.metric("archive_size", static_cast<double>(a.bytes) / 1e6, "MB");
  if (static_cast<int>(a.rows.size()) != count) {
    lab.fail(
        "the archive keeps " + std::to_string(a.rows.size()) + " demos of " + std::to_string(count),
        "next: a duplicate was kept twice or a demo lost; check archive_add's hash match");
  }

  // Each search against the script's own count.
  struct Search {
    const char* name;
    gh::DemoQuery q;
  };
  std::vector<Search> searches;
  auto q_of = [](auto set) {
    gh::DemoQuery q;
    set(q);
    return q;
  };
  searches.push_back({"everything", gh::DemoQuery{}});
  searches.push_back({"player=quill", q_of([](gh::DemoQuery& q) { q.player = "quill"; })});
  searches.push_back(
      {"player=MAR (part, any case)", q_of([](gh::DemoQuery& q) { q.player = "MAR"; })});
  searches.push_back({"mode=4on4", q_of([](gh::DemoQuery& q) { q.mode = "4on4"; })});
  searches.push_back({"mode=1on1 map=crypt", q_of([](gh::DemoQuery& q) {
                        q.mode = "1on1";
                        q.map = "crypt";
                      })});
  searches.push_back({"tag=league", q_of([](gh::DemoQuery& q) { q.tag = "league"; })});
  searches.push_back({"days=7", q_of([](gh::DemoQuery& q) { q.days = 7; })});
  searches.push_back({"team=blue player=vale", q_of([](gh::DemoQuery& q) {
                        q.team = "blue";
                        q.player = "vale";
                      })});
  gl::LabTable found{"SEARCHES", {"search", "found", "expected", "pages", "newest"}, {}};
  gl::LabTable costs{"WHAT A SEARCH COSTS", {"search", "us per search"}, {}};
  for (const Search& s : searches) {
    int expected = 0;
    for (const Made& d : made) {
      const gh::DemoQuery& q = s.q;
      if (!q.mode.empty() && d.mode != q.mode) continue;
      if (!q.map.empty() && d.map != q.map) continue;
      if (!q.tag.empty() && !d.tagged) continue;
      if (q.days > 0 && kNow - d.start > int64_t{86400} * q.days) continue;
      if (!q.team.empty() && (d.mode == "1on1" || d.mode == "ffa")) continue;
      if (!q.player.empty()) {
        std::string want = q.player;
        std::transform(want.begin(), want.end(), want.begin(), [](char c) {
          return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        });
        const bool hit = std::any_of(d.players.begin(), d.players.end(), [&](std::string n) {
          std::transform(n.begin(), n.end(), n.begin(), [](char c) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          });
          return n.find(want) != std::string::npos;
        });
        if (!hit) continue;
      }
      expected++;
    }
    const gh::DemoPage page = gh::archive_search(a, s.q, kNow);
    found.rows.push_back({s.name, std::to_string(page.total), std::to_string(expected),
                          std::to_string(page.pages),
                          page.rows.empty() ? "-" : gh::utc_text(page.rows[0]->start_s)});
    if (page.total != expected) {
      lab.fail(std::string("search ") + s.name + " found " + std::to_string(page.total) +
                   ", the script has " + std::to_string(expected),
               "next: check archive_search's filter for that field");
    }
    constexpr int kRuns = 200;
    const auto t0 = std::chrono::steady_clock::now();
    int sink = 0;
    for (int k = 0; k < kRuns; k++) sink += gh::archive_search(a, s.q, kNow).total;
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() /
        kRuns;
    costs.rows.push_back(
        {s.name, std::to_string(static_cast<int64_t>(std::llround(us))) + (sink < 0 ? "?" : "")});
  }
  lab.table(found);
  lab.table(costs, gl::Det::measured);

  // The Demos page, as JSON and as HTML.
  gh::Answer out;
  gh::archive_answer(&a, "/v1/demos?player=quill", kNow, out);
  lab.metric("demos_json_page", static_cast<double>(out.body.size()), "bytes");
  gh::HubSpec spec;
  spec.game = "Shooter's Edge";
  spec.id = "shooters_edge";
  for (const ModeDef& m : kModes) spec.modes.push_back({m.name, 1});
  gh::Poller none;
  const std::string page = gh::hub_answer(none, spec, "/demos", 0, &a, kNow).body;
  const std::string searched = gh::hub_answer(none, spec, "/demos?player=quill", 0, &a, kNow).body;
  lab.attach("demos.html", std::vector<uint8_t>(page.begin(), page.end()));
  lab.metric("demos_page", static_cast<double>(page.size()), "bytes");

  // Two weeks kept: the league's demos stay.
  int tagged_before = 0;
  for (const gh::DemoRow& r : a.rows) tagged_before += r.tags.empty() ? 0 : 1;
  const int dropped = gh::archive_prune(a, 0, int64_t{14} * 86400, kNow);
  int tagged_after = 0;
  for (const gh::DemoRow& r : a.rows) tagged_after += r.tags.empty() ? 0 : 1;
  lab.metric("pruned_to_two_weeks", dropped, "demos");
  lab.metric("kept_after_prune", static_cast<double>(a.rows.size()), "demos");
  if (tagged_after != tagged_before) {
    lab.fail("the prune dropped a tagged demo",
             "next: make archive_prune keep every demo with a tag");
  }

  const std::string browser = lab.arg("browser") == "0" ? "" : ge::lab::hub_browser::find_browser();
  if (browser.empty()) {
    lab.note("no headless Chromium (GE_CHROMIUM, /opt/pw-browsers/chromium): the page not drawn");
    return;
  }
  std::error_code ec;
  const fs::path dir = fs::temp_directory_path(ec) / "ge-hub-archive";
  fs::create_directories(dir, ec);
  if (auto pic = ge::lab::hub_browser::hub_draw(browser, dir, "demos", page, 1280, 1000, false)) {
    lab.shot({"demos-desktop",
              "THE DEMOS PAGE, 1280 WIDE",
              "GET /demos over the month's archive: the search form, the newest demos first, 15 "
              "a page, each with its players, length, Play and Download; the scores folded.",
              "newest first; the league's demos tagged; a demo cut short says so; no score shown "
              "until a row's 'scores' is opened.",
              {{"demos", lab.arg("demos")}}},
             *pic, gl::Det::measured);
  }
  if (auto pic =
          ge::lab::hub_browser::hub_draw(browser, dir, "demos-quill", searched, 390, 1500, true)) {
    lab.shot({"demos-phone",
              "A PLAYER'S DEMOS ON A PHONE, DARK",
              "GET /demos?player=quill at a phone's width in the dark scheme.",
              "only demos with quill in them; the map and length columns dropped to fit, Play over "
              "Download; nothing wider than "
              "the screen.",
              {{"demos", lab.arg("demos")}}},
             *pic, gl::Det::measured);
  }
  fs::remove_all(dir, ec);
}

const gl::LabSpec kSpec{
    "hub-archive",
    gl::LabKind::sim,
    "engine",
    "engine/hub (the demo archive)",
    "net",
    "Does the hub's archive keep each demo once by its hash, find what each search should, and "
    "answer a search in well under a millisecond over a month of demos?",
    {
        {"demos", "500", "replay files written over the month"},
        {"seed", "1", "the script's stream"},
        {"browser", "1",
         "draw the Demos page with a headless Chromium when one is found (0: never)"},
    },
    "--demos=120 --browser=0",
    run,
};
GE_LAB(kSpec);

}  // namespace
