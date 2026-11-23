// The room round the ear (room_ear.hpp).
//
// ge:summary bsp_ray, bsp_room and hear_room: room acoustics on a classic map
// ge:keywords ce_acoustics room-sound bsp_ray bsp_room hear_room
// ge:skill audio shooter
#include "shooters_edge/client/room_ear.hpp"

#include <algorithm>

#include "brush/survey.hpp"
#include "core/libm/libm.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kPaceM = 0.25;  // the walk's pace
constexpr int kHalvings = 5;     // the wall found to a pace / 32
constexpr double kReachM = 60;   // a ray past this is open
// A classic map's walls, taken as dressed stone with its clutter (crates, pillars, ledges the probe
// sees as a plain wall): at bare stone's 0.06 its big halls rang for 6 to 12 s (lab se-acoustics on
// both games' maps); at this the tail falls in 1 to 5 s, a stone hall and not a cathedral.
constexpr double kStoneAbsorb = 0.22;

// The cell at engine metres `p` in map units (x, z, y).
ge::brush::Cell cell_m(const ge::brush::Map& map, const double p[3]) {
  const float u[3] = {static_cast<float>(p[0] / ge::brush::kUnitM),
                      static_cast<float>(p[2] / ge::brush::kUnitM),
                      static_cast<float>(p[1] / ge::brush::kUnitM)};
  return ge::brush::cell_at(map, u);
}

bool stops(ge::brush::Cell c) { return c == ge::brush::Cell::solid || c == ge::brush::Cell::sky; }

}  // namespace

ge::audio::RayHit bsp_ray(const ge::brush::Map& map, const double from[3], const double dir[3],
                          double reach_m) {
  ge::audio::RayHit hit;
  double before = 0;
  const int paces = static_cast<int>(reach_m / kPaceM);
  for (int i = 1; i <= paces; ++i) {
    const double t = kPaceM * i;
    const double p[3] = {from[0] + dir[0] * t, from[1] + dir[1] * t, from[2] + dir[2] * t};
    const ge::brush::Cell c = cell_m(map, p);
    if (!stops(c)) {
      before = t;
      continue;
    }
    if (c == ge::brush::Cell::sky) return hit;  // open: the sound leaves
    double lo = before, hi = t;
    for (int k = 0; k < kHalvings; ++k) {
      const double mid = 0.5 * (lo + hi);
      const double q[3] = {from[0] + dir[0] * mid, from[1] + dir[1] * mid, from[2] + dir[2] * mid};
      (stops(cell_m(map, q)) ? hi : lo) = mid;
    }
    // The 38 family's sky is a wall's face: the march says whether this wall was entered by one.
    if (map.family == ge::brush::Family::v38) {
      const double u = 1 / ge::brush::kUnitM;
      const double at[3] = {from[0] * u, from[2] * u, from[1] * u};
      const double way[3] = {dir[0], dir[2], dir[1]};
      if (ge::brush::march(map, at, way, (t + kPaceM) * u, kPaceM * u) == ge::brush::Cell::sky)
        return hit;
    }
    hit.hit = true;
    hit.distance_m = 0.5 * (lo + hi);
    hit.absorption = kStoneAbsorb;
    return hit;
  }
  return hit;
}

ge::audio::RoomSound bsp_room(const ge::brush::Map& map, const double ear[3],
                              const double right[3]) {
  ge::audio::RoomWorld world;
  world.ctx = &map;
  world.ray = [](const void* ctx, const double from[3], const double dir[3], double reach_m) {
    return bsp_ray(*static_cast<const ge::brush::Map*>(ctx), from, dir, reach_m);
  };
  ge::audio::ProbeSettings settings;
  settings.reach_m = kReachM;
  // An ear inside a wall (a noclip, a corpse's view sunk in the floor) hears the room it left.
  if (stops(cell_m(map, ear))) return {};
  const ge::audio::RoomProbe probe = ge::audio::probe_room(world, ear, settings);
  return ge::audio::room_sound(probe, right, settings);
}

