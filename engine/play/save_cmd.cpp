// The `play save` commands (saves.hpp): a run saved off an input log, a save loaded and played on,
// the check of a log's saves against the run never saved, and two saves compared.
//
//   play save write <log> --step=N --out=<file> [--title=..] [--place=..]
//   play save load <file> [--steps=N]        loaded, then the game's pilot plays N steps on
//   play save check <log> [--every=S]        saved every S seconds, each loaded and played on
//   play save diff <a> <b>                   chunk by chunk, then field by field through `state`
//
// ge:summary play save write, load, check and diff: the run's saves on the play command line
// ge:keywords play save write load check diff command-line json chunk state field
// ge:skill save agent-interface
// ge:test tests/play_saves_test.cpp
#include <chrono>
#include <fstream>
#include <sstream>

#include "core/num_text.hpp"
#include "play/saves.hpp"
#include "play/session.hpp"

namespace ge::play {

namespace {

int problem(std::ostream& out, int code, std::string_view what, std::string_view where,
            std::string_view next) {
  out << "{\"error\":" << quote(what) << ",\"where\":" << quote(where)
      << ",\"next\":" << quote(next) << "}\n";
  return code;
}

bool read_bytes(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), {});
  return true;
}

std::optional<InputLog> read_log(const std::string& path, std::string& error) {
  std::vector<uint8_t> bytes;
  if (!read_bytes(path, bytes)) {
    error = "cannot read " + path;
    return std::nullopt;
  }
  auto log = parse_inputs(std::string(bytes.begin(), bytes.end()), error);
  if (!log) error = path + ": " + error;
  return log;
}

std::string flag(const std::map<std::string, std::string>& flags, const std::string& name) {
  const auto it = flags.find(name);
  return it == flags.end() ? "" : it->second;
}

// A whole number flag, `fallback` when absent; false when it does not read.
bool count_flag(const std::map<std::string, std::string>& flags, const std::string& name,
                double fallback, double& out) {
  const std::string v = flag(flags, name);
  if (v.empty()) {
    out = fallback;
    return true;
  }
  char* end = nullptr;
  out = std::strtod(v.c_str(), &end);
  return end == v.c_str() + v.size() && out >= 0;
}

double ms_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

std::string chunks_json(std::span<const uint8_t> bytes) {
  save::FileReader r;
  if (!r.open(bytes)) return "";
  std::string out;
  for (const save::ChunkInfo& c : r.chunks()) {
    out += std::string(out.empty() ? "" : ",") + "{\"tag\":" + quote(save::tag_name(c.tag)) +
           ",\"instance\":" + std::to_string(c.instance) +
           ",\"version\":" + std::to_string(c.version) + ",\"bytes\":" + std::to_string(c.stored) +
           "}";
  }
  return out;
}

int write_cmd(const GameSpec& spec, const std::vector<std::string>& words,
              const std::map<std::string, std::string>& flags, std::ostream& out) {
  const std::string file = flag(flags, "out");
  double step = 0;
  if (words.size() != 2 || file.empty() || !count_flag(flags, "step", 0, step)) {
    return problem(out, 2, "save write takes an input log, --step and --out", "the command line",
                   "play save write runs/a.inputs --step=7200 --out=saves/a.save");
  }
  std::string error;
  const auto log = read_log(words[1], error);
  if (!log) return problem(out, 1, error, words[1], "pass an input log (ge sim writes one)");
  const auto at = static_cast<int64_t>(step);
  if (at > log->steps()) {
    return problem(out, 2,
                   "--step=" + std::to_string(at) + " is past the log's " +
                       std::to_string(log->steps()) + " steps",
                   "the command line", "pass a --step inside the log");
  }
  Setup setup;
  if (!setup_of(spec, *log, setup, error)) return problem(out, 1, error, words[1], "fix the log");
  Session s(spec, setup);
  if (!s.ok()) return problem(out, 1, s.error(), words[1], "fix the log's setup");
  for (int64_t i = 0; i < at; i++) s.step(log->at(i));
  save::Meta meta;
  meta.title = flag(flags, "title");
  meta.place = flag(flags, "place");
  std::vector<uint8_t> bytes;
  const auto t0 = std::chrono::steady_clock::now();
  if (!save_run(spec, setup, s.run(), at, meta, bytes, error)) {
    return problem(out, 1, error, spec.name,
                   "give the game's Run a save and a load (skill: save, docs/saves.md)");
  }
  const double save_ms = ms_since(t0);
  std::ofstream f(file, std::ios::binary);
  f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!f) return problem(out, 1, "cannot write " + file, file, "pass a writable --out");
  out << "{\"file\":" << quote(file) << ",\"step\":" << at << ",\"bytes\":" << bytes.size()
      << ",\"save_ms\":" << core::num::to_fixed(save_ms, 3)
      << ",\"digest\":" << quote(hex64(s.run().digest())) << ",\"chunks\":[" << chunks_json(bytes)
      << "]}\n";
  return 0;
}

