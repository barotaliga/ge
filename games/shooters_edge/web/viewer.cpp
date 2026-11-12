// SHOOTER'S EDGE'S WEB VIEWER (docs/hub.md § Watching in a browser): the game's own world drawing,
// compiled to WebAssembly, on the hub page's canvas through the browser's WebGPU. The page hands
// it its arguments (web/viewer_args.hpp): the content packs it fetches and mounts in order, and the
// demo it fetches by hash and plays at its own pace, drawn between frames, with the effects the
// window draws. The page's status line says what is loading, or why there is no picture.
//
// A live broadcast (`--tv`) is read off the TV node's WebSocket in a later row; until then the
// viewer says so and the page's Open in the game stands.
//
// ge:summary Shooter's Edge's web viewer: packs and a demo fetched, the world drawn on the canvas
// ge:keywords web viewer browser wasm webgpu canvas demo playdemo hub watch shooters_edge_viewer
// ge:skill net shooter
#include <algorithm>
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
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "ui/draw.hpp"
#include "web/browser.hpp"
#include "web/viewer_args.hpp"

namespace cl = shooters_edge::client;

namespace {

constexpr const char* kCanvas = "#view";

// The page's line in a player's words; the detail goes to the browser's console only.
int fail(const std::string& line, const std::string& detail = "") {
  if (!detail.empty()) std::fprintf(stderr, "shooters_edge_viewer: %s\n", detail.c_str());
  ge::web::say(line);
  return 1;
}

// The packs fetched and mounted in order, a later one searched first (as the game's folder).
bool mount_packs(const ge::web::ViewerArgs& a, cl::SearchPath& files, std::string& error) {
  for (size_t i = 0; i < a.packs.size(); i++) {
    ge::web::say("Loading the game's files (" + std::to_string(i + 1) + " of " +
                 std::to_string(a.packs.size()) + ")...");
    std::vector<uint8_t> bytes;
    if (!ge::web::fetch_bytes(a.packs[i], bytes, error)) return false;
    const std::string& url = a.packs[i];
    const std::string name = url.substr(url.rfind('/') + 1);
    if (!cl::add_archive(files, name, std::move(bytes), error)) return false;
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
  if (!args.tv.empty())
    return fail("Live broadcasts are not shown here yet. Open it in the game below.");

  cl::SearchPath files;
  if (!mount_packs(args, files, error))
    return fail("The game's files did not load. Try again in a moment.", error);
  ge::web::say("Loading the demo...");
  std::vector<uint8_t> demo_bytes;
  if (!ge::web::fetch_bytes(args.demo, demo_bytes, error))
    return fail("The demo did not load. Try again in a moment.", error);

  cl::Timedemo t;
  t.name = t.found = args.demo;
  const cl::GameAdapter& game = cl::classic_adapter();
  if (!game.open_demo(game, std::move(demo_bytes), t.demo, error))
    return fail("This demo cannot be played here. Download it and play it in the game.", error);
  t.start = t.demo;
  t.active = t.paced = t.smooth = true;

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

  double last_ms = ge::web::next_frame();
  for (uint64_t frame = 0;; frame++) {
    const double now_ms = ge::web::next_frame();
    const double dt = std::min((now_ms - last_ms) / 1000.0, cl::kMostPlayStep);
    last_ms = now_ms;
    const cl::TimedemoStep st = cl::playdemo_step(t, dt, args.speed, error);
    if (st == cl::TimedemoStep::error)
      return fail("The demo stopped part way. Download it and play it in the game.", error);
    if (st == cl::TimedemoStep::end) {
      ge::web::say("The demo is over.");
      return 0;
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
    cl::draw_world(*device, *world, view, fx, nullptr, dt, overlay, target);
    surface.present();
  }
}
