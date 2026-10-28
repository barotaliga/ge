// The pipeline's stages (pipeline.hpp).
//
// ge:summary Runs the pack, textures, lightmaps and sounds stages into the cache, skipping what is current
// ge:keywords pipeline run stage key skip textures index lightmaps sounds wav pack notice
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
#include "lab/png.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

constexpr const char* kNotice =
    "This folder was made on this machine from your own copy of the classic shareware pack.\n"
    "The pack's licence lets it be passed on only unchanged; everything else here is derived\n"
    "from it, so keep it on this machine and do not share or publish it. Delete the folder at\n"
    "any time: the pipeline makes it again from the pack.\n";

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

std::string stage_key(Stage s, const RunOptions& o) {
  std::string text = stage_name(s);
  if (s == Stage::textures) {
    const EnhanceOptions& e = o.textures;
    text += " v" + std::to_string(kTextureStageVersion) + " scale " + std::to_string(e.scale) +
            " sharpen " + num(e.sharpen) + " grain " + num(e.grain) + " relief " + num(e.relief) +
            " limit " + std::to_string(o.texture_limit);
  } else if (s == Stage::lightmaps) {
    text += " v" + std::to_string(kLightmapStageVersion);
  } else if (s == Stage::sounds) {
    const SoundOptions& d = o.sounds;
    text += " v" + std::to_string(kSoundStageVersion) + " rate " + std::to_string(d.rate) +
            " denoise " + num(d.denoise) + " floor " + num(d.floor_db) + " extend " +
            num(d.extend) + " to " + num(d.extend_to_hz) + " rolloff " + num(d.rolloff_db) +
            " knee " + num(d.knee);
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

bool run_pack(const Source& src, const fs::path& dir, StageResult& r, std::string* error) {
  if (!write_whole(dir / "pack" / "pak0.pak", src.pack) ||
      !write_text(dir / "NOTICE.txt", kNotice)) {
    *error = "cannot write into " + dir.string() + "; check the cache folder is writable";
    return false;
  }
  std::string text = "route " + std::string(route_name(src.route)) + "\npath " + src.path +
                     "\npack-sha256 " + src.pack_sha256 + "\npack " + src.known->what + "\n";
  if (!src.zip_sha256.empty()) {
    text += "zip-sha256 " + src.zip_sha256 + "\nzip " +
            (src.zip ? src.zip->what : "not a known release zip; its pack is") + "\n";
  }
  if (!write_text(dir / "source.txt", text)) {
    *error = "cannot write " + (dir / "source.txt").string();
    return false;
  }
  r.files = 3;
  r.note = std::string("the pack, verified: ") + src.known->what;
  return true;
}

bool run_textures(const Source& src, const fs::path& dir, const RunOptions& o, StageResult& r,
                  std::string* error) {
  Palette palette;
  std::vector<PackTexture> textures;
  if (!pack_textures(src.pack, &palette, &textures, error)) return false;
  if (o.texture_limit > 0 && textures.size() > o.texture_limit) textures.resize(o.texture_limit);
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
  }
  if (!write_text(dir / "textures" / "index.txt", index)) {
    *error = "cannot write " + (dir / "textures" / "index.txt").string();
    return false;
  }
  r.files++;
  r.note = std::to_string(textures.size()) + " textures at " + std::to_string(o.textures.scale) +
           "x, " + std::to_string(glowing) + " with glow";
  return true;
}

bool run_lightmaps(const Source& src, const fs::path& dir, StageResult& r, std::string* error) {
  vfs::Pak pak;
  if (!vfs::read_pak(src.pack, &pak, error)) return false;
  for (const vfs::PakEntry* e : maps_of(pak)) {
    brush::Map map;
    if (!brush::read_bsp(vfs::pak_file(src.pack, *e), &map, error)) {
      *error = e->path + ": " + *error;
      return false;
    }
    if (!light_map(map, map_name(e->path), dir / "lightmaps", error)) return false;
    r.files++;
  }
  r.note = std::to_string(r.files) + " maps' own light packed (the rebake is row M40's)";
  return true;
}

bool run_sounds(const Source& src, const fs::path& dir, const RunOptions& o, StageResult& r,
                std::string* error) {
  vfs::Pak pak;
  if (!vfs::read_pak(src.pack, &pak, error)) return false;
  std::vector<const vfs::PakEntry*> sounds;
  for (const vfs::PakEntry& e : pak.entries) {
    if (e.path.rfind("sound/", 0) == 0 && e.path.ends_with(".wav")) sounds.push_back(&e);
  }
  std::sort(sounds.begin(), sounds.end(), [](auto* a, auto* b) { return a->path < b->path; });
  std::string index = "# path seconds loop-start source-hz noise-db removed-db added-db\n";
  for (const vfs::PakEntry* e : sounds) {
    Pcm in;
    if (!read_wav(vfs::pak_file(src.pack, *e), &in, error)) {
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
  }
  if (!write_text(dir / "sounds" / "index.txt", index)) {
    *error = "cannot write " + (dir / "sounds" / "index.txt").string();
    return false;
  }
  r.files++;
  r.note = std::to_string(sounds.size()) + " sounds at " + std::to_string(o.sounds.rate) + " Hz";
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
  if (source.known == nullptr) {
    *error = "the source is not a verified pack; open it with open_source first";
    return false;
  }
  const fs::path root = root_in.empty() ? default_cache_root() : root_in;
  if (root.empty()) {
    *error = "no cache folder: set GE_CLASSIC_CACHE to a folder on this machine";
    return false;
  }
  report->dir = pack_dir(root, source.pack_sha256);
  for (const Stage s : kStages) {
    StageResult r;
    r.stage = s;
    const bool asked =
        opts.only.empty() || std::find(opts.only.begin(), opts.only.end(), s) != opts.only.end();
    const std::string key = stage_key(s, opts);
    if (!asked) {
      r.note = "not asked for";
      report->stages.push_back(r);
      continue;
    }
    if (!opts.force && recorded_key(report->dir, stage_name(s)) == key) {
      r.note = "up to date";
      report->stages.push_back(r);
      continue;
    }
    const auto t0 = std::chrono::steady_clock::now();
    bool ok = false;
    switch (s) {
      case Stage::pack:
        ok = run_pack(source, report->dir, r, error);
        break;
      case Stage::textures:
        ok = run_textures(source, report->dir, opts, r, error);
        break;
      case Stage::lightmaps:
        ok = run_lightmaps(source, report->dir, r, error);
        break;
      case Stage::sounds:
        ok = run_sounds(source, report->dir, opts, r, error);
        break;
    }
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    r.ran = true;
    report->stages.push_back(r);
    if (!ok) {
      *error = std::string(stage_name(s)) + " stage: " + *error;
      return false;
    }
    if (!write_record(report->dir, stage_name(s), key)) {
      *error =
          "cannot record the " + std::string(stage_name(s)) + " stage in " + report->dir.string();
      return false;
    }
  }
  return true;
}

}  // namespace ge::classicenhance
