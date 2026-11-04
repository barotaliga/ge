// A map loaded behind the window (map_load.hpp).
//
// ge:summary The map loader's threads and steps: the file, the world's CPU half, the compile
// ge:keywords map-load load_map load_world step_load cancel_load future thread
// ge:skill render shooter
#include "shooters_edge/client/map_load.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <utility>

namespace shooters_edge::client {

namespace {

// A thread's step that threw (out of memory, a file system's error): the step fails with it.
std::string failed_on(const std::string& what, const std::exception& e) {
  return what + " did not read: " + e.what() + "; next: read the lines above, then load it again";
}

// The threads' steps: a throw is the step's failure, with its reason.
MapRead read_map(const SearchPath* files, const std::string& name) {
  MapRead r;
  try {
    r.map = load_named_map(*files, name, r.error);
  } catch (const std::exception& e) {
    r.map.reset();
    r.error = failed_on("the map " + name, e);
  }
  return r;
}

WorldReadDone read_world_of(const SearchPath* files, const GameAdapter* game,
                            const std::shared_ptr<const LoadedMap>& map,
                            const std::shared_ptr<const std::vector<std::string>>& names,
                            const look::Picture& picture) {
  WorldReadDone r;
  try {
    r.read = read_world(*files, *map, picture, {*names, {}}, r.error, *game);
  } catch (const std::exception& e) {
    r.read.reset();
    r.error = failed_on("the world of " + map->name, e);
  }
  return r;
}

template <class T>
bool done(const std::future<T>& f) {
  return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

}  // namespace

bool busy(const MapLoad& l) { return l.map.valid() || l.world.valid() || l.compiling != nullptr; }

std::string load_caption(const MapLoad& l) {
  if (!busy(l)) return "";
  std::string s = "LOADING";
  if (!l.what.empty()) s += " ";
  for (const char c : l.what) s += (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
  return s;
}

void load_map(MapLoad& l, const SearchPath& files, const std::string& name, double now_s) {
  l.what = name;
  l.since_s = now_s;
  l.map = std::async(std::launch::async, read_map, &files, name);
}

void load_world(MapLoad& l, const SearchPath& files, const LoadedMap& map,
                const look::Picture& picture, std::span<const std::string> models,
                const GameAdapter& game, double now_s) {
  if (l.world.valid()) l.dropped.push_back(std::move(l.world));
  l.compiling.reset();
  if (l.what.empty() || l.what != map.name) {
    l.what = map.name;
    l.since_s = now_s;
  }
  // The thread's own copies: the map's bytes (the server keeps its own map), the names.
  auto copy = std::make_shared<LoadedMap>();
  copy->name = map.name;
  copy->from_file = map.from_file;
  copy->file = map.file;
  std::shared_ptr<const LoadedMap> at = std::move(copy);
  auto listed = std::make_shared<const std::vector<std::string>>(models.begin(), models.end());
  l.world = std::async(std::launch::async, read_world_of, &files, &game, at, listed, picture);
}

LoadStep step_load(MapLoad& l, ge::gpu::Device& device, uint32_t width, uint32_t height,
                   wgpu::TextureFormat format, const look::Picture& picture,
                   std::span<const ge::viewmodel::HeldModel> weapons, bool wait) {
  LoadStep out;
  // The reads let run out, dropped once they have.
  std::erase_if(l.dropped, [](const std::future<WorldReadDone>& f) { return done(f); });
  if (l.map.valid() && (wait || done(l.map))) {
    MapRead r = l.map.get();
    out.map = std::move(r.map);
    if (!out.map) {
      out.failed = LoadFailed::map;
      out.error = r.error;
      l.what.clear();
    }
    return out;  // the caller starts it on the server, then asks for its world
  }
  if (l.world.valid() && (wait || done(l.world))) {
    WorldReadDone r = l.world.get();
    if (!r.read) {
      out.failed = LoadFailed::world;
      out.error = r.error;
      l.what.clear();
      return out;
    }
    l.compiling =
        begin_world(device, std::move(r.read), width, height, format, picture, weapons, out.error);
    if (!l.compiling) {
      out.failed = LoadFailed::world;
      l.what.clear();
      return out;
    }
  }
  if (l.compiling && (wait || world_compiled(device, *l.compiling))) {
    if (!finish_world(device, *l.compiling, width, height, out.error)) {
      out.failed = LoadFailed::world;
      l.compiling.reset();
      l.what.clear();
      return out;
    }
    out.world = std::move(l.compiling);
    l.what.clear();
  }
  return out;
}

void cancel_load(MapLoad& l) {
  if (l.map.valid()) l.map.wait();
  l.map = {};
  if (l.world.valid()) l.world.wait();
  l.world = {};
  for (std::future<WorldReadDone>& f : l.dropped) f.wait();
  l.dropped.clear();
  l.compiling.reset();
  l.what.clear();
}

}  // namespace shooters_edge::client
