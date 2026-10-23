// THE LAB CONTRACT: how anything in the tree (an engine subsystem, a plugin, a game) declares a
// lab, and the context a lab runs in. A lab answers ONE question about its subject by running
// the real code and writing what came back as numbers, tables and captioned pictures, so an
// agent can measure and LOOK instead of guessing. Labs are built once, at engine level, so
// every subsystem, plugin and game plugs into the same contract (docs/labs.md is the design).
//
// A lab is a `LabSpec` (what it is, which flags it takes, how to run it at smoke size) and a
// `run` function over a `Lab&`. It registers itself with `GE_LAB(spec)` in a `<name>_lab.cpp`
// file; CMake globs every `*_lab.cpp` under engine/, plugins/ and games/ into the `labs`
// executable, so nothing is registered by hand and a plugin or a game adds labs by adding
// files. The runner (runner.hpp) parses the flags against the spec, runs it, and writes its
// report (report.cpp): stdout for the reader, `report.json`, `metrics.tsv` and the contact
// sheet `index.md` under previews/<lab>/ for the agent and the PR's before-and-after.
//
// THE LAB'S DIGEST is what makes every lab a determinism test for free: an FNV-1a 64 over the
// deterministic metrics' bits, the table cells and every picture's pixels, in the order the
// lab wrote them. `--repeat=2` runs a lab twice in one process and fails if the digests
// differ; `--check-digests` compares them with a committed baseline across compilers.
//
// ge:summary The lab contract: LabSpec, the Lab context, GE_LAB registration, the lab kinds
// ge:keywords lab labs registry contract spec flags metric table shot caption digest
// ge:keywords screenshot simulation scenario drive bot playtest bench attach file wav
// ge:keywords distance-map depth
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "lab/raster.hpp"

namespace ge::lab {

// What a lab exercises. A lab may write numbers AND pictures; the kind says how it gets them.
enum class LabKind : uint8_t {
  sim,       // headless runs of the real code over seeds or a sweep; numbers and tables
  scenario,  // one staged situation (a moment, a scripted input) measured exactly
  picture,   // a captioned picture of a subject: a CPU raster now, a GPU frame from M7
  drive,     // a pilot (the bot or an agent over stdio) plays the game; the run is its report
  bench,     // what something costs; wall-clock metrics, never in the digest
};

std::string_view kind_name(LabKind kind);

// One flag a lab takes beyond the runner's own (runner.hpp lists those). `value` is the
// default as it would be typed; a flag whose default is "" and `boolean` true is a switch.
struct LabFlag {
  const char* name;
  const char* value;
  const char* help;
  bool boolean = false;
};

class Lab;

struct LabSpec {
  const char* id;  // the name on the command line: `make lab LAB=<id>`
  LabKind kind;
  const char* owner;     // "engine", "plugin:<name>" or "game:<name>"
  const char* subject;   // the files or the system it measures
  const char* skill;     // the skill that says how to read what comes back
  const char* question;  // the one question it answers, a sentence
  std::vector<LabFlag> flags;
  const char* smoke;  // the flags CI runs it with (`labs --all --smoke`): seconds, not minutes
  void (*run)(Lab& lab);
};

// Every registered lab, in id order.
const std::vector<const LabSpec*>& registered_labs();
const LabSpec* find_lab(std::string_view id);
// The source file a lab was registered from (the compiler's __FILE__, so most often absolute), or
// "" for a lab registered without one. `ge digest --affected` maps a change to its labs by it.
const char* lab_source(std::string_view id);

struct LabRegister {
  explicit LabRegister(const LabSpec& spec, const char* source = "");
};

#define GE_LAB_CAT2(a, b) a##b
#define GE_LAB_CAT(a, b) GE_LAB_CAT2(a, b)
// Register `spec` (a LabSpec with static storage) with the runner.
#define GE_LAB(spec) static ::ge::lab::LabRegister GE_LAB_CAT(ge_lab_reg_, __LINE__)(spec, __FILE__)

// Whether a number belongs to the digest. Wall-clock times and anything else that differs run
// to run are `measured`: printed and compared, never hashed.
enum class Det : uint8_t { deterministic, measured };

struct Metric {
  std::string key;  // kebab, stable across runs: it is what `--compare` matches on
  double value;
  std::string unit;  // "m", "s", "ms", "count", "%"
  Det det;
};

struct LabTable {
  std::string title;
  std::vector<std::string> columns;
  std::vector<std::vector<std::string>> rows;
};

// HOW FAR AWAY each pixel of a picture is, m, from the camera that took it (+infinity for the
// sky): a GPU lab reads it off the frame's depth (render::read_distance) and hands it with the
// shot, so a diff (diff.hpp) can weigh a change by how near the player it is. It may be smaller
// than the picture (the frame's render scale); a reader takes the nearest sample.
struct DistanceMap {
  int width = 0;
  int height = 0;
  std::vector<float> metres;  // width * height, row-major from the top
};

// A SCREENSHOT'S WORDS. Every picture a lab writes carries them: burned into a caption band
// under the image (so a picture pasted alone still says what it is), written beside it as
// JSON, and listed in the lab's contact sheet. Write them for a reader who sees only this
// picture: what it shows, under which inputs, and what to look for.
struct Shot {
  std::string id;        // file name stem: previews/<lab>/<id>.png
  std::string title;     // one line, upper case on the band: "SEED 38 FROM ABOVE"
  std::string caption;   // what the picture shows, a sentence or two
  std::string look_for;  // what a reviewer checks it for ("gates alternate red and blue")
  std::vector<std::pair<std::string, std::string>> inputs;  // seed, version, flags in force
};

// `picture` with its shot's words burned into a caption band under it: the title, the caption,
// what to look for and the inputs. Every lab shot is written so, and so is `ge shot`'s still.
Picture framed(const Shot& shot, const Picture& picture);

// THE CONTEXT a lab runs in: its flags, and the sinks for what it found.
class Lab {
 public:
  Lab(const LabSpec& spec, std::vector<std::pair<std::string, std::string>> args);

