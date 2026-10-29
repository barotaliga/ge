// LAB `se-audio`: Shooter's Edge's sounds in a scripted fight, rendered, metered and written as
// WAV. The game's own recordings (games/shooters_edge/content/sounds, read by audio::read_wav)
// are played through audio::Channels on the synth's sampled voices, heard by a player standing at
// the origin facing +z: his own shots and voice centred, a rival circling him from behind on the
// left to the front right while firing a rocket, a beam and dying, a grenade's flight and blast,
// ricochets off a far wall, a door grinding behind on the right, and a lamp humming on the left.
//
// What it shows: the fight from above (every source's path), how loud each source is heard over
// the fight, and the mix's waveform, bands and WAV. Its checks are the ones a listener makes
// with eyes shut: the rocket's blast is on the left, the rival's beam on the left, the door on
// the right, the player's own sounds centred, and nothing clips. The fight is a script of the
// classic multiplayer game's sounds at its own distances (world units, attenuation 1/1000 for a
// shot, 1/333 for a fixed ambient); the game's sound messages replace the script when the client
// lands (M18h).
//
// ge:summary Lab: a scripted fight's sounds (the game's WAVs) heard by a player, metered, as WAV
// ge:keywords lab audio shooter fight sounds wav spatial channels listener pan attenuation
// ge:keywords rocket grenade beam door ambient ricochet se-audio
// ge:skill audio labs
// ge:test tests/audio_channels_test.cpp
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "audio/channels.hpp"
#include "audio/meter.hpp"
#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"

