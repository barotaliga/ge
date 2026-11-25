// gpustats: any game's next frames measured off its recorder (gpu_watch.hpp).
//
// ge:summary gpustats: the frames read, their medians, the report, the command
// ge:keywords gpustats gpu-watch report median add_gpustats
// ge:skill render perf
// ge:test tests/render_gpu_watch_test.cpp
#include "render/gpu_watch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "console/console.hpp"
#include "gpu/device.hpp"

namespace ge::render {

namespace {

double median(std::vector<double> v) {
  if (v.empty()) return 0;
  const auto mid = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2);
  std::nth_element(v.begin(), mid, v.end());
  return *mid;
}

std::string fixed(double v, int places) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

}  // namespace

void start_gpu_watch(GpuWatch& w, int frames, bool quiet) {
  w = {};
  w.left = std::clamp(frames, 1, 6000);
  w.quiet = quiet;
}

void gpu_watch_frame(GpuWatch& w, const FrameStats& stats, double frame_s, console::Console& con) {
  if (w.left <= 0) return;
  w.frame_ms.push_back(frame_s * 1000);
  double cpu = stats.prepare_ms;  // the frame's decisions, then each pass's encoding
  for (int i = 0; i < stats.count; ++i) cpu += stats.passes[static_cast<size_t>(i)].cpu_ms;
  w.cpu_ms.push_back(cpu);
  const double span = stats.gpu_span_ms();
  if (!std::isnan(span)) w.span_ms.push_back(span);
  for (int i = 0; i < stats.count; ++i) {
    const PassStat& s = stats.passes[static_cast<size_t>(i)];
    if ((s.draws == 0 && s.dispatches == 0) || std::isnan(s.gpu_ms)) continue;
    auto at = std::find_if(w.pass_ms.begin(), w.pass_ms.end(),
                           [&](const auto& e) { return e.first == s.name; });
    if (at == w.pass_ms.end()) at = w.pass_ms.insert(w.pass_ms.end(), {std::string(s.name), {}});
    at->second.push_back(s.gpu_ms);
  }
  ++w.read;
  if (--w.left > 0) return;
  w.report = gpu_watch_report(w);
  if (!w.quiet)
    for (const std::string& l : w.report) con.print(l);
}

std::vector<std::string> gpu_watch_report(const GpuWatch& w, size_t passes) {
  const double frame = median(w.frame_ms), cpu = median(w.cpu_ms);
  std::vector<std::string> out = {
      "gpustats: frame " + fixed(frame, 2) + " ms (" + fixed(frame > 0 ? 1000 / frame : 0.0, 0) +
      " fps) median over " + std::to_string(w.read) + " frames; the frame's CPU " + fixed(cpu, 2) +
      " ms; the rest (game, effects, HUD, waiting for the display) " +
      fixed(std::max(0.0, frame - cpu), 2) + " ms"};
  if (w.span_ms.empty()) {
    out.push_back("gpustats: GPU frame not measured (this GPU has no timestamp queries)");
    return out;
  }
  out.push_back("gpustats: GPU frame span, median " + fixed(median(w.span_ms), 2) + " ms");
  if (w.pass_ms.empty() || w.read == 0) return out;
  std::vector<std::pair<double, size_t>> order;
  for (size_t i = 0; i < w.pass_ms.size(); ++i) {
    double sum = 0;
    for (const double ms : w.pass_ms[i].second) sum += ms;
    order.push_back({sum / static_cast<double>(w.read), i});
  }
  std::sort(order.begin(), order.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
  out.push_back("  GPU by pass (mean a frame; median where it ran, in how many):");
  for (size_t k = 0; k < order.size() && k < passes; ++k) {
    const auto& [name, ms] = w.pass_ms[order[k].second];
    out.push_back("    " + name + " " + fixed(order[k].first, 3) + " ms; " + fixed(median(ms), 3) +
                  " ms in " + std::to_string(ms.size()) + " of " + std::to_string(w.read));
  }
  return out;
}

void add_gpustats(console::Console& con, GpuWatch& w) {
  con.add_command("gpustats",
                  "gpustats [frames]: what the next frames cost on the GPU (60 by default), the "
                  "dearest passes first",
                  [&w](console::Console& c, const console::Args& a) {
                    start_gpu_watch(w, a.size() > 1 ? std::atoi(a[1].c_str()) : 60, false);
                    c.print("gpustats: reading the next " + std::to_string(w.left) + " frames...");
                  });
}

std::vector<std::string> gpu_notes(const GpuWatch& w, const gpu::Device* device) {
  std::vector<std::string> out = {"# gpu"};
  if (device != nullptr) out.push_back("adapter " + gpu::adapter_line(*device));
  if (w.report.empty()) {
    out.push_back(
        "no frames measured (the frames before the picture drew no world: a menu, the "
        "title or a lobby)");
  } else {
    out.insert(out.end(), w.report.begin(), w.report.end());
  }
  return out;
}

}  // namespace ge::render
