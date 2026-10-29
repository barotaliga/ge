// LAB `net-host`: games hosted from inside a game and found in a server browser
// (engine/net/hosting.hpp, directory.hpp, the rendezvous's list), on the hosting trial's evening
// (hosting_trial.hpp) once under each condition preset and each home router. A listen server on a
// home LAN, ten public arenas, an invite-only game, a listed host no stranger can reach and a
// LAN-only game on a LAN of its own; a friend's browser on the home LAN and a stranger's on the
// internet refresh three times. It answers whether each browser shows every game it should once
// and none it should not, how long a refresh takes and what each game pings, whether every join
// (by the LAN, by the list, by an invite, through the relay) is seated and chats, and whether the
// game started shows as playing and the game stopped leaves the list at once with its forward
// taken down. The browsers' first refresh is drawn as the 2D layer draws a menu (engine/ui), and
// the joins' times as a heatmap. Everything runs on a virtual clock off one seed.
//
// ge:summary Lab: hosted games in a browser: found, pinged, joined, started, unlisted
// ge:keywords lab net hosting server-browser directory lobby listen-server listed invite
// ge:keywords lan-only ping join chat forward unlist picture preset
// ge:skill net labs
// ge:test tests/net_hosting_test.cpp
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/hosting_trial.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gu = ge::ui;

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

const char* found_name(uint8_t from) {
  if (from == (gn::kFoundLan | gn::kFoundListed)) return "lan+list";
  return from & gn::kFoundLan ? "lan" : "list";
}

std::string ping_text(int ms) { return ms < 0 ? "-" : std::to_string(ms); }

const gn::TrialRow* find_row(const gn::TrialRefresh& r, const std::string& name) {
  for (const gn::TrialRow& x : r.rows) {
    if (x.name == name) return &x;
  }
  return nullptr;
}

// What each browser should show at each refresh: every listed game but the stopped HOME, HOME on
// the friend's LAN and in the list as one row, playing once started. MISSING counts what should
// be there and is not, or is stale (a host whose register, advertise or unlist a burst of loss
// held up, a page lost); SHOWN counts what must never be: the invite-only or the LAN-only game.
struct Rows {
  int missing = 0, shown = 0;
};

Rows check_rows(const gn::TrialBrowser& b) {
  Rows out;
  for (size_t k = 0; k < b.refreshes.size(); k++) {
    const gn::TrialRefresh& r = b.refreshes[k];
    out.shown += find_row(r, "basement") ? 1 : 0;
    out.shown += find_row(r, "friends only") ? 1 : 0;
    const gn::TrialRow* home = find_row(r, "home");
    const size_t others = r.rows.size() - (home ? 1 : 0);
    out.missing += others < 11 ? static_cast<int>(11 - others) : 0;
    if (k < 2) {
      const uint8_t from =
          b.name == "friend" ? (gn::kFoundLan | gn::kFoundListed) : gn::kFoundListed;
      out.missing += home && home->from == from ? 0 : 1;
      if (k == 1) out.missing += home && home->phase == ge::net::SessionPhase::playing ? 0 : 1;
    } else {
      out.missing += home ? 1 : 0;  // stale: its unlist not yet in
    }
  }
  return out;
}

