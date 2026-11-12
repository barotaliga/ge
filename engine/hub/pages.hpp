// THE HUB'S PAGES (docs/hub.md § What a game's community gets): HTML the hub writes itself from
// its index, with no build step and no script, so a phone, a browser without script and a search
// engine read the same page. Every page refreshes itself (a meta refresh, 5 s) and is read-only and
// cacheable for seconds.
//
// - `/` (Servers): a card for each server with anyone on it, by interest: the map, the phase and
//   time left, the scoreboard (teams, then players), spectators and broadcast viewers, the
//   address, and Join, Spectate and Watch links; the mode filters; the empty servers listed after.
// - `/players`: every person on every server: name, playing, spectating or a bot, where, the score.
// - `/demos`: the archive (archive.hpp) searched by player, map, mode and tag, newest first; Play
//   opens the game on the demo (`<id>://demo/<sha256>?hub=<host>`, link.hpp), Download fetches it.
// - `/scoreboard/{address}`: one server's scoreboard alone, sized for a streamer's overlay.
// - `/style.css`: the one stylesheet, dark by default, light when the reader's system asks.
//
// The game's words come from its HubSpec (the name, the modes, the score's column); the Join and
// Spectate links use the game's link scheme `<id>://join/<address>` (a client registers it, M56e).
// `hub_answer` routes a path to the API (`/v1...`, api.hpp) or a page.
//
// ge:summary The hub's pages as HTML: servers' cards, players, a scoreboard overlay, the stylesheet
// ge:keywords hub pages html servers players scoreboard overlay card css refresh join spectate
// ge:keywords watch mode-filter escape demos archive search download
// ge:skill net
// ge:test tests/hub_pages_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hub/api.hpp"
#include "hub/archive.hpp"

namespace ge::hub {

constexpr int kPageRefreshS = 5;

// What a page knows beyond the index: the archive (null when the hub keeps none), the wall
// clock's time the archive's ages are counted from, and the host the reader asked for (the Host
// header), which a Play link names so the game fetches the demo from the same hub.
struct Site {
  const Archive* archive = nullptr;
  int64_t unix_s = 0;
  std::string host;
};

// Any GET the hub answers: the API under /v1, the archive's paths, a page, the stylesheet, or a
// 404 page.
Answer hub_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us,
                  const Site& site = {});

// The pages one by one (tests and the lab call them).
std::string servers_page(const Poller& p, const HubSpec& spec, std::string_view mode,
                         int64_t now_us);
std::string players_page(const Poller& p, const HubSpec& spec, int64_t now_us);
std::string scoreboard_page(const HubSpec& spec, const HubNode& n);
// The Demos page: the search form, a row a demo (scores folded away until asked), the pages.
std::string demos_page(const Site& site, const HubSpec& spec, std::string_view target);
const std::string& hub_stylesheet();
// A Unix time as "2026-11-12 01:30" (UTC).
std::string utc_text(int64_t unix_s);

// Text made safe inside HTML (&, <, >, ", ').
std::string html_escape(std::string_view s);

}  // namespace ge::hub
