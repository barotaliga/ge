// LAB `se-demo-video`: RECORDED GAMES TURNED INTO VIDEOS, WITH THEIR VOICES. By default a match is
// played on the fixture halls (plugins/brush) by four scripted clients of the classic protocol in
// two teams, each talking in turn, on our server with voice relayed by team; client 0 records the
// client voice format (`.qwdv`) and the server the multiview voice format (`.mvdv`)
// (net/classic_record.hpp). Each recording is read back for who is heard in it, then turned into a
// video with its sound and voices (client/demo_video.hpp). `--basedir` (with `--game` and
// `--demo`) turns a real game folder's demo into a video instead, `demo1` by default; with `--map`
// too, the match is played on that map of the game folder's (its models and sounds in the video).
// `--voices=<dir>` has each client speak `<dir>/<name>.wav` (red1, red2, blue1, blue2: any rate,
// mono or more) in place of its own synthetic voice; nothing it reads is kept. The match's pack
// carries a stand-in glyph sheet, so the words in the videos' corner are in a pack's own console
// font (`--font=charset`: the engine's charset in its place).
//
// What it shows: who talks when in each recording (the client's holds its own team, the server's
// everyone), three pictures of each video, and what went into each file. The recordings' counts
// are deterministic; the pictures, the files' bytes and the times are measured. The videos are
// attached beside the report.
//
// ge:summary Lab: demos with voice recorded on our server and client, then turned into videos
// ge:keywords lab se-demo-video demo-video demovideo qwdv mvdv voice team-voice video mp4 webm
// ge:keywords record multiview client-recording strip
// ge:skill shooter replay labs
#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <fstream>
#include <iterator>

#include "audio/pcm.hpp"
#include "brush/write.hpp"
#include "charset/charset.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/voice_codec.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_video.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/staged.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/commands.hpp"
#include "vfs/pak.hpp"
#include "vfs/wad.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
namespace gn = ge::net;
namespace sv = shooters_edge::server;
namespace ls = ge::lab::liquid_scene;

constexpr int kClients = 4;
constexpr int kTalk = 360;  // ticks each talker talks (13 ms each: 4.7 s)

// Client i (seed i + 1) talks from tick 150 + 400 * i for kTalk ticks; the last one listens.
bool talk(int tick, int seed) {
  const int from = 150 + 400 * (seed - 1);
  return seed < kClients && tick >= from && tick < from + kTalk;
}

std::string fixed(double v, int places) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

const char* const kNames[kClients] = {"red1", "red2", "blue1", "blue2"};

// `<dir>/<name>.wav` as 48 kHz mono, linearly resampled; empty when there is none.
std::vector<float> speech_of(const std::string& dir, const std::string& name) {
  if (dir.empty()) return {};
  std::ifstream in(dir + "/" + name + ".wav", std::ios::binary);
  if (!in) return {};
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                                   std::istreambuf_iterator<char>()};
  ge::audio::Pcm pcm;
  std::string error;
  if (!ge::audio::read_wav(bytes, name, &pcm, &error) || pcm.rate <= 0) return {};
  std::vector<float> out;
  const double step = static_cast<double>(pcm.rate) / gn::kVoiceRate;
  for (double at = 0; at + 1 < static_cast<double>(pcm.samples.size()); at += step) {
    const auto i = static_cast<size_t>(at);
    const double u = at - static_cast<double>(i);
    out.push_back(static_cast<float>(pcm.samples[i] * (1 - u) + pcm.samples[i + 1] * u));
  }
  return out;
}

struct Match {
  std::vector<uint8_t> bsp;  // the fixture halls' (empty: the game folder's map)
  std::vector<uint8_t> qwdv, mvdv;
  int slots[kClients] = {};
  int64_t relayed = 0, routed = 0;
};

