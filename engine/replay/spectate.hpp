// SPECTATING BY INPUTS (docs/tv.md § Spectating by inputs): a run of any game on the play contract
// (play/game.hpp) written as a replay file version 2 stream as it is played, and followed by a
// viewer that steps its own copy of the run to the player's digest. Bytes a second are the inputs
// a player holds, not the world: a deterministic game's broadcast is a few hundred bytes a second
// between keyframes. A TV node (engine/tv) carries the stream like any other.
//
// THE STREAM. The header names the game and its behaviour version (`GameSpec::version`), with the
// seed and the options in its meta. Each block is one second of steps (`hz` of them) and holds one
// chunk on the `inputs` track: the block's first step, its runs of held inputs (a count, then
// each axis's grid index: a steer's 1/127 steps, a lever's 1/255, a button's 0 or 1) and the run's
// digest after the block's last step. Every `keyframe_seconds` a block also holds, on the
// `keyframe` track, the run saved at its first step (play/saves.hpp's `save_run`), whole or as a
// delta:
//
//   whole   the save as `save_run` wrote it; the block is the stream's keyframe (a TV node starts
//           a late joiner there). The first keyframe and every `full_every`-th after it.
//   delta   "GEKDELTA", then the step it was saved at, the step of the keyframe before it and the
//           save's size (varints), then the save XORed with that keyframe's (zeros past its end).
//           Two saves seconds apart differ in a few hundred bytes of their 120-odd kB, so the XOR
//           is nearly all zeros and the block's zstd packs it to about 1 kB where a whole save
//           takes about 80 kB.
//
// A deterministic run needs no keyframe after its first; the later ones are what a late joiner
// skips ahead by.
//
// THE VIEWER (`Spectator`) is handed the stream as it grows. It waits for a keyframe block, then
// (unless `skip_ahead` is off) looks ahead through every whole block it already has, rebuilds the
// newest save from the whole
// one and the deltas after it, loads the run from that and steps it through every block's inputs
// from there, checking the block's digest after each: a run that parts from the player's stops
// there with the step it parted at. So a viewer who joins late is at the newest keyframe at once,
// not stepping through the minute since the last whole one. A game that keeps no saves can be
// followed only from its first block.
//
// ge:summary Spectating by inputs: a play-contract run as a replay v2 stream, followed by digest
// ge:keywords spectate spectating inputs keyframe save_run load_run digest late-joiner tv
// ge:keywords spectate_writer spectator follow deterministic replay-v2 m23f spectate_pack
// ge:keywords spectate_inputs input-log ger fall-line-replay
// ge:skill replay
// ge:test tests/fall_line_spectate_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "play/game.hpp"
#include "play/inputs.hpp"
#include "replay/stream.hpp"

namespace ge::replay {

inline constexpr uint32_t kInputsTrack = 1;
inline constexpr uint32_t kKeyframeTrack = 2;

struct SpectateOptions {
  int keyframe_seconds = 10;  // a keyframe every this many seconds (0: only the first)
  int full_every = 30;        // every this many keyframes is whole; the rest are deltas (1: all)
};

class SpectateWriter {
 public:
  // The stream's header written onto `out` for a run of `spec` opened on `setup`, which has taken
  // no step yet; `out` is kept and every block appended to it.
  SpectateWriter(const play::GameSpec& spec, const play::Setup& setup, std::vector<uint8_t>& out,
                 const SpectateOptions& options = {});
  // The first keyframe: the run as opened, before its first step. False with `error` when it would
  // not save (a game that keeps no saves: pass `keyframe_seconds` 0).
  bool start(const play::Run& run, std::string& error);
  // The run `run` has just been stepped on `snapped` (each axis's value on its grid, as
  // Session::step steps it). False with `error` when a keyframe would not save.
  bool step(std::span<const double> snapped, const play::Run& run, std::string& error);
  // The open block written and the trailer with the final step and digest.
  void finish(const play::Run& run);
  int64_t steps() const { return steps_; }
  int64_t blocks() const { return writer_.blocks; }

 private:
  bool keyframe(const play::Run& run, std::string& error);
  void close_block(const play::Run& run);

  const play::GameSpec& spec_;
  play::Setup setup_;
  SpectateOptions options_;
  StreamWriter writer_;
  int64_t per_block_ = 1;
  int64_t steps_ = 0;
  int64_t block_first_ = 0;         // the open block's first step
  bool block_keyframe_ = false;     // the open block holds a whole keyframe
  bool block_save_ = false;         // the open block holds a keyframe, whole or a delta
  std::vector<uint8_t> last_save_;  // the keyframe before, whole (what a delta is taken against)
  int64_t last_save_step_ = -1;
  int keys_since_full_ = 0;
  std::vector<int64_t> held_;  // the open run's grid indices
  int64_t held_count_ = 0;
  std::vector<uint8_t> runs_;  // the open block's runs, encoded
  int64_t runs_n_ = 0;
};

struct Spectator {
  bool skip_ahead = true;  // load the newest keyframe the blocks so far hold (false: the first)
  const play::GameSpec* spec = nullptr;
  std::unique_ptr<play::Run> run;
  int64_t step = 0;      // the steps the run has taken (its keyframe's step and every one after)
  int64_t started = -1;  // the step the viewer's run was loaded at
  int64_t deltas = 0;    // delta keyframes applied to reach it
  int64_t blocks = 0;    // blocks stepped through and checked
  uint64_t digest = 0;   // the run's after `step`
  int64_t parted = -1;   // the block after which the digest parted from the player's
  std::string error;     // why it stopped
  StreamReader reader;
  bool have_header = false;
};

// A RUN'S REPLAY ON VERSION 2: an input log (play/inputs.hpp) and its stream are the same run.
// `spectate_pack` steps the run through the log and writes it as the stream a broadcast would have
// been (its keyframes and each block's digest); `spectate_inputs` reads a stream from its first
// block back into the log, every run of held inputs and the final digest, which `ge replay`, `ge
// diverge` and `ge why` then take as any log. False with `error` (a log of another game, a stream
// that does not start at its first block, damage).
bool spectate_pack(const play::GameSpec& spec, const play::InputLog& log, std::vector<uint8_t>& out,
                   std::string& error, const SpectateOptions& options = {});
bool spectate_inputs(const play::GameSpec& spec, std::span<const uint8_t> stream,
                     play::InputLog& out, std::string& error);

// Steps the spectator's run through every whole block of `stream` (the whole stream so far: the
// header, then blocks from wherever the viewer joined) it has not yet followed. False once it has
// stopped (`error`).
bool spectate_follow(Spectator& s, const play::GameSpec& spec, std::span<const uint8_t> stream);

}  // namespace ge::replay
