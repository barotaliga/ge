// The pipeline's stages (pipeline.hpp).
//
// ge:summary Runs the pipeline's stages, pack to pictures, skipping the ones that are current
// ge:keywords pipeline run stage key skip textures index lightmaps sounds wav pack notice models
// ge:keywords pictures
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include "classicenhance/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>

#include "brush/read.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/lightmaps.hpp"
#include "classicenhance/pictures.hpp"
#include "classicenhance/upscaler.hpp"
#include "lab/png.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

constexpr const char* kNotice =
    "This folder was made on this machine from your own copy of the classic packs. Everything\n"
    "here is read or derived from them, so keep it on this machine and do not share or publish\n"
    "it. Delete the folder at any time: the pipeline makes it again from the packs.\n";

std::string fnv_hex(const std::string& text) {
  uint64_t h = 1469598103934665603ull;
  for (const char c : text) h = (h ^ uint8_t(c)) * 1099511628211ull;
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 15; i >= 0; i--, h >>= 4) out[size_t(i)] = kHex[h & 15];
  return out;
}

std::string num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.3f", v);
  return buf;
}

// Which upscaler made the pictures: none (the kernel) keeps the key a run without one always had.
std::string network_key(const EnhanceOptions& e) {
  if (e.network == nullptr) return "";
  return " network " + e.network->digest + " on " + (e.device ? e.device->name() : "cpu");
}

std::string stage_key(Stage s, const RunOptions& o) {
  std::string text = stage_name(s);
  if (s == Stage::textures) {
    const EnhanceOptions& e = o.textures;
    text += " v" + std::to_string(kTextureStageVersion) + " scale " + std::to_string(e.scale) +
            " sharpen " + num(e.sharpen) + " grain " + num(e.grain) + " relief " + num(e.relief) +
            " limit " + std::to_string(o.texture_limit) + network_key(e);
  } else if (s == Stage::lightmaps) {
    text += " v" + std::to_string(kLightmapStageVersion);
  } else if (s == Stage::sounds) {
    const SoundOptions& d = o.sounds;
    text += " v" + std::to_string(kSoundStageVersion) + " rate " + std::to_string(d.rate) +
            " denoise " + num(d.denoise) + " floor " + num(d.floor_db) + " extend " +
            num(d.extend) + " to " + num(d.extend_to_hz) + " rolloff " + num(d.rolloff_db) +
            " knee " + num(d.knee);
  } else if (s == Stage::models) {
    const EnhanceOptions& e = o.textures;
    text += " v" + std::to_string(kModelStageVersion) + " scale " +
            std::to_string(o.models.skin_scale) + " round " + std::to_string(o.models.round) +
            " sharpen " + num(e.sharpen) + " grain " + num(e.grain) + " relief " + num(e.relief) +
            network_key(e);
  } else if (s == Stage::pictures) {
    text += " v" + std::to_string(kPictureStageVersion);
  } else {
    text += " v1";
  }
  return fnv_hex(text);
}

bool save_png(const fs::path& path, const assets::Image& img) {
  return write_whole(path, lab::encode_png({img.width, img.height, img.rgba}));
}

// The maps in the pack, sorted by path.
std::vector<const vfs::PakEntry*> maps_of(const vfs::Pak& pak) {
  std::vector<const vfs::PakEntry*> out;
  for (const vfs::PakEntry& e : pak.entries) {
    if (e.path.rfind("maps/", 0) == 0 && e.path.ends_with(".bsp")) out.push_back(&e);
  }
  std::sort(out.begin(), out.end(), [](auto* a, auto* b) { return a->path < b->path; });
  return out;
}

std::string map_name(const std::string& path) {
  return path.substr(5, path.size() - 9);  // "maps/" ... ".bsp"
}

// The run's reports: a stage starting with its item count, and each item done. `item` is false
// once the run has been asked to stop, with the reason in `*error`.
struct Reports {
  const RunOptions& o;
  Stage stage;
  uint32_t total = 0, done = 0;

  void started(size_t items) {
    total = uint32_t(items);
    if (o.progress) o.progress({Progress::Kind::started, stage, 0, total, {}, nullptr, nullptr});
  }
  bool item(const std::string& name, std::string* error, const assets::Image* before = nullptr,
            const assets::Image* after = nullptr) {
    done++;
    if (o.progress) o.progress({Progress::Kind::item, stage, done, total, name, before, after});
    if (o.cancel != nullptr && o.cancel->load()) {
      *error = "cancelled";
      return false;
    }
    return true;
  }
};

constexpr const char* kUnverified = "unverified: the player's own copy";

