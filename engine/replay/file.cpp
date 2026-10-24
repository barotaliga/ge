// The replay file's writer, parser and I/O (file.hpp).
//
// ge:summary The replay file's text writer and parser, the version check, save and load
// ge:keywords replay file parse_replay to_text plays_on save_replay load_replay mark clip
// ge:skill replay
// ge:test tests/replay_file_test.cpp
#include "replay/file.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "core/num_text.hpp"

namespace ge::replay {

namespace {

bool read_number(std::string_view word, double& out) {
  const std::string s(word);
  if (s.empty()) return false;
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size() && out == out;
}

bool read_step(std::string_view word, int64_t& out) {
  double v = 0;
  if (!read_number(word, v) || v != static_cast<double>(static_cast<int64_t>(v))) return false;
  out = static_cast<int64_t>(v);
  return true;
}

std::vector<std::string_view> words_of(std::string_view line) {
  std::vector<std::string_view> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) i++;
    const size_t from = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') i++;
    if (i > from) out.push_back(line.substr(from, i - from));
  }
  return out;
}

}  // namespace

int64_t ReplayFile::in() const { return std::clamp<int64_t>(clip_in, 0, log.steps()); }

int64_t ReplayFile::out() const {
  const int64_t end = log.steps();
  const int64_t o = clip_out < 0 ? end : std::min(clip_out, end);
  return std::max(o, in());
}

bool mark_kind_ok(std::string_view kind) {
  if (kind.empty()) return false;
  return std::all_of(kind.begin(), kind.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
  });
}

std::string to_text(const ReplayFile& file) {
  using core::num::to_string;
  std::string out = "ge-replay " + std::to_string(kReplayVersion) + "\n";
  out += "game_version " + std::to_string(file.game_version) + "\n";
  if (!file.recorded.empty()) {
    std::string when = file.recorded;
    std::replace(when.begin(), when.end(), '\n', ' ');
    std::replace(when.begin(), when.end(), '\r', ' ');
    out += "recorded " + when + "\n";
  }
  if (file.clip_in != 0 || file.clip_out >= 0) {
    out += "clip " + std::to_string(file.clip_in) + " " + std::to_string(file.clip_out) + "\n";
  }
  for (const Mark& m : file.marks) {
    out += "mark " + std::to_string(m.step) + " " + m.kind + " " + to_string(m.weight) + "\n";
  }
  out += "inputs\n";
  out += play::to_text(file.log);
  return out;
}

std::optional<ReplayFile> parse_replay(std::string_view text, std::string& error) {
  ReplayFile file;
  bool versioned = false, versioned_game = false;
  int line_no = 0;
  size_t at = 0;
  const auto fail = [&](const std::string& what) {
    error = "line " + std::to_string(line_no) + ": " + what;
    return std::nullopt;
  };
  while (at < text.size()) {
    const size_t nl = text.find('\n', at);
    const std::string_view line = text.substr(at, nl == std::string_view::npos ? nl : nl - at);
    at = nl == std::string_view::npos ? text.size() : nl + 1;
    line_no++;
    const std::vector<std::string_view> w = words_of(line);
    if (w.empty() || w[0][0] == '#') continue;
    if (!versioned) {
      if (w[0] != "ge-replay") return fail("a replay file starts `ge-replay <version>`");
      double v = 0;
      if (w.size() != 2 || !read_number(w[1], v) || v != kReplayVersion) {
        return fail("this build reads replay files of version " + std::to_string(kReplayVersion));
      }
      versioned = true;
      continue;
    }
    if (w[0] == "inputs") {
      if (!versioned_game) return fail("`game_version <n>` must come before `inputs`");
      std::string inner;
      std::optional<play::InputLog> log = play::parse_inputs(text.substr(at), inner);
      if (!log) {
        error = "the inputs after line " + std::to_string(line_no) + ": " + inner;
        return std::nullopt;
      }
      file.log = std::move(*log);
      std::stable_sort(file.marks.begin(), file.marks.end(),
                       [](const Mark& a, const Mark& b) { return a.step < b.step; });
      return file;
    }
    if (w[0] == "game_version") {
      double v = 0;
      if (w.size() != 2 || !read_number(w[1], v) || v != static_cast<int32_t>(v)) {
        return fail("`game_version` takes one whole number");
      }
      file.game_version = static_cast<int32_t>(v);
      versioned_game = true;
    } else if (w[0] == "recorded") {
      const size_t from = line.find("recorded") + 8;
      std::string rest(line.substr(from));
      while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.erase(0, 1);
      while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\r')) rest.pop_back();
      file.recorded = rest;
    } else if (w[0] == "clip") {
      if (w.size() != 3 || !read_step(w[1], file.clip_in) || !read_step(w[2], file.clip_out) ||
          file.clip_in < 0 || file.clip_out < -1) {
        return fail("`clip <in> <out>` takes two steps (out -1: the end)");
      }
    } else if (w[0] == "mark") {
      Mark m;
      if (w.size() != 4 || !read_step(w[1], m.step) || m.step < 0 || !mark_kind_ok(w[2]) ||
          !read_number(w[3], m.weight)) {
        return fail("`mark <step> <kind> <weight>`: a step, a kind of a-z 0-9 _, a number");
      }
      m.kind = std::string(w[2]);
      file.marks.push_back(std::move(m));
    } else {
      return fail("unknown line `" + std::string(w[0]) +
                  "`: expected game_version, recorded, clip, mark or inputs");
    }
  }
  error = versioned ? "the file ends before its `inputs` line" : "the file is empty";
  return std::nullopt;
}

bool plays_on(const ReplayFile& file, const play::GameSpec& spec, std::string& error) {
  if (!play::fits(file.log, spec, error)) return false;
  if (file.game_version != spec.version) {
    error = "recorded by " + std::string(spec.name) + " version " +
            std::to_string(file.game_version) + ", and this build plays version " +
            std::to_string(spec.version) +
            ": a change of the game's behaviour plays a different run, so it is refused "
            "(pin the old behaviour to keep such replays playing)";
    return false;
  }
  return true;
}

bool save_replay(const std::string& path, const ReplayFile& file, std::string& error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "cannot write " + path + ": does its directory exist?";
    return false;
  }
  out << to_text(file);
  if (!out.good()) {
    error = "writing " + path + " failed part way (the disk full?)";
    return false;
  }
  return true;
}

std::optional<ReplayFile> load_replay(const std::string& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot read " + path + ": check the path";
    return std::nullopt;
  }
  std::stringstream text;
  text << in.rdbuf();
  std::string inner;
  std::optional<ReplayFile> file = parse_replay(text.str(), inner);
  if (!file) error = path + ": " + inner;
  return file;
}

}  // namespace ge::replay