  const LabSpec& spec() const { return *spec_; }

  // A flag's value as typed (its default when not given).
  const std::string& arg(std::string_view name) const;
  double arg_num(std::string_view name) const;
  bool arg_bool(std::string_view name) const;
  // A comma-separated flag as numbers ("38,7,1").
  std::vector<double> arg_nums(std::string_view name) const;

  // What the lab found. All of them feed the report; the deterministic ones feed the digest.
  void metric(std::string key, double value, std::string unit, Det det = Det::deterministic);
  // A table of times or anything else that differs run to run is `measured`: shown, not hashed.
  void table(LabTable table, Det det = Det::deterministic);
  void note(std::string line);
  // A picture whose pixels differ run to run or machine to machine (a GPU frame: each adapter
  // rasterises in its own way) is `measured`: its id is hashed, its pixels are not.
  // `distance`, when given, is how far away each pixel is: written beside the picture as
  // <id>.distance for a diff, never in the digest.
  void shot(Shot shot, const Picture& picture, Det det = Det::deterministic,
            DistanceMap distance = {});
  // A file written beside the report (previews/<lab>/<name>): a rendered sound's WAV, anything a
  // reader opens with another tool. Its bytes are in the digest unless it is `measured`.
  void attach(std::string name, std::vector<uint8_t> bytes, Det det = Det::deterministic);
  // A failure in the diagnostics' shape: what failed, and the next action. The run carries on
  // so every failure is named; the runner exits non-zero.
  void fail(const std::string& what, const std::string& next_action);

  // What was collected, for the report writer and the runner.
  struct Taken {
    Shot shot;
    Picture picture;  // with its caption band
    Det det;
    int width = 0, height = 0;  // the picture's own size, the band left out (the top-left)
    DistanceMap distance;
  };
  const std::vector<Metric>& metrics() const { return metrics_; }
  const std::vector<LabTable>& tables() const { return tables_; }
  const std::vector<std::string>& notes() const { return notes_; }
  const std::vector<Taken>& shots() const { return shots_; }
  struct File {
    std::string name;
    std::vector<uint8_t> bytes;
    Det det;
  };
  const std::vector<File>& files() const { return files_; }
  const std::vector<std::string>& failures() const { return failures_; }
  const std::vector<std::pair<std::string, std::string>>& args() const { return args_; }
  uint64_t digest() const { return digest_; }

 private:
  void hash(const void* bytes, size_t n);
  const LabSpec* spec_;
  std::vector<std::pair<std::string, std::string>> args_;
  std::vector<Metric> metrics_;
  std::vector<LabTable> tables_;
  std::vector<std::string> notes_;
  std::vector<Taken> shots_;
  std::vector<File> files_;
  std::vector<std::string> failures_;
  uint64_t digest_ = 0xcbf29ce484222325ull;
};

}  // namespace ge::lab
