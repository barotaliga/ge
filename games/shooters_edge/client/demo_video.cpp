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
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "audio/synth.hpp"
#include "audio/voice.hpp"
#include "camera/first_person.hpp"
#include "capture/av_export.hpp"
#include "classicenhance/cached.hpp"
#include "core/output.hpp"
#include "gpu/target.hpp"
#include "lab/gif.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/demo_caption.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/demo_sound.hpp"
#include "shooters_edge/client/demo_voice.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"

namespace shooters_edge::client {

namespace {

namespace se = shooters_edge::hud;

const char* format_name(DemoFormat f) {
  return f == DemoFormat::single      ? "single"
         : f == DemoFormat::client    ? "client"
         : f == DemoFormat::multiview ? "multiview"
                                      : "sequel";
}

// The entity the listener's own sounds come from: the view entity, or the recording player's.
int listener_of(const DemoState& s) {
  return s.format == DemoFormat::single ? s.view_entity : s.player + 1;
}

bool ends_with(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

// `bytes` into `path`. False with the reason.
bool write_file(const std::string& path, const std::vector<uint8_t>& bytes, std::string& error) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    error = "Can't write " + path + ". Check that the folder exists and is writable.";
    return false;
  }
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  if (std::fclose(f) != 0 || !ok) {
    error = "Writing " + path + " failed part way. Check that the disk has room.";
    return false;
  }
  return true;
}

// The words' type: classic-enhance's copy of the packs' glyph sheet in the packs' cache, else
// the packs' own; `from` says which ("charset" with neither: `glyphs` left empty).
void caption_type(const SearchPath& files, ge::ui::Image& glyphs, std::string& from) {
  if (!content_packs(files).empty()) {
    const std::filesystem::path cache = ge::classicenhance::find_cache(content_packs(files));
    // The enhanced sheet (upscaled, still 16 x 16 cells) first, then the cache's original.
    if (!cache.empty() &&
        (caption_glyphs_from_png(cache / "pictures" / "enhanced" / "conchars.png", &glyphs) ||
         caption_glyphs_from_png(cache / "pictures" / "conchars.png", &glyphs))) {
      from = "cache";
      return;
    }
  }
  const CaptionFind find = [&files](std::string_view p, std::vector<uint8_t>& store) {
    return find_file(files, p, store);
  };
  std::string why;
  from = caption_glyphs_from_packs(find, &glyphs, &why) ? "packs" : "charset";
}

}  // namespace

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
    p.alpha = e.alpha;
    p.scale = e.scale;
    for (int c = 0; c < 3; c++) p.colormod[c] = e.colormod[c];
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
      if (static_cast<int>(k) != s.player && s.players[k].present) {
        put(static_cast<int>(k) + 1, s.players[k]);
        if (!out.empty() && out.back().entity == static_cast<int>(k) + 1 && k < s.top.size()) {
          out.back().top = static_cast<uint32_t>(s.top[k]);
          out.back().bottom = static_cast<uint32_t>(s.bottom[k]);
        }
      }
  }
  for (size_t i = 0; i < s.statics.size(); i++) put(-1 - static_cast<int>(i), s.statics[i]);
}

void demo_fx_scene(const DemoState& s, const DemoFx& fx, const LoadedMap* map,
                   const ge::render::FrameView& view, FxScene& out) {
  out.map = map;
  out.server = nullptr;
  out.pose.eye = {view.eye[0], view.eye[1], view.eye[2]};
  out.pose.yaw = view.heading;
  out.pose.pitch = view.pitch;
  out.pose.fov_y = view.fov_y;
  out.missiles = fx.missiles;
  out.explosions = fx.explosions;
  out.effects = fx.effects;
  out.blows = {};
  out.campaign = s.format == DemoFormat::single;  // the single protocol's codes (13: a beam)
  out.effect_models = static_cast<int>(s.models.size());
  demo_placed(s, out.models);
  // The players: a client's or multiview's by slot, the single protocol's the first entities.
  const bool single = s.format == DemoFormat::single;
  const DemoEntity* players = single ? s.entities.data() + 1 : s.players.data();
  const size_t count = single ? std::min<size_t>(s.entities.size() > 0 ? s.entities.size() - 1 : 0,
                                                 static_cast<size_t>(s.max_clients))
                              : s.players.size();
  out.bodies.clear();
  for (size_t k = 0; k < count; k++)
    if (players[k].present && players[k].model > 0)
      out.bodies.push_back({static_cast<int>(k) + 1, players[k].origin, !players[k].dead});
  out.own = listener_of(s);
  const DemoEntity* own =
      out.own >= 1 && static_cast<size_t>(out.own) <= count ? &players[out.own - 1] : nullptr;
  out.me = PlayerState{};
  out.me.entity = out.own;
  out.me.origin = s.eye;
  out.me.health = static_cast<float>(s.stats[kStatHealth]);
  out.me.dead = out.me.health <= 0;
  out.me_effects = own ? own->effects : 0;
  out.waterlevel = out.watertype = 0;
  out.placed_of = [players, count, &s](int e) {
    Placed p;
    if (e < 1 || static_cast<size_t>(e) > count) return p;
    const DemoEntity& d = players[e - 1];
    if (d.model <= 0 || static_cast<size_t>(d.model) >= s.models.size()) return p;
    p.entity = e;
    p.modelindex = d.model;
    p.frame = d.frame;
    p.skin = d.skin;
    p.effects = d.effects;
    p.alpha = d.alpha;
    p.scale = d.scale;
    for (int c = 0; c < 3; c++) p.colormod[c] = d.colormod[c];
    p.origin = d.origin;
    p.angles = d.angles;
    return p;
  };
  out.health_of = [players, count](int e) {
    return e >= 1 && static_cast<size_t>(e) <= count && !players[e - 1].dead ? 1.0f : 0.0f;
  };
}

