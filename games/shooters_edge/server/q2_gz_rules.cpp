// Ground Zero's settings and map rules (q2_gz.hpp): its own cvars, the class names it spawns as
// its own, the items its dmflags and `g_disruptor` leave off the map.
//
// ge:summary Ground Zero's cvars, class names spawned as its own, items its dmflags leave out
// ge:keywords q2 ground-zero cvars strong-mines g-disruptor dmflags
// ge:keywords no-mines no-nukes no-spheres classname-alias
// ge:skill shooter
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <string_view>

#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_pack.hpp"
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
  s.cvars.add(
      {.name = "huntercam",
       .fallback = "1",
       .flags = con::kServer | con::kServerInfo | con::kLatch,
       .help = "Whether a player killed from afar watches his hunter sphere chase his killer.",
       .values = "0: off; 1: on, unless players are forced to respawn (the default)"});
  s.cvars.add({.name = "coop_baseq2",
               .fallback = "0",
               .flags = con::kServer | con::kLatch,
               .help = "Whether co-op picks a map's entities by the base game's rules.",
               .values = "0: the pack's, where some appear only in co-op and some never in it (the "
                         "default); 1: the base game's, by skill alone"});
}

int gz_coop_left_out(const Q2Server& s, int flags) {
  if (s.opt.pack != Q2Pack::ground_zero) return -1;
  const auto* coop = s.cvars.find("coop");
  if (coop == nullptr || !coop->on()) return -1;  // single player: the base game's rule
  const auto* base = s.cvars.find("coop_baseq2");
  if (base != nullptr && base->on()) return -1;
  if ((flags & 4096) != 0) return 1;       // never in co-op
  if ((flags & 0x700) == 0x700) return 0;  // in co-op only, at every skill
  return -1;
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
    return true;                          // every mode
  if (!s.cvars.find("deathmatch")->on())  // deathmatch's alone
    return name == "ammo_nuke" || name == "item_doppleganger" || name == "item_sphere_hunter" ||
           name == "item_sphere_vengeance";
  const int flags = s.cvars.find("dmflags")->integer();
  const bool sphere = name.starts_with("item_sphere_");
  return ((flags & 131072) != 0 && (name == "ammo_prox" || name == "ammo_tesla")) ||
         ((flags & 524288) != 0 && name == "ammo_nuke") || ((flags & 1048576) != 0 && sphere);
}

}  // namespace shooters_edge::server
