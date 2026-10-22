// PLAYING A GAME WITHOUT A SCREEN: everything the engine does with a `Run` (game.hpp), the same
// for every game. Every input goes through `Session::step`, which snaps it onto its axes' grids,
// records it on the input log and steps the run, so a run played by an agent, by the game's
// pilot or from a log is written down the same way and replays to the same digest.
//
// THE LINE PROTOCOL (`ge play`): one JSON object a line each way.
// - The session opens with `hello`: the game, the seed and options, the step rate, the axes, the
//   longest hold and the ops.
// - `{"op":"observe"}` answers what the player sees (the game's `observe`), with `step` and `t`.
// - `{"op":"act","input":{"<axis>":v,...},"steps":n}` holds the input for n steps (1..
//   `kMostHold`, default 1) and answers the observation after them. An absent axis is 0; a
//   `press` axis is taken on the first step only. Past the end it answers an error.
// - `{"op":"pilot","steps":n}` lets the game's own pilot play n steps (an agent hands over and
//   takes back), then answers the observation.
// - `{"op":"state"}` answers everything that decides the next step (the game's `state`).
// - `{"op":"inputs"}` answers the input log so far, as its text.
// - `{"op":"end"}` answers the report, the steps and the digest, and ends the session.
// - Anything else answers `{"op":"error","error":...,"next":...}`.
// Every answer is one line and carries the op it answers.
//
// THE BATCH RUNS (`ge sim`, `ge replay`, `ge state`, `ge diverge`, `ge shot`): the pilot from
// the start to the end, a log played back, a run's state at a step, the first step two logs part,
// and a still of a run at a step.
//
// ge:summary The play session over a Run: the line protocol, the pilot, replay, state, diverge
// ge:keywords play session protocol observe act pilot state inputs end replay diverge divergence
// ge:keywords pilot_run state_at first-difference field-diff hello shot_at still view caption
// ge:keywords lens lens-ask open_lens shoot_lens gpu-view mode
// ge:skill agent-interface
// ge:test tests/play_session_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "play/game.hpp"
#include "play/inputs.hpp"

namespace ge::play {

// The longest one `act` or `pilot` may hold for, in steps.
inline constexpr int32_t kMostHold = 1200;
// The most differing fields `diverge` lists.
inline constexpr size_t kMostFields = 24;

class Session {
 public:
  Session(const GameSpec& spec, const Setup& setup);
  // Whether the game opened the run; `error` says why not.
  bool ok() const { return run_ != nullptr; }
  const std::string& error() const { return error_; }

  // One step on `input` (one value per axis), snapped, recorded and stepped.
  void step(std::span<const double> input);
  // Nothing more to play: the run is over or at the spec's longest run.
  bool done() const;
  int64_t steps() const { return steps_; }
  Run& run() { return *run_; }
  const Run& run() const { return *run_; }
  const GameSpec& spec() const { return spec_; }
  // The log so far, its digest filled.
  InputLog inputs() const;

  // The protocol: the first line, and one answer a line.
  std::string hello() const;
  std::string handle(std::string_view line);
  bool ended() const { return ended_; }

 private:
  std::string observation(std::string_view op);

  const GameSpec& spec_;
  Setup setup_;
  std::unique_ptr<Run> run_;
  std::string error_;
  InputRecorder recorder_;
  std::vector<double> snapped_;
  int64_t steps_ = 0;
  int64_t most_steps_ = 0;
  bool ended_ = false;
};

// A run's end: its log, report and digest.
struct Played {
  InputLog log;
  std::string report;  // the game's JSON object
  uint64_t digest = 0;
  int64_t steps = 0;
  bool over = false;  // the game said so (false: it hit the longest run)
};

// The game's pilot from the start to the end (or the spec's longest run).
std::optional<Played> pilot_run(const GameSpec& spec, const Setup& setup, std::string& error);

// The setup a log was recorded with.
bool setup_of(const GameSpec& spec, const InputLog& log, Setup& out, std::string& error);

// `log` played back on its setup, every step of it.
std::optional<Played> replay(const GameSpec& spec, const InputLog& log, std::string& error);

// The run's state after `step` steps of `log` (0: before the first).
std::optional<std::string> state_at(const GameSpec& spec, const InputLog& log, int64_t step,
                                    std::string& error);

struct FieldDiff {
  std::string key;
  std::string a, b;  // JSON text; "absent" when a side lacks the key
};

struct Divergence {
  bool same = true;               // every step of both, the same run
  int64_t input_step = -1;        // the first step whose inputs differ (0-based), -1 if none
  int64_t state_step = -1;        // the steps after which the states first differ, -1 if none
  std::vector<FieldDiff> fields;  // at `state_step`, the first `kMostFields`
  size_t more_fields = 0;         // beyond them
  bool setup_differs = false;     // the logs' seeds or options differ
  int64_t steps_a = 0, steps_b = 0;
  uint64_t digest_a = 0, digest_b = 0;  // each at its own end
};

// `a` and `b` stepped side by side: the first input that differs and the first step after which
// the runs differ, with the fields of `state` that differ there.
std::optional<Divergence> diverge(const GameSpec& spec, const InputLog& a, const InputLog& b,
                                  std::string& error);

// A still of a run: `view` after `step` steps, framed with its caption band (lab.hpp's
// `framed`: the title, the game's caption, the seed, the options, the step and the digest).
struct Still {
  lab::Picture picture;
  std::string view;
  std::string caption;  // the game's own line
  int64_t step = 0;
  uint64_t digest = 0;  // the run's digest at `step`
};

// WHAT DRAWS A VIEW: a Lens is the game's own CPU view (Run::shoot), or with `gpu` the registered
// renderer's view in one of its modes (game.hpp § THE GPU'S PICTURES), opened once for a shot or
// a whole recording, so the renderer's frame carries from one picture to the next.
struct LensAsk {
  std::string view;  // "" the first view
  bool gpu = false;
  std::string mode;           // gpu only: "" the renderer's first mode
  int width = 0, height = 0;  // gpu only: 0 the renderer's own size
};

struct Lens {
  const View* view = nullptr;
  const View* mode = nullptr;          // gpu only
  std::unique_ptr<Renderer> renderer;  // null: the game's CPU view
  int width = 0, height = 0;           // gpu only: the picture's size
  int64_t drawn = 0;                   // pictures taken; the first is a cut
  // "map" for a CPU view, "chase/depth" for a GPU one: how a file and a band name it.
  std::string name() const;
};

// The lens `ask` names for `spec`, or false with `error` naming the views (and modes) there are.
bool open_lens(const GameSpec& spec, const LensAsk& ask, Lens& out, std::string& error);
// `run` as it stands, through `lens`.
bool shoot_lens(Lens& lens, Run& run, lab::Picture& out, std::string& caption, std::string& error);
// The lens's views (and modes) as JSON array bodies, for a command's answer.
std::string lens_views_json(const GameSpec& spec, bool gpu);

// `log` played to `step` and its view `view` drawn ("" the spec's first view).
std::optional<Still> shot_at(const GameSpec& spec, const InputLog& log, int64_t step,
                             std::string_view view, std::string& error);
// The same through the lens `ask` names (a GPU view with --gpu).
std::optional<Still> shot_at(const GameSpec& spec, const InputLog& log, int64_t step,
                             const LensAsk& ask, std::string& error);

// A JSON string: quotes and backslashes escaped, newlines as \n, other control bytes dropped.
std::string quote(std::string_view s);

}  // namespace ge::play