bool run_pack(const Source& src, const fs::path& dir, Reports& rep, StageResult& r,
              std::string* error) {
  rep.started(1);
  bool copied = write_whole(dir / "pack" / "pak0.pak", src.pack);
  for (const LaterPack& p : src.later)
    copied = copied && write_whole(dir / "pack" / p.name, p.bytes);
  if (!copied || !write_text(dir / "NOTICE.txt", kNotice)) {
    *error = "cannot write into " + dir.string() + "; check the cache folder is writable";
    return false;
  }
  std::string text = "route " + std::string(route_name(src.route)) + "\npath " + src.path +
                     "\npack-sha256 " + src.pack_sha256 + "\npack " +
                     (src.known != nullptr ? src.known->what : kUnverified) + "\n";
  for (const LaterPack& p : src.later) {
    text += "later " + p.name + " " + p.sha256 + " " +
            (p.known != nullptr ? p.known->what : kUnverified) + "\n";
  }
  if (!src.zip_sha256.empty()) {
    text += "zip-sha256 " + src.zip_sha256 + "\nzip " +
            (src.zip ? src.zip->what : "not a known release zip; its pack is") + "\n";
  }
  if (!write_text(dir / "source.txt", text)) {
    *error = "cannot write " + (dir / "source.txt").string();
    return false;
  }
  r.files = 3 + uint32_t(src.later.size());
  r.note = src.known != nullptr ? std::string("the pack, verified: ") + src.known->what
                                : std::string("the pack, ") + kUnverified;
  if (!src.later.empty()) r.note += "; and " + std::to_string(src.later.size()) + " later pack(s)";
  return rep.item("pak0.pak", error);
}

bool run_textures(const Source& src, const fs::path& dir, const RunOptions& o, Reports& rep,
                  StageResult& r, std::string* error) {
  Palette palette;
  std::vector<PackTexture> textures;
  if (!pack_textures(src.content(), &palette, &textures, error)) return false;
  if (o.texture_limit > 0 && textures.size() > o.texture_limit) textures.resize(o.texture_limit);
  rep.started(textures.size());
  std::string index = "# name folder width height material glows (first map)\n";
  uint32_t glowing = 0;
  for (const PackTexture& t : textures) {
    const Enhanced e = enhance_texture(t.name, t.indexed, t.width, t.height, palette, o.textures);
    const std::string folder = safe_name(t.name);
    const fs::path at = dir / "textures" / folder;
    bool ok = save_png(at / "albedo.png", e.albedo);
    r.files++;
    if (e.height.width > 0) {
      ok = ok && save_png(at / "height.png", e.height) && save_png(at / "normal.png", e.normal) &&
           save_png(at / "roughness.png", e.roughness);
      r.files += 3;
    }
    if (e.glows) {
      ok = ok && save_png(at / "emissive.png", e.emissive);
      r.files++;
      glowing++;
    }
    if (!ok) {
      *error = "cannot write the maps of texture '" + t.name + "' under " + at.string();
      return false;
    }
    index += t.name + " " + folder + " " + std::to_string(t.width) + " " +
             std::to_string(t.height) + " " + material_name(e.material) + " " +
             (e.glows ? "glows" : "-") + " " + t.map + "\n";
    if (o.progress) {
      const assets::Image source = to_rgba(t.indexed, t.width, t.height, palette);
      if (!rep.item(t.name, error, &source, &e.albedo)) return false;
    } else if (!rep.item(t.name, error)) {
      return false;
    }
  }
  if (!write_text(dir / "textures" / "index.txt", index)) {
    *error = "cannot write " + (dir / "textures" / "index.txt").string();
    return false;
  }
  r.files++;
  r.note =
      std::to_string(textures.size()) + " textures at " + std::to_string(o.textures.scale) +
      "x by " +
      (o.textures.network ? "the trained network on the " +
                                (o.textures.device ? o.textures.device->name() : std::string("cpu"))
                          : std::string("the kernel")) +
      ", " + std::to_string(glowing) + " with glow";
  return true;
}

bool run_lightmaps(const Source& src, const fs::path& dir, Reports& rep, StageResult& r,
                   std::string* error) {
  vfs::Pak pak;
  if (!vfs::read_pak(src.content(), &pak, error)) return false;
  const std::vector<const vfs::PakEntry*> maps = maps_of(pak);
  rep.started(maps.size());
  for (const vfs::PakEntry* e : maps) {
    brush::Map map;
    if (!brush::read_bsp(vfs::pak_file(src.content(), *e), &map, error)) {
      *error = e->path + ": " + *error;
      return false;
    }
    if (!light_map(map, map_name(e->path), dir / "lightmaps", error)) return false;
    r.files++;
    if (!rep.item(map_name(e->path), error)) return false;
  }
  r.note = std::to_string(r.files) + " maps' own light packed (the rebake is row M40's)";
  return true;
}

