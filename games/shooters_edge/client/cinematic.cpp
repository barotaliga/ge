// Quake II's cinematics in the window (cinematic.hpp).
//
// ge:summary The cinematic chain played: files opened, frames on the clock, the skip, the sound
// ge:keywords cinematic cin movie victory skip sound fit
// ge:skill classic ui
// ge:test tests/shooters_edge_cinematic_test.cpp
#include "shooters_edge/client/cinematic.hpp"

#include <algorithm>

#include "assets/pcx.hpp"

namespace shooters_edge::client {
namespace {

namespace cc = ge::classic::content;

void to_picture(ge::ui::Image& img, int width, int height, std::span<const uint8_t> pixels,
                std::span<const uint8_t> palette) {
  img.width = width;
  img.height = height;
  img.rgba.resize(size_t(width) * size_t(height) * 4);
  for (size_t i = 0; i < pixels.size() && i * 4 < img.rgba.size(); i++) {
    const size_t p = size_t(pixels[i]) * 3;
    img.rgba[i * 4 + 0] = palette[p];
    img.rgba[i * 4 + 1] = palette[p + 1];
    img.rgba[i * 4 + 2] = palette[p + 2];
    img.rgba[i * 4 + 3] = 255;
  }
  img.revision++;
}

void silence(Cinematic& c, ge::audio::Synth& synth) {
  if (c.left_sound) {
    synth.sample_stop(c.left_id);
    synth.sample_stop(c.right_id);
    c.retired.push_back(std::move(c.left_sound));
    c.retired.push_back(std::move(c.right_sound));
  }
  std::erase_if(c.retired,
                [&](const std::unique_ptr<ge::audio::Pcm>& p) { return !synth.reads(*p); });
}

// The file shown now put away.
void close(Cinematic& c, ge::audio::Synth& synth) {
  silence(c, synth);
  c.showing.clear();
  c.store.clear();
  c.bytes = {};
  c.still = false;
  c.shown = -1;
  c.picture = {};
}

// The chain's next file opened; false when it cannot be (said why) and is passed over.
bool open_next(Cinematic& c, const SearchPath& files, ge::audio::Synth& synth, double t) {
  const size_t plus = c.left.find('+');
  const std::string name = c.left.substr(0, plus);
  c.left = plus == std::string::npos ? std::string() : c.left.substr(plus + 1);
  const bool still = name.size() > 4 && name.ends_with(".pcx");
  const std::string path = (still ? "pics/" : "video/") + name;
  c.bytes = find_file(files, path, c.store);
  if (c.bytes.empty()) {
    c.said.push_back(path + " is not in the game's files; it is passed over.");
    return false;
  }
  std::string error;
  if (still) {
    ge::assets::IndexedImage img;
    if (!ge::assets::decode_pcx(c.bytes, path, &img, &error)) {
      c.said.push_back(error);
      return false;
    }
    to_picture(c.picture, img.width, img.height, img.indices, img.palette);
    c.showing = name;
    c.still = true;
    return true;
  }
  if (!cc::open_cin(c.bytes, path, &c.cin, &error)) {
    c.said.push_back(error);
    return false;
  }
  c.showing = name;
  if (c.cin.rate > 0) {
    c.left_sound = std::make_unique<ge::audio::Pcm>();
    c.right_sound = std::make_unique<ge::audio::Pcm>();
    cc::cin_sound(c.cin, &c.left_sound->samples, &c.right_sound->samples);
    c.left_sound->rate = c.right_sound->rate = c.cin.rate;
    ge::audio::SamplePlay p;
    p.pan = -1;
    c.left_id = synth.sample(*c.left_sound, p);
    p.pan = 1;
    c.right_id = synth.sample(*c.right_sound, p);
  }
  c.started = t;
  return true;
}

}  // namespace

void cinematic_start(Cinematic& c, const std::string& files) {
  c.left = c.left.empty() ? files : c.left + "+" + files;
}

bool cinematic_frame(Cinematic& c, const SearchPath& files, ge::audio::Synth& synth, bool held,
                     bool paused, bool map_asked, double t) {
  if (c.still && map_asked) close(c, synth);
  while (c.showing.empty() && !c.left.empty()) open_next(c, files, synth, t);
  if (c.showing.empty()) {
    silence(c, synth);
    return false;
  }
  if (c.still) return true;  // the game's last picture: until the menu starts a game
  if (paused) {
    c.started = t - double(std::max<int64_t>(c.shown, 0)) / cc::kCinFps;  // the clock held
    return true;
  }
  if (held && t - c.started > kCinematicSkipAfter) {
    close(c, synth);
    return cinematic_frame(c, files, synth, false, false, false, t);
  }
  const int64_t want = int64_t((t - c.started) * cc::kCinFps);
  while (c.shown < want) {  // one frame a window frame at most behind: slower, never dropped
    std::string error;
    const cc::CinStep step = cc::next_cin_frame(c.cin, &c.frame, &error);
    if (step != cc::CinStep::frame) {
      if (step == cc::CinStep::fault) c.said.push_back(c.showing + ": " + error);
      close(c, synth);
      return cinematic_frame(c, files, synth, false, false, false, t);
    }
    c.shown++;
    if (c.shown < want) continue;
    to_picture(c.picture, c.cin.width, c.cin.height, c.frame.pixels, c.frame.palette);
  }
  return true;
}

void draw_cinematic(ge::ui::DrawList& list, const Cinematic& c, int width, int height) {
  if (c.showing.empty()) return;
  list.rect({0, 0, float(width), float(height)}, {0, 0, 0, 1});
  if (c.picture.empty()) return;
  const float scale =
      std::min(float(width) / float(c.picture.width), float(height) / float(c.picture.height));
  const float w = float(c.picture.width) * scale, h = float(c.picture.height) * scale;
  ge::ui::Box b;
  b.rect = {(float(width) - w) / 2, (float(height) - h) / 2, w, h};
  b.image = &c.picture;
  b.uv = {0, 0, float(c.picture.width), float(c.picture.height)};
  b.feather = 0;
  list.box(b);
}

}  // namespace shooters_edge::client
