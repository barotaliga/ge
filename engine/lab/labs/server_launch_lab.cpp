// LAB `server-launch`: a game starting its own dedicated server and finding it again
// (engine/server/spawn.hpp), on the sample game's real binary (`gesample`) over this machine's
// loopback. The first player starts the server with a cfg, joins its session, sees the seats
// fill, and leaves; the server keeps running. A second program, knowing only the game's state
// folder, finds the record, pings the server and reads back the cfg's name and the seats, switches
// its mods by a console line (`mod arena`: the layers read again, the match restarted under the
// mod's settings, the cfg handed over still winning), then stops it; the server's port answers
// nothing after, and the record is gone.
//
// Every step's outcome is in the digest; how long each took is measured (the wall clock).
//
// ge:summary Lab: a game starts its dedicated server, joins, leaves, a second program finds it,
// ge:summary switches its mods and stops it
// ge:keywords lab server-launch spawn dedicated-server find-again stop gesample loopback join
// ge:keywords mod-switch hosted_command
// ge:skill net labs
// ge:test tests/server_spawn_test.cpp
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "net/node.hpp"
#include "net/session.hpp"
#include "net/transport.hpp"
#include "server/process.hpp"
#include "server/sample.hpp"
#include "server/spawn.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace fs = std::filesystem;
namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace gu = ge::ui;
using Clock = std::chrono::steady_clock;

double since_ms(Clock::time_point t) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

std::string tail(const std::string& path) {
  std::ifstream f(path);
  std::string all, l;
  while (std::getline(f, l)) all += l + "\n";
  return all.size() > 600 ? all.substr(all.size() - 600) : all;
}

// The steps as a picture: the server's life as a bar down the left, each step a row in its
// player's lane, its outcome beside it, a step that went as it should in green.
gl::Picture steps_picture(const gl::LabTable& steps, const std::vector<bool>& good) {
  const int row_h = 52, top = 120;
  const int kW = 1500, kH = top + row_h * static_cast<int>(steps.rows.size()) + 40;
  gu::Image img = gu::make_image(kW, kH, {0.07f, 0.09f, 0.13f, 1});
  gu::DrawList dl;
  dl.begin(kW, kH);
  const gu::Font& bold = gu::ui_font(gu::Weight::semibold);
  const gu::Font& body = gu::ui_font(gu::Weight::medium);
  const gu::Color ink{0.93f, 0.95f, 0.98f, 1}, dim{0.50f, 0.57f, 0.67f, 1};
  const gu::Color first{0.36f, 0.70f, 1.0f, 1}, second{0.98f, 0.70f, 0.30f, 1};
  const gu::Color ok{0.40f, 0.85f, 0.50f, 1}, bad{1.0f, 0.40f, 0.40f, 1};
  dl.text(bold, 30, 24, 46, "A SERVER STARTED, FOUND AGAIN, ITS MODS SWITCHED, STOPPED", ink);
  dl.text(body, 18, 24, 76,
          "blue: the first player (starts, joins, leaves); orange: a second program with only "
          "the game's state folder",
          dim);
  dl.text(bold, 15, 24, top - 14, "SERVER", dim);
  // The server's life: from the start's row to the stop's.
  size_t last_alive = 0;
  for (size_t r = 0; r < steps.rows.size(); r++)
    if (steps.rows[r][0].rfind("stop", 0) == 0) last_alive = r;
  dl.rect({40, static_cast<float>(top + 10), 14,
           static_cast<float>(row_h * static_cast<int>(last_alive + 1) - 20)},
          {0.25f, 0.55f, 0.35f, 1}, 4);
  for (size_t r = 0; r < steps.rows.size(); r++) {
    const float y = static_cast<float>(top) + static_cast<float>(r) * row_h;
    const bool is_first = steps.rows[r][1] == "first player";
    const float x = is_first ? 90 : 160;
    dl.rect({x, y + 8, 10, static_cast<float>(row_h - 16)}, is_first ? first : second, 3);
    dl.text(bold, 18, x + 24, y + 32, steps.rows[r][0], ink);
    dl.text(body, 18, 760, y + 32, steps.rows[r][2], good[r] ? ok : bad);
  }
  gu::render(dl, img);
  gl::Picture pic;
  pic.width = img.width;
  pic.height = img.height;
  pic.rgba = std::move(img.rgba);
  return pic;
}

