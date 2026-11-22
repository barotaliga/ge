// A game's book of its maps (map_book.hpp): its rows by map, as the nav's, and a map's base name.
//
// ge:summary A game's book of its maps: rows by map, as the nav's tricks, the map's base name
// ge:keywords map-book nav_tricks nav_gap_tricks map_base
// ge:skill shooterbots bots
// ge:test tests/shooterbots_nav_gaps_test.cpp
#include "shooterbots/map_book.hpp"

namespace ge::shooterbots {

std::span<const RocketTrick> rocket_tricks(const MapBook& book, std::string_view map) {
  return book.rocket_tricks ? book.rocket_tricks(map) : std::span<const RocketTrick>{};
}

std::span<const GapTrick> gap_tricks(const MapBook& book, std::string_view map) {
  return book.gap_tricks ? book.gap_tricks(map) : std::span<const GapTrick>{};
}

std::span<const ItemRoute> item_routes(const MapBook& book, std::string_view map) {
  return book.item_routes ? book.item_routes(map) : std::span<const ItemRoute>{};
}

std::span<const ItemRule> item_rules(const MapBook& book, std::string_view map) {
  return book.item_rules ? book.item_rules(map) : std::span<const ItemRule>{};
}

std::vector<NavTrick> nav_tricks(std::span<const RocketTrick> tricks) {
  std::vector<NavTrick> out;
  out.reserve(tricks.size());
  for (const RocketTrick& t : tricks) {
    NavTrick n;
    n.from = t.from;
    n.to = t.to;
    n.back = t.flip;
    n.pitch = t.pitch;
    n.run = t.run;
    n.level = t.level;
    n.combo = t.combo;
    out.push_back(n);
  }
  return out;
}

std::vector<NavGapTrick> nav_gap_tricks(std::span<const GapTrick> tricks) {
  std::vector<NavGapTrick> out;
  out.reserve(tricks.size());
  for (const GapTrick& t : tricks) {
    NavGapTrick n;
    n.from = t.from;
    n.to = t.to;
    n.speed = t.speed;
    n.level = t.level;
    out.push_back(n);
  }
  return out;
}

std::string_view map_base(std::string_view map) {
  if (const size_t slash = map.find_last_of('/'); slash != std::string_view::npos)
    map = map.substr(slash + 1);
  if (map.size() > 4 && map.substr(map.size() - 4) == ".bsp") map = map.substr(0, map.size() - 4);
  return map;
}

}  // namespace ge::shooterbots