void demo_fx_step(const DemoState& s, DemoFx& fx, const LoadedMap* map,
                  const ge::render::FrameView& view, FxState& state, double t, double dt,
                  FxFrame& out) {
  thread_local FxScene scene;
  demo_fx_frame(s, fx);
  demo_fx_scene(s, fx, map, view, scene);
  step_fx(scene, state, t, dt, out);
}

bool demo_video(ge::gpu::Device& device, const SearchPath& files, const DemoVideoOptions& o,
                DemoVideoReport& report, std::string& error, const DemoVideoPicture& picture,
                const DemoVideoLerp& picture_lerp) {
  report = DemoVideoReport{};
  report.adapter = ge::gpu::adapter_line(device);
  if (o.width <= 0 || o.height <= 0 || o.width % 2 != 0 || o.height % 2 != 0 || !(o.fps > 0)) {
    char fps[32];
    std::snprintf(fps, sizeof fps, "%g", o.fps);
    error = "the video's size must be even on both sides and its frame rate over 0 (asked " +
            std::to_string(o.width) + "x" + std::to_string(o.height) + " at " + fps + " fps)";
    return false;
  }
  Timedemo t;
  if (!timedemo_start(files, o.demo, t, error)) return false;
  t.demo.track = o.track;
  t.start.track = o.track;
  t.smooth = o.lerp;
  report.found = t.found;
  report.format =
      std::string(format_name(t.demo.state.format)) + (t.demo.state.voice_format ? " + voice" : "");
  const bool gif = o.gif || ends_with(o.file, ".gif");
  if (gif && !(o.gif_width >= 16)) {
    error = "a GIF's width must be 16 px or more (asked " + std::to_string(o.gif_width) + ")";
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
  // The status bar as the window draws it with a classic game loaded: the packs' own pictures
  // (enhanced when ce_pictures is on and the cache has them), else the free art.
  se::HudArt art;
  if (o.hud) art = load_classic_pictures(files, look.ce.pictures).hud;
  report.hud_art = art.bar.empty() ? "free" : "packs";
  if (art.bar.empty()) art = se::free_art();
  art.finish();
  ge::ui::Image glyphs;
  if (o.text || o.messages) caption_type(files, glyphs, report.text_font);
  DemoNotices notices;  // the prints and the centre print the recorded player read
  const auto w = static_cast<uint32_t>(o.width), h = static_cast<uint32_t>(o.height);
  ge::gpu::Target out = ge::gpu::create_target(device, "demo-video.frame", w, h);
  std::unique_ptr<LoadedMap> map;
  WorldDrawPtr world;
  size_t world_models = 0;  // the model list's length the world was made with
  FxFrame fx;
  FxState fx_state;  // the demo's effects as they live (world_fx.hpp)
  DemoFx demo_fx;    // its temporary entities this level (demo_fx.hpp)
  fx_state.on = fx_switches(look);
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> pal = find_file(files, "gfx/palette.lmp", store);
    fx_state.palette.assign(pal.begin(), pal.end());
  }
  ge::particles::Book book;  // the game's files' effects (particles/*.fx)
  {
    std::vector<std::string> faults;
    load_effects(files, book, faults);
  }
  fx_state.book = &book;
  const DemoState& read = t.demo.state;  // the last frame read (what is heard)

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
      map = load_named_map(files, read.map, error);
      if (!map) {
        error = "the demo's map " + read.map + ": " + error;
        return false;
      }
      world.reset();
      last = -1;
      demo_fx = {};
      const std::vector<uint8_t> palette = std::move(fx_state.palette);
      fx_state = {};
      fx_state.on = fx_switches(look);
      fx_state.palette = palette;
      fx_state.book = &book;
    }
    demo_fx_frame(read, demo_fx);
    const double dt = last < 0 ? 0 : std::clamp(read.time - last, 0.0, kMostDemoGap);
    last = read.time;
    vt += dt;
    if (o.messages) notices.take(read, vt);
    if (o.sound && !gif) {
      sounds.level(read, map ? &map->bsp.map : nullptr);
      if (vt >= from) {  // the clip's sounds, on the clip's own clock
        exp.sound_to(std::llround((vt - from) * ge::audio::kRate));
        sounds.frame(read, listener_of(read), static_cast<float>(dt));
        for (const DemoVoice& v : read.voice) voices.play(v);
      }
    }
    return true;
  };

  if (o.autotrack) {  // the hottest player's cuts, worked out over the whole demo first
    DemoScan scan;
    if (!scan_demo(t.start, scan, error)) return false;
    if (!autotrack_plan(t.start, scan, o.autotrack_set, report.autotrack, error)) return false;
  }
  const auto follow = [&] {
    if (!o.autotrack) return;
    const int slot = autotrack_at(report.autotrack, vt);
    if (slot >= 0) t.demo.track = slot;
  };
  if (o.highlight > 0) from = 1e300;  // nothing heard until the highlight is known
  if (!step()) return false;
  if (o.highlight > 0) {  // the clip is a highlight's window
    DemoScan scan;
    if (!scan_demo(t.start, scan, error)) return false;
    const int subject = read.format == DemoFormat::single ? -1 : read.player;
    const ge::replay::Highlights found = demo_highlights(scan, subject);
    if (static_cast<size_t>(o.highlight) > found.list.size()) {
      error = "the demo has " + std::to_string(found.list.size()) + " highlights for " +
              player_name(scan.names, subject) + ", not " + std::to_string(o.highlight) +
              " (demo_highlights lists them while it plays)";
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
    while (!ended && vt < at) {
      follow();
      if (!step()) return false;
    }
    if (ended) break;
    // The picture at `at` is drawn between the last two frames read (demo_lerp.hpp): the
    // timeline runs `vt - at` ahead of it, and in a level the demo's clock with it.
    const DemoState& s = o.lerp ? lerp_at(t.lerp, read, read.time - (vt - at)) : read;
    if (picture_lerp) picture_lerp(t.lerp);
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
                      se::hud_style(art));
    }
    if (o.text || o.messages) {
      const std::string title =
          map->bsp.map.entities.empty() ? "" : map->bsp.map.entities[0].get("message");
      draw_caption(overlay, glyphs, o.width, o.height,
                   o.text ? demo_caption(s, title, at) : std::vector<CaptionLine>{},
                   o.messages ? notices.at(at) : std::vector<CaptionLine>{});
      if (o.messages && at < notices.centre_until)
        draw_centre(overlay, glyphs, o.width, o.height, notices.centre);
    }
    if (k == 0) {  // what went off before the clip's start is not thrown at its first frame
      fx_state.explosions_seen = demo_fx.explosions.size();
      fx_state.effects_seen = demo_fx.effects.size();
    }
    fx_state.models = &client::world_models(*world);
    thread_local FxScene scene;
    demo_fx_scene(s, demo_fx, map.get(), view, scene);
    step_fx(scene, fx_state, at, 1.0 / o.fps, fx);
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
              std::to_string(vt) + " s long); pick a start inside the demo";
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

