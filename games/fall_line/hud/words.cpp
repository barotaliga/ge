// Fall Line's HUD words defined: the numbers put into their lines.
//
// ge:summary Fall Line's HUD words defined: numbers formatted into lines
// ge:keywords hud words format seed distance seconds clock altitude run-name news
// ge:skill ui
// ge:test tests/fall_line_hud_test.cpp
#include "fall_line/hud/words.hpp"

#include <cmath>

#include "core/num_text.hpp"

namespace fall_line::hud::words {

namespace {

namespace num = ge::core::num;
namespace mg = ge::mountain::mapgen;

const char* grade_word(mg::PisteGrade g) {
  switch (g) {
    case mg::PisteGrade::green:
      return "GREEN";
    case mg::PisteGrade::blue:
      return "BLUE";
    case mg::PisteGrade::red:
      return "RED";
    case mg::PisteGrade::black:
      return "BLACK";
  }
  return "";
}

std::string upper(std::string s) {
  for (char& c : s)
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return s;
}

}  // namespace

std::string seed_at(double seed, double x, double z) {
  return num::to_fixed(seed, 0) + " @ " + num::to_fixed(x, 0) + "," + num::to_fixed(z, 0);
}

std::string distance(double m) {
  if (m >= 1000) return num::to_fixed(m / 1000, 1) + " KM";
  return num::to_fixed(m, 0) + " M";
}

std::string seconds(double s) { return num::to_fixed(s, 2) + " S"; }

std::string clock(double s) {
  const auto whole = static_cast<int64_t>(std::floor(s < 0 ? 0 : s));
  const int64_t sec = whole % 60;
  return std::to_string(whole / 60) + (sec < 10 ? ":0" : ":") + std::to_string(sec);
}

std::string altitude(double m) { return num::to_fixed(m, 0) + " M"; }

std::string run_name(const mg::Run& run) {
  if (run.kind == mg::RunKind::road) return "ROAD " + upper(run.id);
  return "RUN " + upper(run.id) + " " + grade_word(run.grade);
}

std::string landed(double air_s, double g) {
  return "AIR " + num::to_fixed(air_s, 2) + " S  " + num::to_fixed(g, 1) + " G";
}

std::string wipeout(ge::skier::Wipeout cause) {
  switch (cause) {
    case ge::skier::Wipeout::tree:
      return "WIPEOUT - A TREE";
    case ge::skier::Wipeout::landing:
      return "WIPEOUT - THE LANDING";
    case ge::skier::Wipeout::across:
      return "WIPEOUT - CAUGHT AN EDGE";
    case ge::skier::Wipeout::hurt:
      return "WIPEOUT - HIT";
    case ge::skier::Wipeout::none:
      break;
  }
  return "WIPEOUT";
}

std::string onto(const mg::Run& run) { return "ONTO " + run_name(run); }

std::string arrived(const mg::Run& run) { return "FOOT OF " + upper(run.id); }

std::string lift_top(double ride_s) { return "TOP OF THE LIFT  " + clock(ride_s); }

std::string wanted(sim::WantedChange change, int32_t stars) {
  switch (change) {
    case sim::WantedChange::raised:
      return "WANTED " + std::to_string(stars);
    case sim::WantedChange::lowered:
      return "WANTED DOWN TO " + std::to_string(stars);
    case sim::WantedChange::lost:
      return kEscaped;
    case sim::WantedChange::seen:
      return "SPOTTED";
    case sim::WantedChange::searching:
      return kSearching;
    case sim::WantedChange::arrested:
      return kArrested;
    case sim::WantedChange::killed:
      return kDied;
  }
  return "";
}

}  // namespace fall_line::hud::words
