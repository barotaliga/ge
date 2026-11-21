// A moving body on the wire (move/wire.hpp): its fields, the command's bytes, the split.
//
// ge:summary The body's fields and the command's bytes packed and unpacked, the split step
// ge:keywords move wire pack unpack quantize command msec split step_wire
// ge:skill move net
// ge:test tests/move_wire_test.cpp
#include "move/wire.hpp"

#include <cmath>

#include "core/math.hpp"

namespace ge::move {

namespace {

enum : uint8_t {
  kX,
  kY,
  kZ,
  kVx,
  kVy,
  kVz,
  kFlags,
  kCrouch,
  kSince,
  kBuffer,
  kSlide,
  kNx,
  kNz,
  kGx,
  kGy,
  kGz,
};

// The ground's solid shares the flags word with twelve bits of state: 12 bits of its own.
constexpr uint32_t kMaxSolid = 4095;
constexpr uint32_t kMaxAirJumps = 15;
enum : uint8_t {
  kBitGrounded = 1u << 2,
  kBitDucked = 1u << 3,
  kBitSliding = 1u << 4,
  kBitJumpHeld = 1u << 5,
  kBitJumpRising = 1u << 6,
  kBitOnLadder = 1u << 7,
};

constexpr WireField kFields[kWireFields] = {
    {"x", -16384, 16384, 28},       {"y", -16384, 16384, 28},       {"z", -16384, 16384, 28},
    {"vx", -8192, 8192, 26},        {"vy", -8192, 8192, 26},        {"vz", -8192, 8192, 26},
    {"flags", 0, 16777215, 24},     {"crouch", 0, 1, 12},           {"since_ground", 0, 8, 16},
    {"buffer_left", -1, 3, 16},     {"slide_ready", -1, 7, 16},     {"normal_x", -1, 1, 16},
    {"normal_z", -1, 1, 16},        {"ground_vx", -4096, 4096, 20}, {"ground_vy", -4096, 4096, 20},
    {"ground_vz", -4096, 4096, 20},
};

double clamp_to(int i, double v) {
  const WireField& f = kFields[i];
  return v < f.min ? f.min : v > f.max ? f.max : v;
}

int8_t stick(double v) {
  const double c = v < -1 ? -1 : v > 1 ? 1 : v;
  return static_cast<int8_t>(std::floor(c * 127 + 0.5));
}

}  // namespace

const WireField* wire_fields() { return kFields; }

void pack_body(const MoveState& s, double* f) {
  f[kX] = s.pos.x, f[kY] = s.pos.y, f[kZ] = s.pos.z;
  f[kVx] = s.vel.x, f[kVy] = s.vel.y, f[kVz] = s.vel.z;
  uint32_t bits = static_cast<uint32_t>(s.mode) & 3u;
  if (s.grounded) bits |= kBitGrounded;
  if (s.ducked) bits |= kBitDucked;
  if (s.sliding) bits |= kBitSliding;
  if (s.jump_held) bits |= kBitJumpHeld;
  if (s.jump_rising) bits |= kBitJumpRising;
  if (s.on_ladder) bits |= kBitOnLadder;
  const auto air = static_cast<uint32_t>(s.air_jumps_left < 0 ? 0 : s.air_jumps_left);
  bits |= (air > kMaxAirJumps ? kMaxAirJumps : air) << 8;
  bits |= (s.ground_solid > kMaxSolid ? kMaxSolid : s.ground_solid) << 12;
  f[kFlags] = bits;
  f[kCrouch] = s.crouch;
  f[kSince] = s.since_ground;
  f[kBuffer] = s.buffer_left;
  f[kSlide] = s.slide_ready;
  f[kNx] = s.ground_normal.x, f[kNz] = s.ground_normal.z;
  f[kGx] = s.ground_vel.x, f[kGy] = s.ground_vel.y, f[kGz] = s.ground_vel.z;
  for (int i = 0; i < kWireFields; i++) f[i] = clamp_to(i, f[i]);
}

MoveState unpack_body(const double* f) {
  MoveState s;
  s.pos = {f[kX], f[kY], f[kZ]};
  s.vel = {f[kVx], f[kVy], f[kVz]};
  // The flags arrive as the nearest whole number a quantizer gave back.
  const auto bits = static_cast<uint32_t>(std::floor(f[kFlags] + 0.5));
  s.mode = static_cast<Mode>(bits & 3u);
  s.grounded = (bits & kBitGrounded) != 0;
  s.ducked = (bits & kBitDucked) != 0;
  s.sliding = (bits & kBitSliding) != 0;
  s.jump_held = (bits & kBitJumpHeld) != 0;
  s.jump_rising = (bits & kBitJumpRising) != 0;
  s.on_ladder = (bits & kBitOnLadder) != 0;
  s.air_jumps_left = static_cast<int32_t>((bits >> 8) & kMaxAirJumps);
  s.ground_solid = (bits >> 12) & kMaxSolid;
  s.crouch = f[kCrouch];
  s.since_ground = f[kSince] >= kFields[kSince].max ? kSinceNever : f[kSince];
  s.buffer_left = f[kBuffer];
  s.slide_ready = f[kSlide];
  const double nx = f[kNx], nz = f[kNz], rest = 1 - nx * nx - nz * nz;
  s.ground_normal = {nx, rest > 0 ? std::sqrt(rest) : 0.0, nz};
  s.ground_vel = {f[kGx], f[kGy], f[kGz]};
  return s;
}

void pack_cmd(const MoveCmd& cmd, int msec, uint8_t* b) {
  b[0] = static_cast<uint8_t>(stick(cmd.forward));
  b[1] = static_cast<uint8_t>(stick(cmd.side));
  b[2] = static_cast<uint8_t>(stick(cmd.up));
  double turn = core::num::mod(cmd.yaw, core::kTau) / core::kTau;  // the sign of the yaw
  if (turn < 0) turn += 1;
  const auto yaw = static_cast<uint32_t>(std::floor(turn * 65536 + 0.5)) & 0xffffu;
  b[3] = static_cast<uint8_t>(yaw & 0xff), b[4] = static_cast<uint8_t>(yaw >> 8);
  double p = std::floor(cmd.pitch / (core::kPi / 2) * 32767 + 0.5);
  p = p < -32767 ? -32767 : p > 32767 ? 32767 : p;
  const auto pitch = static_cast<uint16_t>(static_cast<int16_t>(p));
  b[5] = static_cast<uint8_t>(pitch & 0xff), b[6] = static_cast<uint8_t>(pitch >> 8);
  b[7] = static_cast<uint8_t>((cmd.jump ? 1 : 0) | (cmd.crouch ? 2 : 0) | (cmd.walk ? 4 : 0) |
                              (cmd.sprint ? 8 : 0));
  b[8] = static_cast<uint8_t>(msec < 1 ? 1 : msec > 255 ? 255 : msec);
}

MoveCmd unpack_cmd(const uint8_t* b, double* dt) {
  MoveCmd cmd;
  cmd.forward = static_cast<int8_t>(b[0]) / 127.0;
  cmd.side = static_cast<int8_t>(b[1]) / 127.0;
  cmd.up = static_cast<int8_t>(b[2]) / 127.0;
  cmd.yaw = (b[3] | (b[4] << 8)) * (core::kTau / 65536);
  cmd.pitch =
      static_cast<int16_t>(static_cast<uint16_t>(b[5] | (b[6] << 8))) * (core::kPi / 2 / 32767);
  cmd.jump = (b[7] & 1) != 0;
  cmd.crouch = (b[7] & 2) != 0;
  cmd.walk = (b[7] & 4) != 0;
  cmd.sprint = (b[7] & 8) != 0;
  if (dt != nullptr) *dt = b[8] / 1000.0;
  return cmd;
}

int split_count(double dt, double max_dt) {
  if (!(max_dt > 0) || dt <= max_dt) return 1;
  auto n = static_cast<int>(dt / max_dt);
  if (n * max_dt < dt) n++;
  return n < 1 ? 1 : n;
}

MoveEvents step_move_split(const MoveSpec& spec, const World& world, MoveState& s,
                           const MoveCmd& cmd, double dt, double max_dt) {
  const int n = split_count(dt, max_dt);
  if (n == 1) return step_move(spec, world, s, cmd, dt);
  const double piece = dt / n;
  MoveEvents all;
  for (int i = 0; i < n; i++) {
    const MoveEvents ev = step_move(spec, world, s, cmd, piece);
    all.bits = static_cast<uint16_t>(all.bits | ev.bits);
    if (ev.impact > all.impact) all.impact = ev.impact;
    if (ev.blocked > all.blocked) all.blocked = ev.blocked;
    all.stepped += ev.stepped;
  }
  return all;
}

MoveEvents step_wire(const MoveSpec& spec, const World& world, double max_dt, double* fields,
                     const uint8_t* cmd) {
  double dt = 0;
  const MoveCmd c = unpack_cmd(cmd, &dt);
  MoveState s = unpack_body(fields);
  const MoveEvents ev = step_move_split(spec, world, s, c, dt, max_dt);
  pack_body(s, fields);
  return ev;
}

}  // namespace ge::move
