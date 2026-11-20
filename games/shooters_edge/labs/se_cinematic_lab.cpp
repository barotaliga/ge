// LAB se-cinematic: Quake II's cinematics as the window shows them (client/cinematic.hpp). A
// movie written here by the fixture writer (a lit ball crossing a sky, 3 s with a tone) plays
// through the window's player on the wall clock; its frames are drawn by the CPU raster at a wide
// and a tall screen (fitted, its shape kept, on black), and the game's last picture after it. With
// `--game=<basedir>/baseq2` the player's own `pics/victory.pcx` is drawn instead of the stand-in
// (a private page only: the picture is never committed).
//
// ge:summary Lab: Quake II's cinematics in the window: a fixture movie's frames, the last picture
// ge:keywords lab se-cinematic cinematic cin movie victory pcx fit pillarbox screenshot
// ge:skill classic labs
// ge:test tests/shooters_edge_cinematic_test.cpp
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "audio/synth.hpp"
#include "classic/content/cin.hpp"
#include "classicenhance/profile.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/cinematic.hpp"
#include "ui/raster.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace cc = ge::classic::content;
namespace ui = ge::ui;

constexpr int kW = 160, kH = 120, kFrames = 42;

// The stand-in movie: a sky graded top to bottom (indices 16..79) and a ball (index 200..207)
// crossing it, the palette a dusk; a 440 Hz tone at 22050 Hz mono.
std::vector<uint8_t> movie() {
  std::vector<std::vector<uint8_t>> frames;
  for (int k = 0; k < kFrames; k++) {
    std::vector<uint8_t> f(size_t(kW * kH));
    const int bx = 10 + k * (kW - 20) / kFrames, by = kH / 2 + (k % 14) - 7;
    for (int y = 0; y < kH; y++)
      for (int x = 0; x < kW; x++) {
        const int dx = x - bx, dy = y - by;
        const int d2 = dx * dx + dy * dy;
        f[size_t(y) * size_t(kW) + size_t(x)] =
            d2 < 100 ? uint8_t(200 + d2 / 13) : uint8_t(16 + y * 64 / kH);
      }
    frames.push_back(std::move(f));
  }
  std::vector<uint8_t> pal(768, 0);
  for (int i = 16; i < 80; i++) {
    const int u = i - 16;
    pal[size_t(i) * 3] = uint8_t(30 + u * 3);
    pal[size_t(i) * 3 + 1] = uint8_t(20 + u);
    pal[size_t(i) * 3 + 2] = uint8_t(90 - u);
  }
  for (int i = 200; i < 208; i++) {
    pal[size_t(i) * 3] = 255;
    pal[size_t(i) * 3 + 1] = uint8_t(240 - (i - 200) * 12);
    pal[size_t(i) * 3 + 2] = uint8_t(160 - (i - 200) * 16);
  }
  std::vector<int16_t> tone(size_t(22050) * kFrames / 14);
  int32_t phase = 0;
  for (int16_t& s : tone) {  // a triangle wave of 440 Hz, a quarter of full scale
    phase = (phase + 440 * 4 * 8192 / 22050 * 4) % (4 * 8192 * 4);
    const int32_t p = phase / 4;
    s = int16_t(p < 16384 ? p - 8192 : 24576 - p);
  }
  return cc::write_cin(kW, kH, frames, pal, 22050, 1, tone);
}

std::vector<uint8_t> stand_in_picture() {
  ge::assets::IndexedImage img;
  img.width = 320;
  img.height = 240;
  img.indices.resize(size_t{320} * 240);
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++)
      img.indices[size_t(y) * 320 + size_t(x)] = uint8_t((x / 40 + y / 40) % 2 == 0 ? 1 : 2);
  img.palette.assign(768, 0);
  img.palette[3] = 40, img.palette[4] = 40, img.palette[5] = 60;
  img.palette[6] = 200, img.palette[7] = 170, img.palette[8] = 90;
  return ge::assets::encode_pcx(img);
}

ge::lab::Picture draw(const cl::Cinematic& c, int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  cl::draw_cinematic(dl, c, w, h);
  ui::Image img = ui::make_image(w, h, ui::hex(0x804080));  // the world behind, never seen
  ui::render(dl, img);
  ge::lab::Picture pic;
  pic.width = img.width, pic.height = img.height, pic.rgba = std::move(img.rgba);
  return pic;
}

void run(ge::lab::Lab& lab) {
  std::string error;
  cl::SearchPath files;
  const std::string folder = lab.arg("game");
  if (!folder.empty()) {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    const ge::classicenhance::Profile* q2 = ge::classicenhance::find_profile("q2");
    if (q2 == nullptr || !cl::open_game_dir(base, game, files, error, *q2)) {
      lab.fail("se-cinematic: " + error, "Pass --game=<basedir>/baseq2. Skill: classic.");
      return;
    }
  }
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries = {{"video/lab.cin", movie()}};
  if (folder.empty()) entries.emplace_back("pics/victory.pcx", stand_in_picture());
  std::vector<uint8_t> pak;
  if (!ge::vfs::write_pak(entries, &pak, &error) ||
      !cl::add_archive(files, "lab.pak", std::move(pak), error)) {
    lab.fail("se-cinematic: the lab's pack: " + error, "Fix the fixture writer. Skill: classic.");
    return;
  }
  ge::audio::Synth synth;
  cl::Cinematic c;
  cl::cinematic_start(c, "lab.cin+victory.pcx");
  const struct {
    double t;
    const char* id;
    const char* title;
  } moments[] = {{0.0, "start", "THE MOVIE'S FIRST FRAME"},
                 {1.5, "middle", "THE MOVIE AT 1.5 S: FRAME 21"},
                 {3.5, "victory", "THE GAME'S LAST PICTURE, AFTER THE MOVIE"}};
  int64_t sound = c.left_sound ? int64_t(c.left_sound->samples.size()) : 0;
  for (const auto& m : moments) {
    cl::cinematic_frame(c, files, synth, false, false, false, 100.0 + m.t);
    if (c.left_sound) sound = int64_t(c.left_sound->samples.size());
    for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
      const std::string size = std::to_string(w) + "x" + std::to_string(h);
      lab.shot({std::string(m.id) + "-" + size,
                std::string(m.title) + " AT " + size,
                "Black over the whole screen, the picture fitted with its shape kept.",
                "nothing of the world behind (purple) shows; no stretch",
                {{"showing", c.showing}, {"frame", std::to_string(c.shown)}}},
               draw(c, w, h));
    }
    if (std::string(m.id) == "middle") lab.metric("frame_at_1_5s", double(c.shown), "frame");
  }
  lab.metric("movie_sound", double(sound), "samples");
  lab.metric("still_after", c.still ? 1 : 0, "bool");
  for (const std::string& line : c.said) lab.fail(line, "Check the fixture. Skill: classic.");
}

const ge::lab::LabSpec kSpec{
    "se-cinematic",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/cinematic (Quake II's movies and last picture in the window)",
    "classic",
    "Does a Quake II movie play at 14 frames a second, fitted to any screen, and give way to the "
    "game's last picture?",
    {{"game", "", "the player's Quake II folder (<basedir>/baseq2): its own victory picture"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
