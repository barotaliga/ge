// THE HOME MAP (`sv_homemap`): a server nobody plays on goes back to its home map, so the next
// player finds the map its owner chose rather than whatever the last ones left it on.
//
// `home_due`, once a frame with the players on the server (spectators count, bots do not): when
// there have been none for `kHomeDelay` seconds of the server's clock and the map is not the home
// map, true once, and the game changes to it. A player arriving starts the wait again.
//
// ge:summary The home map: an empty server goes back to sv_homemap after ten seconds
// ge:keywords sv_homemap home-map empty-server reset default-map home_due
// ge:skill net
// ge:test tests/server_homemap_test.cpp
#pragma once

#include <string_view>

namespace ge::server {

constexpr double kHomeDelay = 10;  // s of an empty server before it goes home

struct HomeWatch {
  double empty_since = -1;  // when the last player left, s (-1: someone is on)
};

bool home_due(HomeWatch& w, std::string_view home, std::string_view current, int players,
              double now);

}  // namespace ge::server