// The match on `map_name` from `files` (none: the fixture halls), recorded. False with the reason.
bool play_match(int ticks, const cl::SearchPath* files, const std::string& map_name,
                const std::string& voices, Match& out, std::string& error) {
  std::unique_ptr<cl::LoadedMap> map;
  if (files) {
    map = cl::load_named_map(*files, map_name, error);
  } else {
    if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &out.bsp,
                              &error))
      return false;
    map = cl::map_from_bytes(out.bsp, "halls", error);
  }
  if (!map) return false;
  std::vector<float> speech[kClients];
  for (int i = 0; i < kClients; i++) speech[i] = speech_of(voices, kNames[i]);
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) return false;
  sv::QcServerOptions so;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
  if (!qc) return false;
  sv::server_command(*qc, "deathmatch 1");
  if (!sv::load_map(*qc, map->spec, error)) return false;
  sv::use_map_sets(*qc, map->bsp);
  gn::LinkConditions lan;
  gn::conditions_preset("lan", lan);
  gn::SimNetwork net = gn::make_sim_network(1, lan);
  gn::Transport st;
  gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
  auto server = std::make_unique<cn::ClassicServer>();
  cn::ServerConfig cfg;
  cfg.voice = true;
  if (!cn::classic_start(*server, *qc, st, cfg, error)) return false;
  cn::MultiviewRecorder mvd;
  cn::mvd_start(mvd, true);
  server->mvd = &mvd;
  gn::Transport ct[kClients];
  auto c = std::make_unique<cn::ClassicClientEnd[]>(kClients);
  for (int i = 0; i < kClients; i++) {
    gn::open_sim_transport(ct[i], net,
                           gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
    cn::ClientConfig cc;
    cc.name = kNames[i];
    if (!speech[i].empty()) cc.speech = &speech[i];
    cc.team = i < 2 ? "red" : "blue";
    cc.topcolor = cc.bottomcolor = i < 2 ? 4 : 13;
    cc.qport = static_cast<uint16_t>(100 + i);
    cc.seed = i + 1;
    cc.voice = true;
    cc.talk = &talk;
    cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
  }
  cn::ClientRecorder rec;
  cn::record_start(rec, true);
  c[0].record = &rec;
  int64_t now = 0;
  for (int f = 0; f < ticks; f++) {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(*server, now);
    for (int i = 0; i < kClients; i++) cn::client_tick(c[i], now);
  }
  cn::mvd_stop(mvd);
  for (int i = 0; i < kClients; i++) out.slots[i] = c[i].playernum;
  out.qwdv = std::move(rec.bytes);
  out.mvdv = std::move(mvd.bytes);
  out.relayed = server->counts.voice_in;
  out.routed = server->counts.voice_out;
  return true;
}

// Who is heard in a recording: each talker's voice messages a second.
struct Heard {
  std::set<int> talkers;
  int64_t messages = 0, frames = 0;
  std::vector<gl::Trace> traces;  // a trace a talker: frames a second over the demo's clock
  std::string error;
};

Heard heard_in(const std::vector<uint8_t>& bytes, const Match& m) {
  Heard h;
  cl::Demo d;
  if (!cl::open_demo(bytes, d, h.error)) return h;
  std::vector<std::vector<double>> per(kClients);
  double t0 = -1;
  while (cl::next_frame(d, h.error) == cl::DemoRead::frame) {
    h.frames++;
    if (t0 < 0) t0 = d.state.time;
    for (const cl::DemoVoice& v : d.state.voice) {
      h.messages++;
      h.talkers.insert(v.sender);
      std::vector<std::span<const uint8_t>> f;
      cl::voice_frames(v, f);
      for (int i = 0; i < kClients; i++) {
        if (m.slots[i] != v.sender) continue;
        const auto sec = static_cast<size_t>(d.state.time - t0);
        if (per[i].size() <= sec) per[i].resize(sec + 1, 0);
        per[i][sec] += static_cast<double>(f.size());
      }
    }
  }
  size_t seconds = 1;
  for (const std::vector<double>& p : per) seconds = std::max(seconds, p.size());
  for (int i = 0; i < kClients; i++) {
    gl::Trace t{std::string(kNames[i]) + " (slot " + std::to_string(m.slots[i]) + ")", {}, {}};
    per[i].resize(seconds, 0);
    for (size_t s = 0; s < seconds; s++) {
      t.x.push_back(static_cast<double>(s));
      t.y.push_back(per[i][s]);
    }
    h.traces.push_back(t);
  }
  return h;
}

