// The `classicenhance` executable, run through `ge tool classicenhance -- <command> ...`: the
// pipeline on the player's own classic packs (the shareware pack or the full game), from the
// command line. Every command prints one JSON object on stdout (what an agent reads) and exits 0
// when it did what was asked, 1 when the source was refused or a stage failed (the object's "error"
// says why and what to do), 2 on a usage error.
//
//   verify <path>                      find the packs in <path> and say which release each is
//   run <path> [--cache=<dir>] [--only=<stage,...>] [--force] [--scale=<n>] [--limit=<n>]
//       [--round=<n>] [--upscaler=network|kernel] [--weights=<file>] [--device=auto|gpu|cpu]
//                                      verify, then run the stages into the cache
//
// The textures (and the models' skins) are upscaled by the trained network (upscaler.hpp) at 4×:
// its weights are fetched once with curl into the cache's `weights/` folder and checked against
// their published SHA-256, or read from --weights. --upscaler=kernel, or another --scale, uses
// the edge-following kernel instead and fetches nothing. The network runs on the GPU
// (app/upscaler_gpu.hpp) when the build has one and the adapter is not a software one, else on
// the CPU; --device picks.
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
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "classicenhance/cache.hpp"
#include "classicenhance/pipeline.hpp"
#include "classicenhance/source.hpp"
#include "classicenhance/upscaler.hpp"
#ifdef GE_CLASSICENHANCE_GPU
#include "classicenhance/app/upscaler_gpu.hpp"
#endif

namespace fs = std::filesystem;
using namespace ge::classicenhance;

namespace {

constexpr const char* kHelp =
    "classicenhance: the player's classic packs (the shareware pack or the full game) enhanced "
    "into a local "
    "cache\n"
    "  verify <path>     find the pack in <path> (the release zip, an installed folder or its "
    "pak0.pak, with any later packs) and say which game (its profile) and release each is\n"
    "  run <path> [--cache=<dir>] [--only=pack,textures,lightmaps,sounds,models,pictures] "
    "[--force] "
    "[--scale=<1..8>] [--limit=<n>] [--round=<0..4>] [--upscaler=network|kernel] "
    "[--weights=<file>] [--device=auto|gpu|cpu]\n"
    "  status [--cache=<dir>]\n"
    "The cache defaults to GE_CLASSIC_CACHE, else the platform's per-user cache folder.\n"
    "The textures are upscaled by a trained network at 4x; its weights (CC0) are fetched once "
    "into the cache's weights/ folder. --upscaler=kernel needs no download.\n";

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
  std::string j = "{\"profile\":" + esc(std::string(s.profile->id)) +
                  ",\"route\":" + esc(route_name(s.route)) + ",\"path\":" + esc(s.path) +
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
  std::string cache, only, upscaler = "network", weights, device = "auto";
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
    if (value("cache", &f->cache) || value("only", &f->only) || value("weights", &f->weights)) {
      continue;
    }
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
    } else if (value("device", &f->device)) {
      if (f->device != "auto" && f->device != "gpu" && f->device != "cpu") {
        *error = "--device takes auto (the GPU unless it is a software one), gpu or cpu";
        return false;
      }
    } else if (value("upscaler", &f->upscaler)) {
      if (f->upscaler != "network" && f->upscaler != "kernel") {
        *error = "--upscaler takes network (the trained network) or kernel (no download)";
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

// The trained upscaler's weights: --weights, else the cache's copy, fetched when missing.
bool network(const Flags& f, Upscaler* out, std::string* note, std::string* error) {
  fs::path file = f.weights;
  if (file.empty()) {
    const fs::path root = f.cache.empty() ? default_cache_root() : fs::path(f.cache);
    file = root / "weights" / std::string(kUpscalerWeights.file);
    std::error_code ec;
    if (!fs::exists(file, ec)) {
      fs::create_directories(file.parent_path(), ec);
      const fs::path part = file.string() + ".part";
      const std::string cmd = "curl -fsSL --retry 2 -o \"" + part.string() + "\" \"" +
                              std::string(kUpscalerWeights.url) + "\"";
      if (std::system(cmd.c_str()) != 0) {
        fs::remove(part, ec);
        *error = "could not fetch the upscaler's weights from " +
                 std::string(kUpscalerWeights.url) +
                 "; check the network and that curl is installed, give a copy with --weights, or "
                 "run with --upscaler=kernel";
        return false;
      }
      fs::rename(part, file, ec);
    }
  }
  bool known = false;
  if (!open_upscaler(file, *out, &known, *error)) return false;
  if (f.weights.empty() && !known) {
    std::error_code ec;
    fs::remove(file, ec);
    *error = "the fetched upscaler weights' SHA-256 is " + out->digest + ", not the published " +
             std::string(kUpscalerWeights.sha256) + "; removed them, run again to fetch them again";
    return false;
  }
  *note = file.string() + (known ? "" : " (not the published weights)");
  return true;
}

}  // namespace

static int run_main(int argc, char** argv) {
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
  Upscaler upscaler;
  std::string weights;
  const bool pictures =
      opts.only.empty() ||
      std::find(opts.only.begin(), opts.only.end(), Stage::textures) != opts.only.end() ||
      std::find(opts.only.begin(), opts.only.end(), Stage::models) != opts.only.end() ||
      std::find(opts.only.begin(), opts.only.end(), Stage::pictures) != opts.only.end();
  if (f.upscaler == "network" && f.scale == 4 && pictures) {
    if (!network(f, &upscaler, &weights, &error)) return fail(error);
    opts.textures.network = &upscaler;
  }
#ifdef GE_CLASSICENHANCE_GPU
  std::unique_ptr<ge::gpu::Device> gpu_device;
  std::unique_ptr<GpuUpscaler> gpu;
  if (opts.textures.network && f.device != "cpu") {
    gpu_device = ge::gpu::create_device({.label = "classicenhance"});
    if (gpu_device && (f.device == "gpu" || !gpu_device->report.software)) {
      gpu = GpuUpscaler::create(*gpu_device, upscaler, error);
      if (!gpu && f.device == "gpu") return fail(error);
    }
    if (!gpu && f.device == "gpu") {
      return fail("--device=gpu but no GPU adapter could be made; run with --device=cpu");
    }
    if (gpu) {
      opts.textures.device = gpu.get();
      weights += " on " + ge::gpu::adapter_line(*gpu_device);
    }
  }
#else
  if (f.device == "gpu") return fail("--device=gpu but this build has no GPU (GE_GPU off)");
#endif
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
  std::printf("{\"ok\":%s,%s\"source\":%s,\"dir\":%s,\"upscaler\":%s,\"stages\":[%s]}\n",
              ok ? "true" : "false", ok ? "" : ("\"error\":" + esc(error) + ",").c_str(),
              source_json(src).c_str(), esc(report.dir.string()).c_str(),
              opts.textures.network ? esc(weights).c_str() : "\"kernel\"", stages.c_str());
  return ok ? 0 : 1;
}

// What nothing else caught ends the program here, with a line that says so, rather than in
// std::terminate's bare abort.
int main(int argc, char** argv) {
  try {
    return run_main(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(
        stderr,
        "classicenhance: stopped by an uncaught exception: %s. This is a bug: run the same "
        "command under lldb or gdb with a breakpoint on __cxa_throw to see where it "
        "was thrown.\n",
        e.what());
  } catch (...) {
    std::fprintf(
        stderr,
        "classicenhance: stopped by an uncaught exception that is not a std::exception. This "
        "is a bug: run the same command under lldb or gdb with a breakpoint on "
        "__cxa_throw to see where it was thrown.\n");
  }
  return 1;
}
