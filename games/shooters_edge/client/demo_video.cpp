// A recorded game turned into a video (demo_video.hpp).
//
// ge:summary demo_video's run: the frame clock over the demo, the world drawn, the sound rendered
// ge:keywords demo-video export frame-clock placed statics readback sound voice clip gif
// ge:skill shooter replay render
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_video.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string_view>
#include <vector>

#include "audio/synth.hpp"
#include "audio/voice.hpp"
#include "camera/first_person.hpp"
#include "capture/av_export.hpp"
#include "gpu/target.hpp"
#include "lab/gif.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/demo_sound.hpp"
#include "shooters_edge/client/demo_voice.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/hud/layout.hpp"

namespace shooters_edge::client {

namespace {

namespace se = shooters_edge::hud;

const char* format_name(DemoFormat f) {
  return f == DemoFormat::single ? "single" : f == DemoFormat::client ? "client" : "multiview";
}

// The entity the listener's own sounds come from: the view entity, or the recording player's.
int listener_of(const DemoState& s) {
  return s.format == DemoFormat::single ? s.view_entity : s.player + 1;
}

// What the demo has standing in the world this frame: its entities with a model (not the eye's
// own), the other players, the level's statics.
void demo_placed(const DemoState& s, std::vector<Placed>& out) {
  out.clear();
  const int models = static_cast<int>(s.models.size());
  const int me = listener_of(s);
  const auto put = [&](int entity, const DemoEntity& e) {
    if (e.model <= 0 || e.model >= models) return;
    Placed p;
    p.entity = entity;
    p.modelindex = e.model;
    p.frame = e.frame;
    p.skin = e.skin;
    p.effects = e.effects;
    p.origin = e.origin;
    p.angles = e.angles;
    out.push_back(p);
  };
  for (size_t n = 1; n < s.entities.size(); n++) {
    if (static_cast<int>(n) == me || !s.entities[n].present) continue;
    if (s.format != DemoFormat::single && n <= s.players.size()) continue;  // players: below
    put(static_cast<int>(n), s.entities[n]);
  }
  if (s.format != DemoFormat::single) {
    for (size_t k = 0; k < s.players.size(); k++)
      if (static_cast<int>(k) != s.player && s.players[k].present)
        put(static_cast<int>(k) + 1, s.players[k]);
  }
  for (size_t i = 0; i < s.statics.size(); i++) put(-1 - static_cast<int>(i), s.statics[i]);
}

bool ends_with(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

// `bytes` into `path`. False with the reason.
bool write_file(const std::string& path, const std::vector<uint8_t>& bytes, std::string& error) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    error = "cannot write " + path + "; next: check the folder exists and is writable";
    return false;
  }
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  if (std::fclose(f) != 0 || !ok) {
    error = "writing " + path + " failed part way; next: check the disk has room";
    return false;
  }
  return true;
}

}  // namespace

