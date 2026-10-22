// The bench (bench.hpp).
//
// ge:summary The bench: replays a log repeatedly, times each step and folds its zones per step
// ge:keywords play bench timing median repeats zones allocations series spans outliers
// ge:skill agent-interface perf
// ge:test tests/play_session_test.cpp
#include "play/bench.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

#include "core/num_text.hpp"
#include "play/session.hpp"
#include "prof/memory.hpp"
#include "prof/zones.hpp"

namespace ge::play {

namespace {

// Room for this many spans a step; a step that opens more drops the rest (prof::dropped).
constexpr int64_t kSpansPerStep = 24;
constexpr int64_t kMostSpans = int64_t{1} << 22;
constexpr size_t kSlowest = 5;

struct Repeat {
  std::vector<int64_t> step_ns;
  std::vector<int64_t> step_allocs;
  std::vector<size_t> bounds;  // spans().size() before each step, and after the last
  int64_t open_ns = 0;
  int64_t open_allocs = 0;
  std::vector<prof::ZoneTotal> open_rows;  // the zones the open ran
  uint64_t digest = 0;
  int64_t total_ns = 0;
};

// One replay of the log with every zone recording. The recording stays in prof:: until the next
// start, so the last repeat's spans are read in place.
bool one_repeat(const GameSpec& spec, const Setup& setup, const InputLog& log, int64_t steps,
                Repeat& r, std::string& error) {
  const size_t axes = spec.axes.size();
  std::vector<double> in(axes);
  r.step_ns.assign(static_cast<size_t>(steps), 0);
  r.step_allocs.assign(static_cast<size_t>(steps), 0);
  r.bounds.assign(static_cast<size_t>(steps) + 1, 0);
  prof::start(static_cast<size_t>(std::min(steps * kSpansPerStep, kMostSpans)));
  const int64_t a0 = prof::allocations().count;
  const int64_t t0 = prof::now_ns();
  std::unique_ptr<Run> run = spec.open(setup, error);
  r.open_ns = prof::now_ns() - t0;
  r.open_allocs = prof::allocations().count - a0;
  if (!run) {
    prof::stop();
    return false;
  }
  // The spans of the open are not the steps': the step rows start empty.
  prof::totals(prof::spans(), r.open_rows);
  prof::clear();
  for (int64_t i = 0; i < steps; i++) {
    const std::span<const double> raw = log.at(i);
    for (size_t k = 0; k < axes; k++) in[k] = snap_axis(raw[k], spec.axes[k].kind);
    r.bounds[static_cast<size_t>(i)] = prof::spans().size();
    const int64_t allocs = prof::allocations().count;
    const int64_t start = prof::now_ns();
    run->step(in);
    r.step_ns[static_cast<size_t>(i)] = prof::now_ns() - start;
    r.step_allocs[static_cast<size_t>(i)] = prof::allocations().count - allocs;
  }
  r.bounds[static_cast<size_t>(steps)] = prof::spans().size();
  prof::stop();
  r.digest = run->digest();
  r.total_ns = 0;
  for (const int64_t ns : r.step_ns) r.total_ns += ns;
  return true;
}

std::string fixed(double v, int places) { return core::num::to_fixed(v, places); }

}  // namespace

double quantile(std::vector<double> v, double q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const auto k = static_cast<size_t>(std::ceil(q * static_cast<double>(v.size())));
  return v[std::min(v.size() - 1, k == 0 ? 0 : k - 1)];
}

std::optional<BenchRun> bench(const GameSpec& spec, const InputLog& log, const BenchOptions& o,
                              std::string& error) {
  Setup setup;
  if (!setup_of(spec, log, setup, error)) return std::nullopt;
  const int64_t steps = o.most_steps < 0 ? log.steps() : std::min(o.most_steps, log.steps());
  const int repeats = std::max(1, o.repeats);
  // The repeats time the steps; one more, the profile repeat, is the one whose zones are read
  // (every repeat records, so the times include what recording costs, and compare).
  std::vector<Repeat> reps(static_cast<size_t>(repeats));
  for (Repeat& r : reps) {
    if (!one_repeat(spec, setup, log, steps, r, error)) return std::nullopt;
  }
  Repeat med;
  if (!one_repeat(spec, setup, log, steps, med, error)) return std::nullopt;
  const std::span<const prof::Span> spans = prof::spans();
  BenchRun b;
  b.steps = steps;
  b.digest = reps[0].digest;
  for (const Repeat& r : reps) b.same_digest = b.same_digest && r.digest == b.digest;
  b.same_digest = b.same_digest && med.digest == b.digest;
  b.dropped_spans = static_cast<int64_t>(prof::dropped());
  if (steps == log.steps() && log.digest) b.same_digest = b.same_digest && b.digest == *log.digest;

  std::vector<double> opens;
  opens.reserve(reps.size());
  for (const Repeat& r : reps) opens.push_back(static_cast<double>(r.open_ns) / 1e6);
  b.open_ms = quantile(opens, 0.5);
  b.open_allocs = reps[0].open_allocs;
  for (const prof::ZoneTotal& t : med.open_rows) {
    b.open_zones.push_back({std::string(prof::zone_name(t.name)), t.calls,
                            static_cast<double>(t.excl_ns) / 1e6,
                            static_cast<double>(t.incl_ns) / 1e6});
  }
  b.step_us.resize(static_cast<size_t>(steps));
  std::vector<double> across(static_cast<size_t>(repeats));
  for (int64_t i = 0; i < steps; i++) {
    for (size_t k = 0; k < reps.size(); k++) {
      across[k] = static_cast<double>(reps[k].step_ns[static_cast<size_t>(i)]) / 1e3;
    }
    b.step_us[static_cast<size_t>(i)] = quantile(across, 0.5);
  }
  b.step_allocs = reps[0].step_allocs;

  // The profile repeat's zones, folded a step at a time.
  std::vector<prof::ZoneTotal> rows;
  prof::totals(spans, rows);
  std::vector<uint32_t> ids;
  std::map<uint32_t, size_t> column;
  for (const prof::ZoneTotal& t : rows) {
    column[t.name] = ids.size();
    ids.push_back(t.name);
  }
  const size_t z = ids.size();
  std::vector<double> excl_us(static_cast<size_t>(steps) * z, 0);
  std::vector<int64_t> excl_allocs(static_cast<size_t>(steps) * z, 0);
  std::vector<double> incl_us(z, 0), calls(z, 0);
  std::vector<prof::ZoneTotal> step_rows;
  double tracked_us = 0;
  for (int64_t i = 0; i < steps; i++) {
    const size_t a = med.bounds[static_cast<size_t>(i)];
    const size_t e = med.bounds[static_cast<size_t>(i) + 1];
    prof::totals(spans.subspan(a, e - a), step_rows);
    for (const prof::ZoneTotal& t : step_rows) {
      const size_t c = column[t.name];
      excl_us[static_cast<size_t>(i) * z + c] = static_cast<double>(t.excl_ns) / 1e3;
      excl_allocs[static_cast<size_t>(i) * z + c] = t.excl_allocs;
      incl_us[c] += static_cast<double>(t.incl_ns) / 1e3;
      calls[c] += static_cast<double>(t.calls);
    }
    for (size_t s = a; s < e; s++) {
      if (spans[s].depth == 0 && spans[s].dur_ns >= 0) {
        tracked_us += static_cast<double>(spans[s].dur_ns) / 1e3;
      }
    }
  }
  double med_total_us = static_cast<double>(med.total_ns) / 1e3;
  if (med_total_us <= 0) med_total_us = 1;
  b.untracked_share = std::max(0.0, 1 - tracked_us / med_total_us);

  b.warmup_steps = std::min(steps, static_cast<int64_t>(std::llround(spec.hz)));
  std::vector<int64_t> zone_allocs(z, 0);
  for (int64_t i = b.warmup_steps; i < steps; i++) {
    if (b.step_allocs[static_cast<size_t>(i)] <= 0) continue;
    b.steps_allocating++;
    if (b.first_allocating < 0) b.first_allocating = i;
    for (size_t c = 0; c < z; c++) zone_allocs[c] += excl_allocs[static_cast<size_t>(i) * z + c];
  }
  const double n = steps > 0 ? static_cast<double>(steps) : 1;
  for (size_t c = 0; c < z; c++) {
    ZoneCost zc;
    zc.name = std::string(prof::zone_name(ids[c]));
    zc.calls_per_step = calls[c] / n;
    double sum = 0;
    for (int64_t i = 0; i < steps; i++) sum += excl_us[static_cast<size_t>(i) * z + c];
    zc.excl_us_per_step = sum / n;
    zc.incl_us_per_step = incl_us[c] / n;
    zc.share = sum / med_total_us;
    zc.allocs = zone_allocs[c];
    b.zones.push_back(zc);
  }
  if (b.steps_allocating > 0) {
    const auto most = std::max_element(zone_allocs.begin(), zone_allocs.end());
    b.alloc_zone = most == zone_allocs.end() || *most == 0
                       ? ""
                       : b.zones[static_cast<size_t>(most - zone_allocs.begin())].name;
  }
  std::sort(b.zones.begin(), b.zones.end(), [](const ZoneCost& x, const ZoneCost& y) {
    return x.excl_us_per_step > y.excl_us_per_step;
  });

  std::vector<int64_t> by_time(static_cast<size_t>(steps));
  for (int64_t i = 0; i < steps; i++) by_time[static_cast<size_t>(i)] = i;
  std::sort(by_time.begin(), by_time.end(), [&](int64_t x, int64_t y) {
    return b.step_us[static_cast<size_t>(x)] > b.step_us[static_cast<size_t>(y)];
  });
  for (size_t k = 0; k < std::min(kSlowest, by_time.size()); k++) {
    const int64_t i = by_time[k];
    SlowStep s{i, b.step_us[static_cast<size_t>(i)], ""};
    double best = 0;
    for (size_t c = 0; c < z; c++) {
      const double v = excl_us[static_cast<size_t>(i) * z + c];
      if (v > best) {
        best = v;
        s.zone = std::string(prof::zone_name(ids[c]));
      }
    }
    b.slowest.push_back(s);
  }

  // The series: one row a step.
  std::string& t = b.series;
  t = "# title: " + std::string(spec.name) + " seed " + core::num::to_string(setup.seed) +
      ": each step's cost\n# x: step\n# unit.step_us: us\n# unit.allocs: count\n";
  t += "# budget_us: " + fixed(1e6 / spec.hz, 3) + "\nstep\tstep_us\tallocs";
  for (size_t c = 0; c < z; c++) t += "\t" + std::string(prof::zone_name(ids[c])) + "_us";
  t += "\n";
  for (int64_t i = 0; i < steps; i++) {
    t += std::to_string(i) + "\t" + fixed(b.step_us[static_cast<size_t>(i)], 3) + "\t" +
         std::to_string(b.step_allocs[static_cast<size_t>(i)]);
    for (size_t c = 0; c < z; c++) t += "\t" + fixed(excl_us[static_cast<size_t>(i) * z + c], 3);
    t += "\n";
  }
  // The spans round the slowest step.
  if (!by_time.empty()) {
    const int64_t mid = by_time[0];
    const int64_t a = std::max<int64_t>(0, mid - o.window / 2);
    const int64_t e = std::min<int64_t>(steps, a + o.window);
    std::string& sp = b.spans;
    sp = "# title: " + std::string(spec.name) + " seed " + core::num::to_string(setup.seed) +
         ": zones of steps " + std::to_string(a) + ".." + std::to_string(e - 1) +
         " (the slowest is " + std::to_string(mid) + ")\n# unit.start_ns: ns\n# unit.dur_ns: ns\n";
    sp += "name\tdepth\tstart_ns\tdur_ns\n";
    for (size_t s = med.bounds[static_cast<size_t>(a)]; s < med.bounds[static_cast<size_t>(e)];
         s++) {
      const prof::Span& x = spans[s];
      if (x.dur_ns < 0) continue;
      sp += std::string(prof::zone_name(x.name)) + "\t" + std::to_string(x.depth) + "\t" +
            std::to_string(x.start_ns) + "\t" + std::to_string(x.dur_ns) + "\n";
    }
  }
  return b;
}

}  // namespace ge::play
