// The way through's keys (way_keys.hpp).
//
// ge:summary Keycard hatches: the walls a key's lock turns away, shut until the key is held
// ge:keywords way_key_hatches trigger_usekey func_rotate_door func_movewall keycard hatch
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#include "shooterbots/way_keys.hpp"

#include <cstdlib>
#include <cstring>

#include "shooterbots/way_parts.hpp"

namespace ge::shooterbots {

bool way_quake_keys(const std::vector<EntityBlock>& blocks) {
  for (const EntityBlock& b : blocks) {
    const char* cn = block_value(b, "classname");
    if (cn && std::strncmp(cn, "item_key", 8) == 0) return true;
  }
  return false;
}

void way_key_hatches(WayThrough& w, const BotView& s, const std::vector<EntityBlock>& blocks,
                     std::vector<int>& gates) {
  if (!way_quake_keys(blocks)) return;
  for (const EntityBlock& lock : blocks) {
    const char* cn = block_value(lock, "classname");
    const char* target = block_value(lock, "target");
    const char* model = block_value(lock, "model");
    if (!cn || !target || !model || model[0] != '*' || std::strcmp(cn, "trigger_usekey") != 0)
      continue;
    const char* sfv = block_value(lock, "spawnflags");
    const char* msg = block_value(lock, "message");
    const int sf = sfv ? std::atoi(sfv) : 0;
    const int key = (sf & 16) != 0                                 ? kKeySilver
                    : (sf & 8) != 0                                ? kKeyGold
                    : msg && std::strstr(msg, "silver") != nullptr ? kKeySilver
                    : msg && std::strstr(msg, "gold") != nullptr   ? kKeyGold
                                                                   : -1;
    if (key < 0) continue;
    for (const EntityBlock& door : blocks) {
      const char* dc = block_value(door, "classname");
      const char* dn = block_value(door, "targetname");
      const char* dt = block_value(door, "target");
      if (!dc || !dn || !dt || std::strcmp(dc, "func_rotate_door") != 0 ||
          std::strcmp(dn, target) != 0)
        continue;
      for (const EntityBlock& wall : blocks) {
        const char* wc = block_value(wall, "classname");
        const char* wn = block_value(wall, "targetname");
        const char* wm = block_value(wall, "model");
        if (!wc || !wn || !wm || std::strcmp(wc, "func_movewall") != 0 || std::strcmp(wn, dt) != 0)
          continue;
        const int e = way_parts::edict_of(s, wm);
        if (e < 0) continue;
        w.locked.push_back(e);
        w.locked_at.push_back(s.sv.edicts[static_cast<size_t>(e)].origin);
        w.locked_turns.push_back(0);
        w.key_doors.push_back({e, key});
        gates.push_back(e);
      }
    }
  }
}

}  // namespace ge::shooterbots
