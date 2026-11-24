// The soundtrack store (soundtrack.hpp).
//
// ge:summary A game's soundtrack kept as music/trackNN.flac with a manifest; sources decoded
// ge:keywords soundtrack music track flac manifest ogg wav decode store_track
// ge:skill classic audio
// ge:test tests/classic_content_import_test.cpp
#include "classic/content/soundtrack.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "assets/sha256.hpp"
#include "audio/flac.hpp"

namespace ge::classic::content {

namespace fs = std::filesystem;

// The ogg reader (music_ogg.cpp, over stb_vorbis).
bool decode_ogg(std::span<const uint8_t> bytes, Samples* out, std::string* error);

namespace {

// Quake's disc: ten music tracks, their lengths as the disc plays them.
constexpr uint16_t kQ1Seconds[] = {305, 143, 498, 362, 440, 515, 333, 386, 210, 313};

constexpr Soundtrack kSoundtracks[] = {
    {"q1", 2, 11, kQ1Seconds},
    {"q2", 2, 11, {}},
};

// Recorded from real discs as they are seen; none yet.
constexpr std::span<const KnownTrack> kKnown = {};

std::string lower(std::string_view s) {
  std::string o(s);
  for (char& c : o) c = char(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

int rank(std::string_view from) {
  if (from == "disc") return 0;
  if (from == "flac" || from == "wav") return 1;
  return 2;
}

std::vector<uint8_t> pcm_bytes(const Samples& s) {
  const int bytes = (s.bits + 7) / 8;
  std::vector<uint8_t> out;
  out.reserve(s.data.size() * size_t(bytes));
  for (int32_t v : s.data)
    for (int b = 0; b < bytes; b++) out.push_back(uint8_t(uint32_t(v) >> (8 * b)));
  return out;
}

bool write_file(const fs::path& p, std::span<const uint8_t> bytes) {
  const fs::path part = p.string() + ".part";
  {
    std::ofstream o(part, std::ios::binary);
    o.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!o) return false;
  }
  std::error_code ec;
  fs::rename(part, p, ec);
  return !ec;
}

bool write_manifest(const fs::path& dir, const std::vector<StoredTrack>& tracks) {
  std::string text =
      "# The soundtrack as this game keeps it: one line a track (see the classic content layer's\n"
      "# soundtrack store). pcm: SHA-256 of the samples as little-endian 16-bit.\n";
  for (const StoredTrack& t : tracks) {
    text += t.file + " frames=" + std::to_string(t.frames) + " rate=" + std::to_string(t.rate) +
            " channels=" + std::to_string(t.channels) + " bits=" + std::to_string(t.bits) +
            " pcm=" + t.pcm_sha256 + " from=" + t.from + " source=" + t.source_sha256 +
            " known=" + (t.known.empty() ? "-" : t.known) + "\n";
  }
  return write_file(dir / "tracks.txt",
                    std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t le32(const uint8_t* p) { return uint32_t(le16(p)) | uint32_t(le16(p + 2)) << 16; }

bool decode_wav(std::span<const uint8_t> b, Samples* out, std::string* error) {
  if (b.size() < 12 || std::string_view(reinterpret_cast<const char*>(b.data()), 4) != "RIFF" ||
      std::string_view(reinterpret_cast<const char*>(b.data() + 8), 4) != "WAVE") {
    *error = "not a WAV file (no RIFF WAVE header)";
    return false;
  }
  int fmt = 0;
  for (size_t at = 12; at + 8 <= b.size();) {
    const std::string_view id(reinterpret_cast<const char*>(&b[at]), 4);
    const uint32_t n = le32(&b[at + 4]);
    const size_t body = at + 8;
    if (body + n > b.size() && id != "data") break;
    if (id == "fmt " && n >= 16) {
      fmt = le16(&b[body]);
      out->channels = le16(&b[body + 2]);
      out->rate = int(le32(&b[body + 4]));
      out->bits = le16(&b[body + 14]);
    } else if (id == "data") {
      if ((fmt != 1 && fmt != 0xfffe) || (out->bits != 16 && out->bits != 24) ||
          out->channels < 1) {
        *error = "the WAV file is not 16 or 24-bit integer samples; convert it to FLAC first";
        return false;
      }
      const int w = out->bits / 8;
      const size_t count = std::min<size_t>(n, b.size() - body) / size_t(w);
      out->data.resize(count - count % size_t(out->channels));
      for (size_t i = 0; i < out->data.size(); i++) {
        const uint8_t* p = &b[body + i * size_t(w)];
        out->data[i] =
            w == 2
                ? int16_t(le16(p))
                : int32_t(uint32_t(p[0]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 24) >> 8;
      }
      return true;
    }
    at = body + n + (n & 1);
  }
  *error = "the WAV file has no sample data";
  return false;
}

}  // namespace

const Soundtrack* soundtrack_of(std::string_view game) {
  for (const Soundtrack& s : kSoundtracks)
    if (s.game == game) return &s;
  return nullptr;
}

std::span<const KnownTrack> known_tracks() { return kKnown; }

std::string track_file(int track) {
  char b[24];
  std::snprintf(b, sizeof b, "track%02d.flac", track);
  return b;
}

std::string track_path(const std::string& game_dir, int track) {
  if (track < 1 || track > 99 || game_dir.empty()) return {};
  const std::filesystem::path p = std::filesystem::path(game_dir) / "music" / track_file(track);
  std::error_code ec;
  return std::filesystem::is_regular_file(p, ec) ? p.string() : std::string{};
}

namespace {

// `name` as a key: `\` as `/`, no leading or doubled `/`, lower case, a music extension dropped.
std::string music_key(std::string_view name) {
  std::string k;
  for (const char c : name) {
    const char d = c == '\\' ? '/' : c;
    if (d == '/' && (k.empty() || k.back() == '/')) continue;
    k += d;
  }
  k = lower(k);
  for (const std::string_view ext : {".flac", ".opus", ".ogg", ".mp3", ".wav"})
    if (k.size() > ext.size() && k.ends_with(ext)) {
      k.resize(k.size() - ext.size());
      break;
    }
  return k;
}

}  // namespace

std::string music_file(const std::string& game_dir, std::string_view name) {
  const std::string key = music_key(name);
  if (game_dir.empty() || key.empty() || key.find("..") != std::string::npos) return {};
  const std::filesystem::path root = std::filesystem::path(game_dir) / "music";
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec)) return {};
  std::vector<std::pair<std::string, std::filesystem::path>> found;  // A to Z without case
  for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
       !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    const std::filesystem::path& p = it->path();
    if (p.filename().string().starts_with('.')) {
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(ec) || lower(p.extension().string()) != ".flac") continue;
    std::string rel = p.lexically_relative(root).generic_string();
    rel.resize(rel.size() - 5);  // ".flac"
    if (lower(rel) == key)
      found.emplace_back(lower(p.lexically_relative(root).generic_string()), p);
  }
  if (found.empty()) return {};
  std::sort(found.begin(), found.end());
  return found.front().second.string();
}

int q2_remap_track(std::string_view game_folder, int track) {
  if (track < 2 || track > 11) return track;
  // The folder's last name in any case, as `-game` gives it ("XATRIX", "C:\Quake2\xatrix\").
  while (!game_folder.empty() && (game_folder.back() == '/' || game_folder.back() == '\\'))
    game_folder.remove_suffix(1);
  const size_t slash = game_folder.find_last_of("/\\");
  const std::string folder =
      lower(slash == std::string_view::npos ? game_folder : game_folder.substr(slash + 1));
  if (folder == "rogue") return track + 10;
  static constexpr int kXatrix[10] = {9, 13, 14, 7, 16, 2, 15, 3, 4, 18};
  if (folder == "xatrix") return kXatrix[track - 2];
  return track;
}

int level_track(std::string_view entities) {
  const size_t open = entities.find('{');
  if (open == std::string_view::npos) return 0;
  const size_t close = entities.find('}', open);
  std::string_view world =
      entities.substr(open, close == std::string_view::npos ? close : close - open);
  for (size_t at = world.find("\"sounds\""); at != std::string_view::npos;
       at = world.find("\"sounds\"", at + 1)) {
    size_t v = world.find('"', at + 8);
    if (v == std::string_view::npos) return 0;
    int n = 0;
    bool any = false;
    for (v++; v < world.size() && world[v] >= '0' && world[v] <= '9' && n < 1000; v++, any = true)
      n = n * 10 + (world[v] - '0');
    return any ? n : 0;
  }
  return 0;
}

const char* source_name(MusicSource s) {
  switch (s) {
    case MusicSource::disc:
      return "disc";
    case MusicSource::flac:
      return "flac";
    case MusicSource::wav:
      return "wav";
    case MusicSource::ogg:
      return "ogg";
  }
  return "?";
}

bool music_source_of(std::string_view path, MusicSource* out) {
  const std::string ext = lower(fs::path(std::string(path)).extension().string());
  if (ext == ".flac")
    *out = MusicSource::flac;
  else if (ext == ".wav")
    *out = MusicSource::wav;
  else if (ext == ".ogg")
    *out = MusicSource::ogg;
  else
    return false;
  return true;
}

bool decode_music(std::span<const uint8_t> bytes, MusicSource kind, Samples* out,
                  std::string* error) {
  *out = Samples();
  if (kind == MusicSource::wav) return decode_wav(bytes, out, error);
  if (kind == MusicSource::ogg) return decode_ogg(bytes, out, error);
  audio::FlacInfo info;
  if (!audio::flac_decode(bytes, &out->data, &info, error)) return false;
  out->channels = info.channels;
  out->rate = info.rate;
  out->bits = info.bits;
  return true;
}

std::vector<int> track_numbers(std::span<const std::string> paths) {
  std::vector<int> out(paths.size(), 0);
  std::vector<size_t> bare;
  int lowest_bare = 1000;
  for (size_t i = 0; i < paths.size(); i++) {
    const std::string stem = lower(fs::path(paths[i]).stem().string());
    const size_t t = stem.find("track");
    size_t at = t == std::string::npos ? 0 : t + 5;
    while (t != std::string::npos && at < stem.size() && !std::isdigit((unsigned char)stem[at]))
      at++;
    size_t end = at;
    while (end < stem.size() && end - at < 3 && std::isdigit((unsigned char)stem[end])) end++;
    if (end == at) continue;
    const int n = std::stoi(stem.substr(at, end - at));
    if (n < 1 || n > 99) continue;
    out[i] = n;
    if (t == std::string::npos) {
      bare.push_back(i);
      lowest_bare = std::min(lowest_bare, n);
    }
  }
  if (lowest_bare == 1)
    for (size_t i : bare) out[i]++;
  return out;
}

std::vector<StoredTrack> read_manifest(const std::string& game_dir) {
  std::vector<StoredTrack> out;
  std::ifstream in(fs::path(game_dir) / "music" / "tracks.txt");
  for (std::string line; std::getline(in, line);) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ws(line);
    StoredTrack t;
    ws >> t.file;
    if (std::sscanf(t.file.c_str(), "track%d.flac", &t.track) != 1) continue;
    for (std::string kv; ws >> kv;) {
      const size_t eq = kv.find('=');
      if (eq == std::string::npos) continue;
      const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
      if (k == "frames")
        t.frames = std::stoull(v);
      else if (k == "rate")
        t.rate = std::stoi(v);
      else if (k == "channels")
        t.channels = std::stoi(v);
      else if (k == "bits")
        t.bits = std::stoi(v);
      else if (k == "pcm")
        t.pcm_sha256 = v;
      else if (k == "from")
        t.from = v;
      else if (k == "source")
        t.source_sha256 = v;
      else if (k == "known")
        t.known = v == "-" ? "" : v;
    }
    out.push_back(t);
  }
  return out;
}

bool store_track(const std::string& game_dir, std::string_view game, int track,
                 const Samples& samples, MusicSource from, const std::string& source_sha256,
                 StoredTrack* out, std::string* error) {
  StoredTrack t;
  t.track = track;
  t.file = track_file(track);
  t.channels = samples.channels;
  t.rate = samples.rate;
  t.bits = samples.bits;
  t.frames = samples.data.size() / size_t(std::max(1, samples.channels));
  t.pcm_sha256 = assets::sha256_hex(pcm_bytes(samples));
  t.from = source_name(from);
  t.source_sha256 = source_sha256.empty() ? t.pcm_sha256 : source_sha256;
  for (const KnownTrack& k : kKnown)
    if (k.game == game && k.track == track && k.pcm_sha256 == t.pcm_sha256) t.known = k.what;
  if (const Soundtrack* s = soundtrack_of(game); s != nullptr && track >= s->first &&
                                                 size_t(track - s->first) < s->seconds.size() &&
                                                 t.rate > 0) {
    const double secs = double(t.frames) / t.rate, want = s->seconds[size_t(track - s->first)];
    t.length_check = secs > want - 2.5 && secs < want + 2.5 ? 1 : -1;
  }
  const fs::path dir = fs::path(game_dir) / "music";
  std::vector<StoredTrack> all = read_manifest(game_dir);
  auto have =
      std::find_if(all.begin(), all.end(), [&](const StoredTrack& o) { return o.track == track; });
  std::error_code ec;
  const bool on_disk = fs::exists(dir / t.file, ec);
  if (have != all.end() && on_disk &&
      (have->pcm_sha256 == t.pcm_sha256 || rank(have->from) < rank(t.from))) {
    *out = *have;
    out->kept = false;
    out->length_check = t.length_check;
    return true;
  }
  fs::create_directories(dir, ec);
  const std::vector<uint8_t> flac =
      audio::flac_encode(samples.data, samples.channels, samples.rate, samples.bits,
                         {{"TRACKNUMBER", std::to_string(track)},
                          {"GAME", std::string(game)},
                          {"SOURCE", t.from},
                          {"SOURCE_SHA256", t.source_sha256}});
  if (!write_file(dir / t.file, flac)) {
    *error = "could not write " + (dir / t.file).string() +
             "; check that the game's data folder can be written and the disk has room";
    return false;
  }
  t.kept = true;
  if (have != all.end())
    *have = t;
  else
    all.push_back(t);
  std::sort(all.begin(), all.end(),
            [](const StoredTrack& a, const StoredTrack& b) { return a.track < b.track; });
  if (!write_manifest(dir, all)) {
    *error =
        "could not write " + (dir / "tracks.txt").string() + "; check the folder's permissions";
    return false;
  }
  *out = t;
  return true;
}

}  // namespace ge::classic::content
