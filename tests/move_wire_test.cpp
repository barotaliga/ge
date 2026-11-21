// A moving body on the wire (engine/move/wire.hpp), held to M28d's promises: everything a step
// reads survives the body's row, so a body rebuilt from it steps as the one it was packed from; the
// row on a network's grid is a fixed point; the command's bytes say the same thing twice; a long
// command is split in equal pieces under the cap, the count settled by its length alone.
//
// ge:summary Tests: the body's row, the command's bytes, a long command split under a cap
// ge:keywords test move wire pack unpack quantize command msec split step_wire prediction
// ge:skill move testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "move/box_world.hpp"
#include "move/scenarios.hpp"
#include "move/wire.hpp"
#include "net/replicate.hpp"

namespace mv = ge::move;
namespace gn = ge::net;

namespace {

constexpr double kHz = 60;

gn::FieldSpec spec_of(int i) {
  const mv::WireField& w = mv::wire_fields()[i];
  return gn::FieldSpec{w.min, w.max, w.bits, false};
}

// Every step of a scenario stepped again from its state's row: how far apart the two ends were.
struct Replayed {
  const mv::MoveSpec* spec = nullptr;
  mv::World world;
  mv::MoveState prev;
  bool has_prev = false;
  double worst = 0;
  int steps = 0;
  static void on(void* ctx, const mv::MoveState& s, const mv::MoveCmd& cmd, const mv::MoveEvents&) {
    auto* r = static_cast<Replayed*>(ctx);
    if (r->has_prev) {
      double row[mv::kWireFields];
      mv::pack_body(r->prev, row);
      mv::MoveState again = mv::unpack_body(row);
      mv::step_move(*r->spec, r->world, again, cmd, 1 / kHz);
      const double d =
          mv::length(mv::sub(again.pos, s.pos)) + mv::length(mv::sub(again.vel, s.vel));
      r->worst = d > r->worst ? d : r->worst;
      r->steps++;
    }
    r->prev = s;
    r->has_prev = true;
  }
};

}  // namespace

TEST(every_field_a_step_reads_survives_the_bodys_row) {
  int runs = 0;
  for (const mv::MoveScenario& sc : mv::move_scenarios()) {
    if (sc.animate != nullptr) continue;  // the world moves under it: not a static replay
    const mv::MoveSpec& spec = *mv::find_preset(sc.preset);
    const mv::BoxWorld bw = mv::scenario_world(sc, spec);
    Replayed r;
    r.spec = &spec;
    r.world = mv::as_world(bw);
    mv::run_move_scenario(sc, spec, kHz, Replayed::on, &r);
    // The ground's height is rebuilt from its plan (a last bit on a ramp), nothing else moves.
    CHECK_MSG(r.worst < 1e-6, std::string(sc.id) + ": a body rebuilt from its row stepped " +
                                  std::to_string(r.worst) + " L away; next: a field the step " +
                                  "reads is missing from pack_body (engine/move/wire.cpp)");
    CHECK(r.steps > 0);
    runs++;
  }
  CHECK(runs >= 20);
}

TEST(the_row_on_the_networks_grid_is_a_fixed_point) {
  mv::MoveState s;
  s.pos = {123.456, -7.25, 3000.125};
  s.vel = {-320.5, 270, 12.0625};
  s.grounded = true;
  s.ducked = true;
  s.jump_held = true;
  s.air_jumps_left = 2;
  s.ground_solid = 17;
  s.crouch = 0.4;
  s.since_ground = mv::kSinceNever;
  s.buffer_left = -0.004;
  s.slide_ready = 0.75;
  s.ground_normal = {0.6, 0.8, 0};
  s.ground_vel = {64, -32, 0};
  double row[mv::kWireFields], once[mv::kWireFields], twice[mv::kWireFields];
  mv::pack_body(s, row);
  for (int i = 0; i < mv::kWireFields; i++)
    once[i] = gn::dequantize(spec_of(i), gn::quantize(spec_of(i), row[i]));
  mv::pack_body(mv::unpack_body(once), twice);
  for (int i = 0; i < mv::kWireFields; i++) {
    const uint32_t a = gn::quantize(spec_of(i), once[i]), b = gn::quantize(spec_of(i), twice[i]);
    CHECK_MSG(a == b, std::string(mv::wire_fields()[i].name) + " moved on its second trip");
  }
  const mv::MoveState back = mv::unpack_body(once);
  CHECK(back.grounded && back.ducked && back.jump_held && !back.sliding && !back.on_ladder);
  CHECK(back.air_jumps_left == 2 && back.ground_solid == 17);
  CHECK(back.since_ground == mv::kSinceNever);
  CHECK(back.buffer_left < 0);
  CHECK(std::fabs(back.ground_normal.y - 0.8) < 1e-4);
  CHECK(std::fabs(back.pos.z - 3000.125) < 1e-3);
}

