// A SERVER ON THE BENCH for the loop's tests: a world of boxes (a floor at z = 0 by default), a
// program whose functions are the test's own lambdas, and a record of every call. A test adds its
// functions with `add` (each returns its function number), then `start`s the server.
//
// ge:summary Tests' helper: a shooter server over box brushes with a lambda program
// ge:keywords test bench fixture server lambda-program box-world calls
// ge:skill shooter testing
#pragma once

#include <functional>
#include <vector>

#include "shooter/brush_hull.hpp"
#include "shooter/server.hpp"

namespace ge::test::loop {

namespace sh = ge::shooter;

inline sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x, 0},   {{-1, 0, 0}, -lo.x, 3}, {{0, 1, 0}, hi.y, 1},
              {{0, -1, 0}, -lo.y, 3}, {{0, 0, 1}, hi.z, 2},   {{0, 0, -1}, -lo.z, 3}};
  return b;
}

struct Bench {
  std::vector<sh::ModelData> data;  // [0] the world, then the brush models
  std::vector<std::function<void(sh::Server&)>> fns;
  std::vector<sh::Func> calls;  // every function run, in order
  sh::Server sv;

  sh::Func add(std::function<void(sh::Server&)> f) {
    fns.push_back(std::move(f));
    return static_cast<sh::Func>(fns.size());
  }

  static void call(void* ctx, sh::Server& sv, sh::Func fn) {
    auto* b = static_cast<Bench*>(ctx);
    b->calls.push_back(fn);
    b->fns[static_cast<size_t>(fn - 1)](sv);
  }

  // The world's brushes, then one brush list per extra model (modelindex 2, 3, ...).
  void start(const std::vector<sh::Brush>& world = {box({-1024, -1024, -64}, {1024, 1024, 0})},
             const std::vector<std::vector<sh::Brush>>& extra = {}, sh::ServerSettings set = {},
             sh::Program program = {}) {
    data.clear();
    data.push_back(sh::build_brush_model(world));
    for (const auto& m : extra) data.push_back(sh::build_brush_model(m));
    std::vector<sh::BrushModel> models;
    models.reserve(data.size());
    for (const sh::ModelData& d : data) models.push_back(d.model());
    program.ctx = this;
    program.call = &Bench::call;
    sh::create_server(sv, set, models, program);
  }

  sh::Edict& ent(int e) { return sv.edicts[static_cast<size_t>(e)]; }

  // A new entity: a box of `mins`..`maxs` at `origin`, linked.
  int spawn_box(sh::Vec3f origin, sh::Vec3f mins, sh::Vec3f maxs, int solid, int movetype) {
    const int e = sh::spawn_edict(sv);
    sh::Edict& ed = ent(e);
    ed.solid = solid;
    ed.movetype = movetype;
    sh::set_size(sv, e, mins, maxs);
    sh::set_origin(sv, e, origin);
    return e;
  }
};

}  // namespace ge::test::loop
