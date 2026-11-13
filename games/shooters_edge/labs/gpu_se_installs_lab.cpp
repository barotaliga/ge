// LAB `se-installs`: THE CLASSIC INSTALLS A PLAYER DROPS THE GAME BESIDE. A parent folder holding
// classic installs (client/install_search.hpp) searched as the window searches the folder it was placed
// in: each install found by its folder's name or by what it holds, its release told by its packs'
// sizes, the one the window would play picked; then, for each, its configs run in the install's
// own order (client/install_configs.hpp) on a console of its own, the files listed in the order
// they ran; its search path opened with the bundled client's folder in it; and its start map drawn
// from the player's start, with its place names counted (client/place_names.hpp).
//
// Without `--parent` the installs are our own: three folders written under the temporary folder
// (the first game's shareware and registered packs, the sequel's demo, by their sizes alone, and a
// folder with a game's name but no game), so the table and its digest need no game data. With
// `--parent=<folder>` they are the player's (private pictures only).
//
// ge:summary Lab: the classic installs beside the game found, their configs run, a map each drawn
// ge:keywords lab se-installs installs parent-folder release config-order nquake start-map
// ge:keywords place-names client-folder
// ge:skill shooter labs
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "classicenhance/profile.hpp"
#include "console/console.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/install_configs.hpp"
#include "shooters_edge/client/install_search.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/profiles/game_profile.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace fs = std::filesystem;
namespace pf = shooters_edge::profiles;
namespace sh = ge::shooter;

constexpr float kEyeHeight = 22;  // the eye over a start's origin, units

void sized(const fs::path& p, uintmax_t size) {
  fs::create_directories(p.parent_path());
  { std::ofstream f(p, std::ios::binary); }
  fs::resize_file(p, size);
}

// Our own parent folder: installs known by their packs' sizes, and one that is no install.
fs::path fixture_parent() {
  const fs::path root = fs::temp_directory_path() / "ge_lab_se_installs";
  fs::remove_all(root);
  const ge::classicenhance::Profile& one = ge::classicenhance::kProfiles[0];
  const ge::classicenhance::Profile& two = ge::classicenhance::kProfiles[1];
  for (const cl::KnownReleasePack& k : cl::kKnownReleasePacks) {
    const bool first = k.profile == one.id;
    const std::string folder = std::string(first ? "game-one-" : "game-two-") +
                               std::string(k.release == "full game" ? "full" : k.release);
    const fs::path base = root / folder / std::string(first ? one.base_dir : two.base_dir);
    if (k.pack != "pak0.pak") sized(base / "pak0.pak", 18689235);
    sized(base / std::string(k.pack), k.size);
  }
  fs::create_directories(root / "nquake2" / "music");
  return root;
}

