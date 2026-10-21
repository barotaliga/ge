// plugins/mountain (mapgen, analysis) against tests/golden/mountain_generate.txt, maps built at
// version 1. Each row is one map: its level_digest, the attempt the search accepted, a 64-bit
// FNV-1a over each section of the level (the byte layout below is the golden's generator's
// too; keep the two in step), the
// analysis' findings and the debug lines of the attempts it refused. M4a's exit.
//
// ge:summary Golden test: the v1 generator's maps, section by section
// ge:keywords test golden mapgen
// ge:skill testing mountain
#include "mountain/mapgen/generate.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/output.hpp"
#include "golden.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/mapgen/digest.hpp"

namespace mg = ge::mountain::mapgen;
using ge::test::unhex;

namespace {

// A little-endian byte writer hashed as it goes (FNV-1a 64): golden-mapgen.mjs's `Section`.
struct Section {
  uint64_t h = 0xcbf29ce484222325ull;
  void byte(uint8_t b) { h = (h ^ b) * 0x100000001b3ull; }
  void raw(const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; i++) byte(b[i]);
  }
  void f64(double v) {
    uint64_t u;
    std::memcpy(&u, &v, 8);
    for (int i = 0; i < 8; i++) byte(uint8_t(u >> (8 * i)));
  }
  void f64(const std::optional<double>& v) {
    // An absent value is written as the quiet NaN.
    if (v) return f64(*v);
    for (int i = 0; i < 8; i++) byte(uint8_t(0x7ff8000000000000ull >> (8 * i)));
  }
  void i32(int32_t v) {
    const auto u = uint32_t(v);
    for (int i = 0; i < 4; i++) byte(uint8_t(u >> (8 * i)));
  }
  void str(const std::string& s) {
    for (const char c : s) byte(uint8_t(c));
    byte(0);
  }
  std::string hex() const {
    char out[17];
    std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(h));
    return out;
  }
};

std::string grid_hash(const ge::core::Heightfield& f) {
  Section s;
  s.raw(f.data.data(), f.data.size() * sizeof(float));
  return s.hex();
}

// Every section the golden row names, as the script writes it.
std::vector<std::pair<std::string, std::string>> sections(const mg::Level& l) {
  std::vector<std::pair<std::string, std::string>> out;
  out.emplace_back("ground", grid_hash(l.ground));
  out.emplace_back("packed", grid_hash(l.packed));
  out.emplace_back("crust", l.crust ? grid_hash(*l.crust) : "-");
  Section s;
  for (const auto& p : l.track.points) {
    for (const double v : {p.x, p.z, p.y, p.s, p.heading, p.width}) s.f64(v);
  }
  out.emplace_back("points", s.hex());
  s = {};
  for (const auto& c : l.checkpoints) {
    for (const double v : {c.x, c.z, c.y, c.heading, c.width, c.s}) s.f64(v);
    s.byte(c.colour == mg::GateColour::red ? 0 : 1);
  }
  out.emplace_back("gates", s.hex());
  s = {};
  for (const double v : {l.spawn.x, l.spawn.z, l.spawn.heading}) s.f64(v);
  for (const auto& g : l.grid) {
    for (const double v : {g.x, g.z, g.heading}) s.f64(v);
  }
  out.emplace_back("start", s.hex());
  s = {};
  for (const auto& t : l.trees) {
    for (const double v : {t.x, t.z, t.y, t.height, t.radius, t.age, t.crown}) s.f64(v);
    s.byte(uint8_t(t.kind));
    s.i32(t.clump);
  }
  out.emplace_back("trees", s.hex());
  s = {};
  for (const auto& k : l.kickers) {
    s.str(k.id);
    for (const double v : {k.x, k.z, k.y, k.heading, k.height, k.ramp, k.landing, k.width}) {
      s.f64(v);
    }
    s.byte(k.on_track ? 1 : 0);
    s.f64(k.s);
  }
  out.emplace_back("kickers", s.hex());
  s = {};
  for (const auto& c : l.cliffs) {
    s.str(c.id);
    for (const double v :
         {c.x, c.z, c.y, c.heading, c.drop, c.face, c.apron, c.landing, c.shelf, c.width}) {
      s.f64(v);
    }
    s.byte(c.on_track ? 1 : 0);
    s.f64(c.s);
  }
  out.emplace_back("cliffs", s.hex());
  s = {};
  for (const auto& d : l.drifts) {
    s.f64(d.from);
    s.f64(d.to);
  }
  out.emplace_back("drifts", s.hex());
  s = {};
  const auto& m = l.mountain;
  for (const double v :
       {m.summit.x, m.summit.y, m.summit.z, m.base.x, m.base.y, m.base.z, m.vertical, m.altitude,
        m.tree_line, m.sea, l.sun.hour, l.sun.day_of_year, l.sun.latitude}) {
    s.f64(v);
  }
  s.byte(uint8_t(l.weather.kind));
  for (const double v : {l.weather.snowfall, l.weather.fog, l.weather.wind, l.weather.wind_from}) {
    s.f64(v);
  }
  s.byte(l.weather.evening ? 1 : 0);
  out.emplace_back("world", s.hex());
  return out;
}

