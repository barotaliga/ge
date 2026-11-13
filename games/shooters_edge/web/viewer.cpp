// SHOOTER'S EDGE'S WEB VIEWER (docs/hub.md § Watching in a browser): the game's own world drawing,
// compiled to WebAssembly, on the hub page's canvas through the browser's WebGPU. The page hands
// it its arguments (web/viewer_args.hpp): the content packs it fetches and mounts in order, and the
// demo it fetches by hash and plays at its own pace, drawn between frames, with the effects the
// window draws. A demo is watched like a video with the window's own controls (demo_playback.hpp):
// the play bar with its highlights over the picture, and on the canvas's keys Space or K pause,
// Left and Right skip, Up and Down change the speed, Page Up and Page Down jump between highlights.
// The page's status line says what is loading, or why there is no picture.
//
// A LIVE BROADCAST (`--tv=ws://...`) is read off the TV node's WebSocket (tv/websocket.hpp): the
// viewer says hello as a viewer, the node's messages are turned back into the replay file as it
// grows (tv/node.hpp's `Watch`), and its whole blocks are unpacked into the recording as they come
// (`unpack_more`). Play starts two blocks in, so a block's second of travel is already in hand;
// when the recording runs out before the next block, the picture holds its last frame.
//
// ge:summary Shooter's Edge's web viewer: packs and a demo fetched, the world drawn on the canvas
// ge:keywords web viewer browser wasm webgpu canvas demo playdemo hub watch shooters_edge_viewer
// ge:keywords live broadcast tv websocket transport seek pause speed play-bar
// ge:skill net shooter
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "camera/first_person.hpp"
#include "gpu/device.hpp"
#include "gpu/surface.hpp"
#include "platform/window.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/demo_fx.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "tv/node.hpp"
#include "ui/draw.hpp"
#include "vfs/unpack.hpp"
#include "web/browser.hpp"
#include "web/viewer_args.hpp"

namespace cl = shooters_edge::client;

