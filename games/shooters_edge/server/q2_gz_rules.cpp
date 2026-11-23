// Ground Zero's settings and map rules (q2_gz.hpp): its own cvars, the class names it spawns as
// its own, the items its dmflags and `g_disruptor` leave off the map.
//
// ge:summary Ground Zero's cvars, class names spawned as its own, items its dmflags leave out
// ge:keywords q2 ground-zero cvars strong-mines g-disruptor  dmflags
// ge:keywords no-mines no-nukes no-spheres classname-alias
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <string_view>

#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace con = ge::console;

void gz_cvars(Q2Server& s) {
  s.cvars.add({.name = "strong_mines",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether an armed prox mine always lives its full 45 seconds.",
               .values = "0: it lives less with double damage or the quad (the default); 1: always "
                         "45 seconds"});
  s.cvars.add({.name = "g_disruptor",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether the Disruptor and its rounds are left on the map.",
               .values = "0: both taken off (the default); 1: both left, and the Bandolier and "
                         "the Ammo Pack carry more rounds"});
}

std::string_view gz_classname(std::string_view classname) {
  // Its own names for the base game's and the other pack's.
  constexpr std::pair<std::string_view, std::string_view> kAs[] = {
      {"weapon_nailgun", "weapon_etf_rifle"},   {"ammo_nails", "ammo_flechettes"},
      {"weapon_heatbeam", "weapon_plasmabeam"}, {"ammo_magslug", "ammo_flechettes"},
      {"ammo_trap", "weapon_proxlauncher"},     {"weapon_boomer", "weapon_etf_rifle"},
      {"weapon_phalanx", "weapon_plasmabeam"},
  };
  for (const auto& [from, to] : kAs)
    if (classname == from) return to;
  return classname;
}

bool gz_left_out(const Q2Server& s, int item) {
  if (item < 0 || item >= kSqItemCount) return false;
  const std::string_view name = kSqItems[item].classname;
  const auto* disruptor = s.cvars.find("g_disruptor");
  if ((name == "weapon_disintegrator" || name == "ammo_disruptor") &&
      (disruptor == nullptr || !disruptor->on()))
    return true;  // every mode
  if (!s.cvars.find("deathmatch")->on()) return false;
  const int flags = s.cvars.find("dmflags")->integer();
  const bool sphere = name.starts_with("item_sphere_");
  return ((flags & 131072) != 0 && (name == "ammo_prox" || name == "ammo_tesla")) ||
         ((flags & 524288) != 0 && name == "ammo_nuke") || ((flags & 1048576) != 0 && sphere);
}

}  // namespace shooters_edge::server