TEST(the_commands_bytes_say_the_same_thing_twice) {
  mv::MoveCmd cmd;
  cmd.forward = 1;
  cmd.side = -0.5;
  cmd.yaw = -1.25;  // a heading west of north: wrapped into the turn
  cmd.pitch = 0.3;
  cmd.jump = true;
  cmd.sprint = true;
  uint8_t a[mv::kWireCmdBytes], b[mv::kWireCmdBytes];
  mv::pack_cmd(cmd, 17, a);
  double dt = 0;
  const mv::MoveCmd got = mv::unpack_cmd(a, &dt);
  mv::pack_cmd(got, 17, b);
  for (int i = 0; i < mv::kWireCmdBytes; i++) CHECK(a[i] == b[i]);
  CHECK(got.forward == 1 && std::fabs(got.side + 0.5) < 0.01);
  CHECK(std::fabs(got.yaw - (-1.25 + ge::core::kTau)) < 1e-4);
  CHECK(std::fabs(got.pitch - 0.3) < 1e-4);
  CHECK(got.jump && got.sprint && !got.crouch && !got.walk);
  CHECK(dt == 0.017);
  mv::pack_cmd(cmd, 900, a);  // a hitch of most of a second: the byte's most
  mv::unpack_cmd(a, &dt);
  CHECK(dt == 0.255);
}

TEST(a_long_command_is_split_in_equal_pieces_under_the_cap) {
  CHECK(mv::split_count(1 / 60.0, 0) == 1);
  CHECK(mv::split_count(1 / 60.0, 1 / 60.0) == 1);
  CHECK(mv::split_count(0.017, 1 / 60.0) == 2);
  CHECK(mv::split_count(0.25, 1 / 60.0) == 15);
  CHECK(mv::split_count(0.255, 0.05) == 6);
  // A 0.1 s jump command under a 1/60 cap is the same as six steps of a sixth of it.
  const mv::MoveSpec& spec = *mv::find_preset("classic_arena");
  mv::BoxWorld bw;
  mv::add_box(bw, {-4096, -64, -4096}, {4096, 0, 4096});
  const mv::World world = mv::as_world(bw);
  mv::MoveCmd cmd;
  cmd.forward = 1;
  cmd.jump = true;
  mv::MoveState a = mv::stand_at(spec, world, {0, 0, 0}), b = a;
  const mv::MoveEvents ev = mv::step_move_split(spec, world, a, cmd, 0.1, 1 / 60.0);
  for (int i = 0; i < 6; i++) mv::step_move(spec, world, b, cmd, 0.1 / 6);
  CHECK_BITS(a.pos.y, b.pos.y);
  CHECK_BITS(a.pos.z, b.pos.z);
  CHECK_BITS(a.vel.y, b.vel.y);
  CHECK(ev.has(mv::kJumped));
  // The step through the wire is the same step: the row and the bytes in, the row out.
  double row[mv::kWireFields];
  mv::pack_body(mv::stand_at(spec, world, {0, 0, 0}), row);
  uint8_t bytes[mv::kWireCmdBytes];
  mv::pack_cmd(cmd, 100, bytes);
  const mv::MoveEvents wev = mv::step_wire(spec, world, 1 / 60.0, row, bytes);
  CHECK(wev.has(mv::kJumped));
  CHECK(std::fabs(row[1] - a.pos.y) < 1e-9 && std::fabs(row[2] - a.pos.z) < 1e-9);
}