int load_cmd(const GameSpec& spec, const std::vector<std::string>& words,
             const std::map<std::string, std::string>& flags, std::ostream& out) {
  double steps = 0;
  if (words.size() != 2 || !count_flag(flags, "steps", 0, steps)) {
    return problem(out, 2, "save load takes a save file and --steps", "the command line",
                   "play save load saves/a.save --steps=1200");
  }
  std::vector<uint8_t> bytes;
  if (!read_bytes(words[1], bytes))
    return problem(out, 1, "cannot read " + words[1], words[1], "pass a save file");
  std::string error;
  int64_t step = 0;
  const auto t0 = std::chrono::steady_clock::now();
  std::unique_ptr<Run> run = load_run(spec, bytes, step, error, words[1]);
  const double load_ms = ms_since(t0);
  if (!run) return problem(out, 1, error, words[1], "ge save inspect " + words[1]);
  const uint64_t loaded = run->digest();
  std::vector<double> in(spec.axes.size(), 0);
  int64_t played = 0;
  bool piloted = true;
  for (; played < static_cast<int64_t>(steps) && !run->over(); played++) {
    std::fill(in.begin(), in.end(), 0.0);
    piloted = run->pilot(in);
    for (size_t a = 0; a < in.size(); a++) in[a] = snap_axis(in[a], spec.axes[a].kind);
    run->step(in);
  }
  out << "{\"file\":" << quote(words[1]) << ",\"step\":" << step
      << ",\"load_ms\":" << core::num::to_fixed(load_ms, 3)
      << ",\"digest_loaded\":" << quote(hex64(loaded)) << ",\"played\":" << played
      << ",\"piloted\":" << (piloted ? "true" : "false")
      << ",\"over\":" << (run->over() ? "true" : "false")
      << ",\"digest\":" << quote(hex64(run->digest())) << ",\"report\":" << run->report() << "}\n";
  return 0;
}

int check_cmd(const GameSpec& spec, const std::vector<std::string>& words,
              const std::map<std::string, std::string>& flags, std::ostream& out) {
  double every = 0;
  if (words.size() != 2 || !count_flag(flags, "every", 10, every) || every <= 0) {
    return problem(out, 2, "save check takes an input log and --every (seconds)",
                   "the command line", "play save check runs/a.inputs --every=10");
  }
  std::string error;
  const auto log = read_log(words[1], error);
  if (!log) return problem(out, 1, error, words[1], "pass an input log (ge sim writes one)");
  std::vector<int64_t> at;
  const auto stride = static_cast<int64_t>(every * spec.hz);
  for (int64_t i = 0; i <= log->steps(); i += std::max<int64_t>(stride, 1)) at.push_back(i);
  const auto saved = check_saves(spec, *log, at, error);
  if (saved.empty())
    return problem(out, 1, error, words[1], "give the game's Run a save and a load");
  int parted = 0;
  std::string rows;
  for (const SavedAt& s : saved) {
    parted += s.parted >= 0;
    rows += std::string(rows.empty() ? "" : ",") + "{\"step\":" + std::to_string(s.step) +
            ",\"bytes\":" + std::to_string(s.bytes) +
            ",\"save_ms\":" + core::num::to_fixed(s.save_ms, 3) +
            ",\"load_ms\":" + core::num::to_fixed(s.load_ms, 3) +
            ",\"parted\":" + std::to_string(s.parted) + "}";
  }
  out << "{\"log\":" << quote(words[1]) << ",\"steps\":" << log->steps()
      << ",\"saves\":" << saved.size() << ",\"parted\":" << parted << ",\"at\":[" << rows << "]}\n";
  return parted ? 1 : 0;
}

