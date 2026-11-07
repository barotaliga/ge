// The client's prediction on the original move (predict_original.hpp).
//
// ge:summary The original move predicted on a client: the stand-in server, placed, then stepped
// ge:keywords prediction original-move stand-in-server predict_player_original
// ge:skill shooter net
// ge:test tests/shooter_predict_original_test.cpp
#include "shooter/predict_original.hpp"

#include "shooter/frame_recording.hpp"
#include "shooter/framemove.hpp"

namespace ge::shooter {

namespace {

constexpr int kPlaceMax = 64;  // movers, and players, each
constexpr int kPieceMs = 50;   // a command longer than this is split in halves, as the server does
constexpr int kPlayer = 1;     // the player's entity

void run_piece(Server& sv, const MoveCmd& cmd) {
  const double dt = cmd.msec * 0.001;
  const FrameMoveSettings set = frame_settings(sv.set);
  Edict& ed = sv.edicts[kPlayer];
  ed.button2 = static_cast<float>((cmd.buttons & kButtonJump) >> 1);
  frame_think(sv, kPlayer, frame_command(cmd), set, dt);
  if (!sv.set.program_jumps) frame_program_move(sv, kPlayer, ed.button2 != 0, dt);
  frame_move(sv, kPlayer, set, dt);
}

void run_split(Server& sv, const MoveCmd& cmd) {
  if (cmd.msec <= kPieceMs) {
    run_piece(sv, cmd);
    return;
  }
  MoveCmd half = cmd;
  half.msec = static_cast<uint8_t>(cmd.msec / 2);
  run_split(sv, half);
  run_split(sv, half);
}

// Slot `e` as a fresh entity in use.
Edict& take(Server& sv, int e) {
  Edict fresh;
  fresh.free = false;
  sv.edicts[static_cast<size_t>(e)] = fresh;
  return sv.edicts[static_cast<size_t>(e)];
}

}  // namespace

void original_prediction_level(OriginalPrediction& p, const std::vector<BrushModel>& models,
                               const ServerSettings& set) {
  ServerSettings s = set;
  s.maxclients = 1;
  s.max_edicts = 2 + 2 * kPlaceMax;
  s.first_dynamic = 2;
  s.single_player_clock = false;
  s.playermove = PlayerMove::original;
  s.start_time = 1;
  create_server(p.sv, s, models, Program{});
  p.sv.num_edicts = s.max_edicts;
  take(p.sv, kPlayer);
  p.placed = 0;
}

void original_prediction_place(OriginalPrediction& p, const ModelAt* movers, int mover_count,
                               const Vec3f* players, int player_count) {
  Server& sv = p.sv;
  for (int i = 0; i < p.placed; i++) free_edict(sv, 2 + i);
  int e = 2;
  for (int i = 0; i < mover_count && i < kPlaceMax; i++) {
    const int index = movers[i].model + 1;  // the server's modelindex: 0 none, 1 the world
    if (movers[i].model <= 0 || static_cast<size_t>(index) >= sv.models.size()) continue;
    Edict& ed = take(sv, e);
    ed.solid = kSolidBsp;
    ed.movetype = kMovetypePush;
    set_brush_model(sv, e, index);
    set_origin(sv, e, movers[i].origin);
    e++;
  }
  for (int i = 0; i < player_count && i < kPlaceMax; i++) {
    Edict& ed = take(sv, e);
    ed.solid = kSolidSlidebox;
    ed.movetype = kMovetypeWalk;
    set_size(sv, e, kPlayerMins, kPlayerMaxs);
    set_origin(sv, e, players[i]);
    e++;
  }
  p.placed = e - 2;
}

MoveState predict_player_original(OriginalPrediction& p, const NetPlayer& acked,
                                  const MoveCmd* cmds, int count) {
  Server& sv = p.sv;
  const MoveState from = unquantize_player(acked, sv.set.move);
  unlink_edict(sv, kPlayer);
  take(sv, kPlayer);
  stand_frame_player(sv, kPlayer, from.origin, from.velocity);
  Edict& ed = sv.edicts[kPlayer];
  if (from.ground != -1) ed.flags |= kFlagOnGround;
  if (acked.jump_held) ed.flags &= ~kFlagJumpReleased;
  for (int i = 0; i < count; i++) run_split(sv, cmds[i]);
  MoveState out = from;
  out.origin = ed.origin;
  out.velocity = ed.velocity;
  out.ground = (ed.flags & kFlagOnGround) != 0 ? 0 : -1;
  out.jump_held = (ed.flags & kFlagJumpReleased) == 0;
  return out;
}

}  // namespace ge::shooter