namespace {

using namespace ge::audio;
namespace gl = ge::lab;
namespace libm = ge::core::libm;
using ge::core::Vec3;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

constexpr double kSeconds = 8.5;
constexpr int kFps = 72;           // the frames the script is stepped at (the game's server rate)
constexpr float kShot = 0.001f;    // a shot's attenuation, per world unit
constexpr float kStatic = 0.003f;  // a fixed ambient's

// Entities: the player, the rival, the world's things.
constexpr int kPlayer = 1, kRival = 2, kRocket = 10, kGrenade = 11, kWall = 20, kDoor = 21,
              kLamp = 22;

// The rival circles the player at 350 units, from behind on the left to the front right.
Vec3 rival_at(double t) {
  const double a = (-120 + 180 * t / kSeconds) * 0.017453292519943295;
  return {350 * libm::sin(a), 0, 350 * libm::cos(a)};
}

Vec3 lerp(Vec3 a, Vec3 b, double u) {
  return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
}

// The rocket: fired by the rival at 1.0 s, past the player's left to the wall behind at 1.4 s.
constexpr double kRocketFrom = 1.0, kRocketTo = 1.4;
const Vec3 kRocketEnd{-60, 0, -200};
// The grenade: launched at 2.0 s toward where the rival will be, bouncing at 2.4, bursting at 2.8.
constexpr double kGrenadeFrom = 2.0, kGrenadeBounce = 2.4, kGrenadeTo = 2.8;
const Vec3 kWallAt{400, 0, 600}, kDoorAt{300, 0, -100}, kLampAt{-250, 64, 150};

struct Cue {
  double t;
  const char* sound;
  int entity;
  int channel;
  float volume;
  float attenuation;
  Loop loop;
};

// THE SCRIPT, in time order. A source with no fixed place is where its entity is at the time.
const Cue kScript[] = {
    {0.0, "ambient/fluorescent_hum", kLamp, 0, 0.5f, kStatic, Loop::whole},
    {0.3, "weapon/shotgun_fire", kPlayer, 1, 1, kShot, Loop::once},
    {0.35, "weapon/ricochet_1", kWall, 0, 1, kShot, Loop::once},
    {0.42, "weapon/ricochet_3", kWall, 0, 1, kShot, Loop::once},
    {1.0, "weapon/rocket_fire", kRival, 1, 1, kShot, Loop::once},
    {1.4, "weapon/explosion", kRocket, 0, 1, kShot, Loop::once},
    {2.0, "weapon/grenade_launch", kPlayer, 1, 1, kShot, Loop::once},
    {kGrenadeBounce, "weapon/grenade_bounce", kGrenade, 1, 1, kShot, Loop::once},
    {2.8, "weapon/explosion", kGrenade, 1, 1, kShot, Loop::once},
    {3.0, "player/pain_2", kRival, 2, 1, kShot, Loop::once},
    {3.5, "weapon/beam_start", kRival, 1, 1, kShot, Loop::once},
    {3.6, "weapon/beam_loop", kRival, 1, 1, kShot, Loop::whole},
    {4.0, "player/pain_1", kPlayer, 2, 1, kShot, Loop::once},
    {5.6, "door/machine_move", kDoor, 1, 1, kShot, Loop::whole},
    {6.0, "weapon/double_shotgun_fire", kPlayer, 1, 1, kShot, Loop::once},
    {6.2, "player/death_1", kRival, 2, 1, kShot, Loop::once},
    {6.25, "player/gib", kRival, 0, 1, kShot, Loop::once},
    {7.0, "door/machine_stop", kDoor, 1, 1, kShot, Loop::once},
    {7.4, "pickup/health", kPlayer, 3, 1, kShot, Loop::once},
};
constexpr double kBeamStop = 5.0;  // the rival lets go of the trigger

Vec3 where(int entity, double t) {
  switch (entity) {
    case kPlayer:
      return {0, 0, 0};
    case kRival:
      return rival_at(t < 6.2 ? t : 6.2);
    case kRocket: {
      const double u = (t - kRocketFrom) / (kRocketTo - kRocketFrom);
      return lerp(rival_at(kRocketFrom), kRocketEnd, u < 0 ? 0 : u > 1 ? 1 : u);
    }
    case kGrenade: {
      const double u = (t - kGrenadeFrom) / (kGrenadeTo - kGrenadeFrom);
      return lerp({0, 0, 0}, rival_at(kGrenadeTo), u < 0 ? 0 : u > 1 ? 1 : u);
    }
    case kWall:
      return kWallAt;
    case kDoor:
      return kDoorAt;
    default:
      return kLampAt;
  }
}

bool load(gl::Lab& lab, const std::string& dir, const std::string& name, Pcm* out) {
  const std::string path = dir + "/" + name + ".wav";
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    lab.fail("se-audio: cannot read " + path,
             "next: run the lab from the repository's root, or pass --sounds=<directory>");
    return false;
  }
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                                   std::istreambuf_iterator<char>()};
  std::string error;
  if (!read_wav(bytes, path, out, &error)) {
    lab.fail("se-audio: " + error,
             "next: the shipped file is damaged; replace it from its source (skill: free-assets)");
    return false;
  }
  return true;
}

// The balance (left -1 .. right +1) of the mix between two times.
float balance_of(const std::vector<float>& mix, double t0, double t1) {
  const size_t a = 2 * static_cast<size_t>(t0 * kRate), b = 2 * static_cast<size_t>(t1 * kRate);
  return measure(std::span<const float>(mix.data() + a, b - a)).balance;
}