bool run_sounds(const Source& src, const fs::path& dir, const RunOptions& o, Reports& reports,
                StageResult& r, std::string* error) {
  vfs::Pak pak;
  if (!vfs::read_pak(src.content(), &pak, error)) return false;
  std::vector<const vfs::PakEntry*> sounds;
  for (const vfs::PakEntry& e : pak.entries) {
    if (e.path.rfind("sound/", 0) == 0 && e.path.ends_with(".wav")) sounds.push_back(&e);
  }
  std::sort(sounds.begin(), sounds.end(), [](auto* a, auto* b) { return a->path < b->path; });
  reports.started(sounds.size());
  std::string index = "# path seconds loop-start source-hz noise-db removed-db added-db\n";
  for (const vfs::PakEntry* e : sounds) {
    Pcm in;
    if (!read_wav(vfs::pak_file(src.content(), *e), &in, error)) {
      *error = e->path + ": " + *error;
      return false;
    }
    SoundReport rep;
    const Pcm out = enhance_sound(in, o.sounds, &rep);
    const std::string rel = e->path.substr(6);  // past "sound/"
    if (!write_whole(dir / "sounds" / rel, write_wav(out))) {
      *error = "cannot write " + (dir / "sounds" / rel).string();
      return false;
    }
    r.files++;
    const double seconds = out.rate > 0 ? double(out.samples.size()) / out.rate : 0.0;
    index += rel + " " + num(seconds) + " " + std::to_string(out.loop_start) + " " +
             num(rep.source_hz) + " " + num(rep.noise_db) + " " + num(rep.removed_db) + " " +
             num(rep.added_db) + "\n";
    if (!reports.item(rel, error)) return false;
  }
  if (!write_text(dir / "sounds" / "index.txt", index)) {
    *error = "cannot write " + (dir / "sounds" / "index.txt").string();
    return false;
  }
  r.files++;
  r.note = std::to_string(sounds.size()) + " sounds at " + std::to_string(o.sounds.rate) + " Hz";
  return true;
}

bool run_models(const Source& src, const fs::path& dir, const RunOptions& o, Reports& rep,
                StageResult& r, std::string* error) {
  vfs::Pak pak;
  if (!vfs::read_pak(src.content(), &pak, error)) return false;
  const vfs::PakEntry* pal = vfs::pak_find(pak, "gfx/palette.lmp");
  if (pal == nullptr || pal->size < 768) {
    *error = "the pack has no gfx/palette.lmp of 768 bytes; it is not the classic pack";
    return false;
  }
  Palette palette;
  std::copy_n(vfs::pak_file(src.content(), *pal).data(), 768, palette.rgb.data());
  ModelStats stats;
  ModelOptions models = o.models;
  models.each = [&](const std::string& name, uint32_t done, uint32_t total) {
    if (done == 0) {
      rep.started(total);
      return true;
    }
    return rep.item(name, error);
  };
  if (!enhance_models(src.content(), palette, dir, models, o.textures, &stats, error)) return false;
  r.files = stats.files;
  r.note = std::to_string(stats.models) + " models as glTF, skins at " +
           std::to_string(o.models.skin_scale) + "x, " + std::to_string(stats.sprites) +
           " sprites; " + std::to_string(stats.replaced) + " drawn by the engine's own effects";
  return true;
}

bool run_pictures(const Source& src, const fs::path& dir, Reports& rep, StageResult& r,
                  std::string* error) {
  Palette palette;
  std::vector<PackPicture> pictures;
  if (!pack_pictures(src.pack, &palette, &pictures, error)) return false;
  rep.started(pictures.size());
  std::string index = "# name width height transparent-index from\n";
  for (const PackPicture& p : pictures) {
    const fs::path at = dir / "pictures" / (p.name + ".png");
    if (!save_png(at, picture_rgba(p.picture, palette, p.transparent))) {
      *error = "cannot write " + at.string();
      return false;
    }
    r.files++;
    index += p.name + " " + std::to_string(p.picture.width) + " " +
             std::to_string(p.picture.height) + " " + std::to_string(p.transparent) + " " + p.from +
             "\n";
    if (!rep.item(p.name, error)) return false;
  }
  if (!write_text(dir / "pictures" / "index.txt", index)) {
    *error = "cannot write " + (dir / "pictures" / "index.txt").string();
    return false;
  }
  r.files++;
  r.note = std::to_string(pictures.size()) + " pictures, unscaled";
  return true;
}

}  // namespace