// THE BROWSER as a game's menu would draw it: a panel a browser, a row a game by ping.
gl::Picture browser_picture(const gn::HostTrial& t) {
  constexpr int kW = 1280, kH = 720;
  gu::Image img = gu::make_image(kW, kH, {0.07f, 0.09f, 0.13f, 1});
  gu::DrawList dl;
  dl.begin(kW, kH);
  const gu::Font& bold = gu::ui_font(gu::Weight::semibold);
  const gu::Font& body = gu::ui_font(gu::Weight::medium);
  const gu::Color ink{0.93f, 0.95f, 0.98f, 1}, dim{0.55f, 0.62f, 0.72f, 1};
  const gu::Color accent{0.36f, 0.70f, 1.0f, 1}, warn{1.0f, 0.72f, 0.30f, 1};
  for (size_t p = 0; p < t.browsers.size() && p < 2; p++) {
    const gn::TrialBrowser& b = t.browsers[p];
    const float x0 = 32.0f + static_cast<float>(p) * 624.0f, y0 = 32;
    dl.rect({x0, y0, 600, 656}, {0.12f, 0.15f, 0.21f, 1}, 14);
    dl.text(bold, 30, x0 + 24, y0 + 46, "FIND A GAME", ink);
    const std::string who =
        b.name == "friend" ? "on the host's home LAN" : "on the internet, behind a NAT";
    dl.text(body, 20, x0 + 24, y0 + 74, who, dim);
    const float cols[4] = {x0 + 24, x0 + 300, x0 + 410, x0 + 500};
    const char* heads[4] = {"NAME", "PLAYERS", "PING", "FOUND"};
    for (int c = 0; c < 4; c++) dl.text(bold, 17, cols[c], y0 + 112, heads[c], dim);
    dl.rect({x0 + 16, y0 + 122, 568, 2}, {0.25f, 0.30f, 0.40f, 1});
    if (b.refreshes.empty()) continue;
    const gn::TrialRefresh& r = b.refreshes[0];
    float y = y0 + 156;
    for (size_t i = 0; i < r.rows.size(); i++) {
      const gn::TrialRow& row = r.rows[i];
      if (i % 2 == 0) dl.rect({x0 + 16, y - 26, 568, 38}, {0.15f, 0.19f, 0.26f, 1}, 6);
      const bool home = row.name == "home";
      dl.text(bold, 22, cols[0], y, row.name, home ? accent : ink);
      float tag = cols[0] + 130;
      if (row.locked) {
        dl.text(bold, 15, tag, y - 2, "LOCKED", warn);
        tag += 70;
      }
      if (row.phase == ge::net::SessionPhase::playing)
        dl.text(bold, 15, tag, y - 2, "PLAYING", warn);
      dl.text(body, 22, cols[1], y,
              std::to_string(row.seated) + " / " + std::to_string(row.max_players), ink);
      dl.text(body, 22, cols[2], y, row.ping_ms < 0 ? "-" : std::to_string(row.ping_ms) + " ms",
              row.ping_ms < 0 ? dim : ink);
      dl.text(body, 20, cols[3], y, found_name(row.from), row.from & gn::kFoundLan ? accent : dim);
      y += 40;
    }
    char line[96];
    std::snprintf(line, sizeof line, "%zu games, refreshed in %.2f s", r.rows.size(),
                  r.done_us < 0 ? -1.0 : static_cast<double>(r.done_us) / 1e6);
    dl.text(body, 18, x0 + 24, y0 + 636, line, dim);
  }
  gu::render(dl, img);
  gl::Picture pic;
  pic.width = img.width;
  pic.height = img.height;
  pic.rgba = std::move(img.rgba);
  return pic;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  std::vector<std::pair<std::string, uint8_t>> routers;
  for (const std::string& r : split(lab.arg("routers"))) {
    if (r == "none") {
      routers.emplace_back(r, uint8_t{0});
      continue;
    }
    gn::PortmapProto p{};
    if (r == "all") {
      routers.emplace_back(r, gn::kPortmapAll);
    } else if (gn::portmap_proto(r, p)) {
      routers.emplace_back(r, gn::portmap_bit(p));
    } else {
      lab.fail("unknown router '" + r + "'",
               "Pick from all, none, pcp, natpmp, upnp (engine/net/portmap.hpp); skill: net.");
    }
  }
  std::vector<std::string> heat_rows;
  std::vector<double> heat;
  std::vector<std::string> join_names;
  for (const std::string& name : split(lab.arg("presets"))) {
    gn::LinkConditions link;
    if (!gn::conditions_preset(name, link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    for (const auto& [router_name, speaks] : routers) {
      gn::HostTrialConfig c;
      c.link = link;
      c.seed = seed;
      c.home_router = speaks;
      const gn::HostTrial t = gn::run_host_trial(c);
      std::string key = name;
      key += "-";
      key += router_name;
      // The stranger's first refresh, row by row.
      gl::LabTable view{key + ": the stranger's browser, first refresh (by ping)",
                        {"game", "players", "phase", "locked", "found", "ping (ms)"},
                        {}};
      for (const gn::TrialRow& r : t.browsers[1].refreshes[0].rows)
        view.rows.push_back({r.name, std::to_string(r.seated) + "/" + std::to_string(r.max_players),
                             ge::net::phase_name(r.phase), r.locked ? "yes" : "",
                             found_name(r.from), ping_text(r.ping_ms)});
      lab.table(std::move(view));
      gl::LabTable joins{
          key + ": the joins", {"browser", "game", "by", "path", "seated (ms)", "chat"}, {}};
      int unseated = 0;
      heat_rows.push_back(key);
      join_names.clear();
      for (const gn::TrialJoin& j : t.joins) {
        joins.rows.push_back({j.browser, j.host, j.lan ? "lan" : "rendezvous",
                              j.lan ? "lan" : gn::reach_path_name(j.path),
                              j.seated_ms < 0 ? "NEVER" : std::to_string(j.seated_ms),
                              j.chat_heard ? "both ways" : "NO"});
        unseated += j.seated_ms < 0 || !j.chat_heard ? 1 : 0;
        heat.push_back(j.seated_ms < 0 ? std::nan("") : static_cast<double>(j.seated_ms));
        join_names.push_back(j.browser + ">" + j.host);
        lab.metric(key + "-" + j.browser + "-" + j.host + "-seated-ms",
                   static_cast<double>(j.seated_ms), "ms");
      }
      lab.table(std::move(joins));
      const Rows rf = check_rows(t.browsers[0]), rs = check_rows(t.browsers[1]);
      const int missing = rf.missing + rs.missing, shown = rf.shown + rs.shown;
      // A link that loses more than 1 % in bursts can hold a register, an advertise or a page up
      // for seconds: what is missing there is reported, not failed.
      const bool strict = link.loss <= 0.01;
      gl::LabTable sum{key + ": the evening",
                       {"refresh (s)", "friend", "stranger", "home forward (s)", "listed",
                        "after stop", "forward closed", "missing rows", "rows never to show"},
                       {}};
      for (int k = 0; k < 3; k++) {
        const auto secs = [&](const gn::TrialBrowser& b) {
          const int64_t d = b.refreshes[static_cast<size_t>(k)].done_us;
          char s[32];
          std::snprintf(s, sizeof s, "%.2f", d < 0 ? -1.0 : static_cast<double>(d) / 1e6);
          return std::string(s);
        };
        char fwd[32];
        std::snprintf(fwd, sizeof fwd, "%.2f",
                      t.home_forward_ms < 0 ? -1.0 : static_cast<double>(t.home_forward_ms) / 1e3);
        sum.rows.push_back({std::to_string(k + 1), secs(t.browsers[0]), secs(t.browsers[1]), fwd,
                            std::to_string(t.listed_at_server), std::to_string(t.listed_after_stop),
                            t.home_forward_closed ? "yes" : "NO", std::to_string(missing),
                            std::to_string(shown)});
        lab.metric(
            key + "-refresh" + std::to_string(k + 1) + "-stranger-ms",
            static_cast<double>(t.browsers[1].refreshes[static_cast<size_t>(k)].done_us) / 1e3,
            "ms");
      }
      lab.table(std::move(sum));
      lab.metric(key + "-missing-rows", missing, "count");
      lab.metric(key + "-shown-rows", shown, "count");
      lab.metric(key + "-home-pings", static_cast<double>(t.home.pings), "count");
      lab.metric(key + "-lists", static_cast<double>(t.server.lists), "count");
      if (shown)
        lab.fail(key + ": " + std::to_string(shown) +
                     " browser rows showed a game that must not be seen (invite-only, LAN-only)",
                 "Read the advertise and list in engine/net/rendezvous.cpp and hosting_stop in "
                 "engine/net/hosting.cpp; skill: net.");
      if (strict && missing)
        lab.fail(key + ": " + std::to_string(missing) + " browser rows were missing or stale",
                 "Run tests/net_hosting_test.cpp and tests/net_directory_test.cpp, then read "
                 "on_listing and on_advert in engine/net/directory.cpp; skill: net.");
      if (strict && unseated)
        lab.fail(
            key + ": " + std::to_string(unseated) + " joins were never seated or never chatted",
            "Read the join in engine/net/hosting_trial.cpp, session_client_attach in "
            "engine/net/session_client.cpp and engine/net/reach.cpp; skill: net.");
      if (strict && (t.home_listed_after_stop || !t.home_forward_closed))
        lab.fail(key + ": the stopped game stayed listed or kept its forward",
                 "Read hosting_stop in engine/net/hosting.cpp; skill: net.");
      if (router_name == routers.front().first) {  // one picture a preset
        lab.shot(
            {"browser-" + name,
             "THE SERVER BROWSER, " + key,
             "Both browsers' first refresh as a game's menu would draw them: the friend on the "
             "host's home LAN (left) and a stranger on the internet behind a NAT (right), each "
             "game a row, by ping.",
             "every game once (HOME found on the friend's LAN and in the list on one row), no "
             "'friends only' (invite-only) or 'basement' (LAN-only), arena 03 LOCKED, arena 08 "
             "PLAYING, the attic last with no ping (no stranger gets through its NAT).",
             {{"preset", name}, {"router", router_name}, {"seed", lab.arg("seed")}}},
            browser_picture(t));
      }
    }
  }
  lab.note(
      "The evening: HOME is a listen server (the host plays) behind a port-restricted NAT whose "
      "router speaks what its column names; the friend's browser is on the same LAN and the "
      "stranger's behind another port-restricted NAT. The ten arenas are public dedicated servers "
      "with 0 to 3 players; 'friends only' is invite-only and the stranger joins it by its invite; "
      "the attic sits behind a symmetric NAT with no router, so it is listed but unpinged and "
      "joined through the relay; the basement is LAN-only on a LAN of its own. The browsers "
      "refresh "
      "at 6 s, 15 s (HOME started at 14 s) and 22 s (HOME stopped at 20 s). A refresh's time is "
      "from "
      "the probe to the last ping answered or lost (three a game, 20 ms apart across the list).");
  if (!heat_rows.empty() && !join_names.empty()) {
    std::string cols;
    for (const std::string& j : join_names) cols += (cols.empty() ? "" : ", ") + j;
    gl::ChartFrame frame{"JOIN TO SEATED, EACH PRESET AND ROUTER", "columns: " + cols, "join",
                         "ms"};
    const gl::Chart chart =
        gl::heatmap_chart(frame, heat_rows, heat, 0, static_cast<int>(join_names.size()));
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach("joins.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot({"joins",
              chart.title,
              "How long each join took from the browser's pick to the lobby's welcome: a row a "
              "preset and home router, a column a join (" +
                  cols + ").",
              "the LAN join lightest; the joins through the rendezvous a few round trips more; the "
              "attic's (relayed) darkest, past the 3 s a direct connect is given first.",
              {{"presets", lab.arg("presets")},
               {"routers", lab.arg("routers")},
               {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot));
  }
}

const gl::LabSpec kSpec{
    "net-host",
    gl::LabKind::sim,
    "engine",
    "engine/net (hosting, directory, the rendezvous's list, discovery)",
    "net",
    "Does a game hosted from inside a game show in every browser that should see it, once, with "
    "its seats, phase and ping, and in none that should not? Does every join (LAN, list, invite, "
    "relay) get seated, and does a stopped game leave the list at once?",
    {
        {"presets", "lan,wifi,bad", "condition presets, comma-separated"},
        {"routers", "all,none", "what HOME's router speaks: all, none, pcp, natpmp, upnp"},
        {"seed", "1", "the network's, the hosts' and the browsers' stream"},
    },
    "--presets=lan,bad --routers=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
