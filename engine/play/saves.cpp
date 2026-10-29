// The run saved and loaded (saves.hpp): the meta and the PLAY chunk, the setup read back, and the
// check of every saved step against the straight run's digests.
//
// ge:summary The run's save and load on the play contract, and check_saves
// ge:keywords play save load check_saves meta setup digest
// ge:skill save agent-interface
// ge:test tests/play_saves_test.cpp
#include "play/saves.hpp"

#include <chrono>

#include "play/session.hpp"

namespace ge::play {

namespace {

struct PlayChunk {
  int64_t step = 0;
  template <class Io>
  friend void describe(Io& io, PlayChunk& v) {
    io(v.step);
  }
};

// How far a run is played before a save is loaded into it in the check.
constexpr int64_t kDirtySteps = 360;

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// One step of `log` at `i`, each value on its axis's grid.
void step_on(const GameSpec& spec, Run& run, const InputLog& log, int64_t i,
             std::vector<double>& in) {
  const std::span<const double> v = log.at(i);
  in.assign(spec.axes.size(), 0);
  for (size_t a = 0; a < in.size() && a < v.size(); a++) in[a] = snap_axis(v[a], spec.axes[a].kind);
  run.step(in);
}

}  // namespace

bool save_run(const GameSpec& spec, const Setup& setup, const Run& run, int64_t step,
              save::Meta meta, std::vector<uint8_t>& out, std::string& error) {
  meta.game = spec.name;
  meta.kind = save::Kind::run;
  meta.play_s = static_cast<double>(step) / spec.hz;
  InputLog header = new_log(spec, setup);
  meta.setup = to_text(header);
  save::FileWriter w(out, meta);
  w.put(kPlayTag, 0, kPlayVersion, PlayChunk{step});
  if (!run.save(w)) {
    error = std::string("the game ") + spec.name + " keeps no saves (its Run::save returns false)";
    return false;
  }
  return w.finish(&error);
}

namespace {

// The save's setup and step read, and `run` (open on that setup, or null to open one here) loaded
// from it.
std::unique_ptr<Run> load_into(const GameSpec& spec, save::FileReader& r, std::unique_ptr<Run> run,
                               int64_t& step, std::string& error, std::string_view name) {
  std::string why;
  const auto header = parse_inputs(r.meta().setup, why);
  Setup setup;
  if (!header || !setup_of(spec, *header, setup, why)) {
    error = std::string(name) + ": the setup in its meta does not open: " + why;
    return nullptr;
  }
  PlayChunk play;
  if (!r.get(kPlayTag, 0, kPlayVersion, play)) {
    error = r.problem();
    return nullptr;
  }
  if (!run) run = spec.open(setup, why);
  if (!run) {
    error = std::string(name) + ": the game does not open its setup: " + why;
    return nullptr;
  }
  if (!run->load(r, why)) {
    error = std::string(name) + ": " + why;
    return nullptr;
  }
  step = play.step;
  return run;
}

}  // namespace

std::unique_ptr<Run> load_run(const GameSpec& spec, std::span<const uint8_t> bytes, int64_t& step,
                              std::string& error, std::string_view name) {
  save::FileReader r;
  if (!r.open(bytes, name, spec.name)) {
    error = r.problem();
    return nullptr;
  }
  return load_into(spec, r, nullptr, step, error, name);
}

std::vector<SavedAt> check_saves(const GameSpec& spec, const InputLog& log,
                                 std::span<const int64_t> at, std::string& error) {
  Setup setup;
  if (!setup_of(spec, log, setup, error)) return {};
  std::unique_ptr<Run> straight = spec.open(setup, error);
  if (!straight) return {};
  const int64_t n = log.steps();
  std::vector<uint64_t> digests(static_cast<size_t>(n) + 1);
  std::vector<double> in;
  std::vector<SavedAt> out;
  std::vector<uint8_t> bytes;
  digests[0] = straight->digest();
  size_t next = 0;
  // The straight run is saved as it passes each step of `at` (in order), and played on.
  std::vector<std::vector<uint8_t>> files(at.size());
  for (int64_t i = 0; i <= n; i++) {
    while (next < at.size() && at[next] == i) {
      SavedAt s;
      s.step = i;
      const auto t0 = std::chrono::steady_clock::now();
      if (!save_run(spec, setup, *straight, i, save::Meta{}, files[next], error)) return {};
      s.save_ms = ms_since(t0);
      s.bytes = files[next].size();
      out.push_back(s);
      next++;
    }
    if (i == n) break;
    step_on(spec, *straight, log, i, in);
    digests[static_cast<size_t>(i) + 1] = straight->digest();
  }
  // The straight run's end as a save: a loaded run that reaches the end with the same digests
  // must also save to the same bytes, which catches a field the digest never folds in.
  std::vector<uint8_t> end_straight, end_loaded;
  if (!save_run(spec, setup, *straight, n, save::Meta{}, end_straight, error)) return {};
  for (size_t k = 0; k < out.size(); k++) {
    SavedAt& s = out[k];
    save::FileReader r;
    if (r.open(files[k]))
      for (const save::ChunkInfo& c : r.chunks())
        s.chunks.emplace_back(save::tag_name(c.tag) + "#" + std::to_string(c.instance), c.stored);
    const auto t0 = std::chrono::steady_clock::now();
    int64_t step = 0;
    if (!load_run(spec, files[k], step, error)) return {};
    s.load_ms = ms_since(t0);
    const auto t1 = std::chrono::steady_clock::now();
    std::unique_ptr<Run> loaded = spec.open(setup, error);
    s.open_ms = ms_since(t1);
    if (!loaded) return {};
    // Loaded into a run already played on (not a fresh one), so a field left out of a describe
    // keeps a value of its own instead of happening to start as the save's.
    for (int64_t i = 0; i < kDirtySteps && i < n; i++) step_on(spec, *loaded, log, i, in);
    if (!r.open(files[k])) {
      error = r.problem();
      return {};
    }
    loaded = load_into(spec, r, std::move(loaded), step, error, "save");
    if (!loaded) return {};
    for (int64_t i = step;; i++) {
      if (loaded->digest() != digests[static_cast<size_t>(i)]) {
        s.parted = i;
        break;
      }
      if (i == n) {
        if (!save_run(spec, setup, *loaded, n, save::Meta{}, end_loaded, error)) return {};
        if (end_loaded != end_straight) s.parted = n;
        break;
      }
      step_on(spec, *loaded, log, i, in);
    }
  }
  return out;
}

}  // namespace ge::play
