// The home map (homemap.hpp states the rule).
//
// ge:summary The home map's wait: an empty server's ten seconds, then home once
// ge:keywords home_due sv_homemap
// ge:skill net
// ge:test tests/server_homemap_test.cpp
#include "server/homemap.hpp"

namespace ge::server {

bool home_due(HomeWatch& w, std::string_view home, std::string_view current, int players,
              double now) {
  if (players > 0) {
    w.empty_since = -1;
    return false;
  }
  if (w.empty_since < 0) w.empty_since = now;
  if (home.empty() || home == current || now - w.empty_since < kHomeDelay) return false;
  w.empty_since = now;  // asked once; again only after another wait
  return true;
}

}  // namespace ge::server
