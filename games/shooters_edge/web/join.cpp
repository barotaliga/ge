// The web player's join mode (join.hpp): the page's WebTransport session as the client's transport,
// the canvas's keys and mouse its hands, each frame the server sent drawn as a joined game's.
//
// ge:summary The web player's join mode defined: the session, the client over it, the frame
// ge:keywords web join webtransport wt_open wt_pipe open_pipe_transport remote_start remote_step
// ge:skill net shooter
#include "shooters_edge/web/join.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "camera/first_person.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "input/state.hpp"
#include "net/transport.hpp"
#include "platform/window.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/demo_fx.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_remote.hpp"
#include "ui/draw.hpp"
#include "web/browser.hpp"
#include "web/file_list.hpp"

namespace shooters_edge::web {

namespace {

namespace cl = client;
namespace gn = ge::net;

// Paints the session has to open in (about ten seconds).
constexpr int kOpenPaints = 600;
// The page's mouse moves in the browser's pixels, about two of a desktop mouse's raw counts.
constexpr double kPageSensitivity = 0.12;  // degrees a pixel
constexpr uint16_t kRight = 79, kLeft = 80, kDown = 81, kUp = 82;

int fail(const std::string& line, const std::string& detail = "") {
  if (!detail.empty()) std::fprintf(stderr, "shooters_edge_viewer: %s\n", detail.c_str());
  ge::web::say(line);
  return 1;
}

// The files of a map the page does not hold, asked of the hub by the map's name and mounted over
// the rest; only those the browser lacks are fetched.
bool fetch_map(const std::string& map, cl::SearchPath& files, ge::web::FileStore& store,
               std::string& error) {
  const std::string said = "Loading " + map + "...";
  std::vector<uint8_t> pak;
  size_t n = 0;
  if (!ge::web::fetch_listed(ge::web::map_list_url(map), store, pak, n, error, said)) return false;
  std::fprintf(stderr, "shooters_edge_viewer: %s: %zu files fetched\n", map.c_str(), n);
  return pak.empty() || cl::add_archive(files, "map-" + map + ".pak", std::move(pak), error);
}

}  // namespace

int join_in_page(const ge::web::ViewerArgs& args, cl::SearchPath& files, ge::web::FileStore& kept,
                 const char* canvas_id) {
  std::string error;
  ge::web::say("Joining the server...");
  const int session = ge::web::wt_open(args.join, args.cert);
  if (session < 0)
    return fail("This browser cannot join a server from the page. Play it in the game below.",
                "no WebTransport session to " + args.join);
  for (int i = 0; i < kOpenPaints && ge::web::wt_state(session) == ge::web::WtState::connecting;
       i++)
    ge::web::next_frame();
  if (ge::web::wt_state(session) != ge::web::WtState::open) {
    const std::string why = ge::web::wt_why(session);
    ge::web::wt_close(session);
    return fail("The server did not answer. Try again in a moment, or join it in the game below.",
                "the session to " + args.join + " did not open: " + why);
  }

  // The server's address is only the pipe's label: every datagram goes into the session, and the
  // forwarder behind it knows where the server is.
  gn::Address server;
  if (!gn::parse_address(args.join_to, server)) server = gn::ipv4(0, 0, 0, 1, 27500);
  gn::Transport transport;
  if (!gn::open_pipe_transport(transport, ge::web::wt_pipe(session, server)))
    return fail("The game did not start here. Play it in the game below.", "no pipe transport");
  net::Remote remote;
  remote.read_file = [&files](const std::string&, const std::string& path) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> got = cl::find_file(files, path, store);
    return std::vector<uint8_t>(got.begin(), got.end());
  };
  net::ClientConfig cfg;
  cfg.name = "web player";
  cfg.voice = false;
  net::remote_start(remote, transport, server, cfg);

  ge::web::say("Starting the picture...");
  std::unique_ptr<ge::gpu::Device> device = ge::gpu::create_device();
  if (!device) return fail("This browser cannot draw the game. Play it in the game below.");
  ge::platform::NativeWindow canvas;
  canvas.kind = ge::platform::NativeWindow::Kind::canvas;
  canvas.canvas = canvas_id;
  ge::gpu::Surface surface;
  if (!surface.create(*device, canvas))
    return fail("This browser cannot draw the game. Play it in the game below.");
  uint32_t w = 0, h = 0;
  ge::web::canvas_size(canvas_id, w, h);
  if (w == 0 || h == 0) return fail("The picture has no room on this page.");
  surface.configure(*device, w, h);