bool demo_video(ge::gpu::Device& device, const SearchPath& files, const DemoVideoOptions& o,
                DemoVideoReport& report, std::string& error, const DemoVideoPicture& picture) {
  report = DemoVideoReport{};
  report.adapter = ge::gpu::adapter_line(device);
  if (o.width <= 0 || o.height <= 0 || o.width % 2 != 0 || o.height % 2 != 0 || !(o.fps > 0)) {
    char fps[32];
    std::snprintf(fps, sizeof fps, "%g", o.fps);
    error = "the video's size must be even on both sides and its frame rate over 0 (asked " +
            std::to_string(o.width) + "x" + std::to_string(o.height) + " at " + fps +
            " fps); next: pass --size=WxH with even sides and --fps=N, N over 0";
    return false;
  }
  Timedemo t;
  if (!timedemo_start(files, o.demo, t, error)) return false;
  t.demo.track = o.track;
  t.start.track = o.track;
  report.found = t.found;
  report.format =
      std::string(format_name(t.demo.state.format)) + (t.demo.state.voice_format ? " + voice" : "");
  const bool gif = o.gif || ends_with(o.file, ".gif");
  if (gif && !(o.gif_width >= 16)) {
    error = "a GIF's width must be 16 px or more (asked " + std::to_string(o.gif_width) +
            "); next: pass --gif-width=N";
    return false;
  }

  ge::audio::Synth synth(1);
  ge::capture::AvExportOptions eo;
  eo.file = o.file;
  eo.container = o.container;
  eo.width = o.width;
  eo.height = o.height;
  eo.fps = o.fps;
  eo.codec = o.codec;
  eo.crf = o.crf;
  eo.synth = o.sound ? &synth : nullptr;
  ge::capture::AvExport exp;
  if (gif) {
    report.container = "gif";
    report.codec = "gif";
  } else {
    if (!exp.open(eo, error)) return false;
    report.container = std::string(exp.doc_type());
    report.codec = std::string(exp.video_codec());
  }
  std::vector<ge::lab::Picture> gif_frames;

  DemoSounds sounds(synth, files, o.volume);
  DemoVoices voices(synth, o.voice_volume);
  const look::Picture look_default;
  const look::Picture& look = o.picture ? *o.picture : look_default;
  se::Hud hud = se::make_hud();
  se::HudArt art = se::free_art();
  art.finish();
  const auto w = static_cast<uint32_t>(o.width), h = static_cast<uint32_t>(o.height);
  ge::gpu::Target out = ge::gpu::create_target(device, "demo-video.frame", w, h);
  std::unique_ptr<LoadedMap> map;
  WorldDrawPtr world;
  size_t world_models = 0;  // the model list's length the world was made with
  FxFrame fx;
  const DemoState& s = t.demo.state;

  double from = std::max(o.from_s, 0.0);  // the clip's start on the demo's timeline, s
  double length = o.seconds;              // its length (0: to the end)
  double vt = 0;                          // the demo's timeline at the last demo frame read, s
  double last = -1;                       // that frame's own time (-1: a level's first)
  bool ended = false;
  // The next demo frame: the clock moved on, its sounds and voices played on their sample.
  const auto step = [&]() -> bool {
    const TimedemoStep st = timedemo_step(t, error);
    if (st == TimedemoStep::error) return false;
    if (st == TimedemoStep::end) {
      ended = true;
      return true;
    }
    report.demo_frames++;
    if (st == TimedemoStep::new_level) {
      report.levels++;
      map = load_named_map(files, s.map, error);
      if (!map) {
        error = "the demo's map " + s.map + ": " + error;
        return false;
      }
      world.reset();
      last = -1;
    }
    const double dt = last < 0 ? 0 : std::clamp(s.time - last, 0.0, kMostDemoGap);
    last = s.time;
    vt += dt;
    if (o.sound && !gif) {
      sounds.level(s, map ? &map->bsp.map : nullptr);
      if (vt >= from) {  // the clip's sounds, on the clip's own clock
        exp.sound_to(std::llround((vt - from) * ge::audio::kRate));
        sounds.frame(s, listener_of(s), static_cast<float>(dt));
        for (const DemoVoice& v : s.voice) voices.play(v);
      }
    }
    return true;
  };

  if (o.highlight > 0) from = 1e300;  // nothing heard until the highlight is known
  if (!step()) return false;
  if (o.highlight > 0) {  // the clip is a highlight's window
    DemoScan scan;
    if (!scan_demo(t.start, scan, error)) return false;
    const int subject = s.format == DemoFormat::single ? -1 : s.player;
    const ge::replay::Highlights found = demo_highlights(scan, subject);
    if (static_cast<size_t>(o.highlight) > found.list.size()) {
      error = "the demo has " + std::to_string(found.list.size()) + " highlights for " +
              player_name(scan.names, subject) + ", not " + std::to_string(o.highlight) +
              "; next: list them with `demo_highlights` while it plays, or pick another --track";
      return false;
    }
    const ge::replay::Highlight& pick = found.list[static_cast<size_t>(o.highlight - 1)];
    from = pick.from_s;
    length = pick.to_s - pick.from_s;
    report.highlight = pick.text;
  }
  report.from_s = from;
  for (int64_t k = 0;; k++) {
    const double at = from + static_cast<double>(k) / o.fps;
    if (length > 0 && at >= from + length) break;
    while (!ended && vt < at)
      if (!step()) return false;
    if (ended) break;
    // The model list grows through a client recording's signon: the world is made again with it.
    if (map && (!world || s.models.size() != world_models)) {
      world.reset();
      world = create_world_draw(device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm, look,
                                {}, error, {s.models, {}});
      if (!world) return false;
      world_models = s.models.size();
    }
    if (!world) continue;
    ge::render::FrameView view = demo_view(s, ge::camera::FirstPersonRow{}.fov_y, at);
    view.frame = static_cast<uint64_t>(k);
    ge::ui::DrawList overlay;
    overlay.begin(o.width, o.height);
    if (o.hud && !s.intermission) {
      se::fill_hud(hud, demo_status(s, at), art);
      hud.layout.draw(overlay, hud.layout.frame(o.width, o.height, se::hud_style()),
                      se::hud_style());
    }
    demo_placed(s, fx.models);
    draw_world(device, *world, view, fx, nullptr, 1.0 / o.fps, overlay, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(device, out);
    const ge::lab::Picture p{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
    if (gif)
      gif_frames.push_back(ge::lab::shrink(p, o.gif_width));
    else if (!exp.frame(p, error))
      return false;
    if (picture) picture(report.frames, at, p);
    report.frames++;
  }
  report.seconds = static_cast<double>(report.frames) / o.fps;
  report.to_s = from + report.seconds;
  if (gif) {
    if (gif_frames.empty()) {
      error = "the clip from " + std::to_string(from) + " s has no pictures (the demo is " +
              std::to_string(vt) + " s); next: pick a start inside the demo";
      return false;
    }
    const int delay_cs = std::max(2, static_cast<int>(std::lround(100.0 / o.fps)));
    std::vector<uint8_t> bytes = ge::lab::encode_gif_frames(gif_frames, delay_cs);
    report.bytes = static_cast<int64_t>(bytes.size());
    if (!o.file.empty()) {
      if (!write_file(o.file, bytes, error)) return false;
    } else if (o.memory) {
      *o.memory = std::move(bytes);
    }
  } else {
    if (!exp.close(error)) return false;
    if (o.file.empty() && o.memory) *o.memory = exp.memory();
    report.bytes = exp.bytes();
  }
  report.sounds = sounds.stats().started;
  report.missing_sounds = sounds.stats().missing;
  report.loops = sounds.stats().loops;
  report.voice_messages = voices.stats().messages;
  report.voice_frames = voices.stats().frames;
  report.voice_concealed = voices.stats().concealed;
  report.talkers = voices.stats().talkers;
  return true;
}

}  // namespace shooters_edge::client