const char* stage_name(Stage s) {
  switch (s) {
    case Stage::pack:
      return "pack";
    case Stage::textures:
      return "textures";
    case Stage::lightmaps:
      return "lightmaps";
    case Stage::sounds:
      return "sounds";
    case Stage::models:
      return "models";
    case Stage::pictures:
      return "pictures";
  }
  return "?";
}

bool stage_named(const std::string& name, Stage* out) {
  for (const Stage s : kStages) {
    if (name == stage_name(s)) {
      *out = s;
      return true;
    }
  }
  return false;
}

bool pack_textures(std::span<const uint8_t> pack_bytes, Palette* palette,
                   std::vector<PackTexture>* out, std::string* error) {
  out->clear();
  vfs::Pak pak;
  if (!vfs::read_pak(pack_bytes, &pak, error)) return false;
  const vfs::PakEntry* pal = vfs::pak_find(pak, "gfx/palette.lmp");
  if (pal == nullptr || pal->size < 768) {
    *error = "the pack has no gfx/palette.lmp of 768 bytes; it is not the classic pack";
    return false;
  }
  std::memcpy(palette->rgb.data(), vfs::pak_file(pack_bytes, *pal).data(), 768);
  std::set<std::string> seen;
  for (const vfs::PakEntry* e : maps_of(pak)) {
    brush::Map map;
    if (!brush::read_bsp(vfs::pak_file(pack_bytes, *e), &map, error)) {
      *error = e->path + ": " + *error;
      return false;
    }
    for (const brush::Texture& t : map.textures) {
      if (t.indexed.empty() || never_drawn(t.name) || !seen.insert(t.name).second) continue;
      out->push_back({t.name, map_name(e->path), int(t.width), int(t.height), t.indexed});
    }
  }
  return true;
}

bool run_pipeline(const Source& source, const fs::path& root_in, const RunOptions& opts,
                  RunReport* report, std::string* error) {
  report->stages.clear();
  if (source.pack_sha256.empty()) {
    *error = "the source is not an opened pack; open it with open_source first";
    return false;
  }
  const fs::path root = root_in.empty() ? default_cache_root() : root_in;
  if (root.empty()) {
    *error = "no cache folder: set GE_CLASSIC_CACHE to a folder on this machine";
    return false;
  }
  report->dir = pack_dir(root, source.set_sha256.empty() ? source.pack_sha256 : source.set_sha256);
  for (const Stage s : kStages) {
    StageResult r;
    r.stage = s;
    const bool asked =
        opts.only.empty() || std::find(opts.only.begin(), opts.only.end(), s) != opts.only.end();
    const std::string key = stage_key(s, opts);
    auto skip = [&](const char* why) {
      r.note = why;
      report->stages.push_back(r);
      if (opts.progress) opts.progress({Progress::Kind::skipped, s, 0, 0, why, nullptr, nullptr});
    };
    if (!asked) {
      skip("not asked for");
      continue;
    }
    if (!opts.force && recorded_key(report->dir, stage_name(s)) == key) {
      skip("up to date");
      continue;
    }
    Reports rep{opts, s};
    const auto t0 = std::chrono::steady_clock::now();
    bool ok = false;
    switch (s) {
      case Stage::pack:
        ok = run_pack(source, report->dir, rep, r, error);
        break;
      case Stage::textures:
        ok = run_textures(source, report->dir, opts, rep, r, error);
        break;
      case Stage::lightmaps:
        ok = run_lightmaps(source, report->dir, rep, r, error);
        break;
      case Stage::sounds:
        ok = run_sounds(source, report->dir, opts, rep, r, error);
        break;
      case Stage::models:
        ok = run_models(source, report->dir, opts, rep, r, error);
        break;
      case Stage::pictures:
        ok = run_pictures(source, report->dir, rep, r, error);
        break;
    }
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    r.ran = true;
    report->stages.push_back(r);
    if (!ok) {
      if (opts.progress)
        opts.progress({Progress::Kind::failed, s, rep.done, rep.total, *error, nullptr, nullptr});
      *error = std::string(stage_name(s)) + " stage: " + *error;
      return false;
    }
    if (!write_record(report->dir, stage_name(s), key)) {
      *error =
          "cannot record the " + std::string(stage_name(s)) + " stage in " + report->dir.string();
      return false;
    }
    if (opts.progress)
      opts.progress({Progress::Kind::finished, s, rep.done, rep.total, r.note, nullptr, nullptr});
  }
  return true;
}

}  // namespace ge::classicenhance
