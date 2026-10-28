// LAB `room`: what the room round the listener does to a sound (engine/audio/room.hpp,
// reverb.hpp), in rooms built of brushes (engine/move's box world, the shape a level's own
// structure answers rays in): a stone closet, a furnished room, a corridor, a stone hall, a great
// hall, a roofless court and an open field. Each room is probed from the ear, drawn from above
// with its 64 rays (each ray's colour its distance, grey where it left the room), and a hand
// clap is played into it through the synth: its picture and WAV, its numbers, and its decay
// measured off the rendered tail (Schroeder's backward integral) against the probe's own
// Eyring prediction, a failure past a quarter off. Then a WALK out of a corridor, through a door
// into the great hall, across it and out into the court, probed every half metre, charted, and
// rendered as one recording of footsteps gliding from room to room.
//
// ge:summary Lab: rooms of brushes probed by rays, a clap in each, the decay checked, a walk
// ge:keywords lab audio room reverb rt60 probe clap impulse-response schroeder walk box-world
// ge:keywords early-reflections eyring corridor hall court open-field
// ge:skill audio labs
// ge:test tests/audio_room_test.cpp
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/room.hpp"
#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/sound.hpp"
#include "move/box_world.hpp"

namespace {

namespace gl = ge::lab;
namespace mv = ge::move;
using namespace ge::audio;
namespace libm = ge::core::libm;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

// A world of brushes and what each brush's faces absorb.
struct Place {
  mv::BoxWorld world;
  std::vector<double> absorption;  // by brush
};

void slab(Place& p, mv::Vec3 lo, mv::Vec3 hi, double absorption) {
  mv::add_box(p.world, lo, hi);
  p.absorption.push_back(absorption);
}

// A room's shell: floor, walls and (with `roof`) a ceiling, 0.4 m thick round the inside
// lo..hi; `door` cuts a 2 m wide, 2.4 m tall gap in the +z wall's middle.
void shell(Place& p, mv::Vec3 lo, mv::Vec3 hi, double absorption, bool roof, bool door = false,
           bool back_door = false) {
  const double t = 0.4;
  slab(p, {lo.x - t, lo.y - t, lo.z - t}, {hi.x + t, lo.y, hi.z + t}, absorption);  // floor
  if (roof) slab(p, {lo.x - t, hi.y, lo.z - t}, {hi.x + t, hi.y + t, hi.z + t}, absorption);
  slab(p, {lo.x - t, lo.y, lo.z - t}, {lo.x, hi.y, hi.z + t}, absorption);  // -x
  slab(p, {hi.x, lo.y, lo.z - t}, {hi.x + t, hi.y, hi.z + t}, absorption);  // +x
  auto end_wall = [&](double z0, double z1, bool gap) {
    if (!gap) {
      slab(p, {lo.x, lo.y, z0}, {hi.x, hi.y, z1}, absorption);
      return;
    }
    const double mid = (lo.x + hi.x) / 2;
    slab(p, {lo.x, lo.y, z0}, {mid - 1, hi.y, z1}, absorption);
    slab(p, {mid + 1, lo.y, z0}, {hi.x, hi.y, z1}, absorption);
    if (hi.y > lo.y + 2.4) slab(p, {mid - 1, lo.y + 2.4, z0}, {mid + 1, hi.y, z1}, absorption);
  };
  end_wall(lo.z - t, lo.z, back_door);
  end_wall(hi.z, hi.z + t, door);
}

RayHit ray_of(const void* ctx, const double from[3], const double dir[3], double reach) {
  const auto& p = *static_cast<const Place*>(ctx);
  const mv::Vec3 a{from[0], from[1], from[2]};
  const mv::Vec3 b{from[0] + dir[0] * reach, from[1] + dir[1] * reach, from[2] + dir[2] * reach};
  const mv::Hit h = mv::trace_box_world(p.world, {{0, 0, 0}, {0, 0, 0}}, a, b);
  if (!h.touched() || h.solid == 0) return {};
  RayHit r{true, h.fraction * reach + p.world.skin, p.absorption[h.solid - 1], {0, 0, 0}};
  r.normal[0] = h.normal.x;
  r.normal[1] = h.normal.y;
  r.normal[2] = h.normal.z;
  return r;
}

struct Scene {
  const char* id;
  const char* title;
  const char* what;
  Place place;
  double ear[3];
};

std::vector<Scene> scenes() {
  std::vector<Scene> s;
  auto add = [&](const char* id, const char* title, const char* what) -> Scene& {
    s.push_back({id, title, what, {}, {0, 1.7, 0}});
    return s.back();
  };
  {
    Scene& c = add("closet", "A STONE CLOSET", "2 x 2.4 x 2 m of bare stone (absorbs 4 %)");
    shell(c.place, {-1, 0, -1}, {1, 2.4, 1}, 0.04, true);
  }
  {
    Scene& c = add("room", "A FURNISHED ROOM", "5 x 3 x 4 m, soft (absorbs 25 %)");
    shell(c.place, {-2.5, 0, -2}, {2.5, 3, 2}, 0.25, true);
    c.ear[0] = 1;
  }
  {
    Scene& c = add("corridor", "A STONE CORRIDOR", "2.4 m wide, 3 m tall, 30 m long (absorbs 5 %)");
    shell(c.place, {-1.2, 0, -15}, {1.2, 3, 15}, 0.05, true);
  }
  {
    Scene& c = add("hall", "A STONE HALL", "20 x 8 x 12 m of stone (absorbs 4 %)");
    shell(c.place, {-10, 0, -6}, {10, 8, 6}, 0.04, true);
  }
  {
    Scene& c = add("great-hall", "A GREAT HALL", "50 x 20 x 30 m of stone (absorbs 3 %)");
    shell(c.place, {-25, 0, -15}, {25, 20, 15}, 0.03, true);
  }
  {
    Scene& c = add("court", "A ROOFLESS COURT", "16 x 6 x 16 m of stone walls under the sky");
    shell(c.place, {-8, 0, -8}, {8, 6, 8}, 0.04, false);
  }
  {
    Scene& c = add("field", "AN OPEN FIELD", "the ground alone, packed snow (absorbs 60 %)");
    slab(c.place, {-200, -1, -200}, {200, 0, 200}, 0.6);
  }
  return s;
}

// The clap: a 6 ms burst of white noise, all of it sent into the room.
std::vector<float> clap_in(const RoomSound& room, double seconds) {
  Synth s(7, EchoRoom{.delay_s = 0.21f, .feedback = 0.24f, .damp_hz = 1800});
  s.room(room, 0);
  s.noise({.duration_ms = 6, .volume = 0.4f, .echo = 1});
  std::vector<float> out(2 * static_cast<size_t>(seconds * kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

// The decay off the backward integral of the left channel after `from_s`: -5 to -25 dB, times 3.
double decay_of(const std::vector<float>& stereo, double from_s) {
  const size_t n = stereo.size() / 2, first = static_cast<size_t>(from_s * kRate);
  std::vector<double> back(n + 1, 0);
  for (size_t i = n; i-- > first;) {
    const double v = stereo[2 * i];
    back[i] = back[i + 1] + v * v;
  }
  const double total = back[first];
  if (total <= 0) return 0;
  double t5 = -1;
  for (size_t i = first; i < n; ++i) {
    const double db = 10 * libm::log(back[i] / total) / libm::log(10.0);
    if (t5 < 0 && db <= -5) t5 = static_cast<double>(i) / kRate;
    if (db <= -25) return 3 * (static_cast<double>(i) / kRate - t5);
  }
  return 0;
}

gl::Rgba ray_colour(const RayHit& h, double reach) {
  if (!h.hit) return {170, 170, 176};
  const double s = std::fmin(1.0, libm::log(1 + h.distance_m) / libm::log(1 + reach / 4));
  return {static_cast<uint8_t>(40 + 200 * s), static_cast<uint8_t>(110 + 60 * (1 - s)),
          static_cast<uint8_t>(230 - 180 * s)};
}

// The room from above: its brushes' footprints, the rays (their xz), the ear.
gl::Picture plan(const Scene& sc, const RoomProbe& p, double reach) {
  constexpr int kSize = 360;
  gl::Picture pic = gl::create_picture(kSize, kSize, {246, 246, 248});
  double half = 3;
  for (int i = 0; i < kProbeRays; ++i) {
    const RayHit& h = p.ray[static_cast<size_t>(i)];
    if (h.hit) half = std::fmax(half, h.distance_m * 1.08);
  }
  half = std::fmin(half, reach / 2);
  const double k = kSize / (2 * half);
  auto px = [&](double x) { return kSize / 2.0 + (x - sc.ear[0]) * k; };
  auto pz = [&](double z) { return kSize / 2.0 - (z - sc.ear[2]) * k; };
  for (const mv::Brush& b : sc.place.world.brushes) {
    if (b.hi.y <= 0.01 || b.lo.y > sc.ear[1]) continue;  // floors and roofs are not walls
    gl::fill_rect(pic, px(b.lo.x), pz(b.hi.z), (b.hi.x - b.lo.x) * k, (b.hi.z - b.lo.z) * k,
                  {70, 70, 80});
  }
  for (int i = 0; i < kProbeRays; ++i) {
    const RayHit& h = p.ray[static_cast<size_t>(i)];
    const auto& d = probe_direction(i);
    const double len = h.hit ? h.distance_m : half * 1.5;
    gl::draw_line(pic, px(sc.ear[0]), pz(sc.ear[2]), px(sc.ear[0] + d[0] * len),
                  pz(sc.ear[2] + d[2] * len), ray_colour(h, reach));
  }
  for (int w = 0; w < p.walls; ++w) {
    const auto& d = p.wall_dir[static_cast<size_t>(w)];
    const double m = p.wall_m[static_cast<size_t>(w)];
    gl::fill_disc(pic, px(sc.ear[0] + d[0] * m), pz(sc.ear[2] + d[2] * m), 4, {220, 60, 50});
  }
  gl::fill_disc(pic, px(sc.ear[0]), pz(sc.ear[2]), 5, {20, 20, 24});
  gl::draw_text(pic, 6, 6, std::string(sc.title) + "  " + fmt(2 * half, 0) + " M ACROSS",
                {20, 20, 24});
  return pic;
}

void rooms(gl::Lab& lab) {
  const ProbeSettings ps;
  const double right[3] = {1, 0, 0};
  gl::LabTable table{"THE ROOMS, PROBED AND CLAPPED IN",
                     {"room", "open", "mfp m", "volume m3", "absorbs", "rt60 s", "measured s",
                      "tail", "taps", "echo"},
                     {}};
  for (const Scene& sc : scenes()) {
    const RoomWorld world{&sc.place, ray_of};
    const RoomProbe p = probe_room(world, sc.ear, ps);
    const RoomSound rs = room_sound(p, right, ps);
    const std::string id = sc.id;
    lab.shot(
        {id + "-plan",
         std::string(sc.title) + " FROM ABOVE",
         std::string(sc.what) +
             ": its walls in dark grey, the ear in black, the 64 probe "
             "rays seen from above (blue near, orange far, grey where a ray left the room) and "
             "the nearest wall in each octant in red, the early echoes' sources.",
         "rays stop at the walls; open rays only where the room is open (the court's sky, the "
         "field's everything above the ground)",
         {{"rays", std::to_string(kProbeRays)}, {"reach", fmt(ps.reach_m, 0) + " m"}}},
        plan(sc, p, ps.reach_m));
    const double seconds = std::fmin(8.0, std::fmax(1.0, p.rt60_s * 1.3 + 0.3));
    const std::vector<float> clap = clap_in(rs, seconds);
    gl::sound_shot(
        lab,
        {id + "-clap",
         std::string("A CLAP IN ") + (sc.title + 2),
         std::string("A 6 ms burst of white noise sent wholly into the room (") + sc.what +
             ") through the synth, the room set from the probe: RT60 " + fmt(p.rt60_s, 2) +
             " s, tail " + fmt(rs.wet, 2) + ", " + std::to_string(rs.taps) +
             " early echoes, outdoor echo " + fmt(rs.echo, 2) + ".",
         "the early echoes as separate ticks in the first tens of ms; the tail's "
         "envelope a straight slope in dB, longer the bigger and harder the room",
         {{"seconds", fmt(seconds, 2)}}},
        clap);
    const double measured =
        p.hits > 0 && rs.wet > 0 ? decay_of(clap, 0.01 + rs.predelay_ms / 1000) : 0;
    if (p.hits > 0 && p.open_share < 0.3 && std::fabs(measured / p.rt60_s - 1) > 0.25) {
      lab.fail(id + ": the rendered clap decays in " + fmt(measured, 2) + " s, the probe said " +
                   fmt(p.rt60_s, 2) + " s",
               "The reverb's loop gains or its damping moved: run tests/audio_reverb_test.cpp. "
               "Skill: audio.");
    }
    lab.metric(id + ".open-share", p.open_share, "share");
    lab.metric(id + ".mean-free-path", p.mean_free_path_m, "m");
    lab.metric(id + ".rt60", p.rt60_s, "s");
    lab.metric(id + ".rt60-measured", measured, "s");
    lab.metric(id + ".tail", rs.wet, "share");
    table.rows.push_back({id, fmt(p.open_share, 2), fmt(p.mean_free_path_m, 2), fmt(p.volume_m3, 0),
                          fmt(p.absorption, 2), fmt(p.rt60_s, 2), fmt(measured, 2), fmt(rs.wet, 2),
                          std::to_string(rs.taps), fmt(rs.echo, 2)});
  }
  lab.table(table);
}

// The walk: a corridor (z -40..-25) opening through a door into the great hall (z -25..5),
// whose far door opens into a roofless court (z 5..21).
Place walk_place() {
  Place p;
  shell(p, {-1.2, 0, -40}, {1.2, 3, -25.4}, 0.05, true);
  shell(p, {-25, 0, -25}, {25, 20, 5}, 0.03, true, true, true);
  shell(p, {-8, 0, 5.4}, {8, 6, 21}, 0.04, false);
  return p;
}

void walk(gl::Lab& lab) {
  const Place place = walk_place();
  const RoomWorld world{&place, ray_of};
  const ProbeSettings ps;
  const double right[3] = {1, 0, 0};
  gl::Trace rt{"RT60 (s)", {}, {}}, tail{"tail (share of the send)", {}, {}},
      open{"open share", {}, {}}, echo{"outdoor echo", {}, {}};
  constexpr double kFrom = -38, kTo = 18, kStep = 0.5;
  // A step every 0.5 m at 1.25 m/s: one probe and one footfall a step, the room glided 0.3 s.
  Synth s(7, EchoRoom{.delay_s = 0.21f, .feedback = 0.24f, .damp_hz = 1800});
  std::vector<float> out;
  const int per_step = static_cast<int>(kStep / 1.25 * kRate);
  std::vector<float> chunk(2 * static_cast<size_t>(per_step));
  const int steps = static_cast<int>((kTo - kFrom) / kStep);
  for (int i = 0; i <= steps; ++i) {
    const double z = kFrom + i * kStep;
    const double ear[3] = {0, 1.7, z};
    const RoomProbe p = probe_room(world, ear, ps);
    const RoomSound rs = room_sound(p, right, ps);
    rt.x.push_back(z);
    rt.y.push_back(p.rt60_s);
    tail.x.push_back(z);
    tail.y.push_back(rs.wet);
    open.x.push_back(z);
    open.y.push_back(p.open_share);
    echo.x.push_back(z);
    echo.y.push_back(rs.echo);
    s.room(rs, 0.3f);
    s.noise({.duration_ms = 25,
             .volume = 0.3f,
             .color = Color::pink,
             .filter = {.type = FilterType::lowpass, .hz = 900},
             .echo = 0.6f});
    s.render(chunk.data(), per_step);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  gl::LineChart line;
  line.frame = {"THE ROOM ALONG A WALK: CORRIDOR, GREAT HALL, COURT",
                "probed every 0.5 m along the walk's centre line", "z (m)", "seconds / share"};
  line.traces = {rt, tail, open, echo};
  line.bands = {{kFrom, -25.4, "corridor"}, {-25, 5, "great hall"}, {5.4, kTo, "court"}};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("walk.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"walk",
            chart.title,
            "The room probed every half metre walking out of a 2.4 m stone corridor, through a "
            "door into a 50 x 20 x 30 m stone hall, across it and out through its far door into a "
            "roofless court. RT60 and the tail's level (the probe's room_sound) and the share of "
            "rays that met nothing, which is also how much of the outdoor echo is heard.",
            "the corridor short and dry, the hall's RT60 rising to seconds once the ear is past "
            "the door, the court's open share about a quarter and its RT60 back under one second",
            {{"step", "0.5 m"}}},
           gl::to_picture(chart.plot));
  gl::sound_shot(lab,
                 {"walk-steps",
                  "FOOTSTEPS ALONG THE WALK",
                  "A footfall every 0.5 m at a walking pace along the same walk, the room probed "
                  "at each step and glided over 0.3 s: the corridor's flutter, the hall's long "
                  "tail opening as the door is passed, the court's short answer and the outdoor "
                  "echo returning under the sky.",
                  "the envelope's floor rising in the hall and falling in the court; no click "
                  "where the room changes",
                  {{"seconds", fmt(static_cast<double>(out.size()) / 2 / kRate, 1)}}},
                 out);
  lab.metric("walk.hall-rt60", rt.y[static_cast<size_t>((-10 - kFrom) / kStep)], "s");
  lab.metric("walk.court-open", open.y.back(), "share");
}

void run(gl::Lab& lab) {
  const std::string only = lab.arg("scene");
  if (only == "all" || only == "rooms") rooms(lab);
  if (only == "all" || only == "walk") walk(lab);
}

const gl::LabSpec kSpec{
    "room",
    gl::LabKind::scenario,
    "engine",
    "engine/audio (the room probe, the reverb)",
    "audio",
    "What does the room round the listener sound like, probed from the level's own structure, "
    "and does its tail decay as the probe says?",
    {
        {"scene", "all", "all, rooms or walk"},
    },
    "--scene=all",
    run,
};
GE_LAB(kSpec);

}  // namespace
