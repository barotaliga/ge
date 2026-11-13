// THE CARVED LETTERS THE STRIPS LACK, TRACED: the menu strips spell only their own rows' words, and
// six letters are in them in neither size (B X Z, b x z). Each of those is kept here as its
// shape on the carved line (rim included), traced at the strips' size and on their measured lines
// (a capital on rows 1 to 16, a small capital on 3 to 15, tails below), and `cut_letters` carves
// it in the pack's own colours, taken from the letters it did cut (carved_letters.hpp).
//
// ge:summary The traced shapes of the carved letters the menu strips lack, for carving at load
// ge:keywords menu classic carved-letters traced-letters masks alphabet small-capitals
// ge:skill ui
// ge:test tests/shooters_edge_menu_test.cpp
#pragma once

#include <span>
#include <string_view>

namespace shooters_edge::menu {

// A traced letter: its first row on the carved line and its rows from there, '#' ink.
struct TracedLetter {
  char c;
  int top;
  std::span<const std::string_view> rows;
};

// Every traced letter, the capitals then the small capitals.
std::span<const TracedLetter> traced_letters();

}  // namespace shooters_edge::menu
