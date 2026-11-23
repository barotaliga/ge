// Quake II's mission packs as one switch (q2_pack.hpp).
//
// ge:summary Quake II's mission packs: the game folder each plays from, the names
// ge:keywords q2 mission-pack xatrix rogue q2_pack_of_folder
// ge:skill shooter
// ge:test tests/shooters_edge_q2_pack_test.cpp
#include "shooters_edge/server/q2_pack.hpp"

#include <cctype>

namespace shooters_edge::server {

namespace {

bool same(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return true;
}

}  // namespace

Q2Pack q2_pack_of_folder(std::string_view folder) {
  while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\'))
    folder.remove_suffix(1);
  if (const size_t cut = folder.find_last_of("/\\"); cut != std::string_view::npos)
    folder.remove_prefix(cut + 1);
  if (same(folder, "xatrix")) return Q2Pack::reckoning;
  if (same(folder, "rogue")) return Q2Pack::ground_zero;
  return Q2Pack::base;
}

std::string_view q2_pack_name(Q2Pack pack) {
  switch (pack) {
    case Q2Pack::reckoning:
      return "The Reckoning";
    case Q2Pack::ground_zero:
      return "Ground Zero";
    case Q2Pack::base:
      break;
  }
  return "Quake II";
}

}  // namespace shooters_edge::server
