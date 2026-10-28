// LAB `input-session`: a scripted session on simulated devices (engine/input/sim.hpp) played
// through three games' action maps at once, so every device kind is shown driving every genre's
// controls: a SKIER's (steer, tuck, brake, jump, reset: keys ramped, a pad, touch), a SHOOTER's
// (move on WASD or a stick, look on the raw mouse, a stick or the gyro, fire, jump, crouch) and
// a RACER's (steer on a wheel's role or a pad, throttle, brake and clutch pedals, the shifter's
// paddles). The session is recorded as an event log, written out, read back and replayed: every
// frame's action values must come back to the bit (the lab fails otherwise). Then two players
// join on their pads, one's cable comes out and goes back in; a rebinding screen captures a
// pad button for JUMP and names its conflicts; and a pad whose gyro reads a bias at rest is held
// still, turned and held still again (engine/input/gyro.hpp): the bias must settle while it is
// still and stay through the turn, so the aim does not creep.
//
// ge:summary Lab: a scripted session on every simulated device through skier, shooter and racer
// ge:summary maps, recorded and replayed to the bit, players joining and rebinding
// ge:keywords lab input session simulated keyboard mouse gamepad wheel pedals touch gyro
// ge:keywords shooter racer skier action-map event-log replay players rebind hot-plug
// ge:keywords gyro-bias still settle creep
// ge:skill input labs
// ge:test tests/input_actions_test.cpp tests/input_sim_test.cpp tests/input_gyro_test.cpp
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "input/actions.hpp"
#include "input/bindings_text.hpp"
#include "input/event_log.hpp"
#include "input/gyro.hpp"
#include "input/names.hpp"
#include "input/players.hpp"
#include "input/rebind.hpp"
#include "input/sim.hpp"
#include "input/touch.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"

namespace {

namespace gl = ge::lab;
namespace in = ge::input;

constexpr const char* kSkier = R"(ge-bindings 1
context ride
bind steer key:a value=-1 ramp=6,9
bind steer key:d ramp=6,9
bind steer pad:left_x deadzone=0.12,0.97
bind steer touch:stick
bind tuck key:w ramp=4,12
bind tuck pad:right_trigger
bind tuck touch:tuck
bind brake key:s ramp=20,30
bind brake pad:left_trigger
bind jump key:space
bind jump pad:south
bind jump touch:jump
bind reset key:r
bind reset pad:north
)";

constexpr const char* kShooter = R"(ge-bindings 1
context play
bind move key:w component=y value=-1
bind move key:s component=y
bind move key:a value=-1
bind move key:d
bind move pad:left_stick
bind look mouse:x value=0.022
bind look mouse:y component=y value=0.022
bind look pad:right_stick zone=scaled_radial deadzone=0.1,0.95 curve=1.6 rate=180
bind look gyro:y value=-57.29578
bind look gyro:x component=y value=-57.29578
bind fire mouse:left
bind fire pad:right_trigger threshold=0.3,0.2
bind jump key:space
bind jump pad:south
bind crouch key:left_ctrl
bind crouch pad:east
)";

constexpr const char* kRacer = R"(ge-bindings 1
context drive
bind steer role:steer
bind steer pad:left_x
bind steer key:left value=-1 ramp=5,8
bind steer key:right ramp=5,8
bind throttle role:throttle
bind throttle pad:right_trigger
bind throttle key:up ramp=8,12
bind brake role:brake
bind brake pad:left_trigger
bind brake key:down ramp=10,15
bind clutch role:clutch
bind shift_up role:shift_up
bind shift_up pad:right_shoulder
bind shift_down role:shift_down
bind shift_down pad:left_shoulder
bind handbrake key:space
bind handbrake pad:west
)";

struct Game {
  const char* name;
  in::ActionMap map;
  in::ActionValues v;
};

Game game(gl::Lab& lab, const char* name, std::vector<in::ActionDef> defs, const char* text) {
  Game g{name, {}, {}};
  g.map.defs = std::move(defs);
  std::string error;
  if (!in::read_bindings(text, g.map, error)) {
    lab.fail(std::string(name) + "'s bindings do not read: " + error,
             "Fix the bindings in engine/lab/labs/input_session_lab.cpp (skill: input).");
  }
  g.v = in::make_values(g.map);
  return g;
}