std::vector<std::string> demo_video_lines(const DemoVideoOptions& o, const DemoVideoReport& r) {
  const auto num = [](double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    return std::string(b);
  };
  const auto clock = [](double s) {
    const auto t = static_cast<long long>(std::floor(std::max(0.0, s) + 1e-3));
    char b[32];
    std::snprintf(b, sizeof b, "%lld:%02lld", t / 60, t % 60);
    return std::string(b);
  };
  std::vector<std::string> out;
  out.push_back("demovideo: " + r.found + " (" + r.format + ") -> " +
                (o.file.empty() ? std::string("memory") : o.file));
  if (!r.highlight.empty() || r.from_s > 0)
    out.push_back("demovideo: the clip " + clock(r.from_s) + " to " + clock(r.to_s) +
                  (r.highlight.empty() ? "" : ": " + r.highlight));
  if (o.autotrack)
    out.push_back("demovideo: autotrack: " + std::to_string(r.autotrack.cuts.size()) +
                  " cuts to the hottest player over " + clock(r.autotrack.length_s));
  out.push_back("demovideo: " + std::to_string(r.frames) + " pictures, " + num(r.seconds) + " s, " +
                std::to_string(r.levels) + " levels, " + std::to_string(o.width) + "x" +
                std::to_string(o.height) + " at " + num(o.fps) + " fps, " + r.container + " " +
                r.codec + ", " + std::to_string(r.bytes) + " bytes, on " + r.adapter);
  out.push_back("demovideo: " + std::to_string(r.sounds) + " sounds, " + std::to_string(r.loops) +
                " loops, " + std::to_string(r.missing_sounds) + " names no pack has; voice " +
                std::to_string(r.voice_frames) + " frames in " + std::to_string(r.voice_messages) +
                " messages, " + std::to_string(r.voice_concealed) + " concealed");
  const std::string words = r.text_font == "cache"   ? "the packs' console font, enhanced"
                            : r.text_font == "packs" ? "the packs' console font"
                            : r.text_font.empty()    ? ""
                                                     : "the engine's charset";
  out.push_back("demovideo: the words " + (words.empty() ? "off" : "in " + words) +
                ", the status bar " +
                (o.hud ? (r.hud_art == "packs" ? "in the packs' pictures" : "in the free pictures")
                       : std::string("off")));
  return out;
}

