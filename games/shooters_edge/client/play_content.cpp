// The files a match on a map draws from, named (play_content.hpp).
//
// ge:summary The content paths a match on a map needs, named after a headless start with bots
// ge:keywords play content files play_files precache
// ge:skill shooter net
// ge:test tests/shooters_edge_play_content_test.cpp
#include "shooters_edge/client/play_content.hpp"

#include <memory>
#include <set>
#include <utility>

#include "shooters_edge/client/demo_content.hpp"

namespace shooters_edge::client {

namespace {

std::string lower(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

}  // namespace

bool map_playable(std::string_view map) { return map.substr(0, 2) != "b_"; }

bool play_files(const SearchPath& files, std::string_view map, const ListenOptions& opt,
                std::vector<std::string>& names, std::string& error) {
  names.clear();
  ge::progs::Progs progs;
  if (!baseline_progs(progs, error)) return false;
  std::unique_ptr<LoadedMap> loaded = load_named_map(files, std::string(map), error);
  if (!loaded) return false;
  ListenOptions o = opt;
  o.files = &files;
  o.lobby = true;
  o.lobby_settings.min_players = 1;
  o.lobby_settings.countdown_s = 1;
  o.lobby_settings.warm_on_join = true;
  o.port = 0;
  o.transport = nullptr;
  Listen l;
  if (!listen_start(l, progs, std::move(loaded), o, error)) return false;
  for (int i = 0; i < kPlayContentBots; i++) listen_lobby(l, "addbot average");
  listen_lobby(l, "warmup");
  listen_lobby(l, "ready");
  std::set<std::string> out;
  const auto take = [&] {
    if (l.server == nullptr) return;
    for (const std::string& m : l.server->models)
      if (!m.empty() && m[0] != '*') out.insert(lower(m));
    for (const std::string& s : l.server->sounds)
      if (!s.empty()) out.insert("sound/" + lower(s));
  };
  take();
  const auto frames = static_cast<int>(kPlayContentS / listen_tic(l));
  for (int i = 0; i < frames; i++) {
    listen_frame(l, {});
    take();
  }
  listen_stop(l);
  std::vector<std::string> basics;
  game_basics(false, basics);
  out.insert(basics.begin(), basics.end());
  const std::string m = lower(std::string(map));
  out.insert("maps/" + m + ".bsp");
  out.insert("maps/" + m + ".lit");
  out.insert("gfx.wad");
  out.insert("gfx/");
  names.assign(out.begin(), out.end());
  return true;
}

}  // namespace shooters_edge::client