mg::RegionId region_of(const std::string& name) {
  if (name == "fell") return mg::RegionId::fell;
  if (name == "continental") return mg::RegionId::continental;
  if (name == "maritime") return mg::RegionId::maritime;
  return mg::RegionId::alpine;
}

std::string word(const std::vector<std::string>& row, const std::string& key) {
  for (const auto& w : row) {
    if (w.rfind(key + "=", 0) == 0) return w.substr(key.size() + 1);
  }
  return "?";
}

std::vector<std::string> debug_lines;

}  // namespace

TEST(single_piste_levels_match_golden) {
  const auto rows = ge::test::golden_words("tests/golden/mountain_generate.txt");
  CHECK(rows.size() == 11);
  ge::core::output::set_debug_enabled(true);
  ge::core::output::set_output_sink([](ge::core::output::Level level, std::string_view msg, void*) {
    if (level == ge::core::output::Level::debug) debug_lines.emplace_back(msg);
  });
  for (const auto& row : rows) {
    const double seed = std::stod(row[0]);
    const std::string where = "seed " + row[0] + " " + row[2] + " (v" + row[1] + ")";
    debug_lines.clear();
    mg::GenerateOptions opts;
    opts.version = std::stoi(row[1]);
    opts.region = region_of(row[2]);
    const auto t0 = std::chrono::steady_clock::now();
    mg::Level level;
    try {
      level = mg::generate_level(seed, opts);
    } catch (const mg::GenerateError& e) {
      ge::test::fail(
          __FILE__, __LINE__,
          where + ": generate_level threw `" + std::string(e.what()).substr(0, 600) +
              "` where the golden has a map. The first draw that differs is upstream of the "
              "refusal (porting/README.md § Bisecting, while porting lasts).");
      continue;
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  %s: %s attempt %d, %zu trees, %.0f ms\n", where.c_str(),
                mg::level_digest(level).c_str(), level.attempt, level.trees.size(), ms);
    const auto expect = [&](const std::string& key, const std::string& got) {
      const std::string want = word(row, key);
      if (got == want) return;
      std::string m = where;
      m.append(": `")
          .append(key)
          .append("` is ")
          .append(got)
          .append(", the golden has ")
          .append(want);
      m += ". The first section that moves (in generate order: ground, points, kickers, cliffs, "
           "packed, drifts, crust, trees, world, gates, start) is the step that diverges; "
           "bisect inside it (the determinism skill).";
      ge::test::fail(__FILE__, __LINE__, m);
    };
    expect("digest", mg::level_digest(level));
    expect("attempt", std::to_string(level.attempt));
    for (const auto& [key, hex] : sections(level)) expect(key, hex);
    const auto analysis = ge::mountain::analysis::analyze_level(level);
    std::string findings;
    for (const auto& f : analysis.findings) {
      if (!findings.empty()) findings += ",";
      findings +=
          f.rule + ":" + (f.severity == ge::mountain::analysis::Severity::error ? "error" : "warn");
    }
    expect("findings", findings.empty() ? "-" : findings);
    std::string log;
    for (const auto& line : debug_lines) log += (log.empty() ? "" : "\n") + line;
    const std::string want_log = unhex(word(row, "log"));
    if (log != want_log) {
      std::string m = where;
      m.append(": the search logged\n  ")
          .append(log)
          .append("\nthe golden logged\n  ")
          .append(want_log);
      m += "\nA refused attempt's findings differ: check the analysis rule the line names, and "
           "its message's number formatting (core/num_text).";
      ge::test::fail(__FILE__, __LINE__, m);
    }
  }
  ge::core::output::set_output_sink(nullptr);
  ge::core::output::set_debug_enabled(false);
}
