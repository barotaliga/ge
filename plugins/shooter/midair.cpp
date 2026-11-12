// Midair (midair.hpp).
//
// ge:summary Midair for any arena shooter: the bands, the push, the lines and the tally
// ge:keywords midair midair_band midair_push midair_line midair-tally k_midair_minheight
// ge:skill shooter
// ge:test tests/shooter_midair_test.cpp
#include "shooter/midair.hpp"

#include "core/num_text.hpp"

namespace ge::shooter {

namespace {

constexpr float kMinHeights[kMidairLevels] = {64, 128, 256, 512, 1024};
constexpr const char* kLevelNames[kMidairLevels] = {"ground", "bronze", "silver", "gold",
                                                    "platinum"};
constexpr const char* kBandNames[kMidairBands] = {"bronze", "silver", "gold", "platinum"};
constexpr int kBandPoints[kMidairBands] = {1, 2, 4, 8};

bool good_level(int level) { return level >= 0 && level < kMidairLevels; }

std::string one_place(double x) { return core::num::to_fixed(x, 1); }

}  // namespace

float midair_min_height(int level) { return good_level(level) ? kMinHeights[level] : 64; }

const char* midair_level_name(int level) {
  return good_level(level) ? kLevelNames[level] : kLevelNames[0];
}

int midair_level(std::string_view text) {
  if (text.size() == 1 && text[0] >= '0' && text[0] < '0' + kMidairLevels) return text[0] - '0';
  return kMidairDefaultLevel;
}

MidairBand midair_band(float rise) {
  if (rise > 1024) return MidairBand::platinum;
  if (rise > 512) return MidairBand::gold;
  if (rise > 256) return MidairBand::silver;
  return MidairBand::bronze;
}

int midair_points(MidairBand b) { return kBandPoints[static_cast<int>(b)]; }

const char* midair_band_name(MidairBand b) { return kBandNames[static_cast<int>(b)]; }

bool midair_counts(float height, int level) { return height >= midair_min_height(level); }

int midair_score(float height, float rise, int level) {
  return midair_counts(height, level) ? midair_points(midair_band(rise)) : 0;
}

MidairPush midair_push(float height, float damage, bool own) {
  MidairPush p;
  const bool weak = damage < kMidairWeakSplash && !own;
  if (weak) p.scale = 11.0f / 8.0f;
  if (height >= kMidairLowAir) {
    p.scale *= 1 + (height - kMidairLowAir) / 64;
  } else {
    // The up part times the damage times 6 or 4, over the classic push of damage × factor.
    p.lift = (weak ? 6.0f : 4.0f) / (8.0f * p.scale);
  }
  return p;
}

std::string midair_line(MidairBand b, float rise, std::string_view shooter,
                        std::string_view victim) {
  std::string name = midair_band_name(b);
  name[0] = static_cast<char>(name[0] - 32);
  std::string line = name;
  line.append(" midair by ").append(shooter).append(" on ").append(victim);
  line.append(" (").append(one_place(rise)).append(" up)\n");
  return line;
}

void midair_note(MidairTally& t, float rise) {
  t.hits[static_cast<int>(midair_band(rise))]++;
  if (t.total() == 1 || rise > t.highest) t.highest = rise;
  t.rise_sum += rise;
}

std::string midair_tally_line(std::string_view name, const MidairTally& t) {
  const int n = t.total();
  if (n == 0) return {};
  std::string line(name);
  line.append(": ").append(std::to_string(n)).append(n == 1 ? " midair (" : " midairs (");
  for (int b = 0; b < kMidairBands; b++) {
    if (b > 0) line.append(", ");
    line.append(kBandNames[b]).append(" ").append(std::to_string(t.hits[b]));
  }
  line.append("), highest ").append(one_place(t.highest));
  line.append(", average ").append(one_place(t.rise_sum / n)).append("\n");
  return line;
}

std::string midair_level_line(int level) {
  std::string line = "Midair hits now count from ";
  line.append(std::to_string(static_cast<int>(midair_min_height(level)))).append(" units up (");
  line.append(midair_level_name(level)).append(")\n");
  return line;
}

}  // namespace ge::shooter