// The player's start of `read`: the first single-player start, else the first deathmatch spot.
bool player_start(const ge::brush::Map& read, sh::Vec3f& at, float& yaw) {
  for (const char* kind : {"info_player_start", "info_player_deathmatch"})
    for (const ge::brush::Entity& e : read.entities)
      if (e.get("classname") == kind) {
        std::sscanf(e.get("origin").c_str(), "%f %f %f", &at.x, &at.y, &at.z);
        yaw = std::strtof(e.get("angle").c_str(), nullptr);
        return true;
      }
  return false;
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 640;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 360;
  const bool own = lab.arg("parent").empty();
  const fs::path parent = own ? fixture_parent() : fs::path(lab.arg("parent"));
  const gl::Det det = own ? gl::Det::deterministic : gl::Det::measured;
  const std::vector<cl::FoundInstall> found = cl::installs_below(parent.string());
  lab.metric("installs", double(found.size()), "count", det);
  const int pick = cl::pick_install(found, "");
  gl::LabTable t{
      "The installs found beside the game", {"#", "folder", "game", "release", "played"}, {}};
  for (size_t i = 0; i < found.size(); ++i)
    t.rows.push_back({std::to_string(i + 1), fs::path(found[i].basedir).filename().string(),
                      std::string(found[i].content->id), found[i].release,
                      int(i) == pick ? "yes" : ""});
  lab.table(t, det);
  if (own && found.size() != std::size(cl::kKnownReleasePacks)) {
    lab.fail("the fixture's installs were not all found",
             "Read installs_below in client/install_search.cpp. Skill: shooter.");
  }

  // The configs each runs, in order.
  gl::LabTable c{"The configs each install runs at the start, in order", {"install", "order"}, {}};
  for (const cl::FoundInstall& f : found) {
    const std::string game = cl::game_of(f.basedir, *f.content);
    ge::console::Console con;
    con.set_search_path(cl::config_dirs(f.basedir, game, *f.content));
    const cl::ConfigsRun ran = cl::start_configs(con, f.basedir, game, *f.content, {});
    std::string order;
    for (const std::string& file : ran.files) {
      std::string shown =
          fs::path(file).is_absolute() ? fs::relative(file, f.basedir).generic_string() : file;
      order += (order.empty() ? "" : " > ") + shown;
    }
    c.rows.push_back({fs::path(f.basedir).filename().string(), order.empty() ? "(none)" : order});
    lab.metric(fs::path(f.basedir).filename().string() + ".configs", double(ran.files.size()),
               "count", det);
  }
  lab.table(c, det);
  if (own) return;

  // Each install's start map, drawn from the player's start.
  auto device = ge::gpu::create_device({.label = "se-installs", .validation = 1});
  ge::gpu::Target out;
  if (device) out = ge::gpu::create_target(*device, "se-installs.final", w, h);
  shooters_edge::look::Picture look;
  look.look = "modern";
  for (const cl::FoundInstall& f : found) {
    const std::string folder = fs::path(f.basedir).filename().string();
    const pf::GameProfile* game = pf::find_game(f.content->id);
    const cl::GameAdapter& adapter = cl::adapter_of(game != nullptr ? *game : pf::classic_game());
    cl::SearchPath files;
    std::string error;
    if (!cl::open_game_dir(f.basedir, cl::game_of(f.basedir, *f.content), files, error,
                           *f.content)) {
      lab.fail(std::string(folder).append(": ").append(error),
               "Read the reason above. Skill: shooter.");
      continue;
    }
    lab.metric(folder + ".archives", double(cl::archive_paths(files).size()), "count", det);
    const std::string name(f.content->start_map);
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes =
        cl::find_file(files, ge::classicenhance::asset_path(f.content->maps, name), store);
    auto map = std::make_unique<cl::LoadedMap>();
    map->name = name;
    map->from_file = true;
    map->file.assign(bytes.begin(), bytes.end());
    ge::brush::Map read;
    if (map->file.empty() || !ge::brush::read_bsp(map->file, &read, &error)) {
      lab.fail(
          std::string(folder).append(": ").append(name).append(" did not read: ").append(error),
          "Check the install's packs. Skill: brush.");
      continue;
    }
    sh::Locs locs;
    adapter.open_locations(adapter, files, name, locs);
    lab.metric(std::string(folder).append(".").append(name).append(".places"),
               double(locs.points.size()), "count", det);
    sh::Vec3f at{0, 0, 0};
    float yaw = 0;
    player_start(read, at, yaw);
    at.z += kEyeHeight;
    if (!device) continue;
    cl::WorldDrawPtr world = cl::create_world_draw(
        *device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {}, error, {}, adapter);
    if (!world) {
      lab.fail(std::string(folder).append(": ").append(error),
               "Read the gpu: lines above. Skill: render.");
      continue;
    }
    ge::render::FrameView view;
    const ge::camera::Vec3 eye = cl::engine_point(at);
    view.eye[0] = eye.x;
    view.eye[1] = eye.y;
    view.eye[2] = eye.z;
    view.heading = cl::engine_heading(yaw);
    view.fov_y = 1.2;
    view.time_s = 1;
    cl::draw_world(*device, *world, view, cl::FxFrame{}, nullptr, 0, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    gl::Picture p = gl::create_picture(int(img.width), int(img.height), {0, 0, 0});
    p.rgba = img.rgba;
    lab.shot({std::string(folder).append("-").append(name),
              std::string(folder)
                  .append(": ")
                  .append(f.content->what)
                  .append(" (")
                  .append(f.release)
                  .append("), ")
                  .append(name),
              "The start map of the install in " + folder +
                  ", found beside the game and opened with its own search path (the bundled "
                  "client's folder in it), drawn from the player's start in the modern look.",
              "the map whole from its start, every wall textured",
              {{"install", folder}, {"map", name}}},
             p, gl::Det::measured);
  }
}

const gl::LabSpec kSpec{
    "se-installs",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the classic installs beside the game: found, their configs run in order, a map each drawn",
    "shooter",
    "Placed above several classic installs, does the game find each one, tell its release, run its "
    "configs in the install's order and open its start map?",
    {{"size", "640,360", "each picture's width and height, px"},
     {"parent", "",
      "a folder holding classic installs (private pictures only); none: our own fixture"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
