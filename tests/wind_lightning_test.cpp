// engine/wind/lightning.hpp and thunder.hpp: a severe cell's flashes sit in the observed bands
// (most in the cloud; a ground flash of about four strokes, a quarter single, about 60 ms apart,
// two in five with a continuing current); the schedule is the same however it is asked for; the
// light integrates in closed form (any split of a window sums to the whole) and a frame holding a
// stroke gets its energy; a ground channel reaches the ground and branches; thunder is heard at
// its distance over the speed of sound, later, quieter and darker farther off and not at all past
// 25 km; and every number keeps its bits in tests/golden/wind_lightning.txt.
//
// A deliberate change moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/wind_lightning_test`, and put the `storm` lab's
// tables before and after in the PR.
//
// ge:summary Test: lightning's bands, schedule, closed-form light, channels, thunder; golden
// ge:keywords test lightning flash stroke interstroke continuing-current channel thunder golden
// ge:skill water testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "wind/lightning.hpp"
#include "wind/thunder.hpp"

namespace {

namespace gw = ge::wind;

constexpr const char* kGolden = "tests/golden/wind_lightning.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

gw::Storm severe() {
  gw::Storm s;
  s.vz = 8;
  s.cells = 1;
  s.cell[0].peak = 1;
  s.cell[0].core = 3500;
  s.cell[0].seed = 1234;
  return s;
}

std::vector<gw::Flash> all_of(const gw::Storm& s) {
  std::vector<gw::Flash> out;
  const double life = gw::life_of(s.cell[0].peak);
  for (int64_t n = 0; n < static_cast<int64_t>(life); ++n) {
    gw::Flash f;
    if (gw::flash_in_slot(s, 0, n, f)) out.push_back(f);
  }
  return out;
}

}  // namespace

TEST(a_severe_cells_flashes_sit_in_the_observed_bands) {
  const auto flashes = all_of(severe());
  CHECK(flashes.size() > 500);
  int ground = 0, single = 0, strokes = 0, held = 0, gaps = 0, blue = 0;
  double log_gap = 0;
  for (const gw::Flash& f : flashes) {
    CHECK(f.length > 0 && f.length <= gw::kMaxFlash);
    if (f.kind != gw::FlashKind::ground) continue;
    ++ground;
    strokes += f.strokes;
    if (f.strokes == 1) ++single;
    if (f.blue) ++blue;
    for (int i = 0; i < f.strokes; ++i) {
      if (f.stroke[i].hold > 0) ++held;
      if (i > 0) {
        const double gap = f.stroke[i].t - f.stroke[i - 1].t;
        CHECK(gap >= 0.008 - 1e-9 && gap <= 0.3 + 1e-9);
        log_gap += std::log(gap);
        ++gaps;
      }
    }
    CHECK(f.ey == 0 && f.y > 3000);
  }
  const double cloud_share = 1 - static_cast<double>(ground) / static_cast<double>(flashes.size());
  CHECK(cloud_share > 0.7 && cloud_share < 0.95);
  const double mean = static_cast<double>(strokes) / ground;
  CHECK(mean > 3 && mean < 4.6);
  CHECK(static_cast<double>(single) / ground > 0.15 && static_cast<double>(single) / ground < 0.35);
  const double gm = std::exp(log_gap / gaps);
  CHECK(gm > 0.045 && gm < 0.08);
  CHECK(static_cast<double>(held) / ground > 0.3 && static_cast<double>(held) / ground < 0.5);
  CHECK(blue < ground / 8);
}

