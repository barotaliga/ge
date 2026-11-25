// A window's frames profiled by zone (frames.hpp).
//
// ge:summary The frame profile's record, its per-frame fold and its report
// ge:keywords prof profile frame-profile frames_tick frames_report percentile worst-frames
// ge:skill perf
// ge:test tests/prof_frames_test.cpp
#include "prof/frames.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <numeric>

namespace ge::prof {

namespace {

double ms(int64_t ns) { return static_cast<double>(ns) * 1e-6; }

// The value at share `q` (0..1) of the sorted values: the nearest rank.
int64_t rank_of(std::vector<int64_t> v, double q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const auto i = static_cast<size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
  return v[std::min(i, v.size() - 1)];
}

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
void line(std::string& out, const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, args);
  va_end(args);
  out += buf;
  out += '\n';
}

// The record's last frame closed: each zone's own time in it, the zones met first added.
void fold_frame(FrameProfile& fp, int64_t frame_ns) {
  const size_t frame = fp.frame_ns.size();
  fp.frame_ns.push_back(frame_ns);
  for (std::vector<int64_t>& z : fp.ns) z.push_back(0);
  totals(spans(), fp.fold);
  for (const ZoneTotal& t : fp.fold) {
    auto it = std::find(fp.zones.begin(), fp.zones.end(), t.name);
    size_t row = static_cast<size_t>(it - fp.zones.begin());
    if (it == fp.zones.end()) {
      fp.zones.push_back(t.name);
      fp.ns.emplace_back(frame + 1, 0);
      fp.ns.back().reserve(static_cast<size_t>(fp.asked));
      fp.allocs.push_back(0);
    }
    fp.ns[row][frame] += t.excl_ns;
    fp.allocs[row] += t.excl_allocs;
  }
  fp.dropped += dropped();
  clear();
}

}  // namespace

void frames_start(FrameProfile& fp, int frames, size_t spans_a_frame) {
  fp = FrameProfile{};
  fp.asked = std::clamp(frames, 1, 100000);
  fp.frame_ns.reserve(static_cast<size_t>(fp.asked));
  fp.last_ns = -1;  // the first tick only opens the first frame
  start(spans_a_frame);
}

void frames_stop(FrameProfile& fp) {
  if (fp.asked == 0) return;
  stop();
  fp.asked = 0;
  fp.done = !fp.frame_ns.empty();
}

bool frames_running(const FrameProfile& fp) { return fp.asked > 0; }

bool frames_tick_with(FrameProfile& fp, int64_t frame_ns) {
  if (fp.asked == 0) return false;
  if (fp.last_ns < 0) {  // the first tick: the frame before was not recorded
    fp.last_ns = 0;
    clear();
    return false;
  }
  fold_frame(fp, frame_ns);
  if (static_cast<int>(fp.frame_ns.size()) < fp.asked) return false;
  frames_stop(fp);
  return true;
}

bool frames_tick(FrameProfile& fp) {
  if (fp.asked == 0) return false;
  const int64_t now = now_ns();
  const bool first = fp.last_ns < 0;
  const int64_t frame = first ? 0 : now - fp.last_ns;
  const bool whole = frames_tick_with(fp, frame);
  fp.last_ns = now;
  return whole;
}

std::string frames_report(const FrameProfile& fp, int worst) {
  std::string out;
  const size_t n = fp.frame_ns.size();
  if (n == 0) return "profile: no frames were recorded\n";
  const int64_t total = std::accumulate(fp.frame_ns.begin(), fp.frame_ns.end(), int64_t{0});
  line(out, "profile: %zu frames, %.1f fps; frame ms median %.2f, p99 %.2f, worst %.2f", n,
       total > 0 ? static_cast<double>(n) / (static_cast<double>(total) * 1e-9) : 0.0,
       ms(rank_of(fp.frame_ns, 0.5)), ms(rank_of(fp.frame_ns, 0.99)),
       ms(*std::max_element(fp.frame_ns.begin(), fp.frame_ns.end())));
  // Time in no zone: each frame's wall time less every zone's own time in it.
  std::vector<int64_t> loose(n);
  for (size_t f = 0; f < n; f++) {
    int64_t in = 0;
    for (const std::vector<int64_t>& z : fp.ns) in += z[f];
    loose[f] = std::max<int64_t>(0, fp.frame_ns[f] - in);
  }
  struct Row {
    std::string name;
    const std::vector<int64_t>* v;
    int64_t sum;
    int64_t allocs;
  };
  std::vector<Row> rows;
  for (size_t z = 0; z < fp.zones.size(); z++)
    rows.push_back({std::string(zone_name(fp.zones[z])), &fp.ns[z],
                    std::accumulate(fp.ns[z].begin(), fp.ns[z].end(), int64_t{0}), fp.allocs[z]});
  rows.push_back(
      {"(no zone)", &loose, std::accumulate(loose.begin(), loose.end(), int64_t{0}), -1});
  std::stable_sort(rows.begin(), rows.end(),
                   [](const Row& a, const Row& b) { return a.sum > b.sum; });
  line(out, "  %-24s %9s %9s %9s %9s %6s %8s", "zone (own time)", "mean ms", "median", "p99",
       "worst", "share", "allocs");
  for (const Row& r : rows) {
    if (r.sum == 0) continue;
    char allocs[32] = "-";
    if (r.allocs >= 0)
      std::snprintf(allocs, sizeof allocs, "%.1f",
                    static_cast<double>(r.allocs) / static_cast<double>(n));
    line(out, "  %-24s %9.3f %9.3f %9.3f %9.3f %5.1f%% %8s", r.name.c_str(),
         ms(r.sum) / static_cast<double>(n), ms(rank_of(*r.v, 0.5)), ms(rank_of(*r.v, 0.99)),
         ms(*std::max_element(r.v->begin(), r.v->end())),
         total > 0 ? 100.0 * static_cast<double>(r.sum) / static_cast<double>(total) : 0.0, allocs);
  }
  // The slowest frames, each with its three dearest zones.
  std::vector<size_t> order(n);
  std::iota(order.begin(), order.end(), size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return fp.frame_ns[a] > fp.frame_ns[b]; });
  const size_t shown = std::min(n, static_cast<size_t>(std::max(worst, 0)));
  if (shown > 0) line(out, "  the slowest frames:");
  for (size_t k = 0; k < shown; k++) {
    const size_t f = order[k];
    std::vector<std::pair<int64_t, std::string>> parts;
    for (const Row& r : rows) parts.emplace_back((*r.v)[f], r.name);
    std::stable_sort(parts.begin(), parts.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    std::string top;
    for (size_t i = 0; i < std::min<size_t>(3, parts.size()) && parts[i].first > 0; i++) {
      char buf[96];
      std::snprintf(buf, sizeof buf, "%s%s %.2f", i ? ", " : "", parts[i].second.c_str(),
                    ms(parts[i].first));
      top += buf;
    }
    line(out, "    frame %zu: %.2f ms (%s)", f + 1, ms(fp.frame_ns[f]), top.c_str());
  }
  if (fp.dropped > 0)
    line(out, "  %zu zone spans did not fit a frame's room and are not counted", fp.dropped);
  return out;
}

}  // namespace ge::prof
