// THE SAMPLE MOD `vampire` (docs/mods.md): a Shooter's Edge mod in C++ that reads and changes the
// players alone, so it plays on either game's rules (sim/mod_api.hpp § On the sequel's rules). It
// changes one rule of the match:
//
// - A BLOW FEEDS ITS GIVER: a player's blow on another (the `damage` hook) heals the giver by half
//   the points it deals, up to kMostHealth. The blow itself lands as the game made it.
//
// It counts the blows it fed, the health they gave and the deaths it saw (the `death` hook) in
// `State`, the block the game keeps and folds into its digest, and writes a line to the log at
// every kill. No global, no clock, no <cmath>.
//
// ge:summary The sample mod vampire: a player's blow heals him by half its points, on any rules
// ge:keywords mod sample-mod vampire damage-hook death-hook health sequel-mods state-block
// ge:skill mod shooter
// ge:test tests/shooters_edge_rules_mod_test.cpp
#include "shooters_edge/sim/mod_api.hpp"

namespace {

constexpr float kMostHealth = 200;

struct State {
  int32_t blows_fed;
  int32_t health_given;
  int32_t deaths;
  int32_t kills;
};

const se_mod_services_v1* se(const ge_mod_host_v1* h) {
  return static_cast<const se_mod_services_v1*>(h->game);
}

int32_t damage(void* state, const ge_mod_host_v1* h, int32_t target, int32_t attacker,
               int32_t points) {
  State& s = *static_cast<State*>(state);
  const se_mod_services_v1* g = se(h);
  const int32_t players = g->max_players(h->ctx);
  if (attacker < 1 || attacker > players || attacker == target || points <= 0) return points;
  const float health = g->get_float(h->ctx, attacker, SE_FIELD_HEALTH);
  if (health <= 0 || health >= kMostHealth) return points;
  float fed = static_cast<float>(points / 2);
  if (health + fed > kMostHealth) fed = kMostHealth - health;
  g->set_float(h->ctx, attacker, SE_FIELD_HEALTH, health + fed);
  s.blows_fed++;
  s.health_given += static_cast<int32_t>(fed);
  return points;
}

void death(void* state, const ge_mod_host_v1* h, int32_t victim, int32_t killer) {
  State& s = *static_cast<State*>(state);
  s.deaths++;
  if (killer != 0 && killer != victim) {
    s.kills++;
    h->log_line(h->ctx, GE_MOD_LOG_INFO, "vampire: a kill fed its killer");
  }
}

const se_mod_hooks_v1 kHooks = {
    sizeof(se_mod_hooks_v1),
    SE_MOD_API,
    nullptr,  // match_start
    nullptr,  // start_frame
    nullptr,  // player_pre_think
    nullptr,  // player_post_think
    nullptr,  // client_connect
    nullptr,  // put_client_in_server
    nullptr,  // client_disconnect
    nullptr,  // call
    damage,
    death,
};

const ge_mod_v1 kMod = {
    sizeof(ge_mod_v1), GE_MOD_ABI,    "vampire", "1.0.0", SE_MOD_GAME,
    SE_MOD_API,        sizeof(State), nullptr,   &kHooks,
};

}  // namespace

GE_MOD_EXPORT const ge_mod_v1* ge_mod_entry(uint32_t host_abi) {
  return GE_MOD_ABI_MAJOR_OF(host_abi) == GE_MOD_ABI_MAJOR ? &kMod : nullptr;
}
