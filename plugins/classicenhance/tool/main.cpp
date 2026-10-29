// The `classicenhance` executable, run through `ge tool classicenhance -- <command> ...`: the
// pipeline on the player's own classic packs (the shareware pack or the full game), from the
// command line. Every command prints one JSON object on stdout (what an agent reads) and exits 0
// when it did what was asked, 1 when the source was refused or a stage failed (the object's "error"
// says why and what to do), 2 on a usage error.
//
//   verify <path>                      find the packs in <path> and say which release each is
//   run <path> [--cache=<dir>] [--only=<stage,...>] [--force] [--scale=<n>] [--limit=<n>]
//       [--round=<n>]
//                                      verify, then run the stages into the cache
//   status [--cache=<dir>]             the cache's pack folders and the stages each has done
//   help                               this list
//
// ge:summary The classicenhance tool: verify a source, run the pipeline, show the cache, as JSON
// ge:keywords classicenhance tool executable command verify run status json cache stages
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "classicenhance/cache.hpp"
#include "classicenhance/pipeline.hpp"
#include "classicenhance/source.hpp"

namespace fs = std::filesystem;
using namespace ge::classicenhance;

namespace {

constexpr const char* kHelp =
    "classicenhance: the player's classic packs (the shareware pack or the full game) enhanced "
    "into a local "
    "cache\n"
    "  verify <path>     find the pack in <path> (the release zip, an installed folder or its "
    "pak0.pak, with any later packs) and say which release each is\n"
    "  run <path> [--cache=<dir>] [--only=pack,textures,lightmaps,sounds,models,pictures] "
    "[--force] "
    "[--scale=<1..8>] [--limit=<n>] [--round=<0..4>]\n"
    "  status [--cache=<dir>]\n"
    "The cache defaults to GE_CLASSIC_CACHE, else the platform's per-user cache folder.\n";

std::string esc(const std::string& s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (uint8_t(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", unsigned(uint8_t(c)));
      out += buf;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

int fail(const std::string& error, int code = 1) {
  std::printf("{\"ok\":false,\"error\":%s}\n", esc(error).c_str());
  return code;
}

std::string source_json(const Source& s) {
  std::string j = "{\"route\":" + esc(route_name(s.route)) + ",\"path\":" + esc(s.path) +
                  ",\"pack_sha256\":" + esc(s.pack_sha256) +
                  ",\"pack_bytes\":" + std::to_string(s.pack.size()) +
                  ",\"pack\":" + (s.known ? esc(s.known->what) : std::string("null")) +
                  ",\"verified\":" + (s.known ? "true" : "false");
  if (!s.zip_sha256.empty()) {
    j += ",\"zip_sha256\":" + esc(s.zip_sha256) +
         ",\"zip\":" + (s.zip ? esc(s.zip->what) : std::string("null"));
  }
  if (!s.later.empty()) {
    j += ",\"later\":[";
    for (size_t i = 0; i < s.later.size(); i++) {
      const LaterPack& p = s.later[i];
      j += std::string(i ? "," : "") + "{\"name\":" + esc(p.name) + ",\"sha256\":" + esc(p.sha256) +
           ",\"bytes\":" + std::to_string(p.bytes.size()) +
           ",\"pack\":" + (p.known ? esc(p.known->what) : std::string("null")) +
           ",\"verified\":" + (p.known ? "true" : "false") + "}";
    }
    j += "]";
  }
  return j + "}";
}

struct Flags {
  std::vector<std::string> words;
  std::string cache, only;
  bool force = false;
  int scale = 4;
  int round = 0;
  uint32_t limit = 0;
};

bool parse(int argc, char** argv, Flags* f, std::string* error) {
  for (int i = 2; i < argc; i++) {
    const std::string a = argv[i];
    auto value = [&](const char* name, std::string* out) {
      const std::string p = std::string("--") + name + "=";
      if (a.rfind(p, 0) != 0) return false;
      *out = a.substr(p.size());
      return true;
    };
    std::string v;
    if (value("cache", &f->cache) || value("only", &f->only)) continue;
    if (a == "--force") {
      f->force = true;
    } else if (value("scale", &v)) {
      f->scale = std::atoi(v.c_str());
      if (f->scale < 1 || f->scale > 8) {
        *error = "--scale takes 1 to 8";
        return false;
      }
    } else if (value("round", &v)) {
      f->round = std::atoi(v.c_str());
      if (f->round < 0 || f->round > 4) {
        *error = "--round takes 0 (smooth shading only) to 4 (each triangle into 16)";
        return false;
      }
    } else if (value("limit", &v)) {
      f->limit = uint32_t(std::strtoul(v.c_str(), nullptr, 10));
    } else if (a.rfind("--", 0) == 0) {
      *error = "unknown flag " + a + "; `classicenhance help` lists them";
      return false;
    } else {
      f->words.push_back(a);
    }
  }
  return true;
}

int status(const Flags& f) {
  const fs::path root = f.cache.empty() ? default_cache_root() : fs::path(f.cache);
  std::string packs;
  std::error_code ec;
  if (fs::is_directory(root, ec)) {
    for (const auto& e : fs::directory_iterator(root, ec)) {
      if (!e.is_directory()) continue;
      std::string stages;
      for (const StageRecord& r : read_records(e.path())) {
        stages += std::string(stages.empty() ? "" : ",") + esc(r.stage);
      }
      packs += std::string(packs.empty() ? "" : ",") + "{\"dir\":" + esc(e.path().string()) +
               ",\"stages\":[" + stages + "]}";
    }
  }
  std::printf("{\"ok\":true,\"cache\":%s,\"packs\":[%s]}\n", esc(root.string()).c_str(),
              packs.c_str());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string cmd = argc > 1 ? argv[1] : "help";
  if (cmd == "help" || cmd == "--help" || cmd == "-h") {
    std::fputs(kHelp, stdout);
    return 0;
  }
  Flags f;
  std::string error;
  if (!parse(argc, argv, &f, &error)) return fail(error, 2);
  if (cmd == "status") return status(f);
  if (cmd != "verify" && cmd != "run") {
    return fail("unknown command '" + cmd + "'; `classicenhance help` lists them", 2);
  }
  if (f.words.size() != 1) {
    return fail(cmd + " takes one path: the release zip, an installed folder or its pak0.pak", 2);
  }
  Source src;
  if (!open_source(f.words[0], &src, &error)) return fail(error);
  if (cmd == "verify") {
    std::printf("{\"ok\":true,\"source\":%s}\n", source_json(src).c_str());
    return 0;
  }
  RunOptions opts;
  opts.force = f.force;
  opts.textures.scale = f.scale;
  opts.models.skin_scale = f.scale;
  opts.models.round = f.round;
  opts.texture_limit = f.limit;
  for (size_t at = 0; at < f.only.size();) {
    const size_t comma = std::min(f.only.find(',', at), f.only.size());
    Stage s;
    if (!stage_named(f.only.substr(at, comma - at), &s)) {
      return fail("--only names an unknown stage '" + f.only.substr(at, comma - at) +
                      "'; the stages are pack, textures, lightmaps, sounds, models",
                  2);
    }
    opts.only.push_back(s);
    at = comma + 1;
  }
  RunReport report;
  const bool ok = run_pipeline(src, f.cache, opts, &report, &error);
  std::string stages;
  for (const StageResult& r : report.stages) {
    char secs[32];
    std::snprintf(secs, sizeof secs, "%.2f", r.seconds);
    stages += std::string(stages.empty() ? "" : ",") + "{\"stage\":" + esc(stage_name(r.stage)) +
              ",\"ran\":" + (r.ran ? "true" : "false") + ",\"files\":" + std::to_string(r.files) +
              ",\"seconds\":" + secs + ",\"note\":" + esc(r.note) + "}";
  }
  std::printf("{\"ok\":%s,%s\"source\":%s,\"dir\":%s,\"stages\":[%s]}\n", ok ? "true" : "false",
              ok ? "" : ("\"error\":" + esc(error) + ",").c_str(), source_json(src).c_str(),
              esc(report.dir.string()).c_str(), stages.c_str());
  return ok ? 0 : 1;
}
