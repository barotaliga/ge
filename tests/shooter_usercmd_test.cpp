// THE CLIENT'S COMMANDS: the command rate is the server's maxfps or the player's lower ask, never
// under 10; a rendered frame's time pays for commands one period at a time and a slow frame drops
// what is over two periods; the lengths carry their fraction (77 a second averages 12.987 ms, every
// one 12 or 13), and one over 250 ms becomes 100; each key's share of the period; the axes on the
// protocol's grid (down to a multiple of 4, within 508) and the speed key's factor; a button set
// when its key was down at any moment; the angles on the 16-bit grid.
//
// ge:summary Tests: the client's command clock, lengths, key shares, axis and angle grids
// ge:keywords test usercmd cmd-clock maxfps msec key-share axis-grid quantize-angle buttons
// ge:skill shooter testing
#include <string>

#include "check.hpp"
#include "shooter/usercmd.hpp"

namespace sh = ge::shooter;

TEST(the_command_rate_is_the_servers_or_lower_never_under_ten) {
  CHECK(sh::create_cmd_clock().period == 1.0 / 77);
  CHECK(sh::create_cmd_clock(77, 30).period == 1.0 / 30);
  CHECK(sh::create_cmd_clock(77, 200).period == 1.0 / 77);  // a higher ask is not granted
  CHECK(sh::create_cmd_clock(77, 4).period == 1.0 / 10);
  CHECK(sh::create_cmd_clock(5, 0).period == 1.0 / 10);
}

TEST(rendered_frames_pay_for_commands_a_period_at_a_time) {
  sh::CmdClock c = sh::create_cmd_clock();  // 12.987 ms
  CHECK(!sh::cmd_due(c, 0.008));
  CHECK(sh::cmd_due(c, 0.008));  // 16 ms: one command, 3.013 ms left
  CHECK(c.acc > 0.003 && c.acc < 0.0031);
  CHECK(!sh::cmd_due(c, 0.008));
  // A slow frame (more than two periods) gives one command and drops the rest.
  CHECK(sh::cmd_due(c, 0.1));
  CHECK(c.acc == 0);
}

TEST(command_lengths_carry_their_fraction_and_average_the_period) {
  sh::CmdClock c = sh::create_cmd_clock();
  int total = 0, twelves = 0;
  for (int i = 0; i < 77 * 4; i++) {
    const int ms = sh::next_msec(c);
    CHECK(ms == 12 || ms == 13);
    if (ms == 12) twelves++;
    total += ms;
  }
  // Four seconds of commands spend four seconds, give or take the fraction still held.
  CHECK_MSG(total >= 3999 && total <= 4000, "four seconds of commands: " + std::to_string(total));
  CHECK(twelves >= 3 && twelves <= 5);  // one 12 a second, the rest 13

  sh::CmdClock slow = sh::create_cmd_clock(77, 0);
  slow.period = 0.3;  // a 300 ms command is longer than 250: sent as 100
  CHECK(sh::next_msec(slow) == 100);
}

TEST(each_key_counts_its_share_of_the_period) {
  sh::KeyState held;
  sh::key_press(held);
  CHECK(sh::key_share(held) == 1.0f);        // pressed during it and still held
  CHECK(sh::key_share(held, true) == 0.5f);  // ... a look key: half
  held.pressed = false;
  CHECK(sh::key_share(held) == 1.0f);  // held the whole time

  sh::KeyState released = held;
  sh::key_release(released);
  CHECK(sh::key_share(released) == 0.0f);

  sh::KeyState tap;
  sh::key_press(tap);
  sh::key_release(tap);
  CHECK(sh::key_share(tap) == 0.25f);

  sh::KeyState again = held;
  sh::key_release(again);
  sh::key_press(again);
  CHECK(sh::key_share(again) == 0.75f);

  sh::KeyState never;
  CHECK(sh::key_share(never) == 0.0f);
}

TEST(the_axes_sit_on_the_protocols_grid) {
  CHECK(sh::quantize_axis(400) == 400);
  CHECK(sh::quantize_axis(3) == 0);
  CHECK(sh::quantize_axis(-1) == -4);  // down, toward negative infinity
  CHECK(sh::quantize_axis(-100) == -100);
  CHECK(sh::quantize_axis(-101) == -104);
  CHECK(sh::quantize_axis(800) == 508);
  CHECK(sh::quantize_axis(-800) == -508);
}

TEST(a_command_takes_the_keys_the_speed_key_and_the_buttons) {
  sh::MoveKeys k;
  sh::key_press(k.forward);
  sh::key_press(k.moveleft);
  sh::key_release(k.moveleft);  // a tap: a quarter of 400
  sh::key_press(k.jump);
  sh::key_release(k.jump);  // down for a moment of the period: the button is set
  sh::MoveCmd c = sh::build_cmd(k, {10, 90, 0}, 13, 7);
  CHECK(c.msec == 13);
  CHECK(c.forward == 400);
  CHECK(c.side == -100);
  CHECK(c.up == 0);
  CHECK(c.buttons == sh::kButtonJump);
  CHECK(c.impulse == 7);
  CHECK(!k.forward.pressed && !k.jump.released);  // the edges are spent

  sh::key_press(k.speed);
  c = sh::build_cmd(k, {}, 13, 0);
  CHECK(c.forward == 508);  // 800 clamped to the grid
  CHECK(c.buttons == 0);    // the jump key is up the whole period
}

TEST(the_angles_sit_on_the_sixteen_bit_grid) {
  CHECK(sh::quantize_angle(90) == 90.0f);
  CHECK(sh::quantize_angle(-45) == -45.0f);
  const float step = 360.0f / 65536;
  const float q = sh::quantize_angle(10.001f);
  CHECK(q - 10.001f < step && 10.001f - q < step);
  CHECK(sh::quantize_angle(q) == q);       // already on the grid
  CHECK(sh::quantize_angle(180) == -180);  // read back signed
  CHECK(sh::quantize_angle(270) == -90);
}
