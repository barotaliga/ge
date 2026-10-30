// `shooters_edge_server`: Shooter's Edge's headless dedicated server. It runs the server's host
// (server/host.hpp) on the run clock with its bot slots filled, prints a status line every few
// seconds of server time and the run's digest at the end, and writes the run's input log; given a
// log, it replays it and names the first frame that leaves it. With `--realtime` the frames are
// paced to the wall clock (a server people play on); without, it runs as fast as it can (a test,
// a lab, `ge server`). The run itself never reads the wall clock: pacing only waits between
// frames.
//
// Until the protocols land (M18g, M18h) no client can join over the network, so its slots are
// bots: the stand-in wanderer until Shooter's Edge's bots land (M18r).
//
// ge:summary The headless dedicated server: bots, status lines, the digest, the input log
// ge:keywords dedicated-server shooters_edge_server headless main bots status digest input-log
// ge:keywords replay realtime flags
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "core/num.hpp"
#include "shooters_edge/server/host.hpp"
#include "shooters_edge/server/stand_in.hpp"

namespace {

namespace se = shooters_edge::server;

struct Flags {
  int bots = 8;
  int maxclients = 0;  // 0: as many as the bots, at least 8
  double seconds = 60;
  uint32_t seed = 1;
  std::string map = "arena";
  std::string log;
  std::string replay;
  double status_every = 5;
  bool realtime = false;
};

const char* kHelp =
    "shooters_edge_server: Shooter's Edge's headless dedicated server.\n"
    "\n"
    "  --bots=N         bot slots to fill (default 8; stand-in wanderers until M18r)\n"
    "  --maxclients=N   player slots, 1..32 (default: the bots, at least 8)\n"
    "  --for=S          seconds of server time to run (default 60; 0 runs until stopped)\n"
    "  --seed=N         the run's seed (the bots' draws; default 1)\n"
    "  --map=NAME       the map (default arena; compiled maps load with M18b)\n"
    "  --log=PATH       write the run's input log here\n"
    "  --replay=PATH    replay an input log and name the first frame that leaves it\n"
    "  --status=S       a status line every S seconds of server time (default 5; 0 none)\n"
    "  --realtime       pace the frames to the wall clock\n"
    "  --help           this\n";

bool parse(int argc, char** argv, Flags& f, std::string& error) {
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    const size_t eq = a.find('=');
    const std::string key = a.substr(0, eq);
    const std::string val = eq == std::string::npos ? "" : a.substr(eq + 1);
    char* end = nullptr;
    const auto num = [&](double& out) {
      out = std::strtod(val.c_str(), &end);
      if (val.empty() || *end != '\0') {
        error = "flag " + key;
        error.append(" needs a number, got '").append(val).append("'");
        return false;
      }
      return true;
    };
    double v = 0;
    if (key == "--bots" || key == "--maxclients" || key == "--seed") {
      if (!num(v)) return false;
      if (key == "--bots") f.bots = static_cast<int>(v);
      if (key == "--maxclients") f.maxclients = static_cast<int>(v);
      if (key == "--seed") f.seed = static_cast<uint32_t>(v);
    } else if (key == "--for") {
      if (!num(f.seconds)) return false;
    } else if (key == "--status") {
      if (!num(f.status_every)) return false;
    } else if (key == "--map") {
      f.map = val;
    } else if (key == "--log") {
      f.log = val;
    } else if (key == "--replay") {
      f.replay = val;
    } else if (key == "--realtime") {
      f.realtime = true;
    } else {
      error = "unknown flag '" + a + "'";
      return false;
    }
  }
  if (f.bots < 0 || f.seconds < 0) {
    error = "--bots and --for must not be negative";
    return false;
  }
  if (f.maxclients == 0) f.maxclients = f.bots > 8 ? f.bots : 8;
  return true;
}

int replay(const Flags& f) {
  std::ifstream in(f.replay, std::ios::binary);
  if (!in) {
    std::fprintf(stderr, "shooters_edge_server: cannot read %s; next: check the path\n",
                 f.replay.c_str());
    return 1;
  }
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),
                                   std::istreambuf_iterator<char>()};
  se::InputLog log;
  std::string error;
  if (!se::read_input_log(bytes, log, error)) {
    std::fprintf(stderr, "shooters_edge_server: %s: %s\n", f.replay.c_str(), error.c_str());
    return 1;
  }
  int diverged = -1;
  std::vector<uint64_t> hashes;
  if (!se::replay_input_log(log, diverged, error, &hashes)) {
    std::fprintf(stderr, "shooters_edge_server: %s: %s\n", f.replay.c_str(), error.c_str());
    return 1;
  }
  const uint64_t last = hashes.empty() ? 0 : hashes.back();
  if (diverged >= 0) {
    std::printf(
        "replay: %s leaves the log at frame %d of %zu; "
        "next: diff the state there with the run that wrote it\n",
        f.replay.c_str(), diverged, log.frames.size());
    return 2;
  }
  std::printf("replay: %s agrees on all %zu frames; digest %016llx\n", f.replay.c_str(),
              log.frames.size(), static_cast<unsigned long long>(last));
  return 0;
}

