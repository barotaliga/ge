// FALL LINE'S DIRECTORS: the replay's film camera and the race's trackside broadcast.
//
// THE FILM CAMERA draws the director's cut (engine/replay/director.hpp): each shot names a lens
// in Fall Line's shot rules (sim/moments.cpp), and this file says what each name is:
//   side     a long lens planted beside the lip, a little ahead, low: the flight across the frame
//   front    planted down the line ahead of the moment, the skier running at it
//   aerial   planted high and wide over the finish
//   impact   planted low just past a fall, waiting for it
//   chase    behind him on the path, the one lens that moves with him
// A replay knows the run, so the plan is made ONCE from the cut and the player's path (one point
// a step): every planted lens is placed by `plant_beside` where it sees most of its shot, and
// every pose after that is a PURE function of the plan and the step (`film_pose`): the aim is the
// path smoothed over a window, the chase stands behind the smoothed path. Any frame of a replay
// draws on its own, as a video export needs, and every shot's first frame is a cut.
//
// THE BROADCAST stands trackside lenses down the piste (`trackside_lenses`, one every
// `kTracksideEvery` of it, each planted where it sees the stretch running toward it, all on the
// right of the line, one that sees too little left out) and cuts between them live by the engine's
// broadcast rules (`direct_broadcast`).
//
// ge:summary Fall Line's replay film camera (the director's lenses) and trackside broadcast
// ge:keywords camera film replay director lens side front aerial impact chase trackside broadcast
// ge:keywords plan_film film_pose trackside_lenses broadcast_pose cut
// ge:skill camera replay
// ge:test tests/fall_line_film_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "camera/plant.hpp"
#include "mountain/mapgen/types.hpp"
#include "replay/director.hpp"
#include "skier/skier.hpp"

namespace fall_line::camera {

inline constexpr double kFilmWindowS = 0.5;     // the aim's smoothing window, s (whole width)
inline constexpr double kTracksideEvery = 110;  // m of piste between trackside lenses
inline constexpr double kTracksideOff = 8;      // m a trackside lens stands off the piste
inline constexpr double kTracksideSeen = 0.6;   // a lens seeing less of its stretch is left out

enum class FilmLens : uint8_t { side, front, aerial, impact, chase };

// The lens a shot rule names; chase for a name it does not know.
FilmLens film_lens_named(const std::string& name);
const char* film_lens_name(FilmLens lens);

struct FilmShot {
  FilmLens lens = FilmLens::chase;
  int64_t from = 0, to = 0, mark = 0;  // the run's steps
  ge::camera::Planting plant{};        // a planted lens's place (unused by the chase)
};

struct FilmPlan {
  std::vector<ge::camera::PathPoint> path;  // the player, one point a step of the run
  std::vector<FilmShot> shots;              // the cut's shots, in its order
  size_t half_steps = 0;                    // the smoothing's half window, steps
  double hz = 120;                          // the run's steps a second
};

// The skier as a path point (his feet and his velocity).
ge::camera::PathPoint path_point(const ge::skier::SkierState& s);

// The film plan of a cut: `path` holds the player at every step of the run (index = step).
FilmPlan plan_film(const ge::replay::Cut& cut, std::span<const ge::replay::Mark> marks,
                   std::vector<ge::camera::PathPoint> path, const ge::camera::World& world,
                   double hz);

// The pose of shot `shot` at run step `step` (fractional: drawn between steps). Pure.
ge::camera::Pose film_pose(const FilmPlan& plan, const ge::camera::World& world, size_t shot,
                           double step);

// The trackside lenses down the level's piste.
std::vector<ge::camera::PlantedRow> trackside_lenses(const ge::mountain::mapgen::Level& level,
                                                     const ge::camera::World& world);

// The broadcast's frame: the live lens (by the rules) aimed at him. Live, so it keeps state.
struct Broadcast {
  std::vector<ge::camera::PlantedRow> lenses;
  ge::camera::BroadcastRule rule{};
  ge::camera::BroadcastState state{};
  ge::camera::PlantedState aim{};
};
// The 180° rule keeps to the piste's direction where he is, not to the swing of his turns.
ge::camera::Pose broadcast_pose(Broadcast& b, const ge::mountain::mapgen::Level& level,
                                const ge::camera::Subject& s, const ge::camera::World& world,
                                double dt);

}  // namespace fall_line::camera