  const cl::GameAdapter& game = cl::classic_adapter();
  const look::Picture look;
  cl::FxState fx_state;
  fx_state.on = cl::fx_switches(look);
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = cl::find_file(files, "gfx/palette.lmp", store);
    fx_state.palette.assign(pal.begin(), pal.end());
  }
  ge::particles::Book book;
  {
    std::vector<std::string> faults;
    cl::load_effects(files, book, faults);
  }
  fx_state.book = &book;
  cl::ControlSettings controls;
  controls.sensitivity = kPageSensitivity;
  ge::input::InputState in;
  std::unique_ptr<cl::LoadedMap> map;
  cl::WorldDrawPtr world;
  size_t world_models = 0;
  cl::DemoFx demo_fx;
  cl::FxFrame fx;
  cl::FxScene scene;
  std::string status;  // the status line's last words
  const auto say = [&status](const std::string& line) {
    if (line != status) ge::web::say(status = line);
  };

  double t = 0;
  double last_ms = ge::web::next_frame();
  for (uint64_t frame = 0;; frame++) {
    const double now_ms = ge::web::next_frame();
    const double dt = std::min((now_ms - last_ms) / 1000.0, cl::kMostPlayStep);
    last_ms = now_ms;
    t += dt;
    if (ge::web::wt_state(session) == ge::web::WtState::closed) {
      const std::string why = ge::web::wt_why(session);
      net::remote_disconnect(remote);
      return fail("The connection to the server was lost. Reload the page to join again.", why);
    }

    const bool locked = ge::web::take_play_input(canvas_id, in);
    if (!locked) in.mouse.dx = in.mouse.dy = 0;
    cl::Moves moves = cl::key_moves(in);
    moves.left = ge::input::key_held(in, kLeft);
    moves.right = ge::input::key_held(in, kRight);
    moves.forward = moves.forward || ge::input::key_held(in, kUp);
    moves.back = moves.back || ge::input::key_held(in, kDown);
    net::remote_hands(remote, moves, in, controls, dt);
    std::vector<std::string> lines;
    const net::RemoteStep step = net::remote_step(remote, dt, lines);
    for (const std::string& line : lines) std::fprintf(stderr, "%s\n", line.c_str());
    if (!remote.active) {
      ge::web::wt_close(session);
      return fail("The server let you go. Reload the page to join again.");
    }

    const cl::DemoState& s = remote.demo.state;
    if (step == net::RemoteStep::new_level) {
      say("Loading " + s.map + "...");
      std::vector<uint8_t> held;
      if (cl::find_file(files, "maps/" + s.map + ".bsp", held).empty() &&
          !fetch_map(s.map, files, kept, error))
        return fail("This hub does not have the map " + s.map + ", so the game cannot be shown.",
                    error);
      map = cl::load_named_map(files, s.map, error);
      if (!map)
        return fail("This hub does not have the map " + s.map + ", so the game cannot be shown.",
                    error);
      world.reset();
      demo_fx = {};
      std::vector<uint8_t> palette = std::move(fx_state.palette);
      fx_state = {};
      fx_state.on = cl::fx_switches(look);
      fx_state.palette = std::move(palette);
      fx_state.book = &book;
      net::remote_world(remote, &map->spec.models);
    }
    if (!map) {
      say("Joining the server...");
      continue;
    }
    if (!world || s.models.size() != world_models) {
      world = cl::create_world_draw(*device, files, *map, w, h, surface.format(), look, {}, error,
                                    {s.models, {}}, game);
      if (!world)
        return fail(
            "This browser's graphics cannot draw this game's world yet. Play it in the game "
            "below.",
            error);
      world_models = s.models.size();
    }
    say(locked ? "" : "Click the picture to play. Escape lets the mouse go.");
    if (step != net::RemoteStep::waiting) cl::demo_fx_frame(s, demo_fx);
    ge::render::FrameView view = cl::demo_view(s, ge::camera::FirstPersonRow{}.fov_y, t);
    view.frame = frame;
    fx_state.models = &cl::world_models(*world);
    cl::demo_fx_scene(s, demo_fx, map.get(), view, scene);
    cl::step_fx(scene, fx_state, t, dt, fx);
    const wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    ge::ui::DrawList overlay;
    overlay.begin(static_cast<int>(w), static_cast<int>(h));
    cl::draw_world(*device, *world, view, fx, nullptr, dt, overlay, target);
    surface.present();
  }
}

}  // namespace shooters_edge::web
