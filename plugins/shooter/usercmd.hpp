// THE CLIENT'S COMMANDS: how a client turns keys and a view into the commands it sends, at its
// physics rate and independent of its render rate.
//
// THE RATE (`create_cmd_clock`): the server's `maxfps` (77 by default), or lower when the player
// sets one, never below 10. Each rendered frame adds its time to an accumulator; reaching one
// period produces a command and takes one period off (the whole accumulator when it holds more than
// two: a slow machine does not catch up) (`cmd_due`).
// THE LENGTH (`next_msec`): a running total of command milliseconds gains the period x 1000; the
// command's msec is its whole part and the fraction is kept (77 a second gives 12 and 13 ms
// commands averaging 12.987); a length over 250 is replaced by 100.
// THE AXES (`build_cmd`): each key's share of the period (`key_share`): held throughout 1, pressed
// during it and still held 1 (0.5 for a look key), released during it 0, pressed and released
// within it 0.25, released and pressed again 0.75. Speeds 400 forward, back, sideways and up, x2
// with the speed key held; each axis rounded DOWN to a multiple of 4 and clamped to +-508 (the grid
// the protocol carries).
// THE BUTTONS: attack 1, jump 2, use 4, second attack 8, each set when its key was down at any
// moment of the period.
// THE ANGLES (`quantize_angle`): on the protocol's 16-bit grid (nearest, 65536 steps a turn, read
// back signed), so a client predicts with exactly the angles the server will use.
//
// ge:summary The client's commands: the command rate, msec carry, key shares, axes, the angle grid
// ge:keywords usercmd client cmd-rate maxfps msec key-state key_share axes forwardmove sidemove
// ge:keywords upmove speed-key buttons angle-grid quantize_angle build_cmd cmd_due next_msec
// ge:skill shooter input
// ge:test tests/shooter_usercmd_test.cpp
#pragma once

#include <cstdint>

#include "shooter/pmove.hpp"

namespace ge::shooter {

constexpr int kDefaultMaxFps = 77;
constexpr int kMinCmdRate = 10;
constexpr int kAxisGrid = 4;
constexpr int kAxisMax = 508;

// One key over one command period: down at its end, and whether it was pressed or released
// during it. `key_press` / `key_release` record an edge; `build_cmd` clears the edges.
struct KeyState {
  bool down = false;
  bool pressed = false;
  bool released = false;
};
void key_press(KeyState& k);
void key_release(KeyState& k);
// The key's share of the period, 0..1; a look key's fresh press counts half.
float key_share(const KeyState& k, bool look_key = false);

struct MoveKeys {
  KeyState forward, back, moveleft, moveright, moveup, movedown, speed;
  KeyState attack, jump, use, attack2;
};

// The client's movement speeds, units/s.
struct MoveSpeeds {
  float forward = 400, back = 400, side = 400, up = 400;
  float speedkey = 2;  // cl_movespeedkey: the factor while the speed key is held
};

struct CmdClock {
  double period = 1.0 / kDefaultMaxFps;  // s
  double acc = 0;                        // render time not yet spent on a command, s
  double ms_total = 0;                   // command milliseconds not yet given out (the fraction)
};

// The clock at the rate the server allows (`server_maxfps`) and the player asks (0: no ask).
CmdClock create_cmd_clock(int server_maxfps = kDefaultMaxFps, int player_rate = 0);
// Adds a rendered frame's time; true when a command is due (one period taken off).
bool cmd_due(CmdClock& clock, double frame_s);
// The next command's length, ms.
uint8_t next_msec(CmdClock& clock);

// An angle on the protocol's grid, degrees.
float quantize_angle(float deg);
// An axis on the protocol's grid: rounded down to a multiple of 4, clamped to +-508.
int16_t quantize_axis(float v);

// The command for this period; clears every key's edges.
MoveCmd build_cmd(MoveKeys& keys, Vec3f view, uint8_t msec, uint8_t impulse,
                  const MoveSpeeds& speeds = {});

}  // namespace ge::shooter
