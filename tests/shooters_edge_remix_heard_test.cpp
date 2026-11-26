// Classic remix's sounds heard in the window's own game (`cr_sounds`, M18u13c): on Quake's rules
// with `cr_sounds q2`, every player sound the rules start (the jump first) is read as Quake II's, a
// name both games ship is read as Quake II's own copy, and the players' footsteps are Quake II's.
//
// ge:summary Tests cr_sounds on the window's own game: each player sound read as the chosen game's
// ge:keywords test remix cr_sounds heard_as listen-hearing jump-sound player-sounds
// ge:skill shooter testing
#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio/synth.hpp"
#include "audio/wav.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/hull.hpp"
#include "shooter/pmove.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

std::vector<uint8_t> hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 256);
  box(512, -512, -16, 528, 512, 256);
  box(-528, 512, -16, 528, 528, 256);
  box(-528, -528, -16, 528, -512, 256);
  br::Entity start;
  start.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "90"}};
  world.entities.push_back(start);
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  return bsp;
}

std::vector<uint8_t> wav(int frames) {
  std::vector<float> s(static_cast<size_t>(frames), 0.1f);
  return ge::audio::wav_bytes(s, 1, 11025);
}

}  // namespace

TEST(cr_sounds_q2_reads_the_jump_as_quake_ii_s_on_quake_s_rules) {
  cl::RemixFiles remix;
  remix.played = "q1";
  remix.state.on = true;
  remix.state.other = "q2";
  remix.heard = "q2";
  // Quake II's files first, Quake's over them, as the remix mounts them: both ship
  // `items/protect.wav`, the pentagram's and the invulnerability's.
  std::vector<uint8_t> theirs, mine_pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"sound/player/male/jump1.wav", wav(2205)},
                                {"sound/items/protect.wav", wav(6615)}},
                               &theirs, &error),
            error);
  CHECK_MSG(ge::vfs::write_pak({{"maps/hall.bsp", hall()},
                                {"sound/player/plyrjmp8.wav", wav(1100)},
                                {"sound/items/protect.wav", wav(3300)}},
                               &mine_pak, &error),
            error);
  CHECK(cl::add_archive(remix.files, "q2.pak", theirs, error));
  CHECK(cl::add_archive(remix.files, "pak0.pak", mine_pak, error));
  remix.other_sources = 1;
  cl::SearchPath mine;
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::Listen l;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  ge::audio::Synth synth;
  cl::ListenHearing hearing(synth);
  for (int i = 0; i < 120; i++) {
    cl::listen_frame(l, {});
    hearing.frame(l, remix.heard_from(mine), 1.0f / 72, remix.heard, remix.heard_theirs());
  }
  sh::MoveCmd jump;
  jump.angles = {0, 90, 0};
  jump.buttons = sh::kButtonJump;
  for (int i = 0; i < 40; i++) {
    cl::listen_frame(l, jump);
    hearing.frame(l, remix.heard_from(mine), 1.0f / 72, remix.heard, remix.heard_theirs());
  }
  CHECK(hearing.sounds() != nullptr);
  if (hearing.sounds() == nullptr) return;
  std::string lines;
  for (const std::string& s : hearing.sounds()->list_lines()) lines += s + "\n";
  CHECK_MSG(lines.find("player/male/jump1.wav") != std::string::npos, lines);
  CHECK_MSG(lines.find("player/plyrjmp8.wav") == std::string::npos, lines);
  CHECK_MSG(hearing.stats()->started > 0, lines);
  // The name both ship: Quake II's own copy (0.60 s), not Quake's over it (0.30 s).
  CHECK_MSG(lines.find("0.60 s") != std::string::npos && lines.find("0.30 s") == std::string::npos,
            lines);
}

// `cr_sounds q2` on Quake's rules sets `g_footsteps 1`: a player running past 225 sounds Quake II's
// footsteps, one a stride; standing still, none.
TEST(cr_sounds_q2_gives_quake_s_rules_quake_ii_s_footsteps) {
  cl::RemixFiles remix;
  remix.played = "q1";
  remix.state.on = true;
  remix.state.other = "q2";
  remix.heard = "q2";
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"maps/hall.bsp", hall()}}, &pak, &error), error);
  CHECK(cl::add_archive(remix.files, "pak0.pak", pak, error));
  const std::vector<std::string> lines = cl::worn_lines(remix, {});
  CHECK(std::find(lines.begin(), lines.end(), "set g_footsteps 1") != lines.end());
  cl::SearchPath mine;
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::Listen l;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  for (const std::string& line : lines) opt.arsenal.push_back(line);
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  const auto steps = [&](const sh::MoveCmd& cmd, int frames) {
    int n = 0;
    for (int i = 0; i < frames; i++) {
      l.sounds.clear();
      cl::listen_frame(l, cmd);
      for (const cl::DemoSound& d : l.sounds)
        if (d.sound > 0 && static_cast<size_t>(d.sound) < l.server->sounds.size() &&
            l.server->sounds[static_cast<size_t>(d.sound)].starts_with("player/step"))
          n++;
    }
    return n;
  };
  sh::MoveCmd still;
  still.angles = {0, 90, 0};
  CHECK(steps(still, 216) == 0);  // spawned, standing: no step
  sh::MoveCmd run = still;
  run.forward = 400;
  const int ran = steps(run, 144);  // two seconds at a run: a stride each 0.4 s
  CHECK_MSG(ran >= 3 && ran <= 6, std::to_string(ran));
}
