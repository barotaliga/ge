// LAB `se-ambient`: Shooter's Edge's ambience by leaf, heard on a walk. The fixture halls
// (brush::sample_fixture) are written as a 29-family map with the levels a map's compiler stores
// per leaf: the hall open to the sky, the side room by water, the east door between them hearing a
// little of each. A listener stands in the hall, walks through the door into the side room, waits
// and walks back, and shooters_edge::Ambience plays the game's own `ambient/leaf_sky` and
// `ambient/leaf_water` at what each leaf says.
//
// What it shows: the walk from above over the rooms, each layer's target and level over the walk
// (the level trailing the target at the bed's rate), and the mix with its WAV. Its checks: the sky
// is up in the hall and gone in the side room, the water the other way round, no level jumps
// faster than the rate, a silent layer holds no voice, and nothing clips.
//
// ge:summary Lab: the ambience by leaf on a walk through the fixture halls, charted and as WAV
// ge:keywords lab audio ambience ambient leaf water sky walk crossfade bed se-ambient
// ge:skill audio labs
// ge:test tests/shooters_edge_ambience_test.cpp
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"
#include "shooters_edge/sound/ambience.hpp"

namespace {

using namespace ge::audio;
namespace gl = ge::lab;
namespace br = ge::brush;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

constexpr int kFps = 72;  // the frames the walk is stepped at (the game's server rate)

// The walk: where the listener is at each time (map units, z up), straight between.
struct Point {
  double t;
  float x, y;
};
constexpr Point kWalk[] = {{0.0, 160, 192},  {2.0, 160, 192}, {4.0, 480, 192},  {4.4, 528, 192},
                           {6.0, 704, 256},  {9.5, 704, 256}, {11.1, 528, 192}, {11.5, 480, 192},
                           {13.5, 160, 192}, {16.0, 160, 192}};
constexpr double kSeconds = 16.0;
constexpr float kEar = 64;  // the listener's height above the floor

void at(double t, float out[3]) {
  size_t i = 0;
  while (i + 2 < std::size(kWalk) && kWalk[i + 1].t <= t) i++;
  const Point &a = kWalk[i], &b = kWalk[i + 1];
  double u = (t - a.t) / (b.t - a.t);
  u = u < 0 ? 0 : u > 1 ? 1 : u;
  out[0] = static_cast<float>(a.x + (b.x - a.x) * u);
  out[1] = static_cast<float>(a.y + (b.y - a.y) * u);
  out[2] = kEar;
}

// The halls with their ambient levels: the hall under open sky, the side room by water, the east
// door hearing some of each.
br::Fixture halls() {
  br::Fixture fx = br::sample_fixture();
  fx.rooms[0].ambient[br::kAmbientSky] = 230;
  fx.rooms[1].ambient[br::kAmbientSky] = 90;
  fx.rooms[1].ambient[br::kAmbientWater] = 60;
  fx.rooms[2].ambient[br::kAmbientWater] = 200;
  return fx;
}

bool load(gl::Lab& lab, const std::string& path, Pcm* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    lab.fail("se-ambient: cannot read " + path,
             "next: run the lab from the repository's root, or pass --sounds=<directory>");
    return false;
  }
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                                   std::istreambuf_iterator<char>()};
  std::string error;
  if (!read_wav(bytes, path, out, &error)) {
    lab.fail("se-ambient: " + error,
             "next: the shipped file is damaged; replace it from its source (skill: free-assets)");
    return false;
  }
  return true;
}