namespace out = ge::core::output;

int demo_video_main(ge::console::Console& console, const Args& args, const SearchPath& files,
                    const shooters_edge::look::Picture& picture, const DemoPlayback& playback) {
  const VideoSettings& video = playback.video;
  // A video draws in the newer look, then video.cfg, then the + words again (video_cfg.hpp).
  std::error_code ec;
  const std::filesystem::path cfg = apply_video_cfg(
      console, {std::filesystem::path(files.folder), std::filesystem::current_path(ec)}, args.plus);
  out::status("shooters_edge: -demovideo in r_look " + picture.look + ", " +
              (cfg.empty() ? std::string("video.cfg's built-in defaults (no video.cfg in the game "
                                         "folder or here)")
                           : "video.cfg from " + cfg.string()));
  auto device = ge::gpu::create_device({.label = "demovideo", .validation = 0});
  if (!device) {
    out::error("shooters_edge: no GPU device for -demovideo; see docs/rendering.md");
    return 1;
  }
  DemoVideoOptions o;
  o.demo = args.demovideo;
  o.file = args.video;
  // The flags given win; video.cfg's cvars (video_cfg.hpp) fill the rest.
  int width = args.width, height = args.height;
  if (!args.size_given && !size_named(video.size, width, height)) {
    out::error("shooters_edge: demovideo_size \"" + video.size +
               "\" is not WxH or 480p, 720p, 1080p, 1440p, 4k; next: fix it in video.cfg");
    return 2;
  }
  const std::string codec = args.codec_given ? args.codec : video.codec;
  o.width = width & ~1;
  o.height = height & ~1;
  o.fps = args.fps_given ? args.fps : video.fps;
  o.codec = codec == "none" ? "" : codec;
  o.hud = video.hud;
  o.text = video.text;
  o.messages = video.messages;
  o.seconds = args.to > args.from ? args.to - args.from : args.seconds;
  o.from_s = args.from;
  o.highlight = args.highlight;
  o.gif_width = args.gif_width;
  o.track = playback.track;
  o.lerp = playback.lerp != 0;
  o.autotrack = args.autotrack;
  o.picture = &picture;
  DemoVideoReport r;
  std::string error;
  const auto shown = [](int64_t frame, double t, const ge::lab::Picture&) {
    if (frame % 300 == 0)
      out::status("demovideo: picture " + std::to_string(frame) + " at " +
                  std::to_string(static_cast<int>(t)) + " s");
  };
  if (!demo_video(*device, files, o, r, error, shown)) {
    out::error("shooters_edge: -demovideo " + args.demovideo + ": " + error +
               "; see docs/shooters-edge.md § A demo as a video");
    return 2;
  }
  for (const std::string& line : demo_video_lines(o, r)) out::status(line);
  return 0;
}

}  // namespace shooters_edge::client