int diff_cmd(const GameSpec& spec, const std::vector<std::string>& words, std::ostream& out) {
  if (words.size() != 3) {
    return problem(out, 2, "save diff takes two save files", "the command line",
                   "play save diff saves/a.save saves/b.save");
  }
  std::vector<uint8_t> a, b;
  if (!read_bytes(words[1], a))
    return problem(out, 1, "cannot read " + words[1], words[1], "pass a save");
  if (!read_bytes(words[2], b))
    return problem(out, 1, "cannot read " + words[2], words[2], "pass a save");
  save::FileReader ra, rb;
  if (!ra.open(a, words[1], spec.name))
    return problem(out, 1, ra.problem(), words[1], "ge save inspect " + words[1]);
  if (!rb.open(b, words[2], spec.name))
    return problem(out, 1, rb.problem(), words[2], "ge save inspect " + words[2]);
  // Chunk by chunk: every tag and instance on either side, the same when their bytes are.
  std::string chunks;
  int differ = 0;
  const auto row = [&](const save::ChunkInfo* x, const save::ChunkInfo* y) {
    const save::ChunkInfo& c = x ? *x : *y;
    const bool same = x && y && x->raw == y->raw &&
                      std::equal(a.begin() + static_cast<std::ptrdiff_t>(x->offset),
                                 a.begin() + static_cast<std::ptrdiff_t>(x->offset + x->raw),
                                 b.begin() + static_cast<std::ptrdiff_t>(y->offset));
    differ += !same;
    chunks += std::string(chunks.empty() ? "" : ",") +
              "{\"tag\":" + quote(save::tag_name(c.tag) + "#" + std::to_string(c.instance)) +
              ",\"a\":" + (x ? std::to_string(x->raw) : "null") +
              ",\"b\":" + (y ? std::to_string(y->raw) : "null") +
              ",\"same\":" + (same ? "true" : "false") + "}";
  };
  for (const save::ChunkInfo& c : ra.chunks()) {
    if (c.tag != save::kMetaTag) row(&c, rb.find(c.tag, c.instance));
  }
  for (const save::ChunkInfo& c : rb.chunks()) {
    if (c.tag != save::kMetaTag && !ra.find(c.tag, c.instance)) row(nullptr, &c);
  }
  // Field by field: both loaded, their `state` compared.
  std::string error;
  int64_t sa = 0, sb = 0;
  std::unique_ptr<Run> la = load_run(spec, a, sa, error, words[1]);
  if (!la) return problem(out, 1, error, words[1], "ge save inspect " + words[1]);
  std::unique_ptr<Run> lb = load_run(spec, b, sb, error, words[2]);
  if (!lb) return problem(out, 1, error, words[2], "ge save inspect " + words[2]);
  Divergence d;
  compare_states(la->state(), lb->state(), d);
  std::string fields;
  for (const FieldDiff& f : d.fields) {
    fields += std::string(fields.empty() ? "" : ",") + "{\"key\":" + quote(f.key) +
              ",\"a\":" + quote(f.a) + ",\"b\":" + quote(f.b) + "}";
  }
  const bool same = differ == 0 && d.fields.empty();
  out << "{\"same\":" << (same ? "true" : "false") << ",\"step_a\":" << sa << ",\"step_b\":" << sb
      << ",\"setup_differs\":" << (ra.meta().setup != rb.meta().setup ? "true" : "false")
      << ",\"chunks_differ\":" << differ << ",\"chunks\":[" << chunks << "],\"fields\":[" << fields
      << "],\"more_fields\":" << d.more_fields << "}\n";
  return same ? 0 : 1;
}

}  // namespace

int save_command(const GameSpec& spec, const std::vector<std::string>& words,
                 const std::map<std::string, std::string>& flags, std::ostream& out) {
  const std::string sub = words.empty() ? "" : words[0];
  if (sub == "write") return write_cmd(spec, words, flags, out);
  if (sub == "load") return load_cmd(spec, words, flags, out);
  if (sub == "check") return check_cmd(spec, words, flags, out);
  if (sub == "diff") return diff_cmd(spec, words, out);
  return problem(out, 2, "save takes write, load, check or diff", "the command line",
                 "play save write <log> --step=N --out=<file>");
}

}  // namespace ge::play
