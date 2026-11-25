// WHAT A QUAKE II CLIENT FETCHES: the player's `allow_download` switches, by the kind of file.
// `all` 0 fetches nothing on joining, -1 refuses even a typed `download`; each kind's switch
// gates the files of its folder (maps, players' figures, models and sprites, sounds, wall
// textures and skies, pictures) and `others` the rest. The client asks the server only for what
// they let through (q2_client.hpp's `fetch`); a file held back is left lacking, as when a server
// refuses it.
//
// ge:summary Quake II's client download switches by kind of file, and the check a path passes
// ge:keywords allow_download allow_download_players allow_download_models allow_download_sounds
// ge:keywords allow_download_maps allow_download_textures allow_download_pics allow_download_others
// ge:keywords download-gate fetch-kinds q2_download_allowed
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#pragma once

#include <string_view>

namespace shooters_edge::net {

struct Q2Downloads {
  int all = 1;  // allow_download: 1 fetch, 0 none on joining, -1 none even typed
  bool maps = true, players = true, models = true, sounds = true, textures = true, pics = true;
  bool others = false;
};

// Whether a file the client lacks at `path` ("maps/q2dm1.bsp") is fetched on joining.
inline bool q2_download_allowed(const Q2Downloads& d, std::string_view path) {
  if (d.all <= 0) return false;
  const auto under = [&path](std::string_view dir) { return path.substr(0, dir.size()) == dir; };
  if (under("maps/")) return d.maps;
  if (under("players/")) return d.players;
  if (under("models/") || under("sprites/")) return d.models;
  if (under("sound/")) return d.sounds;
  if (under("textures/") || under("env/")) return d.textures;
  if (under("pics/")) return d.pics;
  return d.others;
}

}  // namespace shooters_edge::net