void talk_chart(gl::Lab& lab, const std::string& id, const std::string& title,
                const std::string& caption, const std::string& look, const Heard& h) {
  gl::LineChart line;
  line.frame = {title, "voice frames (20 ms) a second in the recording, by talker", "t (s)",
                "frames a second"};
  line.traces = h.traces;
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({id, chart.title, caption, look, {}}, gl::to_picture(chart.plot));
}

// One recording turned into a video: three pictures and the report, the file attached.
// A stand-in for a pack's console font: the palette the world draws when a pack has none (a grey
// ramp) with one gold entry, and a gfx.wad whose 128 x 128 glyph sheet is the engine's 8 x 16
// pixel charset halved to 8 x 8 cells, white from 0 and gold from 128, index 0 see-through.
bool stand_in_font(std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries,
                   std::string& error) {
  constexpr uint8_t kGold = 254, kWhite = 255;
  std::vector<uint8_t> palette(768);
  for (int i = 0; i < 256; i++)
    palette[static_cast<size_t>(i) * 3] = palette[static_cast<size_t>(i) * 3 + 1] =
        palette[static_cast<size_t>(i) * 3 + 2] = static_cast<uint8_t>(i);
  palette[kGold * 3] = 0xf2;
  palette[kGold * 3 + 1] = 0xc0;
  palette[kGold * 3 + 2] = 0x60;
  const ge::charset::Atlas atlas = ge::charset::build_atlas(ge::charset::FontId::terminal, 128);
  std::vector<uint8_t> sheet(128 * 128, 0);
  for (int code = 33; code < 127; code++) {
    const ge::charset::CellRect c = ge::charset::cell_rect(128, static_cast<uint8_t>(code));
    for (int half = 0; half < 2; half++) {
      const int k = code + half * 128;
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
          const int ink =
              std::max(atlas.at(c.x + x, c.y + 2 * y), atlas.at(c.x + x, c.y + 2 * y + 1));
          if (ink >= 128)
            sheet[static_cast<size_t>(((k >> 4) * 8 + y) * 128 + (k & 15) * 8 + x)] =
                half == 0 ? kWhite : kGold;
        }
    }
  }
  std::vector<uint8_t> wad;
  if (!ge::vfs::write_wad({{"conchars", ge::vfs::kRawLump, sheet}}, &wad, &error)) return false;
  entries.push_back({"gfx/palette.lmp", std::move(palette)});
  entries.push_back({"gfx.wad", std::move(wad)});
  return true;
}

