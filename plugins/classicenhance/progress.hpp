// THE PIPELINE WATCHED: what `run_pipeline` reports while it works, and the board that turns those
// reports into what a menu shows: each stage waiting, running, skipped, done or failed, how far
// it is through its items, the item it is on, the last textures it finished (the pack's picture
// beside the enhanced one), the time so far and the time left.
//
// - **A report** (`Progress`, pipeline.hpp) is sent by the pipeline from the thread it runs on: a stage
//   started (with its item count), an item done, a stage finished, skipped (up to date or not
//   asked for) or failed. A texture's item carries its source and its albedo, which the board
//   shrinks into a thumbnail before the call returns.
// - **The board** (`ProgressBoard`) takes the reports under a lock and hands out a copy
//   (`BoardView`) a view draws from on another thread. Its clock is the caller's (`on` takes the
//   time), so a recorded run makes the same board every time and a lab pictures it.
// - **A job** (`EnhanceJob`) runs the pipeline on a thread of its own with a board, and can be
//   asked to stop: the pipeline checks between items and fails the stage it is in with
//   "cancelled", keeping every stage it finished.
//
// The time left is the running stage's rate (its seconds so far over its items so far, or the
// last stage's that had one) times its items left. A waiting stage's items are not known until it
// starts, so the time left covers the stage running, and the stages list says what follows.
//
// ge:summary The pipeline's progress: reports, the board a menu draws from, a cancellable job
// ge:keywords classicenhance progress pipeline board stage running done skipped failed thumbnail
// ge:keywords eta time-left cancel job thread menu snapshot
// ge:skill classicenhance
// ge:test tests/classicenhance_progress_test.cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "assets/image.hpp"
#include "classicenhance/pipeline.hpp"

namespace ge::classicenhance {

enum class StageState : uint8_t { waiting, running, skipped, done, failed };
const char* state_name(StageState s);

struct StageView {
  Stage stage = Stage::pack;
  StageState state = StageState::waiting;
  uint32_t done = 0, total = 0;
  std::string item;  // the last item, or why it was skipped or failed
  double seconds = 0;
};

// A texture as the board keeps it: both pictures shrunk to fit kThumbSize, aspect kept.
struct Thumb {
  std::string name;
  assets::Image before, after;
};

struct BoardView {
  std::vector<StageView> stages;  // one a stage, in the pipeline's order
  std::vector<Thumb> recent;      // the last textures finished, newest first
  uint32_t textures_done = 0;
  double elapsed = 0;         // seconds since the first report
  double left = -1;           // seconds still to go; -1 before there is a rate to go by
  bool started = false;       // a report has come in
  bool finished = false;      // every stage done or skipped
  bool failed = false;        // a stage failed (`error` says which and why)
  std::string error;
  // How far through the whole run, 0..1: finished stages count whole, the running one by items.
  double fraction() const;
};

class ProgressBoard {
 public:
  static constexpr int kThumbSize = 96;  // px, the longer side
  static constexpr size_t kRecent = 6;

  ProgressBoard();
  // One report at `now_s` (the caller's clock, seconds).
  void on(const Progress& p, double now_s);
  BoardView view() const;

 private:
  mutable std::mutex mutex_;
  BoardView board_;
  double t0_ = 0;
  std::vector<double> stage_start_;
};

// `image` shrunk so its longer side is at most `size` px (box filtered, aspect kept); an image
// already that small comes back as it is.
assets::Image shrink(const assets::Image& image, int size);

// The pipeline on a thread of its own, reporting to its board by the steady clock.
class EnhanceJob {
 public:
  EnhanceJob() = default;
  EnhanceJob(const EnhanceJob&) = delete;
  EnhanceJob& operator=(const EnhanceJob&) = delete;
  ~EnhanceJob();  // asks the run to stop and waits for it

  // Starts the run (a job runs once); false when one is already running.
  bool start(Source source, std::filesystem::path root, RunOptions opts);
  void cancel() { cancel_.store(true); }
  bool running() const { return running_.load(); }
  BoardView view() const { return board_.view(); }
  // After the run: what `run_pipeline` reported (empty before it ends).
  RunReport report() const;

 private:
  ProgressBoard board_;
  std::atomic<bool> cancel_{false}, running_{false};
  std::thread thread_;
  mutable std::mutex mutex_;
  RunReport report_;
  Source source_;
};

}  // namespace ge::classicenhance