namespace {

constexpr const char* kCanvas = "#view";
// Blocks in hand before a live broadcast starts playing (each one second of the match).
constexpr int64_t kLiveLead = 2;
// Paints a WebSocket has to open in (about ten seconds).
constexpr int kOpenPaints = 600;

// The page's line in a player's words; the detail goes to the browser's console only.
int fail(const std::string& line, const std::string& detail = "") {
  if (!detail.empty()) std::fprintf(stderr, "shooters_edge_viewer: %s\n", detail.c_str());
  ge::web::say(line);
  return 1;
}

// The packs fetched and mounted in order, a later one searched first (as the game's folder).
bool mount_packs(const ge::web::ViewerArgs& a, cl::SearchPath& files, std::string& error) {
  for (size_t i = 0; i < a.packs.size(); i++) {
    const std::string said = a.packs.size() == 1
                                 ? std::string("Loading the game's files...")
                                 : "Loading the game's files (" + std::to_string(i + 1) + " of " +
                                       std::to_string(a.packs.size()) + ")...";
    ge::web::say(said);
    std::vector<uint8_t> bytes;
    if (!ge::web::fetch_bytes(a.packs[i], bytes, error, said)) return false;
    const std::string& url = a.packs[i];
    const std::string name = url.substr(url.rfind('/') + 1);
    if (ge::vfs::wrapping_of(bytes) != ge::vfs::Wrapping::none) {  // a demo's zstd content pack
      ge::vfs::Unpacked u;
      if (!ge::vfs::unpack_one(std::move(bytes), u, error)) return false;
      bytes = std::move(u.bytes);
    }
    if (!cl::add_archive(files, name, std::move(bytes), error)) return false;
  }
  return true;
}

// A live broadcast's end of the page: the node's socket, the stream as it grows, the recording.
struct Live {
  int socket = -1;
  ge::tv::Watch watch;
  cl::DemoUnpacker unpacker;
  std::vector<uint8_t> opening;  // the recording before it opens (`kLiveLead` blocks)
  bool open = false;             // the recording is `t.demo` and grows there
};

// What the node sent since the last paint taken in, and the blocks it completed unpacked. False
// with the page's line in `line` and the detail in `error` when the broadcast cannot go on.
bool pump(Live& live, cl::Timedemo& t, const cl::GameAdapter& game, std::string& line,
          std::string& error) {
  std::vector<uint8_t> msg;
  while (ge::web::ws_take(live.socket, msg)) {
    if (!ge::tv::watch_receive(live.watch, msg)) {
      line = "The broadcast sent something this viewer cannot read.";
      error = live.watch.error;
      return false;
    }
  }
  if (!live.watch.refused.empty()) {
    line = "The broadcast turned this viewer away: " + live.watch.refused;
    return false;
  }
  if (!live.watch.have_header) return true;
  std::vector<uint8_t>& into = live.open ? t.demo.bytes : live.opening;
  std::string cut;
  if (!cl::unpack_more(live.unpacker, live.watch.stream, into, cut)) {
    line = "This broadcast cannot be shown here. Open it in the game below.";
    error = cut;
    return false;
  }
  if (!live.open && live.unpacker.blocks >= kLiveLead && !live.opening.empty()) {
    if (!game.open_demo(game, std::move(live.opening), t.demo, error)) {
      line = "This broadcast cannot be shown here. Open it in the game below.";
      return false;
    }
    t.start = t.demo;
    live.open = true;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string_view> words(argv + 1, argv + argc);
  ge::web::ViewerArgs args;
  std::string error;
  if (!ge::web::parse_viewer_args(words, args, error))
    return fail("The page gave the viewer words it does not know.", error);

  cl::SearchPath files;
  if (!mount_packs(args, files, error))
    return fail("The game's files did not load. Try again in a moment.", error);
  cl::Timedemo t;
  const cl::GameAdapter& game = cl::classic_adapter();
  const bool live = !args.tv.empty();
  Live broadcast;
  if (live) {
    ge::web::say("Joining the broadcast...");
    t.name = t.found = args.tv;
    broadcast.socket = ge::web::ws_open(args.tv);
    for (int i = 0; broadcast.socket >= 0 &&
                    ge::web::ws_state(broadcast.socket) == ge::web::WsState::connecting;
         i++) {
      if (i == kOpenPaints) break;
      ge::web::next_frame();
    }
    if (broadcast.socket < 0 || ge::web::ws_state(broadcast.socket) != ge::web::WsState::open)
      return fail("The broadcast did not answer. Open it in the game below.",
                  "no WebSocket to " + args.tv);
    ge::web::ws_send(broadcast.socket, ge::tv::tv_hello("web viewer", false));
  } else {
    ge::web::say("Loading the demo...");
    std::vector<uint8_t> demo_bytes;
    if (!ge::web::fetch_bytes(args.demo, demo_bytes, error, "Loading the demo..."))
      return fail("The demo did not load. Try again in a moment.", error);
    t.name = t.found = args.demo;
    if (!game.open_demo(game, std::move(demo_bytes), t.demo, error))
      return fail("This demo cannot be played here. Download it and play it in the game.", error);
    t.start = t.demo;
  }
  t.active = t.paced = t.smooth = true;
  cl::DemoPlayback playback;
  playback.speed = args.speed;
  if (!live && !cl::playback_begin(playback, t, error))
    std::fprintf(stderr, "shooters_edge_viewer: no highlights: %s\n", error.c_str());

  ge::web::say("Starting the picture...");
  std::unique_ptr<ge::gpu::Device> device = ge::gpu::create_device();
  if (!device) return fail("This browser cannot draw the game. Play it in the game below.");
  ge::platform::NativeWindow canvas;
  canvas.kind = ge::platform::NativeWindow::Kind::canvas;
  canvas.canvas = kCanvas;
  ge::gpu::Surface surface;
  if (!surface.create(*device, canvas))
    return fail("This browser cannot draw the game. Play it in the game below.");
  uint32_t w = 0, h = 0;
  ge::web::canvas_size(kCanvas, w, h);
  if (w == 0 || h == 0) return fail("The picture has no room on this page.");
  surface.configure(*device, w, h);

  const shooters_edge::look::Picture look;
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
  std::unique_ptr<cl::LoadedMap> map;
  cl::WorldDrawPtr world;
  size_t world_models = 0;
  cl::DemoFx demo_fx;
  cl::FxFrame fx;
  cl::FxScene scene;

  std::string waiting = "Joining the broadcast...";  // the status line while a broadcast starts
  double last_ms = ge::web::next_frame();
  for (uint64_t frame = 0;; frame++) {
    const double now_ms = ge::web::next_frame();
    const double dt = std::min((now_ms - last_ms) / 1000.0, cl::kMostPlayStep);
    last_ms = now_ms;
    if (live) {
      std::string line;
      if (!pump(broadcast, t, game, line, error)) return fail(line, error);
      if (!broadcast.open) {
        if (ge::web::ws_state(broadcast.socket) == ge::web::WsState::closed)
          return fail("The broadcast stopped before the match was shown.");
        const char* line =
            broadcast.watch.have_header ? "Waiting for the match..." : "Joining the broadcast...";
        if (waiting != line) ge::web::say(waiting = line);
        continue;
      }
    }
    ge::web::PageInput page;
    ge::web::take_input(kCanvas, page);
    std::array<bool, 512> keys{};  // went down this paint, by scancode
    for (const int k : page.pressed)
      if (k >= 0 && k < static_cast<int>(keys.size())) keys[static_cast<size_t>(k)] = true;
    std::vector<std::string> lines;
    const cl::PlaybackInput in{keys, page.x, page.y, page.button};
    const cl::TimedemoStep st =
        live ? cl::playdemo_step(t, dt, 1.0, error)
             : cl::playback_step(playback, t, in, static_cast<int>(w), static_cast<int>(h), dt,
                                 now_ms / 1000.0, lines, error);
    for (const std::string& l : lines) std::fprintf(stderr, "%s\n", l.c_str());
    if (st == cl::TimedemoStep::error)
      return fail(live ? "The broadcast stopped part way. Open it in the game below."
                       : "The demo stopped part way. Download it and play it in the game.",
                  error);
    if (st == cl::TimedemoStep::end) {
      if (!live) {
        ge::web::say("The demo is over.");
        return 0;
      }
      if (broadcast.watch.ended ||
          ge::web::ws_state(broadcast.socket) == ge::web::WsState::closed) {
        ge::web::say("The broadcast is over.");
        ge::web::ws_close(broadcast.socket);
        return 0;
      }
      t.clock = t.demo.state.time;  // ran out before the next block: hold the last frame
    }
    const cl::DemoState& read = t.demo.state;
    if (st == cl::TimedemoStep::new_level) {
      ge::web::say("Loading " + read.map + "...");
      map = cl::load_named_map(files, read.map, error);
      if (!map)
        return fail("This hub does not have the map " + read.map + ", so the demo cannot be shown.",
                    error);
      world.reset();
      demo_fx = {};
      std::vector<uint8_t> palette = std::move(fx_state.palette);
      fx_state = {};
      fx_state.on = cl::fx_switches(look);
      fx_state.palette = std::move(palette);
      fx_state.book = &book;
    }
    if (!map) continue;
    cl::demo_fx_frame(read, demo_fx);
    const cl::DemoState& s = cl::demo_drawn(t);
    if (!world || s.models.size() != world_models) {
      world = cl::create_world_draw(*device, files, *map, w, h, surface.format(), look, {}, error,
                                    {s.models, {}});
      if (!world)
        return fail(
            "This browser's graphics cannot draw this game's world yet. Play it in the game "
            "below.",
            error);
      world_models = s.models.size();
      ge::web::say("");
    }
    ge::render::FrameView view = cl::demo_view(s, ge::camera::FirstPersonRow{}.fov_y, t.clock);
    view.frame = frame;
    fx_state.models = &cl::world_models(*world);
    cl::demo_fx_scene(s, demo_fx, map.get(), view, scene);
    cl::step_fx(scene, fx_state, t.clock, dt, fx);
    const wgpu::TextureView target = surface.acquire(*device);
    if (!target) continue;
    ge::ui::DrawList overlay;
    overlay.begin(static_cast<int>(w), static_cast<int>(h));
    if (!live)
      cl::playback_draw(overlay, playback, t, static_cast<int>(w), static_cast<int>(h),
                        now_ms / 1000.0);
    cl::draw_world(*device, *world, view, fx, nullptr, dt, overlay, target);
    surface.present();
  }
}