// The first player: joins the session at this machine's `port`, waits for a seat, then leaves.
// The seat's state at the end ("seated" when it got one).
std::string join_and_leave(int port, uint32_t protocol, int wait_ms) {
  gn::Transport t;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), nullptr)) return "no socket";
  gn::Node node;
  gn::NodeConfig nc;
  nc.protocol = protocol;
  nc.channels.count = 1;
  nc.channels.delivery[0] = gn::Delivery::reliable;
  nc.seed = 77;
  gn::node_init(node, nc, t);
  gn::SessionClient client;
  gn::session_client_init(client, node, 0);
  const Clock::time_point start = Clock::now();
  const auto now_us = [&] { return static_cast<int64_t>(since_ms(start) * 1000); };
  gn::session_client_join(client, gn::loopback_v4(static_cast<uint16_t>(port)), "first player", 0,
                          now_us());
  while (since_ms(start) < wait_ms && client.state != gn::ClientState::seated) {
    gn::node_update(node, now_us());
    gn::session_client_update(client, now_us());
    gn::node_flush(node, now_us());
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  const std::string state = gn::client_state_name(client.state);
  gn::session_client_leave(client, now_us());
  gn::node_flush(node, now_us());
  gn::close_transport(t);
  return state;
}

void run(gl::Lab& lab) {
  const std::string program = gs::program_path(gs::self_dir(nullptr), "gesample");
  if (!fs::exists(program)) {
    lab.fail("no sample server beside the labs: " + program,
             "Run make build, which makes gesample beside the labs; skill: net.");
    return;
  }
  const int port = static_cast<int>(lab.arg_num("port"));
  const uint32_t protocol = gs::sample_game().protocol;
  const fs::path state = fs::temp_directory_path() / ("ge_server_launch_" + std::to_string(port));
  std::error_code ec;
  fs::remove_all(state, ec);
  gl::LabTable steps{
      "the first player starts and joins; a second program finds, switches and stops",
      {"step", "who", "outcome"},
      {}};
  gl::LabTable times{"how long each step took on this machine", {"step", "ms"}, {}};
  auto step = [&](const std::string& what, const std::string& who, const std::string& outcome,
                  double ms) {
    steps.rows.push_back({what, who, outcome});
    times.rows.push_back({what, ge::core::num::to_fixed(ms, 0)});
  };

  // 1. Started with a cfg handed over.
  gs::SpawnAsk ask;
  ask.program = program;
  ask.root = state.string();
  ask.state = state.string();
  ask.port = port;
  ask.cfg = "sv_hostname \"" + lab.arg("name") + "\"\n";
  // The game's folders: the base game's settings, and a mod's over them.
  fs::create_directories(state / "base");
  fs::create_directories(state / "arena");
  std::ofstream(state / "base" / "server.cfg") << "sv_maxclients 6\nsv_bots 2\n";
  std::ofstream(state / "arena" / "server.cfg")
      << "sv_maxclients 4\nsv_bots 3\nsv_hostname \"Arena Night\"\n";
  gs::Hosted h;
  std::string error;
  Clock::time_point t = Clock::now();
  const bool started = gs::spawn_server(ask, h, error);
  step("start the dedicated server, a cfg handed over", "first player",
       started ? "started, recorded as hosted-" + std::to_string(port) + ".txt" : error,
       since_ms(t));
  if (!started) {
    lab.fail("the server did not start: " + error,
             "Read spawn_server in engine/server/spawn.cpp; skill: net.");
    lab.table(std::move(steps));
    return;
  }
  t = Clock::now();
  gn::Advert first;
  const bool answered = gs::server_answers(port, protocol, 20000, &first);
  step("ping it until it answers", "first player",
       answered ? std::string("answers as \"") + first.name + "\"" : "no answer", since_ms(t));
  if (!answered)
    lab.fail("the started server never answered a ping; its log:\n" + tail(h.log),
             "Read run in engine/server/main.cpp; skill: net.");

  // 2. Joined, then left; the server runs on.
  t = Clock::now();
  const std::string seat = answered ? join_and_leave(port, protocol, 10000) : "not tried";
  step("join its session", "first player", seat == "seated" ? "seated" : "not seated: " + seat,
       since_ms(t));
  if (seat != "seated")
    lab.fail("the first player got no seat (" + seat + ")",
             "Read session_host in engine/net/session.cpp; skill: net.");
  t = Clock::now();
  const bool still = gs::hosted_alive(h);
  step("leave; the server is left running", "first player",
       still ? "the server runs on" : "the server stopped", since_ms(t));

  // 3. A second program, with only the state folder.
  t = Clock::now();
  const std::vector<gs::Hosted> found = gs::hosted_list(state.string());
  const bool one = found.size() == 1 && found[0].pid == h.pid && gs::hosted_alive(found[0]);
  step("read the game's state folder", "second program",
       one ? "one record, its process running" : std::to_string(found.size()) + " records",
       since_ms(t));
  gn::Advert again;
  t = Clock::now();
  const bool pinged = one && gs::server_answers(found[0].port, protocol, 5000, &again);
  step("ping the recorded port", "second program",
       pinged ? std::string("\"") + again.name + "\", " + std::to_string(again.seated) + " of " +
                    std::to_string(again.max_players) + " seats"
              : "no answer",
       since_ms(t));
  if (!pinged || std::string(again.name) != lab.arg("name"))
    lab.fail("the second program did not find the server by its record and a ping",
             "Read server_answers in engine/server/spawn.cpp; skill: net.");
  t = Clock::now();
  gn::Advert after;
  bool switched = pinged && gs::hosted_command(state.string(), port, "mod arena", error);
  if (switched) {
    switched = false;
    for (int i = 0; i < 40 && !switched; i++)
      switched = gs::server_answers(port, protocol, 500, &after) && after.max_players == 4;
  }
  step("switch its mods: mod arena", "second program",
       switched
           ? std::string("\"") + after.name + "\", " + std::to_string(after.seated) + " of " +
                 std::to_string(after.max_players) + " seats: arena's, the cfg's name kept"
           : "not switched: " + (error.empty() ? std::string("the seats never changed") : error),
       since_ms(t));
  if (!switched || std::string(after.name) != lab.arg("name"))
    lab.fail(
        "the mod switch did not restart the match under arena's settings; its log:\n" + tail(h.log),
        "Read server_switch_mods in engine/server/main.cpp; skill: net.");
  t = Clock::now();
  const bool stopped = one && gs::stop_hosted(state.string(), found[0], error);
  step("stop it", "second program", stopped ? "stopped, record dropped" : "not stopped: " + error,
       since_ms(t));
  t = Clock::now();
  const bool silent = !gs::server_answers(port, protocol, 800);
  const bool gone = gs::hosted_list(state.string()).empty() && !gs::process_alive(h.pid);
  step("ping the port again", "second program",
       silent && gone ? "no answer, no record" : "still there", since_ms(t));
  if (!stopped || !silent || !gone) {
    lab.fail("the server did not stop cleanly: " + error,
             "Read stop_hosted in engine/server/spawn.cpp; skill: net.");
    if (gs::process_alive(h.pid)) gs::process_stop(h.pid, 2000, error);
  }
  lab.metric("started", started ? 1 : 0, "count");
  lab.metric("seated", seat == "seated" ? 1 : 0, "count");
  lab.metric("found_again", pinged ? 1 : 0, "count");
  lab.metric("stopped", stopped && silent && gone ? 1 : 0, "count");
  lab.metric("seats_seen_by_the_second", pinged ? again.seated : -1, "players");
  lab.metric("switched", switched ? 1 : 0, "count");
  lab.metric("slots_after_the_switch", switched ? after.max_players : -1, "players");
  const std::vector<bool> good = {started, answered, seat == "seated", still,         one,
                                  pinged,  switched, stopped,          silent && gone};
  lab.shot({"steps",
            "A SERVER STARTED, FOUND AGAIN, ITS MODS SWITCHED, STOPPED",
            "The sample game's dedicated server (gesample) started by the first player with a cfg "
            "naming it, joined and left; then a second program reads the game's state folder, "
            "pings the recorded port, reads the name and seats back, switches its mods to arena "
            "by a console line, sees arena's seats under the cfg's name, and stops it.",
            "every outcome green; the server's bar runs from the start to the stop; 2 of 6 seats "
            "before the switch, 3 of 4 after it, the cfg's name on both.",
            {{"port", lab.arg("port")}, {"name", lab.arg("name")}}},
           steps_picture(steps, good));
  lab.table(std::move(steps));
  lab.table(std::move(times), gl::Det::measured);
  fs::remove_all(state, ec);
}

const gl::LabSpec kSpec{
    "server-launch",
    gl::LabKind::scenario,
    "engine",
    "engine/server/spawn (a game's dedicated server started, found again, its mods switched, "
    "stopped)",
    "net",
    "Can a game start its dedicated server with a cfg, join it and leave it running, and can a "
    "second program find that server from the game's state folder alone, switch its mods and "
    "stop it?",
    {
        {"port", "27652", "the loopback port the server is started on"},
        {"name", "Launch Night", "the server's name, handed over in its cfg"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
