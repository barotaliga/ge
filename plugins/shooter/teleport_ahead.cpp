// Teleports a client foresees (teleport_ahead.hpp).
//
// ge:summary Teleports a client foresees: the table, the prediction's hit, the guess and its answer
// ge:keywords teleport teleporter predict-teleport tele_guess teleporter_at
// ge:skill shooter net
// ge:test tests/shooter_teleport_ahead_test.cpp
#include "shooter/teleport_ahead.hpp"

namespace ge::shooter {

namespace {

float wrap180(float d) {
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

bool overlaps(const Teleporter& t, Vec3f lo, Vec3f hi) {
  return !(lo.x > t.hi.x || lo.y > t.hi.y || lo.z > t.hi.z || hi.x < t.lo.x || hi.y < t.lo.y ||
           hi.z < t.lo.z);
}

float magnitude(float d) { return d < 0 ? -d : d; }

Vec3f middle(const Teleporter& t) { return (t.lo + t.hi) * 0.5f; }

float distance(Vec3f a, Vec3f b) { return length(a - b); }

}  // namespace

int teleporter_at(const Teleporters& t, Vec3f origin, Vec3f mins, Vec3f maxs, float grow,
                  int skip) {
  const Vec3f g{grow, grow, grow};
  const Vec3f lo = origin + mins - g, hi = origin + maxs + g;
  for (size_t i = 0; i < t.size(); i++)
    if (static_cast<int>(i) != skip && overlaps(t[i], lo, hi)) return static_cast<int>(i);
  return -1;
}

void teleport_state(MoveState& s, const Teleporter& t) {
  s.origin = t.dest;
  s.velocity = t.velocity;
  s.ground = -1;
}

MoveState predict_player_teleports(const NetPlayer& acked, const MoveCmd* cmds, int count,
                                   const MoveSettings& set, const MoveWorld& world, MoveState* each,
                                   const Teleporters* tele, int skip, TeleHit* hit) {
  MoveState s = unquantize_player(acked, set);
  if (hit != nullptr) *hit = {};
  for (int i = 0; i < count; i++) {
    run_command(s, cmds[i], set, world);
    if (tele != nullptr && s.type == MoveType::normal) {
      const int k = teleporter_at(*tele, s.origin, kPlayerMins, kPlayerMaxs, kTeleLinkGrow, skip);
      if (k >= 0) {
        teleport_state(s, (*tele)[static_cast<size_t>(k)]);
        if (hit != nullptr && hit->cmd < 0) *hit = {i, k};
      }
    }
    if (each != nullptr) each[i] = s;
  }
  return s;
}

Vec3f tele_guess_hit(TeleGuess& g, const Teleporters& t, uint32_t seq, int index, Vec3f cmd_angles,
                     Vec3f& view) {
  if (index < 0 || static_cast<size_t>(index) >= t.size()) return {};
  if (g.seq != 0 && g.index == index) {  // the same one, its command found again
    g.seq = seq;
    return {};
  }
  const Teleporter& tl = t[static_cast<size_t>(index)];
  const Vec3f to{tl.angles.x, view.y + wrap180(tl.angles.y - cmd_angles.y), 0};
  const Vec3f turn = to - view;
  view = to;
  g.seq = seq;
  g.index = index;
  g.turn = turn;
  g.undone = false;
  return turn;
}

Vec3f tele_guess_acked(TeleGuess& g, const Teleporters& t, uint32_t acked, Vec3f server_origin,
                       Vec3f mins, Vec3f maxs, Vec3f& view) {
  if (g.skip >= 0 && teleporter_at(t, server_origin, mins, maxs, kTeleLinkGrow) != g.skip)
    g.skip = -1;  // out of the box the server kept him in
  if (g.seq == 0 || g.index < 0 || static_cast<size_t>(g.index) >= t.size()) return {};
  if (acked < g.seq) return {};
  const Teleporter& tl = t[static_cast<size_t>(g.index)];
  if (distance(server_origin, tl.dest) < distance(server_origin, middle(tl))) {
    g.seq = 0;  // the server has him there: right
    g.index = -1;
    return {};
  }
  if (acked - g.seq < kTeleUndoAfter) return {};
  view = view - g.turn;  // wrong: the turn taken back, his own turning since kept
  g.undone = true;
  g.skip = g.index;
  g.seq = 0;
  return -g.turn;
}

bool tele_guess_setangle(TeleGuess& g, const Teleporters& t, Vec3f server_angles, Vec3f& view) {
  if (g.index < 0 || static_cast<size_t>(g.index) >= t.size()) return false;
  const Teleporter& tl = t[static_cast<size_t>(g.index)];
  const bool same = magnitude(wrap180(server_angles.y - tl.angles.y)) < kTeleSameDeg &&
                    magnitude(wrap180(server_angles.x - tl.angles.x)) < kTeleSameDeg;
  if (!same) {
    const int skip = g.skip;
    g = TeleGuess{};
    g.skip = skip;
    return false;
  }
  if (g.undone) {  // undone too soon: turned again
    view = view + g.turn;
    g.skip = -1;
  }
  g.seq = 0;
  g.index = -1;
  g.undone = false;
  return true;
}

}  // namespace ge::shooter
