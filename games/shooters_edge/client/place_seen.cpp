// The player's place for the place names, watched or played (place_seen.hpp).
//
// ge:summary The place names' questions answered off the view watched or the window's own player
// ge:keywords add_place_queries clear-line looked-at
// ge:skill shooter
// ge:test tests/shooters_edge_place_names_test.cpp
#include "shooters_edge/client/place_seen.hpp"

#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/map_line.hpp"

namespace shooters_edge::client {

void add_place_queries(ge::console::Console& con, PlaceNames& places, const Listen& listen,
                       const PlaceSeen& seen, const GameAdapter& adapter, const SearchPath& files) {
  using V = ge::shooter::Vec3f;
  // Whether nothing solid stands between two points in the played or watched map (`loc_trace`).
  places.clear_line = [&listen, &seen](V from, V to) {
    const server::BspMap* map = seen.on ? seen.map : (listen.map ? &listen.map->bsp : nullptr);
    if (map == nullptr) return true;
    const V end = line_end(*map, from, to);
    return end.x == to.x && end.y == to.y && end.z == to.z;
  };
  places.looked_at = [&listen, &seen](V& at) {
    V from, angles;
    const server::BspMap* map = nullptr;
    if (seen.on) {
      from = seen.at;
      angles = seen.angles;
      map = seen.map;
    } else if (listen.server != nullptr && listen.map && listen.server->world.entities.size() > 1) {
      from = listen.server->world.entities[1].origin;
      angles = listen.server->world.entities[1].view_angles;
      map = &listen.map->bsp;
    } else {
      return false;
    }
    const V f = ge::shooter::angle_vectors(angles).forward;
    const V to = {from.x + f.x * kLookReach, from.y + f.y * kLookReach, from.z + f.z * kLookReach};
    at = map != nullptr ? line_end(*map, from, to) : to;
    return true;
  };
  add_place_commands(con, places, adapter, files, [&listen, &seen](V& at) {
    if (seen.on) {
      at = seen.at;
      return true;
    }
    if (listen.server == nullptr || !listen.map || listen.server->world.entities.size() < 2)
      return false;
    at = listen.server->world.entities[1].origin;
    return true;
  });
}

}  // namespace shooters_edge::client
