// LAB `se-remix-sounds`: CLASSIC REMIX'S SOUNDS (`cr_sounds`, profiles/remix_sounds.hpp). Every
// row of the sound table drawn as a band: the part both sounds play, the classic game's sound on
// the left and the sequel's on the right, each with its waveform read from the game's own packs
// (`--game`, `--sequel`; without them the names alone, the smoke run). A second table proves
// `cr_items` alone never changes a sound: each sound the classic rules' worn sequel arsenal names
// is heard as the classic game's while `cr_sounds` is 0, and as the sequel's once it is 2.
//
// ge:summary Lab: classic remix's sound table, each pair's waveforms off the games' own packs
// ge:keywords lab se-remix-sounds cr_sounds classic-remix sound-swap heard_as waveform
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_test.cpp
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "audio/pcm.hpp"
#include "classicenhance/profile.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/profiles/remix_sounds.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace ce = ge::classicenhance;
namespace pf = shooters_edge::profiles;

constexpr int kWidth = 1400;
constexpr int kBand = 44;      // one row's height, pixels
constexpr int kWhat = 260;     // the part's column
constexpr int kName = 250;     // a sound's name
constexpr int kWave = 320;     // a sound's waveform
constexpr int kPerSheet = 36;  // rows a sheet

const gl::Rgba kPaper{246, 244, 238, 255};
const gl::Rgba kInk{30, 30, 34, 255};
const gl::Rgba kFaint{150, 150, 150, 255};
const gl::Rgba kClassic{150, 82, 40, 255};
const gl::Rgba kSequel{40, 96, 150, 255};

// A game's files, or none when its install is not given or does not open.
std::optional<cl::SearchPath> open_files(gl::Lab& lab, const std::string& dir,
                                         std::string_view id) {
  if (dir.empty()) return std::nullopt;
  cl::SearchPath files;
  std::string error;
  if (!cl::open_game_dir(dir, "", files, error, *ce::find_profile(id))) {
    lab.note(std::string(id) + ": " + error);
    return std::nullopt;
  }
  return files;
}

// The sound `name` read from `files`, or none.
std::optional<ge::audio::Pcm> read_sound(const std::optional<cl::SearchPath>& files,
                                         std::string_view name) {
  if (!files || name.empty()) return std::nullopt;
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = cl::find_file(*files, "sound/" + std::string(name), store);
  ge::audio::Pcm pcm;
  std::string error;
  if (bytes.empty() || !ge::audio::read_wav(bytes, name, &pcm, &error)) return std::nullopt;
  return pcm;
}

// The sound's waveform, each column its range, over `kWave` pixels for 2 s.
void wave(gl::Picture& p, int x, int y, const ge::audio::Pcm& pcm, gl::Rgba c) {
  const int mid = y + kBand / 2;
  const double per = 2.0 * pcm.rate / kWave;  // frames a column
  for (int i = 0; i < kWave; i++) {
    const auto a = static_cast<size_t>(i * per), b = static_cast<size_t>((i + 1) * per);
    if (a >= pcm.samples.size()) break;
    float lo = 0, hi = 0;
    for (size_t k = a; k < std::min(b, pcm.samples.size()); k++)
      lo = std::min(lo, pcm.samples[k]), hi = std::max(hi, pcm.samples[k]);
    const double h = kBand * 0.5 - 4;
    gl::draw_line(p, x + i, mid - hi * h, x + i, mid - lo * h, c);
  }
}

// One side of a row: the name and the waveform (or what is missing).
void side(gl::Picture& p, int x, int y, std::string_view name,
          const std::optional<ge::audio::Pcm>& pcm, bool looked, gl::Rgba c, int& found) {
  if (name.empty()) {
    gl::draw_text(p, x, y + 16, "(silent: no such sound)", kFaint);
    return;
  }
  gl::draw_text(p, x, y + 16, name, c);
  if (pcm) {
    wave(p, x + kName, y, *pcm, c);
    found++;
  } else if (looked) {
    gl::draw_text(p, x + kName, y + 16, "(not in the packs)", kFaint);
  }
}