bool video(gl::Lab& lab, ge::gpu::Device* device, const cl::SearchPath& files,
           const std::string& demo, const std::string& tag) {
  const auto size = lab.arg_nums("size");
  cl::DemoVideoOptions o;
  o.demo = demo;
  o.width = size.size() > 0 ? static_cast<int>(size[0]) & ~1 : 640;
  o.height = size.size() > 1 ? static_cast<int>(size[1]) & ~1 : 360;
  o.fps = lab.arg_nums("fps").empty() ? 30 : lab.arg_nums("fps")[0];
  o.seconds = lab.arg_nums("seconds").empty() ? 0 : lab.arg_nums("seconds")[0];
  o.codec = lab.arg("codec");
  o.container = "mkv";
  if (!lab.arg_nums("game-volume").empty())
    o.volume = static_cast<float>(lab.arg_nums("game-volume")[0]);
  // The look: the video's default (modern, every classic-enhance switch on) or the classic.
  shooters_edge::look::Picture look;
  if (lab.arg("look") == "classic") look.look = "classic";
  o.picture = &look;
  std::vector<uint8_t> video_bytes;
  o.memory = &video_bytes;
  const std::string ids[3] = {tag + "-start", tag + "-middle", tag + "-end"};
  // The video's numbers are the GPU's to make: measured, under the same keys with or without one.
  const auto metrics = [&](const cl::DemoVideoReport& r) {
    lab.metric(tag + "-video.demo-frames", static_cast<double>(r.demo_frames), "count",
               gl::Det::measured);
    lab.metric(tag + "-video.voice-messages", static_cast<double>(r.voice_messages), "count",
               gl::Det::measured);
    lab.metric(tag + "-video.voice-frames", static_cast<double>(r.voice_frames), "count",
               gl::Det::measured);
    lab.metric(tag + "-video.pictures", static_cast<double>(r.frames), "count", gl::Det::measured);
    lab.metric(tag + "-video.bytes", static_cast<double>(r.bytes), "count", gl::Det::measured);
  };
  if (!device) {
    ls::placeholders(lab, {ids[0].c_str(), ids[1].c_str(), ids[2].c_str()});
    metrics(cl::DemoVideoReport{});
    lab.attach(tag + ".mkv", {}, gl::Det::measured);  // its name is in the digest either way
    return true;
  }
  // The pictures at a quarter, a half and the end of the video, taken as they go into the file.
  cl::DemoVideoReport r;
  std::string error;
  std::vector<std::pair<int64_t, gl::Picture>> every;
  const auto keep = [&](int64_t k, double, const gl::Picture& p) {
    if (k % 15 == 0) every.emplace_back(k, p);
  };
  if (!cl::demo_video(*device, files, o, r, error, keep)) {
    lab.fail("se-demo-video: " + demo + ": " + error, "Read the reason above. Skill: shooter.");
    return false;
  }
  for (int j = 0; j < 3 && !every.empty(); j++) {
    const size_t i = j == 0 ? every.size() / 4 : j == 1 ? every.size() / 2 : every.size() - 1;
    const auto& [k, p] = every[i];
    lab.shot({ids[j],
              tag + " PICTURE " + std::to_string(k) + " OF " + std::to_string(r.frames),
              "Picture " + std::to_string(k) + " (" + fixed(static_cast<double>(k) / o.fps, 1) +
                  " s) of " + r.found +
                  " turned into a video: the world's frame drawn offscreen "
                  "from the demo's eye, its entities, statics and HUD.",
              "the map from the recorded eye, the other players and items where the demo has "
              "them",
              {{"demo", demo}}},
             p, gl::Det::measured);
  }
  lab.note(tag + ": " + r.found + " (" + r.format + "): " + std::to_string(r.frames) +
           " pictures, " + fixed(r.seconds, 2) + " s, " + std::to_string(r.levels) + " levels, " +
           r.container + " " + r.codec + ", " + std::to_string(r.bytes) + " bytes; " +
           std::to_string(r.sounds) + " sounds, " + std::to_string(r.voice_frames) +
           " voice frames in " + std::to_string(r.voice_messages) + " messages, " +
           std::to_string(r.voice_concealed) + " concealed");
  metrics(r);
  lab.attach(tag + ".mkv", std::move(video_bytes), gl::Det::measured);
  return true;
}

