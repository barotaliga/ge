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

std::string one_place(double x) { return core::num::to_fixed(x, 1); }

}  // namespace

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
    line.append(midair_band_name(static_cast<MidairBand>(b)))
        .append(" ")
        .append(std::to_string(t.hits[b]));
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