void map_shot(gl::Lab& lab, const br::Fixture& fx) {
  gl::LineChart line;
  line.frame = {"THE WALK FROM ABOVE", "the fixture halls, map units", "x (units)", "y (units)"};
  line.frame.height = 760;
  for (size_t r = 0; r < 3; r++) {  // the hall, the east door and the side room: the walk's
    const br::FixtureRoom& rm = fx.rooms[r];
    std::string label = rm.name;
    if (rm.ambient[br::kAmbientSky]) label += " sky " + std::to_string(rm.ambient[br::kAmbientSky]);
    if (rm.ambient[br::kAmbientWater]) {
      label += " water " + std::to_string(rm.ambient[br::kAmbientWater]);
    }
    const double x0 = rm.min[0], y0 = rm.min[1], x1 = rm.max[0], y1 = rm.max[1];
    line.traces.push_back({label, {x0, x1, x1, x0, x0}, {y0, y0, y1, y1, y0}});
  }
  gl::Trace walk{"the walk", {}, {}};
  for (const Point& p : kWalk) {
    walk.x.push_back(p.x);
    walk.y.push_back(p.y);
  }
  line.traces.push_back(walk);
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"walk-map",
            chart.title,
            "The three fixture rooms the walk crosses, with the ambient levels (0..255) each "
            "room's leaf stores, and the listener's walk: from the hall through the east door into "
            "the side room and "
            "back.",
            "the walk staying inside the rooms; the hall marked sky, the side room water",
            {{"family", "29"}}},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  const std::string dir = lab.arg("sounds");
  Pcm water, sky;
  if (!load(lab, dir + "/ambient/leaf_water.wav", &water)) return;
  if (!load(lab, dir + "/ambient/leaf_sky.wav", &sky)) return;
  const br::Fixture fx = halls();
  std::vector<uint8_t> bytes;
  std::string error;
  br::Map map;
  if (!br::write_bsp(fx, br::Family::v29, &bytes, &error) || !br::read_bsp(bytes, &map, &error)) {
    lab.fail("se-ambient: the fixture halls did not write and read back: " + error,
             "next: run tests/brush_read_test (skill: brush)");
    return;
  }
  map_shot(lab, fx);

  const float volume = static_cast<float>(lab.arg_num("volume"));
  Synth s;
  shooters_edge::Ambience amb(water, sky);
  const int per = kRate / kFps;
  const int frames = static_cast<int>(kSeconds * kFps);
  const float dt = 1.0f / kFps;
  std::vector<float> mix, chunk(2 * static_cast<size_t>(per));
  mix.reserve(chunk.size() * static_cast<size_t>(frames));
  gl::Trace tw{"water target", {}, {}}, lw{"water level", {}, {}}, ts{"sky target", {}, {}},
      ls{"sky level", {}, {}};
  float fastest = 0, prev[2] = {0, 0};
  int stray_voices = 0;
  for (int f = 0; f < frames; f++) {
    const double t = static_cast<double>(f) / kFps;
    float p[3], target[4];
    at(t, p);
    shooters_edge::leaf_ambience(map, p, volume, target);
    amb.step(s, map, p, dt, volume);
    const auto& layers = amb.bed().layers();
    for (int k = 0; k < 2; k++) {
      const float d = layers[static_cast<size_t>(k)].level - prev[k];
      fastest = (d < 0 ? -d : d) / dt > fastest ? (d < 0 ? -d : d) / dt : fastest;
      prev[k] = layers[static_cast<size_t>(k)].level;
      if (layers[static_cast<size_t>(k)].level == 0 &&
          layers[static_cast<size_t>(k)].voice.valid()) {
        stray_voices++;
      }
    }
    tw.x.push_back(t), tw.y.push_back(target[br::kAmbientWater]);
    ts.x.push_back(t), ts.y.push_back(target[br::kAmbientSky]);
    lw.x.push_back(t), lw.y.push_back(layers[0].level);
    ls.x.push_back(t), ls.y.push_back(layers[1].level);
    s.render(chunk.data(), per);
    mix.insert(mix.end(), chunk.begin(), chunk.end());
  }

  gl::LineChart line;
  line.frame = {"EACH LAYER'S TARGET AND LEVEL ON THE WALK",
                "target: the leaf's level / 255 x the volume; level: where the bed has walked it",
                "t (s)", "level"};
  line.traces = {ts, ls, tw, lw};
  line.bands = {{2.0, 6.0, "walking in"}, {9.5, 13.5, "walking out"}};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"levels",
            chart.title,
            "The targets jump as the listener crosses from one leaf into the next; each level "
            "follows at the bed's rate (" +
                fmt(shooters_edge::kAmbientRate, 2) +
                " a second), so the sky fades and the water swells over about a second instead "
                "of switching.",
            "every level a ramp, never a step; the sky high in the hall and at 0 in the side "
            "room; the water the other way round",
            {{"volume", lab.arg("volume")}, {"fps", std::to_string(kFps)}}},
           gl::to_picture(chart.plot));

  const Meter m = gl::sound_shot(
      lab,
      {"walk",
       "THE WALK, HEARD",
       "The game's leaf_sky and leaf_water recordings on the ambience bed as the listener walks "
       "from the hall into the side room and back.",
       "the wind (broad, noisy) in the first and last seconds, the water's ripple in the middle, "
       "each fading into the other across the door",
       {{"sounds", dir}, {"volume", lab.arg("volume")}}},
      mix);
  float hall[4], side[4];
  const float hp[3] = {160, 192, kEar}, sp[3] = {704, 256, kEar};
  shooters_edge::leaf_ambience(map, hp, volume, hall);
  shooters_edge::leaf_ambience(map, sp, volume, side);
  lab.metric("walk.hall-sky", hall[br::kAmbientSky], "level");
  lab.metric("walk.side-water", side[br::kAmbientWater], "level");
  lab.metric("walk.fastest", fastest, "level/s");
  lab.metric("walk.stray-voices", stray_voices, "frames");
  if (hall[br::kAmbientSky] <= 0 || hall[br::kAmbientWater] != 0 || side[br::kAmbientWater] <= 0 ||
      side[br::kAmbientSky] != 0) {
    lab.fail("the hall hears sky " + fmt(hall[br::kAmbientSky], 3) + ", water " +
                 fmt(hall[br::kAmbientWater], 3) + "; the side room sky " +
                 fmt(side[br::kAmbientSky], 3) + ", water " + fmt(side[br::kAmbientWater], 3),
             "next: the leaf's ambient bytes (plugins/brush/read_v29.cpp) or leaf_ambience. "
             "Skill: audio.");
  }
  if (fastest > shooters_edge::kAmbientRate * 1.001f) {
    lab.fail("a level moved " + fmt(fastest, 3) + " a second, faster than the rate",
             "next: AmbientBed::step (engine/audio/ambient.cpp). Skill: audio.");
  }
  if (stray_voices > 0) {
    lab.fail("a silent layer held a voice for " + std::to_string(stray_voices) + " frames",
             "next: AmbientBed::step stops a voice when its level reaches 0. Skill: audio.");
  }
  if (m.peak_db > -0.1f) {
    lab.fail("the walk clips: its peak is " + fmt(m.peak_db, 2) + " dBFS",
             "next: lower kAmbientVolume or the layers' gain. Skill: audio.");
  }
}

const gl::LabSpec kSpec{
    "se-ambient",
    gl::LabKind::sim,
    "game:shooters_edge",
    "Shooter's Edge's ambience by leaf (engine/audio/ambient, plugins/brush's leaf levels)",
    "audio",
    "Does the map's own ambience follow the listener from leaf to leaf, swelling and fading "
    "rather than switching?",
    {
        {"sounds", "games/shooters_edge/content/sounds", "the sounds' directory"},
        {"volume", "0.3", "the player's ambient volume"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
