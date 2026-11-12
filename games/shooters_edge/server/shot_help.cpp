// The aim help on a classic mod's server, defined: the grant noted, teammates, the line bent.
//
// ge:summary The aim help on a classic mod's server defined
// ge:keywords aim-help shot_help help_traced_shot team_friends
// ge:skill shooter progs
// ge:test tests/shooters_edge_shot_help_test.cpp
#include "shooters_edge/server/shot_help.hpp"

#include "server/vote.hpp"
#include "shooter/aim_help.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace {

// The classic game's weapons by the item bit a mod holds for them.
struct WeaponBit {
  int bit;
  const char* name;
};
constexpr WeaponBit kWeapons[] = {{4096, "axe"}, {1, "sg"},  {2, "ssg"}, {4, "ng"},
                                  {8, "sng"},    {16, "gl"}, {32, "rl"}, {64, "lg"}};

// Whether `name` is on the list `list` (words split by spaces or commas, `*` patterns).
bool listed(std::string_view list, std::string_view name) {
  size_t i = 0;
  while (i < list.size()) {
    while (i < list.size() && (list[i] == ' ' || list[i] == ',')) i++;
    const size_t start = i;
    while (i < list.size() && list[i] != ' ' && list[i] != ',') i++;
    if (i > start && ge::server::vote_pattern_match(list.substr(start, i - start), name))
      return true;
  }
  return false;
}

std::string text_of(const RulesCommon& s, const char* name) {
  const ge::console::Cvar* v = s.cvars.find(name);
  return v != nullptr ? v->value : std::string();
}

// The weapon slot `slot` holds, by the rules' world.
const char* held(const RulesCommon& s, int slot) {
  return weapon_name(s.world.clients[slot].numbers.weapon);
}

void lead_missile(void* ctx, sh::Server& /*sv*/, int missile) {
  RulesCommon& s = *static_cast<RulesCommon*>(ctx);
  const int shooter = s.sv.edicts[static_cast<size_t>(missile)].owner;
  const int slot = shooter - 1;
  if (slot < 0 || slot >= static_cast<int>(s.sv.clients.size()) || !s.sv.clients[slot].aimhelp)
    return;
  if (!aim_help_weapon_allowed(s, held(s, slot))) return;
  const sh::AimLeeway leeway{s.sv.set.aimhelp_x, s.sv.set.aimhelp_y};
  if (sh::aim_help_missile(s.sv, missile, leeway, team_friends(s, slot)) >= 0)
    s.sv.clients[slot].aim_helped++;
}

}  // namespace

const char* weapon_name(float weapon_bit) {
  const int bit = static_cast<int>(weapon_bit);
  for (const WeaponBit& w : kWeapons)
    if (w.bit == bit) return w.name;
  return "";
}

void add_aim_help(RulesCommon& s) {
  if (s.cvars.find("sv_aimhelp_allow") == nullptr) {
    s.cvars.add({.name = "sv_aimhelp_allow",
                 .fallback = "",
                 .flags = ge::console::kServer,
                 .help = "the weapons the aim help may bend (empty: all but sv_aimhelp_deny's)",
                 .kind = ge::console::CvarKind::text});
    s.cvars.add({.name = "sv_aimhelp_deny",
                 .fallback = "",
                 .flags = ge::console::kServer,
                 .help = "the weapons the aim help never bends (\"rl lg\": not the rocket "
                         "launcher nor the lightning gun)",
                 .kind = ge::console::CvarKind::text});
  }
}

bool aim_help_weapon_allowed(const RulesCommon& s, std::string_view weapon) {
  if (listed(text_of(s, "sv_aimhelp_deny"), weapon)) return false;
  const std::string allow = text_of(s, "sv_aimhelp_allow");
  return allow.find_first_not_of(" ,") == std::string::npos || listed(allow, weapon);
}

void note_aim_help_info(RulesCommon& s, int slot, const std::string& key,
                        const std::string& value) {
  if (key != kAimHelpKey || slot < 0 || slot >= static_cast<int>(s.sv.clients.size())) return;
  s.sv.clients[slot].aimhelp = !value.empty() && value != "0";
  // The lead's rule on the loop, set again with each map's loop (the grant is told it anew).
  if (s.sv.clients[slot].aimhelp) {
    s.sv.missile_watch = &lead_missile;
    s.sv.missile_watch_ctx = &s;
  }
}

uint32_t team_friends(const RulesCommon& s, int slot) {
  const ge::console::Cvar* tp = s.cvars.find("teamplay");
  if (tp == nullptr || !tp->on()) return 0;
  const std::string team = info_value(s.clients[slot].userinfo, "team");
  if (team.empty()) return 0;
  uint32_t mask = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (i != slot && s.clients[i].active && info_value(s.clients[i].userinfo, "team") == team)
      mask |= 1u << i;
  return mask;
}

int help_traced_shot(RulesCommon& s, int pass, sh::Vec3f start, sh::Vec3f& end) {
  const int slot = pass - 1;
  if (slot < 0 || slot >= static_cast<int>(s.sv.clients.size()) || !s.sv.clients[slot].aimhelp)
    return -1;
  if (!aim_help_weapon_allowed(s, held(s, slot))) return -1;
  const sh::AimLeeway leeway{s.sv.set.aimhelp_x, s.sv.set.aimhelp_y};
  const int target = sh::aim_help_line(s.sv, pass, start, end, leeway, team_friends(s, slot));
  if (target >= 0) s.sv.clients[slot].aim_helped++;
  return target;
}

}  // namespace shooters_edge::server