void run(gl::Lab& lab) {
  const auto q1 = open_files(lab, lab.arg("game"), "classic");
  const auto q2 = open_files(lab, lab.arg("sequel"), "sequel");
  const size_t n = std::size(pf::kSoundMatches);
  int found_classic = 0, found_sequel = 0;
  std::vector<std::vector<std::string>> rows;
  for (size_t first = 0, sheet = 1; first < n; first += kPerSheet, sheet++) {
    const size_t last = std::min(n, first + kPerSheet);
    gl::Picture p =
        gl::create_picture(kWidth, static_cast<int>(last - first + 1) * kBand + 8, kPaper);
    const int xc = kWhat, xs = kWhat + kName + kWave + 20;
    gl::draw_text(p, 8, 14, "PART BOTH PLAY", kInk);
    gl::draw_text(p, xc, 14, "THE CLASSIC GAME'S (cr_sounds 1)", kClassic);
    gl::draw_text(p, xs, 14, "THE SEQUEL'S (cr_sounds 2)", kSequel);
    for (size_t i = first; i < last; i++) {
      const pf::SoundMatch& m = pf::kSoundMatches[i];
      const int y = static_cast<int>(i - first + 1) * kBand;
      if ((i - first) % 2 == 0) gl::fill_rect(p, 0, y, kWidth, kBand, {236, 233, 224, 255});
      gl::draw_text(p, 8, y + 16, m.what, kInk);
      side(p, xc, y, m.classic, read_sound(q1, m.classic), q1.has_value(), kClassic, found_classic);
      side(p, xs, y, m.sequel, read_sound(q2, m.sequel), q2.has_value(), kSequel, found_sequel);
      rows.push_back({std::string(m.what), m.classic.empty() ? "(silent)" : std::string(m.classic),
                      std::string(m.sequel)});
    }
    lab.shot({"sound-pairs-" + std::to_string(sheet),
              "THE SOUND TABLE, SHEET " + std::to_string(sheet),
              "Each multiplayer sound of the classic game (left) beside the sequel's that plays "
              "the same part (right), with its first two seconds' waveform read from the game's "
              "own packs.",
              "every row's two sounds play the same part: a shot beside a shot, a blast beside a "
              "blast, a pickup beside a pickup",
              {{"game", lab.arg("game")}, {"sequel", lab.arg("sequel")}}},
             p);
  }
  lab.metric("pairs", static_cast<double>(n), "rows");
  if (q1) lab.metric("classic-found", found_classic, "sounds");
  if (q2) lab.metric("sequel-found", found_sequel, "sounds");
  lab.table({"the sound table", {"part", "classic game", "sequel"}, rows});

  // cr_items alone: the worn sequel arsenal's sounds heard as the classic game's.
  std::vector<std::vector<std::string>> worn;
  const pf::RemixArsenal* a = pf::remix_arsenal("classic", "sequel");
  std::vector<std::string_view> named;
  for (const pf::WeaponRow& w : a->arsenal.weapons)
    named.insert(named.end(), {w.fire_sound, w.land_sound, w.blast_sound});
  for (const pf::ArmourRow& r : a->arsenal.armour) named.push_back(r.sound);
  for (const pf::ItemLook& l : a->arsenal.looks) named.push_back(l.sound);
  int kept = 0;
  for (const std::string_view s : named) {
    if (s.empty() || s == "-") continue;
    const std::string_view own = pf::heard_as(s, "classic");
    kept += own != s;
    worn.push_back({std::string(s), std::string(own), std::string(pf::heard_as(s, "sequel"))});
  }
  lab.metric("worn-heard-as-own", kept, "sounds");
  lab.metric("worn-sounds", static_cast<double>(worn.size()), "sounds");
  lab.table({"cr_items 2 on the classic game: what the worn weapons sound like",
             {"the worn thing names", "heard with cr_sounds 0", "heard with cr_sounds 2"},
             worn});
}

const gl::LabSpec kSpec{
    "se-remix-sounds",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/profiles/remix_sounds: classic remix's sound table (cr_sounds)",
    "shooter",
    "Does every multiplayer sound of each classic game have a partner in the other that plays "
    "the same part, and does cr_items alone keep the played game's sounds?",
    {
        {"game", "", "the classic game's install (the folder holding id1; empty: names only)"},
        {"sequel", "", "the sequel's install (the folder holding baseq2; empty: names only)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
