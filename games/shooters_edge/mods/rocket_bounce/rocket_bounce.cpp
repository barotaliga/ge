// THE SAMPLE MOD `rocket-bounce` (docs/mods.md): a Shooter's Edge mod in C++ against the mod ABI
// (engine/mod/abi.hpp) and the game's API (games/shooters_edge/sim/mod_api.hpp), built by CMake as
// a shared library with its manifest (cmake/Mods.cmake, ge_add_mod) and loaded by the tests and the
// `mod` lab. It changes two rules of the match:
//
// - ROCKETS FLY FASTER: after a player's post-think, a missile the game just fired (`newmis`, a
//   fly-missile) gets its velocity times kRocketScale.
// - GRENADES COUNT THEIR BOUNCES: a grenade the game just threw has its touch taken over by the
//   mod's function 1, which counts the bounce (a touch of anything but its owner), runs the game's
//   own touch after it, and on the kBurstBounce-th bounce sets the grenade's fuse to now, so the
//   game's own think explodes it on the next frame.
//
// Everything the mod remembers is in `State`, the block the game keeps, copies into its snapshots
// and folds into its digest: no global, no clock, no <cmath> (the host's libm is in the table).
//
// ge:summary The sample mod rocket-bounce: faster rockets and a grenade bounce counter, in C++
// ge:keywords mod sample-mod rocket-bounce rocket grenade bounce counter newmis touch fuse
// ge:keywords ge_mod_entry state-block
// ge:skill mod shooter
// ge:test tests/shooters_edge_mod_test.cpp
#include "shooters_edge/sim/mod_api.hpp"

namespace {

constexpr float kRocketScale = 1.5f;
constexpr int32_t kBurstBounce = 3;
constexpr int kSlots = 2048;  // entity numbers the state tracks (the protocol's limit)
constexpr uint32_t kCountBounce = 1;

struct State {
  int32_t rockets_sped;
  int32_t grenades;
  int32_t bounces;
  int32_t bursts;
  int32_t game_touch[kSlots];  // a tracked grenade's own touch function (0: not tracked)
  uint8_t bounces_of[kSlots];
};

const se_mod_services_v1* se(const ge_mod_host_v1* h) {
  return static_cast<const se_mod_services_v1*>(h->game);
}

void post_think(void* state, const ge_mod_host_v1* h, int32_t /*player*/) {
  State& s = *static_cast<State*>(state);
  const se_mod_services_v1* g = se(h);
  const int32_t m = g->newmis(h->ctx);
  if (m <= 0 || m >= kSlots) return;
  const int32_t movetype = g->get_int(h->ctx, m, SE_FIELD_MOVETYPE);
  if (movetype == SE_MOVETYPE_FLYMISSILE) {
    float v[3];
    g->get_vec(h->ctx, m, SE_FIELD_VELOCITY, v);
    for (float& c : v) c = c * kRocketScale;
    g->set_vec(h->ctx, m, SE_FIELD_VELOCITY, v);
    s.rockets_sped++;
  } else if (movetype == SE_MOVETYPE_BOUNCE) {
    s.game_touch[m] = g->get_int(h->ctx, m, SE_FIELD_TOUCH);
    s.bounces_of[m] = 0;
    g->set_int(h->ctx, m, SE_FIELD_TOUCH, g->function(h->ctx, kCountBounce));
    s.grenades++;
  }
}

void count_bounce(State& s, const ge_mod_host_v1* h, int32_t self, int32_t other) {
  const se_mod_services_v1* g = se(h);
  if (self <= 0 || self >= kSlots) return;
  const int32_t game_touch = s.game_touch[self];
  const bool counts = other != g->get_int(h->ctx, self, SE_FIELD_OWNER);
  if (counts) {
    s.bounces++;
    if (s.bounces_of[self] < 255) s.bounces_of[self]++;
  }
  g->call_function(h->ctx, game_touch, self, other);  // the game's own touch, which may explode it
  if (!counts || !g->in_use(h->ctx, self)) return;
  if (g->get_int(h->ctx, self, SE_FIELD_TOUCH) != g->function(h->ctx, kCountBounce)) return;
  if (s.bounces_of[self] == kBurstBounce) {
    g->set_float(h->ctx, self, SE_FIELD_NEXTTHINK, g->time(h->ctx));
    s.bursts++;
    h->log_line(h->ctx, GE_MOD_LOG_INFO, "rocket-bounce: a grenade's third bounce lit its fuse");
  }
}

void call(void* state, const ge_mod_host_v1* h, uint32_t k, int32_t self, int32_t other) {
  State& s = *static_cast<State*>(state);
  if (k == kCountBounce) count_bounce(s, h, self, other);
}

const se_mod_hooks_v1 kHooks = {
    sizeof(se_mod_hooks_v1),
    SE_MOD_API,
    nullptr,  // match_start
    nullptr,  // start_frame
    nullptr,  // player_pre_think
    post_think,
    nullptr,  // client_connect
    nullptr,  // put_client_in_server
    nullptr,  // client_disconnect
    call,
};

const ge_mod_v1 kMod = {
    sizeof(ge_mod_v1), GE_MOD_ABI,    "rocket-bounce", "1.0.0", SE_MOD_GAME,
    SE_MOD_API,        sizeof(State), nullptr,         &kHooks,
};

}  // namespace

GE_MOD_EXPORT const ge_mod_v1* ge_mod_entry(uint32_t host_abi) {
  return GE_MOD_ABI_MAJOR_OF(host_abi) == GE_MOD_ABI_MAJOR ? &kMod : nullptr;
}