TEST(the_schedule_is_the_same_however_it_is_asked_for) {
  const gw::Storm s = severe();
  const double t0 = gw::kTowering + 300, t1 = t0 + 60;
  gw::Flash whole[256], split[256];
  const int n = gw::flashes_between(s, t0, t1, whole, 256);
  CHECK(n > 10 && n < 256);
  for (int i = 1; i < n; ++i) CHECK(whole[i - 1].t0 <= whole[i].t0);
  // Every flash starting in the minute is found again by asking second by second.
  int again = 0;
  for (int k = 0; k < 60; ++k) {
    const int m = gw::flashes_between(s, t0 + k, t0 + k + 1, split, 256);
    for (int i = 0; i < m; ++i) {
      if (split[i].t0 >= t0 + k && split[i].t0 < t0 + k + 1) ++again;
    }
  }
  int starting = 0;
  for (int i = 0; i < n; ++i) starting += whole[i].t0 >= t0 ? 1 : 0;
  CHECK(again == starting);
  CHECK(gw::flashes_between(s, -100, -50, whole, 256) == 0);
}

TEST(the_light_integrates_and_a_frame_gets_its_stroke) {
  const auto flashes = all_of(severe());
  const gw::Flash* g = nullptr;
  for (const gw::Flash& f : flashes) {
    if (f.kind == gw::FlashKind::ground && f.strokes >= 3) {
      g = &f;
      break;
    }
  }
  CHECK(g != nullptr);
  if (g == nullptr) return;
  const double a = g->t0 - 0.1, b = g->t0 + g->length + 0.1;
  const double whole = gw::light_over(*g, a, b) * (b - a);
  double parts = 0;
  for (int i = 0; i < 1000; ++i) {
    const double p0 = a + (b - a) * i / 1000, p1 = a + (b - a) * (i + 1) / 1000;
    parts += gw::light_over(*g, p0, p1) * (p1 - p0);
  }
  CHECK(std::fabs(whole - parts) < 1e-9 * (1 + whole));
  // A 60 Hz frame opening on the first stroke holds nearly all its light.
  const double ts = g->t0 + g->stroke[0].t;
  const double frame = gw::light_over(*g, ts, ts + 1.0 / 60);
  CHECK(frame > 0.95 * g->stroke[0].energy);
  // Nothing before the leader; the leader faint.
  CHECK(gw::light_over(*g, g->t0 - 0.05, g->t0) == 0);
  CHECK(gw::light_over(*g, g->t0, g->t0 + 0.5 * g->leader) < 0.02);
  CHECK(gw::reach_of(0) == 1 && std::fabs(gw::reach_of(4000) - 0.5) < 1e-12);
}

TEST(a_ground_channel_reaches_the_ground_and_branches) {
  const auto flashes = all_of(severe());
  auto channel = std::make_unique<gw::Channel>();
  int grounds = 0;
  for (const gw::Flash& f : flashes) {
    gw::build_channel(f, *channel);
    CHECK(channel->count > 20 && channel->count <= gw::kMaxSegments);
    CHECK(channel->seg[0].order == 0 && channel->seg[0].glow == 1);
    if (f.kind != gw::FlashKind::ground) continue;
    int main = 0, branches = 0;
    float lowest = 1e30f;
    for (int i = 0; i < channel->count; ++i) {
      const gw::Segment& s = channel->seg[i];
      if (s.order == 0) {
        ++main;
        if (s.y1 < lowest) lowest = s.y1;
      } else {
        ++branches;
      }
      CHECK(s.y1 >= f.ey - 1e-3f);
    }
    CHECK(lowest <= static_cast<float>(f.ey) + 1);
    CHECK(main > 40);
    if (++grounds > 60) break;
    (void)branches;
  }
  CHECK(grounds > 30);
}