int run(const Flags& f) {
  se::Host h;
  se::HostSettings s;
  s.maxclients = f.maxclients;
  s.max_edicts = 64 + 16 * f.maxclients < 1024 ? 64 + 16 * f.maxclients : 1024;
  s.seed = f.seed;
  s.map = f.map;
  s.record = !f.log.empty();
  std::string error;
  if (!se::host_create(h, s, error)) {
    std::fprintf(stderr, "shooters_edge_server: %s\n", error.c_str());
    return 1;
  }
  if (f.bots > f.maxclients) {
    std::fprintf(stderr,
                 "shooters_edge_server: %d bots do not fit %d slots; next: raise --maxclients\n",
                 f.bots, f.maxclients);
    return 1;
  }
  std::vector<se::StandIn> stand_ins(static_cast<size_t>(f.bots));
  for (int b = 0; b < f.bots; b++) {
    stand_ins[static_cast<size_t>(b)] = se::make_stand_in(f.seed, b);
    se::host_add_bot(h, se::stand_in_source(stand_ins[static_cast<size_t>(b)]));
  }
  char span[32] = "until stopped";
  if (f.seconds > 0) std::snprintf(span, sizeof span, "%g s", f.seconds);
  std::printf("shooters_edge_server: map %s, %d of %d slots filled by bots, seed %u, %s\n",
              f.map.c_str(), f.bots, f.maxclients, f.seed, span);
  std::fflush(stdout);

  const double step = h.sv.set.mintic;
  const auto frames = static_cast<int64_t>(ge::core::num::round(f.seconds / step));
  const auto status_frames = static_cast<int64_t>(ge::core::num::round(f.status_every / step));
  const auto start = std::chrono::steady_clock::now();
  for (int64_t n = 0; f.seconds == 0 || n < frames; n++) {
    if (f.realtime) {
      const auto due = start + std::chrono::duration<double>(static_cast<double>(n) * step);
      std::this_thread::sleep_until(due);
    }
    se::host_frame(h, nullptr, 0);
    if (status_frames > 0 && (n + 1) % status_frames == 0) {
      std::printf("t=%.1f s frame=%lld players=%d commands=%lld hash=%016llx\n",
                  h.sv.time - h.sv.set.start_time, static_cast<long long>(h.sv.frame),
                  se::host_count(h, se::SlotKind::bot),
                  static_cast<long long>(h.sv.counts.commands),
                  static_cast<unsigned long long>(ge::shooter::server_hash(h.sv)));
      std::fflush(stdout);
    }
  }
  const double wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("done: %lld frames, server time %.3f s, wall %.2f s, digest %016llx\n",
              static_cast<long long>(h.sv.frame), h.sv.time, wall,
              static_cast<unsigned long long>(ge::shooter::server_hash(h.sv)));
  if (!f.log.empty()) {
    const std::vector<uint8_t> bytes = se::write_input_log(h.log);
    std::ofstream out(f.log, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out) {
      std::fprintf(stderr, "shooters_edge_server: cannot write %s; next: check the directory\n",
                   f.log.c_str());
      return 1;
    }
    std::printf("input log: %s (%zu frames, %zu bytes); replay it with --replay=%s\n",
                f.log.c_str(), h.log.frames.size(), bytes.size(), f.log.c_str());
  }
  return 0;
}

}  // namespace

static int run_main(int argc, char** argv) {
  for (int i = 1; i < argc; i++)
    if (std::strcmp(argv[i], "--help") == 0) {
      std::fputs(kHelp, stdout);
      return 0;
    }
  Flags f;
  std::string error;
  if (!parse(argc, argv, f, error)) {
    std::fprintf(stderr, "shooters_edge_server: %s; next: shooters_edge_server --help\n",
                 error.c_str());
    return 1;
  }
  return f.replay.empty() ? run(f) : replay(f);
}

// What nothing else caught ends the program here, with a line that says so, rather than in
// std::terminate's bare abort.
int main(int argc, char** argv) {
  try {
    return run_main(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(
        stderr,
        "shooters_edge_server: stopped by an uncaught exception: %s. This is a bug: run the same "
        "command under lldb or gdb with a breakpoint on __cxa_throw to see where it "
        "was thrown.\n",
        e.what());
  } catch (...) {
    std::fprintf(
        stderr,
        "shooters_edge_server: stopped by an uncaught exception that is not a std::exception. This "
        "is a bug: run the same command under lldb or gdb with a breakpoint on "
        "__cxa_throw to see where it was thrown.\n");
  }
  return 1;
}
