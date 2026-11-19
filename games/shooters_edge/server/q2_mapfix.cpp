// Quake II's maps mended as a server loads them (q2_mapfix.hpp).
//
// ge:summary The visibility patch's clusters by map checksum, and the entity file's name
// ge:keywords q2 vis-patch q2_vis_patch q2_entity_file_name
// ge:skill shooter brush
// ge:test tests/shooters_edge_q2_mapfix_test.cpp
#include "shooters_edge/server/q2_mapfix.hpp"

#include <cstdio>
#include <initializer_list>

#include "classic/protocol/wire.hpp"

namespace shooters_edge::server {

namespace {

// `to`'s bit set in cluster `from`'s visible and hearable rows (a cluster past the map's: none).
void gain(ge::brush::Map& m, int from, int to) {
  if (from < 0 || to < 0 || from >= m.clusters || to >= m.clusters) return;
  const size_t at = size_t(from) * m.row_bytes + size_t(to) / 8;
  const auto bit = static_cast<uint8_t>(1u << (to % 8));
  if (at < m.pvs.size()) m.pvs[at] |= bit;
  if (at < m.phs.size()) m.phs[at] |= bit;
}

void gain_all(ge::brush::Map& m, std::initializer_list<int> from, std::initializer_list<int> to) {
  for (const int f : from)
    for (const int t : to) gain(m, f, t);
}

}  // namespace

bool q2_vis_patch(ge::brush::Map& m, int32_t checksum) {
  if (m.pvs.empty()) return false;
  switch (checksum) {
    case 80717714:  // q2dm1
      gain_all(m, {395}, {176, 183});
      return true;
    case 509300933:  // q2dm3
      gain_all(m, {345, 384}, {466, 484, 692});
      return true;
    case 742046128:  // q2dm8
      gain_all(m, {629, 631, 633, 639}, {908, 909, 910, 915, 923, 924, 927, 930, 938, 939, 947});
      return true;
    case 515801089:  // mgu6m2
      for (int c = 0; c < m.clusters; c++) gain_all(m, {c}, {213, 214, 217});
      return true;
    default:
      return false;
  }
}

std::string q2_entity_file_name(const std::string& map, const std::string& entity_text) {
  size_t n = entity_text.size();
  while (n > 0 && entity_text[n - 1] == '\0') n--;  // the text as stored, less its terminator
  const uint16_t crc = n == 0 ? 0
                              : ge::classic::protocol::crc16(
                                    reinterpret_cast<const uint8_t*>(entity_text.data()), n);
  char hex[8];
  std::snprintf(hex, sizeof hex, "%04x", crc);
  return map + "@" + hex + ".ent";
}

}  // namespace shooters_edge::server