TEST(thunder_comes_at_its_distance_later_quieter_and_darker_farther_off) {
  const auto flashes = all_of(severe());
  const gw::Flash* g = nullptr;
  for (const gw::Flash& f : flashes) {
    if (f.kind == gw::FlashKind::ground && !f.blue) {
      g = &f;
      break;
    }
  }
  CHECK(g != nullptr);
  if (g == nullptr) return;
  auto channel = std::make_unique<gw::Channel>();
  gw::build_channel(*g, *channel);
  auto near = std::make_unique<gw::Thunder>(), far = std::make_unique<gw::Thunder>(),
       gone = std::make_unique<gw::Thunder>();
  const double c = gw::sound_speed(20);
  CHECK(std::fabs(c - 343.42) < 0.01);
  gw::hear_thunder(*g, *channel, g->ex + 1000, 0, g->ez, c, *near);
  gw::hear_thunder(*g, *channel, g->ex + 10000, 0, g->ez, c, *far);
  gw::hear_thunder(*g, *channel, g->ex + 40000, 0, g->ez, c, *gone);
  CHECK(std::fabs(near->first - (g->stroke[0].t + near->nearest / c)) < 1e-12);
  CHECK(near->nearest > 900 && near->nearest < 1100);
  CHECK(far->first > near->first + 25);
  CHECK(near->length > 5 && far->length > 1);
  CHECK(far->peak < 0.1 * near->peak);
  double near_cut = 0, far_cut = 0, nw = 0, fw = 0;
  for (int b = 0; b < near->bins; ++b) {
    near_cut += near->cutoff[b] * near->loud[b];
    nw += near->loud[b];
  }
  for (int b = 0; b < far->bins; ++b) {
    far_cut += far->cutoff[b] * far->loud[b];
    fw += far->loud[b];
  }
  CHECK(near_cut / nw > 2 * far_cut / fw);
  CHECK(gone->bins == 0);
  std::vector<float> pcm(48000), again(48000);
  gw::render_thunder(*near, 48000, 9, 1, pcm);
  gw::render_thunder(*near, 48000, 9, 1, again);
  double top = 0;
  for (size_t i = 0; i < pcm.size(); ++i) {
    CHECK(std::isfinite(pcm[i]) && std::fabs(pcm[i]) < 1);
    top = std::fmax(top, std::fabs(pcm[i]));
  }
  CHECK(pcm == again && top > 0.05);
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  gw::StormSpec spec;
  spec.seed = 38;
  spec.renew = 1800;
  spec.severity = 0.7;
  spec.vx = 6;
  spec.vz = 4;
  const gw::Storm s = gw::lay_storm(spec);
  gw::Flash fl[64];
  const int n = gw::flashes_between(s, 1500, 1530, fl, 64);
  auto channel = std::make_unique<gw::Channel>();
  auto thunder = std::make_unique<gw::Thunder>();
  got.push_back({static_cast<uint64_t>(n)});
  for (int i = 0; i < n && i < 64; ++i) {
    const gw::Flash& f = fl[i];
    gw::build_channel(f, *channel);
    gw::hear_thunder(f, *channel, f.ex + 2000, 0, f.ez, 340, *thunder);
    const gw::Segment& last = channel->seg[channel->count - 1];
    got.push_back({static_cast<uint64_t>(f.kind), static_cast<uint64_t>(f.cell),
                   ge::test::bits(f.t0), ge::test::bits(f.x), ge::test::bits(f.y),
                   ge::test::bits(f.ex), ge::test::bits(f.ez), static_cast<uint64_t>(f.strokes),
                   ge::test::bits(f.stroke[f.strokes - 1].t),
                   ge::test::bits(gw::light_over(f, f.t0, f.t0 + 1.0 / 60)),
                   static_cast<uint64_t>(channel->count), ge::test::bits(double{last.x1}),
                   ge::test::bits(double{last.y1}), ge::test::bits(thunder->first),
                   static_cast<uint64_t>(thunder->bins), ge::test::bits(thunder->peak)});
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# flash count; per flash: kind cell t0 x y ex ez strokes last-t frame-light, "
           "channel count, last x y, thunder first bins peak (wind_lightning_test)\n";
    for (const auto& row : got) {
      std::string line;
      for (const uint64_t word : row) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(word));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  CHECK_MSG(got == want, "the lightning moved; if the move is meant, rewrite " +
                             std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the storm "
                             "lab's tables before and after in the PR (skill: water)");
}