uint64_t hash_values(uint64_t h, const in::ActionValues& v) {
  for (const in::ActionValue& a : v.v) {
    const bool b[3] = {a.held, a.pressed, a.released};
    for (const float f : {a.x, a.y}) {
      uint32_t bits = 0;
      std::memcpy(&bits, &f, sizeof bits);
      for (int i = 0; i < 4; i++) h = (h ^ ((bits >> (8 * i)) & 0xffu)) * 0x100000001b3ull;
    }
    for (const bool x : b) h = (h ^ static_cast<unsigned char>(x)) * 0x100000001b3ull;
  }
  return h;
}

std::string fmt(float v) {
  char b[16];
  std::snprintf(b, sizeof b, "%.3f", static_cast<double>(v));
  return b;
}

// THE GYRO'S BIAS: a pad at rest reading a bias and a little noise, held still, turned at
// 0.6 rad/s for half a second, then held still again.
void gyro_bias(gl::Lab& lab) {
  constexpr double kDt = 1.0 / 120;
  constexpr float kBias[3] = {0.021f, -0.034f, 0.013f};
  in::SimInput sim;
  in::InputState s;
  in::GyroBiases g;
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  gl::Trace raw{"gyro y as read", {}, {}}, aim{"gyro y unbiased", {}, {}},
      bias{"the bias taken", {}, {}};
  double settled_at = -1, worst_bias = 0, creep = 0;
  int creep_n = 0;
  for (int i = 0; i < 360; i++) {
    const double t = i * kDt;
    const float turn = t >= 1.4 && t < 1.9 ? 0.6f : 0.0f;
    const float noise = (i % 2 ? 0.006f : -0.006f) * (i % 5 ? 1.0f : 0.4f);
    in::sim_gyro(sim, pad, kBias[0] + noise, kBias[1] + turn - noise, kBias[2] + noise);
    in::sim_advance(sim, kDt);
    in::sim_frame(sim, s);
    const float read = in::find_slot(s, pad)->gyro[1];
    in::unbias_gyro(s, g, kDt);
    const in::Slot& sl = *in::find_slot(s, pad);
    const in::GyroBias* b = nullptr;
    for (const in::GyroBias& x : g.slot) {
      if (x.seen) b = &x;
    }
    if (b && b->settled && settled_at < 0) settled_at = t;
    if (b && b->settled) {
      for (int a = 0; a < 3; a++) {
        const double e = std::fabs(b->bias[static_cast<size_t>(a)] - kBias[a]);
        worst_bias = e > worst_bias ? e : worst_bias;
      }
      if (turn == 0) {
        creep += sl.gyro[1];
        creep_n++;
      }
    }
    raw.x.push_back(t);
    raw.y.push_back(read);
    aim.x.push_back(t);
    aim.y.push_back(sl.gyro[1]);
    bias.x.push_back(t);
    bias.y.push_back(b ? b->bias[1] : 0);
  }
  const double mean_creep = creep_n ? creep / creep_n : 0;
  lab.metric("gyro.settled-after", settled_at, "s");
  lab.metric("gyro.bias-error", worst_bias, "rad/s");
  lab.metric("gyro.creep-when-still", mean_creep, "rad/s");
  if (settled_at < 0 || settled_at > 1.4 || worst_bias > 0.004 || std::fabs(mean_creep) > 0.002) {
    lab.fail("the gyro's bias did not settle while still (settled at " +
                 fmt(static_cast<float>(settled_at)) + " s, off by " +
                 fmt(static_cast<float>(worst_bias)) + " rad/s)",
             "Run tests/input_gyro_test and read engine/input/gyro.cpp (skill: input).");
  }
  gl::LineChart chart;
  chart.frame = {"THE GYRO'S BIAS, TAKEN WHILE STILL", "a pad at rest, turned, at rest again",
                 "t (s)", "rad/s about the vertical"};
  chart.spikes = false;
  chart.traces = {raw, aim, bias};
  chart.marks = {{1.4, "turn"}, {1.9, "still"}};
  const gl::Chart drawn = gl::line_chart(chart);
  lab.shot({"gyro-bias",
            drawn.title,
            "A pad's gyro about the vertical as read (a bias of -0.034 rad/s and a little noise "
            "at rest), the bias taken while it is still, and the reading with it taken off.",
            "the bias line steps to -0.034 after 0.8 s of rest and holds through the turn; "
            "the unbiased reading sits on 0 at rest and on 0.6 in the turn",
            {}},
           gl::to_picture(drawn.plot));
}