void map_shot(gl::Lab& lab) {
  gl::Trace rival{"rival", {}, {}}, rocket{"rocket", {}, {}}, grenade{"grenade", {}, {}};
  for (int i = 0; i <= 62; i++) {
    const double t = i * 0.1;
    rival.x.push_back(rival_at(t).x);
    rival.y.push_back(rival_at(t).z);
  }
  for (const double u : {0.0, 1.0}) {
    const Vec3 r = where(kRocket, kRocketFrom + u * (kRocketTo - kRocketFrom));
    rocket.x.push_back(r.x);
    rocket.y.push_back(r.z);
    const Vec3 g = where(kGrenade, kGrenadeFrom + u * (kGrenadeTo - kGrenadeFrom));
    grenade.x.push_back(g.x);
    grenade.y.push_back(g.z);
  }
  // The player as a small arrow pointing +z, the fixed sources as small squares.
  gl::Trace player{"player (facing +z)", {-20, 0, 20, 0, 0}, {-20, 30, -20, 30, -30}};
  auto square = [](const char* name, Vec3 at) {
    return gl::Trace{name,
                     {at.x - 15, at.x + 15, at.x + 15, at.x - 15, at.x - 15},
                     {at.z - 15, at.z - 15, at.z + 15, at.z + 15, at.z - 15}};
  };
  gl::LineChart line;
  line.frame = {"THE FIGHT FROM ABOVE", "world units; the player at the origin facing +z",
                "x (units, + is the player's right)", "z (units, + is ahead)"};
  line.frame.height = 760;
  line.traces = {player,
                 rival,
                 rocket,
                 grenade,
                 square("wall (ricochets)", kWallAt),
                 square("door", kDoorAt),
                 square("lamp (hum)", kLampAt)};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"fight-map",
            chart.title,
            "Where every sound of the scripted fight comes from: the rival circling at 350 units "
            "from behind-left (0 s) to front-right (6.2 s, where he dies), the rocket he fires at "
            "1 s across the player's left to the wall behind, the player's grenade to where the "
            "rival is at 2.8 s, the wall the pellets ricochet off, the door and the lamp.",
            "the rocket passing left of the player; the door behind right; the lamp left",
            {{"script", fmt(kSeconds, 1) + " s"}}},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  const std::string dir = lab.arg("sounds");
  std::map<std::string, Pcm> bank;
  for (const Cue& c : kScript) {
    if (bank.count(c.sound)) continue;
    if (!load(lab, dir, c.sound, &bank[c.sound])) return;
  }
  map_shot(lab);

  Synth s;
  Channels ch(s, static_cast<int>(lab.arg_num("channels")));
  const Listener l{.at = {0, 0, 0}, .right = {1, 0, 0}, .entity = kPlayer};
  ch.update(l);
  const int per = kRate / kFps;
  const int frames = static_cast<int>(kSeconds * kFps);
  std::vector<float> mix, chunk(2 * static_cast<size_t>(per));
  mix.reserve(2 * static_cast<size_t>(per) * static_cast<size_t>(frames));
  // How loud each source is heard, frame by frame: the loudest slot it holds.
  struct Line {
    const char* label;
    int entities[2];
  };
  const Line lines[] = {{"player", {kPlayer, kPlayer}},
                        {"rival", {kRival, kRival}},
                        {"rocket, grenade", {kRocket, kGrenade}},
                        {"wall, door", {kWall, kDoor}},
                        {"lamp", {kLamp, kLamp}}};
  std::vector<gl::Trace> heard;
  for (const Line& ln : lines) heard.push_back({ln.label, {}, {}});
  size_t next = 0;
  bool beam_on = true;
  for (int f = 0; f < frames; f++) {
    const double t = static_cast<double>(f) / kFps;
    for (int e : {kRival, kRocket, kGrenade}) ch.move(e, where(e, t));
    while (next < std::size(kScript) && kScript[next].t <= t) {
      const Cue& c = kScript[next++];
      ch.play(bank[c.sound], {.entity = c.entity,
                              .channel = c.channel,
                              .at = where(c.entity, t),
                              .volume = c.volume,
                              .attenuation = c.attenuation,
                              .loop = c.loop});
    }
    if (beam_on && t >= kBeamStop) {
      ch.stop(kRival, 1, 0.05f);
      beam_on = false;
    }
    ch.update(l);
    for (size_t i = 0; i < std::size(lines); i++) {
      float g = 0;
      for (const Channels::Slot& sl : ch.slots()) {
        if (sl.voice.valid() &&
            (sl.entity == lines[i].entities[0] || sl.entity == lines[i].entities[1])) {
          g = sl.heard.gain > g ? sl.heard.gain : g;
        }
      }
      heard[i].x.push_back(t);
      heard[i].y.push_back(g);
    }
    s.render(chunk.data(), per);
    mix.insert(mix.end(), chunk.begin(), chunk.end());
  }

  gl::LineChart line;
  line.frame = {"HOW LOUD EACH SOURCE IS HEARD", "the loudest channel each holds, each frame",
                "t (s)", "gain"};
  line.traces = heard;
  line.marks = {{1.4, "rocket bursts"},
                {2.8, "grenade bursts"},
                {3.6, "beam"},
                {5.6, "door"},
                {6.2, "rival dies"}};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"fight-heard",
            chart.title,
            "The gain Channels hands each sampled voice as the fight plays: the player's own "
            "sounds at full level, the rival's by his distance (350 units: 0.65), the blasts by "
            "theirs, the lamp's hum at its own short reach.",
            "the player's line at 1 whenever he makes a sound; the rival's near 0.65 throughout; "
            "every line dropping to 0 when its sound ends",
            {{"fps", std::to_string(kFps)}}},
           gl::to_picture(chart.plot));

  const Meter m = gl::sound_shot(
      lab,
      {"fight",
       "A SCRIPTED FIGHT, HEARD BY THE PLAYER",
       "The fight's " + std::to_string(std::size(kScript)) +
           " sounds, read from the game's "
           "own WAVs (22 050 Hz) and played on the synth's sampled voices through a table of " +
           lab.arg("channels") + " channels, steered every frame at " + std::to_string(kFps) +
           " Hz.",
       "the shots' and blasts' peaks standing out of a quiet bed; the rocket's blast at 1.4 s "
       "and the beam (3.6-5 s) heavier on the left (up); the door (5.6-7 s) on the right (down)",
       {{"sounds", dir}, {"channels", lab.arg("channels")}}},
      mix);
  const float rocket = balance_of(mix, 1.4, 1.7);
  const float beam = balance_of(mix, 4.2, 4.9);
  const float door = balance_of(mix, 6.65, 6.95);
  lab.metric("fight.rocket-balance", rocket, "L-1..R+1");
  lab.metric("fight.beam-balance", beam, "L-1..R+1");
  lab.metric("fight.door-balance", door, "L-1..R+1");
  lab.metric("fight.stolen", ch.stolen(), "plays");
  lab.metric("fight.skipped", ch.skipped(), "plays");
  lab.metric("fight.dropped", s.stats().dropped, "calls");
  if (rocket > -0.1f) {
    lab.fail("the rocket's blast is heard at balance " + fmt(rocket, 2) + " (want left, < -0.1)",
             "hear's pan against the listener's right (channels.cpp). Skill: audio.");
  }
  if (beam > -0.1f) {
    lab.fail("the rival's beam is heard at balance " + fmt(beam, 2) + " (want left, < -0.1)",
             "Channels::move or update stopped following the rival. Skill: audio.");
  }
  if (door < 0.1f) {
    lab.fail("the door is heard at balance " + fmt(door, 2) + " (want right, > 0.1)",
             "hear's pan (channels.cpp). Skill: audio.");
  }
  if (m.peak_db > -0.1f) {
    lab.fail("the fight clips: its peak is " + fmt(m.peak_db, 2) + " dBFS",
             "the shots are too loud together: lower the volumes or the bus. Skill: audio.");
  }
}

const gl::LabSpec kSpec{
    "se-audio",
    gl::LabKind::sim,
    "game:shooters_edge",
    "engine/audio (sampled voices, channels) with Shooter's Edge's sounds",
    "audio",
    "How does a fight sound to the player when the game's recordings are played where they "
    "happen, through the channel limit?",
    {
        {"sounds", "games/shooters_edge/content/sounds", "the sounds' directory"},
        {"channels", "16", "the channel table's size"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
