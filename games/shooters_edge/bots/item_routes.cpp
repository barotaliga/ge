// Scripted routes to key items and item rules on Quake's maps (quake_maps.hpp), and the book.
//
// ge:summary Quake's maps' scripted routes and item rules, and the four tables as one book
// ge:keywords item-routes scripted-route e1m2 ya-stairs
// ge:skill bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/quake_maps.hpp"

namespace shooters_edge::bots {

namespace {

// e1m2: the yellow armour on its walkway over the pool (feet 376), three steps up each end (288,
// 316, 348, 376; the south's 348 tread 16 deep, y -631..-615), hopped up in one flow at a run as
// people do. Doors 147 (south) and 148 (north) stand over the top two steps until button 511 is
// shot; then they stay open.
constexpr ItemRoute kE1m2[] = {
    {"ya",
     "ya-south",
     {{1340, -720, 288, false},
      {1340, -645, 316, true},
      {1340, -623, 348, true},
      {1340, -598, 376, true},
      {1340, -544, 376, false}},
     5},
    {"ya",
     "ya-north",
     {{1340, -380, 288, false},
      {1340, -447, 316, true},
      {1340, -471, 348, true},
      {1340, -492, 376, true},
      {1340, -544, 376, false}},
     5},
};

// e1m2: the rockets on the plate over the SNG pit are never gone for before the quad.
constexpr ItemRule kE1m2Rules[] = {
    {"rox", 864, -312, 440, "quad", 30},
};

}  // namespace

std::span<const ItemRoute> item_routes(std::string_view map) {
  if (ge::shooterbots::map_base(map) == "e1m2") return kE1m2;
  return {};
}

std::span<const ItemRule> item_rules(std::string_view map) {
  if (ge::shooterbots::map_base(map) == "e1m2") return kE1m2Rules;
  return {};
}

const ge::shooterbots::MapBook& quake_maps() {
  static const ge::shooterbots::MapBook kBook{rocket_tricks, gap_tricks, item_routes, item_rules};
  return kBook;
}

}  // namespace shooters_edge::bots