void run(gl::Lab& lab) {
  const double dt = 1.0 / lab.arg_num("fps");
  std::vector<Game> games;
  games.push_back(game(lab, "skier",
                       {{"steer", in::ActionKind::axis, false, "turn"},
                        {"tuck", in::ActionKind::axis, true, "fold into the tuck"},
                        {"brake", in::ActionKind::axis, true, "skid"},
                        {"jump", in::ActionKind::button, false, "pop"},
                        {"reset", in::ActionKind::button, false, "back on the piste"}},
                       kSkier));
  games.push_back(game(lab, "shooter",
                       {{"move", in::ActionKind::stick, false, "walk"},
                        {"look", in::ActionKind::delta, false, "turn the view, degrees"},
                        {"fire", in::ActionKind::button, false, "shoot"},
                        {"jump", in::ActionKind::button, false, "jump"},
                        {"crouch", in::ActionKind::button, false, "duck"}},
                       kShooter));
  games.push_back(game(lab, "racer",
                       {{"steer", in::ActionKind::axis, false, "turn"},
                        {"throttle", in::ActionKind::axis, true, "accelerate"},
                        {"brake", in::ActionKind::axis, true, "slow"},
                        {"clutch", in::ActionKind::axis, true, "disengage"},
                        {"shift_up", in::ActionKind::button, false, "next gear"},
                        {"shift_down", in::ActionKind::button, false, "last gear"},
                        {"handbrake", in::ActionKind::button, false, "lock the rear"}},
                       kRacer));
  in::TouchLayout touch;
  touch.controls = {{"stick", in::TouchKind::stick, 0, 0.4f, 0.4f, 1, 0.12f, 0, false},
                    {"tuck", in::TouchKind::lever, 0.6f, 0.4f, 1, 1, 0.2f, 1, false},
                    {"jump", in::TouchKind::button, 0.6f, 0, 1, 0.4f, 0.1f, 0, false}};
  in::TouchFrame tf = in::make_touch_frame(touch);
  in::DeviceDb db;
  db.put({in::sim_wheel().key,
          in::DeviceKind::wheel,
          "simulated wheel",
          900,
          {{in::Role::steer, 0, -1, 1},
           {in::Role::throttle, 1, 1, -1},
           {in::Role::brake, 2, 1, -1},
           {in::Role::clutch, 3, 1, -1}},
          {{in::Role::shift_up, 4}, {in::Role::shift_down, 5}}});
  in::Sources from;
  from.db = &db;
  from.layout = &touch;
  from.touch = &tf;

  // THE SCRIPT: what each device does, second by second.
  in::SimInput sim;
  in::InputState s;
  in::EventLog log;
  in::Queue kept;
  const auto kb = in::sim_plug(sim, in::sim_keyboard());
  const auto mouse = in::sim_plug(sim, in::sim_mouse());
  const auto pad = in::sim_plug(sim, in::sim_pad(in::Family::playstation));
  const auto wheel = in::sim_plug(sim, in::sim_wheel());
  const auto screen = in::sim_plug(sim, in::sim_touch());
  for (int a = 1; a <= 3; a++) in::sim_axis(sim, wheel, a, 1.0f);  // pedals at rest
  const int frames = static_cast<int>(lab.arg_num("seconds") / dt);
  std::vector<uint64_t> live;
  gl::Trace ski_steer{"skier steer", {}, {}}, race_steer{"racer steer", {}, {}},
      race_gas{"racer throttle", {}, {}}, shooter_look{"shooter look x", {}, {}};
  gl::LabTable moments{
      "the session at its moments", {"t s", "what happens", "skier", "shooter", "racer"}, {}};
  struct Beat {
    double t;
    const char* what;
  };
  const Beat beats[] = {{0.30, "A held (keys ramp the skier's steer)"},
                        {0.80, "A let go, the pad's stick half right"},
                        {1.30, "the mouse sweeps right (the shooter looks)"},
                        {1.80, "the wheel turned, throttle floored"},
                        {2.05, "the pad's south held, a finger on the tuck lever"},
                        {2.55, "the paddle pulled, the pad turned (gyro)"}};
  size_t beat = 0;
  for (int f = 0; f < frames; f++) {
    const double t = f * dt;
    const auto at = [&](double when) { return t <= when && when < t + dt; };
    if (at(0.1)) in::sim_key(sim, kb, in::key::a, true);
    if (at(0.5)) in::sim_key(sim, kb, in::key::a, false);
    if (at(0.6)) in::sim_axis(sim, pad, static_cast<int>(in::PadAxis::left_x), 0.5f);
    if (t >= 1.0 && t < 1.4) in::sim_mouse_move(sim, mouse, 120 * static_cast<float>(dt) * 60, 0);
    if (at(1.0)) in::sim_axis(sim, pad, static_cast<int>(in::PadAxis::left_x), 0);
    if (at(1.5)) {
      in::sim_axis(sim, wheel, 0, 0.35f);
      in::sim_axis(sim, wheel, 1, -1.0f);
    }
    if (at(2.0)) {
      in::sim_button(sim, pad, static_cast<int>(in::PadButton::south), true);
      in::sim_touch(sim, screen, 1, in::TouchPhase::down, 0.8f, 0.6f);
    }
    if (at(2.1)) in::sim_button(sim, pad, static_cast<int>(in::PadButton::south), false);
    if (at(2.2)) in::sim_touch(sim, screen, 1, in::TouchPhase::move, 0.8f, 0.7f);
    if (at(2.5)) {
      in::sim_button(sim, wheel, 4, true);
      in::sim_gyro(sim, pad, 0, 0.5f, 0);
    }
    if (at(2.6)) {
      in::sim_button(sim, wheel, 4, false);
      in::sim_gyro(sim, pad, 0, 0, 0);
    }
    const uint64_t t_ns = sim.t_ns;
    in::sim_frame(sim, s, &kept);
    log.add(t_ns, kept);
    in::touch_update(touch, s, tf);
    uint64_t h = 0xcbf29ce484222325ull;
    for (Game& g : games) {
      in::evaluate(g.map, s, dt, g.v, from);
      h = hash_values(h, g.v);
    }
    live.push_back(h);
    ski_steer.x.push_back(t);
    ski_steer.y.push_back(games[0].v[0].x);
    race_steer.x.push_back(t);
    race_steer.y.push_back(games[2].v[0].x);
    race_gas.x.push_back(t);
    race_gas.y.push_back(games[2].v[1].x);
    shooter_look.x.push_back(t);
    shooter_look.y.push_back(games[1].v[1].x);
    while (beat < std::size(beats) && t + dt > beats[beat].t) {
      const auto& sk = games[0].v;
      const auto& sh = games[1].v;
      const auto& rc = games[2].v;
      moments.rows.push_back(
          {fmt(static_cast<float>(beats[beat].t)), beats[beat].what,
           "steer " + fmt(sk[0].x) + " tuck " + fmt(sk[1].x) + (sk[3].held ? " JUMP" : ""),
           "move " + fmt(sh[0].x) + "," + fmt(sh[0].y) + " look " + fmt(sh[1].x) + "," +
               fmt(sh[1].y) + (sh[3].held ? " JUMP" : ""),
           "steer " + fmt(rc[0].x) + " gas " + fmt(rc[1].x) + (rc[4].held ? " SHIFT UP" : "")});
      beat++;
    }
    in::sim_advance(sim, dt);
  }
  lab.table(moments);
  lab.metric("skier.steer-peak-from-keys", static_cast<float>(-ski_steer.y[size_t(0.5 / dt)]), "");
  lab.metric("racer.steer-from-wheel", static_cast<float>(race_steer.y.back()), "");
  lab.metric("racer.throttle-from-pedal", static_cast<float>(race_gas.y.back()), "");

  gl::LineChart chart;
  chart.frame = {"ONE SESSION, THREE GAMES", "keys, a pad, a mouse, a wheel and a touch screen",
                 "t (s)", "action value"};
  chart.spikes = false;
  chart.traces = {ski_steer, race_steer, race_gas};
  for (const Beat& b : beats) chart.marks.push_back({b.t, ""});
  const gl::Chart drawn = gl::line_chart(chart);
  lab.shot({"session",
            drawn.title,
            "The skier's steer (keys held then the pad's stick), the racer's steer (the wheel's "
            "role) and throttle (a pedal resting at +1 read through its range) over the session.",
            "the skier's steer eases to -1 while A is held and back, then sits near 0.4 on the "
            "pad; the racer's steer steps to 0.35 and its throttle to 1 at 1.5 s",
            {{"fps", lab.arg("fps")}, {"seconds", lab.arg("seconds")}}},
           gl::to_picture(drawn.plot));

  // THE REPLAY: the log written, read back and played through the same maps.
  const std::string text = in::write_events(log);
  lab.attach("session.events", std::vector<uint8_t>(text.begin(), text.end()));
  in::EventLog back;
  std::string error;
  if (!in::read_events(text, back, error)) {
    lab.fail("the session's event log does not read back: " + error,
             "Run tests/input_sim_test and read engine/input/event_log.cpp (skill: input).");
  }
  in::InputState rs;
  in::TouchFrame rtf = in::make_touch_frame(touch);
  in::Sources rfrom = from;
  rfrom.touch = &rtf;
  for (Game& g : games) g.v = in::make_values(g.map);
  int differ = 0;
  for (size_t i = 0; i < back.frames.size() && i < live.size(); i++) {
    in::update(rs, back.frames[i].q);
    in::touch_update(touch, rs, rtf);
    uint64_t h = 0xcbf29ce484222325ull;
    for (Game& g : games) {
      in::evaluate(g.map, rs, dt, g.v, rfrom);
      h = hash_values(h, g.v);
    }
    differ += h != live[i];
  }
  lab.metric("replay.frames", static_cast<double>(back.frames.size()), "count");
  lab.metric("replay.frames-differing", differ, "count");
  if (differ != 0 || back.frames.size() != live.size()) {
    lab.fail("the replayed event log gave different action values on " + std::to_string(differ) +
                 " frames",
             "Diff the attached session.events against a fresh write, and run tests/input_sim_test "
             "(skill: input).");
  }

  // THE PLAYERS: two pads join, one is pulled out and plugged back in.
  in::SimInput ps;
  in::InputState pst;
  in::Players players;
  players.most = 2;
  gl::LabTable joins{"two players on two pads", {"frame", "what", "events"}, {}};
  const auto step = [&](const char* what) {
    in::sim_frame(ps, pst);
    in::players_update(players, pst);
    std::string ev;
    for (int i = 0; i < players.events_n; i++) {
      const auto& e = players.events[i];
      const char* k = e.kind == in::PlayerEvent::joined ? "joined"
                      : e.kind == in::PlayerEvent::lost ? "lost"
                                                        : "returned";
      ev += (ev.empty() ? "" : "; ") + std::string("player ") + std::to_string(e.player + 1) + " " +
            k;
    }
    joins.rows.push_back({std::to_string(joins.rows.size()), what, ev.empty() ? "-" : ev});
  };
  const auto pa = in::sim_plug(ps, in::sim_pad(in::Family::xbox));
  const auto pb = in::sim_plug(ps, in::sim_pad(in::Family::nintendo));
  step("two pads plugged in");
  in::sim_button(ps, pb, 0, true);
  step("the second pad presses south");
  in::sim_button(ps, pa, 6, true);
  step("the first pad presses start");
  in::sim_unplug(ps, pb);
  step("the second pad's cable comes out");
  in::sim_plug(ps, in::sim_pad(in::Family::nintendo));
  step("a pad of its model goes back in");
  lab.table(joins);
  lab.metric("players.joined", players.p[0].joined + players.p[1].joined, "count");

  // REBINDING: JUMP captured off the pad and its conflicts named.
  in::Capture cap;
  in::Source src;
  std::string device;
  in::begin_capture(cap, s, in::ActionKind::button);
  in::sim_button(sim, pad, static_cast<int>(in::PadButton::east), true);
  in::sim_frame(sim, s);
  const auto result = in::poll_capture(cap, s, src, device);
  const auto clash = in::conflicts(games[1].map, "play", src);
  lab.table({"rebinding the shooter's JUMP",
             {"pressed", "captured", "already on"},
             {{"pad east", result == in::CaptureResult::captured ? in::source_text(src) : "-",
               clash.empty() ? "-" : clash[0]}}});
  if (result != in::CaptureResult::captured || clash.empty() || clash[0] != "crouch") {
    lab.fail("the rebinding screen did not capture pad:east as on CROUCH",
             "Run tests/input_actions_test and read engine/input/rebind.cpp (skill: input).");
  }
  gyro_bias(lab);
}

const gl::LabSpec kSpec{
    "input-session",
    gl::LabKind::scenario,
    "engine",
    "engine/input/",
    "input",
    "Do keys, a mouse, a pad, a wheel and a touch screen drive a skier's, a shooter's and a "
    "racer's controls, does the recorded session replay to the bit, and does a gyro's bias "
    "settle while it is still?",
    {
        {"fps", "60", "frames a second"},
        {"seconds", "3.2", "the session's length"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
