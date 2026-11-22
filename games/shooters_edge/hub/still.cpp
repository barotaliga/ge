// A map's still for the hub: the intermission view in the classic look, and its key (still.hpp).
//
// ge:summary A map's still drawn in the classic look; its key off the map and the code stamp
// ge:keywords hub still draw_still still_key classic-look ce_enabled source-stamp lavapipe
// ge:skill net render shooter
// ge:test tests/shooters_edge_hub_cards_test.cpp
#include "shooters_edge/hub/still.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>

#include "brush/read.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "render/view.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/hub/cards.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "stamps/still_code_stamp.hpp"

namespace shooters_edge::hub {

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;

namespace {

// The map's file out of the game's packs. Empty when there is none.
std::vector<uint8_t> map_file(std::string_view content, std::string_view game, std::string_view map,
                              cl::SearchPath& files, std::string& error) {
  const pf::GameProfile* g = pf::find_game(game);
  if (g == nullptr || !open_hub_content(content, game, files, error)) return {};
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes =
      cl::find_file(files, ge::classicenhance::asset_path(pf::content_of(*g).maps, map), store);
  if (bytes.empty()) error = "the packs hold no map '" + std::string(map) + "'";
  return {bytes.begin(), bytes.end()};
}

uint64_t fnv(uint64_t h, const void* data, size_t n) {
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

// One device for every still (made on first use, on the stills' worker).
std::mutex device_mu;
std::unique_ptr<ge::gpu::Device> device;

}  // namespace

std::string still_key(std::string_view content, std::string_view game, std::string_view map,
                      int width, int height) {
  cl::SearchPath files;
  std::string error;
  const std::vector<uint8_t> bytes = map_file(content, game, map, files, error);
  if (bytes.empty()) return "";
  uint64_t h = 14695981039346656037ull;
  h = fnv(h, bytes.data(), bytes.size());
  h = fnv(h, kStillCodeStamp, sizeof kStillCodeStamp);
  const int size[2] = {width, height};
  h = fnv(h, size, sizeof size);
  char out[17];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(h));
  return out;
}

bool draw_still(std::string_view content, std::string_view game, std::string_view map, int width,
                int height, std::vector<uint8_t>& rgba, std::string& error) {
  cl::SearchPath files;
  std::vector<uint8_t> bytes = map_file(content, game, map, files, error);
  if (bytes.empty()) return false;
  ge::brush::Map read;
  if (!ge::brush::read_bsp(bytes, &read, &error)) return false;
  const StillView at = still_view(read);
  auto loaded = cl::map_from_bytes(bytes, std::string(map), error);
  if (!loaded) return false;

  // The classic settings: the classic look, classic enhance off, everything else the defaults.
  shooters_edge::look::Picture picture;
  ge::console::Console con;
  shooters_edge::look::add_cvars(con, picture);
  con.execute("r_look classic; ce_enabled 0\n");
  shooters_edge::look::apply_ce_enabled(picture, con.cvars());

  std::lock_guard<std::mutex> l(device_mu);
  if (!device) device = ge::gpu::create_device({.label = "hub.still"});
  if (!device) {
    error = "no GPU adapter (install Mesa's Vulkan drivers for lavapipe)";
    return false;
  }
  const cl::GameAdapter& adapter = cl::adapter_of(*pf::find_game(game));
  const auto w = static_cast<uint32_t>(width), h = static_cast<uint32_t>(height);
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *loaded, w, h, wgpu::TextureFormat::RGBA8Unorm, picture,
                            {}, error, {}, adapter);
  if (!world) return false;
  ge::gpu::Target out = ge::gpu::create_target(*device, "hub.still", w, h);
  ge::render::FrameView view;
  const ge::camera::Vec3 eye = cl::engine_point({at.origin[0], at.origin[1], at.origin[2]});
  view.eye[0] = eye.x;
  view.eye[1] = eye.y;
  view.eye[2] = eye.z;
  view.heading = cl::engine_heading(at.yaw);
  view.pitch = cl::engine_pitch(at.pitch);
  // The classic 90 degrees across, the height what the card's shape leaves of it.
  view.fov_y = 2 * std::atan(double(h) / double(w));
  view.time_s = 1;
  const cl::FxFrame fx;
  // Two frames: the first fills the history a frame reads (the sky's tables, the cull's depth).
  for (int k = 0; k < 2; k++) cl::draw_world(*device, *world, view, fx, nullptr, 0, {}, out.view);
  const ge::gpu::Image img = ge::gpu::read_now(*device, out);
  rgba = img.rgba;
  return rgba.size() == size_t(w) * size_t(h) * 4;
}

}  // namespace shooters_edge::hub
