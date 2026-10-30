// The pipeline's board and job (progress.hpp).
//
// ge:summary The progress board's bookkeeping, the thumbnails, the time left, the job's thread
// ge:keywords classicenhance progress board thumbnail shrink eta job thread cancel
// ge:skill classicenhance
// ge:test tests/classicenhance_progress_test.cpp
#include "classicenhance/progress.hpp"

#include <algorithm>
#include <chrono>

namespace ge::classicenhance {

namespace {

constexpr size_t kStageCount = sizeof(kStages) / sizeof(kStages[0]);

size_t index_of(Stage s) { return size_t(s); }

}  // namespace

const char* state_name(StageState s) {
  switch (s) {
    case StageState::waiting:
      return "waiting";
    case StageState::running:
      return "running";
    case StageState::skipped:
      return "skipped";
    case StageState::done:
      return "done";
    case StageState::failed:
      return "failed";
  }
  return "?";
}

double BoardView::fraction() const {
  // Each stage that does work weighs alike; a skipped one is already there.
  double sum = 0;
  for (const StageView& s : stages) {
    if (s.state == StageState::done || s.state == StageState::skipped) {
      sum += 1;
    } else if (s.state == StageState::running && s.total > 0) {
      sum += double(s.done) / double(s.total);
    }
  }
  return stages.empty() ? 0.0 : sum / double(stages.size());
}

assets::Image shrink(const assets::Image& image, int size) {
  const int longer = std::max(image.width, image.height);
  if (longer <= size || image.width <= 0 || image.height <= 0) return image;
  assets::Image out;
  out.width = std::max(1, image.width * size / longer);
  out.height = std::max(1, image.height * size / longer);
  out.rgba.assign(size_t(out.width) * size_t(out.height) * 4, 0);
  for (int y = 0; y < out.height; y++) {
    const int y0 = y * image.height / out.height, y1 = (y + 1) * image.height / out.height;
    for (int x = 0; x < out.width; x++) {
      const int x0 = x * image.width / out.width, x1 = (x + 1) * image.width / out.width;
      uint32_t sum[4] = {0, 0, 0, 0}, n = 0;
      for (int sy = y0; sy < std::max(y1, y0 + 1); sy++) {
        for (int sx = x0; sx < std::max(x1, x0 + 1); sx++) {
          const uint8_t* p = &image.rgba[(size_t(sy) * size_t(image.width) + size_t(sx)) * 4];
          for (int c = 0; c < 4; c++) sum[c] += p[c];
          n++;
        }
      }
      uint8_t* q = &out.rgba[(size_t(y) * size_t(out.width) + size_t(x)) * 4];
      for (int c = 0; c < 4; c++) q[c] = uint8_t((sum[c] + n / 2) / n);
    }
  }
  return out;
}

ProgressBoard::ProgressBoard() : stage_start_(kStageCount, 0) {
  for (const Stage s : kStages) board_.stages.push_back({s, StageState::waiting, 0, 0, {}, 0});
}

void ProgressBoard::on(const Progress& p, double now_s) {
  // The thumbnails are made before the lock and outside the pipeline's pictures' lifetime.
  Thumb thumb;
  const bool has_thumb = p.kind == Progress::Kind::item && p.before && p.after;
  if (has_thumb) thumb = {p.item, shrink(*p.before, kThumbSize), shrink(*p.after, kThumbSize)};
  const std::lock_guard lock(mutex_);
  if (!board_.started) {
    board_.started = true;
    t0_ = now_s;
  }
  board_.elapsed = now_s - t0_;
  StageView& s = board_.stages[index_of(p.stage)];
  switch (p.kind) {
    case Progress::Kind::started:
      s.state = StageState::running;
      s.total = p.total;
      s.done = 0;
      stage_start_[index_of(p.stage)] = now_s;
      break;
    case Progress::Kind::item:
      s.state = StageState::running;
      s.done = p.done;
      s.item = p.item;
      break;
    case Progress::Kind::finished:
      s.state = StageState::done;
      s.done = p.done;
      s.item = p.item;
      break;
    case Progress::Kind::skipped:
      s.state = StageState::skipped;
      s.item = p.item;
      break;
    case Progress::Kind::failed:
      s.state = StageState::failed;
      s.item = p.item;
      board_.failed = true;
      board_.error = std::string(stage_name(p.stage)) + ": " + p.item;
      break;
  }
  if (s.state == StageState::running || p.kind == Progress::Kind::finished ||
      p.kind == Progress::Kind::failed) {
    s.seconds = now_s - stage_start_[index_of(p.stage)];
  }
  if (has_thumb) {
    board_.textures_done++;
    board_.recent.insert(board_.recent.begin(), std::move(thumb));
    if (board_.recent.size() > kRecent) board_.recent.resize(kRecent);
  }
  board_.finished = !board_.failed &&
                    std::all_of(board_.stages.begin(), board_.stages.end(), [](const StageView& v) {
                      return v.state == StageState::done || v.state == StageState::skipped;
                    });
  // The time left: the running stage at its own rate, a waiting stage at the last rate seen.
  board_.left = board_.finished ? 0.0 : -1.0;
  double rate = -1;  // seconds an item
  for (const StageView& v : board_.stages) {
    if ((v.state == StageState::running || v.state == StageState::done) && v.done > 0 &&
        v.seconds > 0) {
      rate = v.seconds / double(v.done);
    }
  }
  if (!board_.finished && !board_.failed && rate >= 0) {
    // A waiting stage's item count comes when it starts, so until then it adds nothing.
    double left = 0;
    for (const StageView& v : board_.stages) {
      if (v.state != StageState::running) continue;
      const double own = v.done > 0 && v.seconds > 0 ? v.seconds / double(v.done) : rate;
      left += own * double(v.total - std::min(v.done, v.total));
    }
    board_.left = left;
  }
}

BoardView ProgressBoard::view() const {
  const std::lock_guard lock(mutex_);
  return board_;
}

EnhanceJob::~EnhanceJob() {
  cancel();
  if (thread_.joinable()) thread_.join();
}

bool EnhanceJob::start(Source source, std::filesystem::path root, RunOptions opts) {
  if (running_.load() || thread_.joinable()) return false;
  running_.store(true);
  source_ = std::move(source);
  opts.cancel = &cancel_;
  opts.progress = [this](const Progress& p) {
    const double now =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    board_.on(p, now);
  };
  thread_ = std::thread([this, root = std::move(root), opts = std::move(opts)] {
    RunReport report;
    std::string error;
    run_pipeline(source_, root, opts, &report, &error);
    {
      const std::lock_guard lock(mutex_);
      report_ = std::move(report);
    }
    running_.store(false);
  });
  return true;
}

RunReport EnhanceJob::report() const {
  const std::lock_guard lock(mutex_);
  return report_;
}

}  // namespace ge::classicenhance