void run(gl::Lab& lab) {
  std::string error;
  auto device = ge::gpu::create_device({.label = "se-demo-video", .validation = 0});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  if (device) lab.note("adapter: " + ge::gpu::adapter_line(*device));
  if (!gn::voice_codec_available())
    lab.note("this build has no Opus (GE_ENCODERS=OFF): the voices are the codec's stand-in");

  const std::string basedir = lab.arg("basedir"), map_name = lab.arg("map");
  cl::SearchPath files;
  if (!basedir.empty()) {
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
    if (map_name.empty()) {
      video(lab, device.get(), files, lab.arg("demo"), "demo");
      return;
    }
  }

  const int ticks =
      lab.arg_nums("ticks").empty() ? 1500 : static_cast<int>(lab.arg_nums("ticks")[0]);
  Match m;
  if (!play_match(ticks, map_name.empty() ? nullptr : &files, map_name, lab.arg("voices"), m,
                  error)) {
    lab.fail("se-demo-video: the match did not run: " + error, "Read the reason. Skill: net.");
    return;
  }
  lab.metric("match.voice-relayed", static_cast<double>(m.relayed), "count");
  lab.metric("match.voice-routed", static_cast<double>(m.routed), "count");
  const Heard q = heard_in(m.qwdv, m), v = heard_in(m.mvdv, m);
  if (!q.error.empty() || !v.error.empty()) {
    lab.fail("se-demo-video: a recording did not read back: " + q.error + v.error,
             "The recorder and the reader disagree; run tests/shooters_edge_classic_record_test. "
             "Skill: shooter.");
    return;
  }
  lab.metric("qwdv.talkers", static_cast<double>(q.talkers.size()), "count");
  lab.metric("qwdv.voice-messages", static_cast<double>(q.messages), "count");
  lab.metric("mvdv.talkers", static_cast<double>(v.talkers.size()), "count");
  lab.metric("mvdv.voice-messages", static_cast<double>(v.messages), "count");
  lab.metric("qwdv.file-bytes", static_cast<double>(m.qwdv.size()), "count");
  lab.metric("mvdv.file-bytes", static_cast<double>(m.mvdv.size()), "count");
  if (q.talkers != std::set<int>{m.slots[0], m.slots[1]})
    lab.fail("the client's .qwdv holds talkers other than its own team",
             "Check the server's voice_route and the recorder's own voice. Skill: net.");
  if (v.talkers != std::set<int>{m.slots[0], m.slots[1], m.slots[2]})
    lab.fail("the server's .mvdv does not hold every talker",
             "Check mvd_voice in classic_record.cpp. Skill: net.");
  talk_chart(lab, "qwdv-talk", "WHO IS HEARD IN RED 1'S CLIENT RECORDING (.QWDV)",
             "Voice frames a second in the client recording red 1 made: its own voice (spliced "
             "in as it spoke) and its team-mate red 2's, routed to it by the server. Blue 1 talks "
             "too, to its own team only, so it is not here.",
             "red 1 and red 2 each talk for about 5 s; blue 1's line stays at zero", q);
  talk_chart(lab, "mvdv-talk", "WHO IS HEARD IN THE SERVER'S MULTIVIEW RECORDING (.MVDV)",
             "Voice frames a second in the server's multiview recording: every talker, whichever "
             "team it was routed to.",
             "three talkers in turn, red 1, red 2, blue 1; blue 2 never talks", v);

  std::vector<uint8_t> pak;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries = {{"red1.qwdv", m.qwdv},
                                                                       {"match.mvdv", m.mvdv}};
  if (!m.bsp.empty()) entries.push_back({"maps/halls.bsp", m.bsp});
  // The words' type: a stand-in console font in the pack as a real pack carries its own
  // (client/demo_caption.hpp); without it (--font=charset) the engine's charset.
  if (lab.arg("font") == "packs" && !stand_in_font(entries, error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  if (!ge::vfs::write_pak(entries, &pak, &error) ||
      !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
    lab.fail(error, "Read the reason above. Skill: shooter.");
    return;
  }
  if (!shooters_edge::labs::stage_asked(lab, files)) return;
  lab.attach("red1.qwdv", m.qwdv, gl::Det::measured);
  lab.attach("match.mvdv", m.mvdv, gl::Det::measured);
  video(lab, device.get(), files, "red1.qwdv", "qwdv");
  video(lab, device.get(), files, "match.mvdv", "mvdv");
}

const gl::LabSpec kSpec{
    "se-demo-video",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/demo_video, net/classic_record (demos with voice, as video)",
    "shooter",
    "Does a recorded game, voices and all, become a video that shows and sounds as it was played?",
    {{"size", "640,360", "the video's width and height, px"},
     {"fps", "30", "the video's frame rate"},
     {"seconds", "0", "the most video a recording makes, s (0: all of it)"},
     {"codec", "auto", "the video's codec: auto, h264, hevc, av1, or empty for uncompressed"},
     {"ticks", "1500", "the match's length in 13 ms ticks"},
     {"basedir", "", "a folder holding a game folder: turn its demo into a video instead"},
     {"game", "id1", "the game folder under --basedir"},
     {"demo", "demo1", "the demo to turn into a video, by name or path"},
     {"game-volume", "0.5", "the game's sounds under the voices, 0..1 (0: the voices alone)"},
     {"map", "", "with --basedir: play the match on this map of the game folder's instead"},
     {"voices", "", "a folder of <name>.wav (red1, red2, blue1, blue2) the clients speak"},
     {"font", "packs", "the words' type: packs (a stand-in pack's glyph sheet) or charset"},
     {"look", "modern",
      "the picture: modern (the video's default, classic-enhance on) or classic"}},
    "--size=160,90 --seconds=2 --ticks=1000",
    run,
};
GE_LAB(kSpec);

}  // namespace