void hear_room(RoomEar& ear, ge::audio::Synth& synth, const ge::brush::Map* map, bool on,
               const double eye[3], double heading, double now_s) {
  if (!on || map == nullptr || map->leaves.empty()) {
    if (ear.wet) synth.room(ge::audio::RoomSound{}, static_cast<float>(kRoomGlideS));
    ear.wet = false;
    ear.next_s = 0;
    return;
  }
  if (ear.wet && now_s < ear.next_s && now_s >= ear.next_s - 2 * kProbeEveryS) return;
  ear.next_s = now_s + kProbeEveryS;
  const double right[3] = {ge::core::libm::cos(heading), 0, -ge::core::libm::sin(heading)};
  synth.room(bsp_room(*map, eye, right), static_cast<float>(ear.wet ? kRoomGlideS : 0));
  ear.wet = true;
}

void add_under_cvars(ge::console::Console& c, UnderEar& s) {
  const ge::console::CvarLayer classic(c.cvars(), "classic");
  c.cvars().bind({.name = "s_underwater",
                  .fallback = "0",
                  .flags = ge::console::kArchive,
                  .help = "Whether the world sounds muffled while your head is under water.",
                  .kind = ge::console::CvarKind::boolean,
                  .values = "0: as in the air (the default, Quake's); 1: muffled (Quake II's)"},
                 &s.on);
  c.cvars().bind({.name = "s_underwater_gain_hf",
                  .fallback = "0.25",
                  .flags = ge::console::kArchive,
                  .help = "How much of the high notes you hear under water, s_underwater on.",
                  .min = 0.001,
                  .max = 1,
                  .values = "0.001..1: 0.25 the default; 1 not muffled"},
                 &s.treble);
  c.cvars().bind({.name = "s_swapstereo",
                  .fallback = "0",
                  .flags = ge::console::kArchive,
                  .help = "Whether the left and right speakers are swapped.",
                  .kind = ge::console::CvarKind::boolean,
                  .values = "0: as they are (the default); 1: swapped"},
                 &s.swap);
  c.cvars().bind({.name = "s_hrtf",
                  .fallback = "0",
                  .help = "Whether the game's sounds are heard as two ears hear them, for "
                          "headphones: a sound to one side reaches the far ear a moment later, "
                          "quieter and duller.",
                  .kind = ge::console::CvarKind::integer,
                  .min = 0,
                  .max = 2,
                  .values = "0: off (the default); 1: as the sound device chooses (off, as it "
                            "names no headphones); 2: on"},
                 &s.hrtf);
  // The software mixer's lead in both games (Quake's `_snd_mixahead`, Quake II's `s_mixahead`):
  // kept and saved for configs and Quake II's sound page; nothing reads it, as the device pulls
  // the sound a chunk at a time (platform/audio.hpp), as Quake II's OpenAL path never reads it.
  c.cvars().add({.name = "s_mixahead",
                 .fallback = "0.1",
                 .flags = ge::console::kArchive,
                 .help = "How far ahead of time the sound is mixed, in seconds. Kept for your "
                         "settings; this sound device keeps its own short lead.",
                 .min = 0,
                 .max = 1,
                 .values = "0.1: the default; 0..1: that many seconds"});
}

void hear_under(RoomEar& ear, ge::audio::Synth& synth, const UnderEar& s, bool under) {
  const float treble = under && s.on ? static_cast<float>(std::clamp(s.treble, 0.001, 1.0)) : 1;
  if (treble == ear.muffled) return;
  synth.muffle(treble, kUnderCornerHz, 0);  // at once, as the head goes in or comes out
  ear.muffled = treble;
}

void hear_swap(ge::audio::Synth& synth, UnderEar& s) {
  if (static_cast<int>(s.swap) == s.swap_sent) return;
  synth.swap_stereo(s.swap);
  s.swap_sent = static_cast<int>(s.swap);
}

void hear_ears(ge::audio::Synth& synth, UnderEar& s) {
  const int on = hrtf_on(s) ? 1 : 0;
  if (on == s.hrtf_sent) return;
  synth.binaural(on != 0);
  s.hrtf_sent = on;
}

}  // namespace shooters_edge::client
