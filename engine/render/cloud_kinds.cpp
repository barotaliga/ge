// The cloud genera's rows (render/cloud_kinds.hpp says what each number is).
//
// ge:summary The cloud genera's rows, and a stack of them read from a name
// ge:keywords render clouds cloud-kinds cloud_stack_named cloud_kind_named
// ge:skill render
// ge:test tests/render_cloud_kinds_test.cpp
#include "render/cloud_kinds.hpp"

#include <array>
#include <cctype>

namespace ge::render {

namespace {

// base, depth, cover, cell bands; stretch, dome, opacity, shadow, life, speed; anvil, erode,
// fibre, patch; the particles' radius, their spread, the halo; the deck; the thinnest patches'
// share of the extinction and a patch's span in cells.
const std::array<CloudKind, 16> kKinds{{
    {"inversion",
     "a sea of cloud filling the valleys under a clear sky",
     {-150, -50},
     {350, 500},
     {0.92, 0.98},
     {2500, 3500},
     2,
     0.15,
     30,
     3,
     2400,
     2,
     0,
     0.35,
     0,
     0,
     9,
     0.35,
     0,
     1,
     0.5,
     3},

    {"fractus",  "ragged scud racing low under the sky, the moon through its gaps",
     {200, 450}, {120, 260},
     {0.2, 0.4}, {200, 400},
     1.4,        0.6,
     30,         2,
     300,        6,
     0,          0.65,
     0,          0.3,
     9,          0.35,
     0,          0,
     0.25,       2},

    {"stratus",   "a low grey lid, nearly even, the hills' tops in it",
     {350, 600},  {300, 500},
     {0.96, 1.0}, {3000, 5000},
     2,           0,
     28,          3,
     2400,        4,
     0,           0.3,
     0,           0,
     9,           0.35,
     0,           1,
     0.12,        2},

    {"stratocumulus",
     "low rolls and lumps packed edge to edge, blue between",
     {900, 1400},
     {300, 500},
     {0.65, 0.82},
     {600, 1000},
     1.6,
     0.5,
     18,
     2.6,
     900,
     5,
     0,
     0.5,
     0,
     0,
     9,
     0.25,
     0,
     0.6,
     0.15,
     3},

    {"cumulus",
     "the fair day's heaps, flat-based, a fifth to a third of the sky",
     {1500, 2000},
     {700, 1200},
     {0.22, 0.36},
     {1000, 1500},
     1.3,
     1,
     28,
     2.4,
     480,
     4,
     0,
     0.6,
     0,
     0,
     10,
     0.35,
     0,
     0,
     0.5,
     2},

    {"congestus",
     "towering heaps, their tops boiling",
     {1500, 1800},
     {1300, 2000},
     {0.32, 0.44},
     {1700, 2400},
     1.3,
     1,
     40,
     3,
     600,
     4,
     0,
     0.6,
     0,
     0,
     10,
     0.35,
     0,
     0,
     0.6,
     2},

    {"cumulonimbus",
     "the storm's tower, its top spread into an anvil",
     {1200, 1500},
     {7000, 9000},
     {0.07, 0.12},
     {9000, 13000},
     1.1,
     1,
     160,
     6,
     1500,
     6,
     0.85,
     0.6,
     0,
     0,
     12,
     0.4,
     0,
     0,
     0.6,
     1.5},

    {"nimbostratus",
     "the dark rain and snow deck, thick and featureless",
     {600, 1000},
     {2200, 3200},
     {0.99, 1.0},
     {5000, 8000},
     2,
     0,
     90,
     5,
     3000,
     6,
     0,
     0.4,
     0,
     0,
     12,
     0.4,
     0,
     1,
     0.5,
     1.5},

    {"altocumulus",
     "rippled sheets of small cells in the middle air",
     {3600, 4500},
     {200, 400},
     {0.45, 0.6},
     {350, 600},
     1.4,
     0.4,
     4,
     0.5,
     900,
     8,
     0,
     0.6,
     0,
     0.5,
     8,
     0.1,
     0,
     0,
     0.3,
     4},

    {"altostratus",
     "a grey veil in the middle air, the sun a pale disc through it",
     {3200, 4200},
     {900, 1400},
     {0.97, 1.0},
     {6000, 9000},
     2.5,
     0,
     32,
     1.2,
     2400,
     10,
     0,
     0.2,
     0,
     0,
     14,
     0.45,
     0,
     1,
     0.05,
     1.5},

    {"lenticular",
     "smooth lenses a mountain wave holds still",
     {3500, 4800},
     {500, 900},
     {0.2, 0.3},
     {2500, 3500},
     2.5,
     0,
     18,
     1.5,
     3600,
     1,
     0,
     0.06,
     0,
     0,
     6,
     0.06,
     0,
     0,
     0.4,
     1},

    {"cirrocumulus",
     "fine high ripples, grains of ice in rows: a mackerel sky",
     {6500, 8000},
     {100, 200},
     {0.5, 0.65},
     {200, 320},
     1.6,
     0.3,
     1.5,
     0.2,
     1200,
     15,
     0,
     0.7,
     0,
     0.6,
     9,
     0.12,
     0,
     0,
     0.3,
     4},

    {"veil",
     "a thin even veil of ice over the whole sky, the sun bright through it in its ring",
     {7000, 8000},
     {500, 800},
     {0.97, 1.0},
     {7000, 10000},
     3,
     0,
     0.45,
     0.12,
     2400,
     15,
     0,
     0.25,
     0.15,
     0,
     30,
     0.45,
     1,
     0,
     0.5,
     1.5},

    {"cirrostratus",
     "a thin milky veil of ice over most of the sky",
     {7000, 7800},
     {600, 900},
     {0.75, 0.92},
     {6000, 9000},
     2.5,
     0,
     0.8,
     0.2,
     1800,
     15,
     0,
     0.6,
     0.4,
     0,
     35,
     0.45,
     0.4,
     0,
     0.3,
     2},

    {"cirrus",
     "high hairs of ice combed out by the wind",
     {7500, 9000},
     {400, 800},
     {0.4, 0.55},
     {2500, 4000},
     5,
     0,
     4,
     0.25,
     1200,
     15,
     0,
     0.7,
     0.8,
     0,
     40,
     0.45,
     0.8,
     0,
     0.1,
     2},

    {"wisps",
     "a few high ice streaks on a fair day",
     {7600, 8800},
     {300, 600},
     {0.02, 0.14},
     {3000, 5000},
     5,
     0,
     0.9,
     0.25,
     1200,
     15,
     0,
     0.6,
     0.8,
     0,
     40,
     0.45,
     0.8,
     0,
     0.3,
     2},
}};

bool same_word(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

}  // namespace

std::span<const CloudKind> cloud_kinds() { return kKinds; }

const CloudKind* cloud_kind_named(std::string_view name) {
  for (const CloudKind& k : kKinds) {
    if (same_word(name, k.name)) return &k;
  }
  return nullptr;
}

const std::string& cloud_kind_list() {
  static const std::string list = [] {
    std::string s;
    for (const CloudKind& k : kKinds) s += (s.empty() ? "" : ", ") + std::string(k.name);
    return s;
  }();
  return list;
}

std::string cloud_stack_named(std::string_view names, const CloudKind* out[kCloudLayers],
                              int& count) {
  count = 0;
  size_t at = 0;
  while (at <= names.size()) {
    const size_t plus = names.find('+', at);
    const size_t end = plus == std::string_view::npos ? names.size() : plus;
    std::string_view word = names.substr(at, end - at);
    while (!word.empty() && word.front() == ' ') word.remove_prefix(1);
    while (!word.empty() && word.back() == ' ') word.remove_suffix(1);
    if (!word.empty()) {
      const CloudKind* k = cloud_kind_named(word);
      if (k == nullptr) {
        return "'" + std::string(word) + "' names no kind of cloud; the kinds are " +
               cloud_kind_list();
      }
      if (count == kCloudLayers) {
        return "more than " + std::to_string(kCloudLayers) + " kinds of cloud in '" +
               std::string(names) + "'; the sky draws " + std::to_string(kCloudLayers) +
               " layers at most";
      }
      out[count++] = k;
    }
    if (plus == std::string_view::npos) break;
    at = plus + 1;
  }
  return {};
}

}  // namespace ge::render
