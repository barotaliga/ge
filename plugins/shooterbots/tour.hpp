// THE TOUR: the order a player takes a set of stops in so the whole round is quickest, as a speed
// runner plans a map he knows (every monster, every secret, every button, then the exit), and not
// the nearest stop each time, which leaves a straggler at the far end of the map for last.
//
// The stops come as a cost table: `cost[i * (n + 1) + j]` the time from stop `i` to stop `j`,
// index 0 where he stands now and 1..n the stops (a time may differ each way: a drop is quick down
// and a long way round back up; `ai::kUnreached`, or anything past `kTourFar`, for no way at all).
// Some stops must come before others (`TourBefore`: a key before its door's button, a trigger
// before what its door hides); `last` (a stop's index, or 0 for none) is the exit, kept at the end.
//
// How: built nearest first among the stops whose stops before are all placed, then bettered by
// moving a run of one to three stops to another place (or-opt) and by turning a run end for end
// (2-opt, weighed whole since a way back may cost more than the way there), each gain taken as
// found in a fixed scan order, round after round until one gains nothing or `kTourRounds` are run;
// then `kTourKicks` times a run moved to a place drawn from a fixed seed and bettered again, kept
// when quicker. Deterministic to the bit: the same table gives the same order everywhere. No
// allocation once `Tour`'s arrays are sized; a few milliseconds for 150 stops.
//
// ge:summary The tour: the quickest order through a map's stops (or-opt, 2-opt, stops before others)
// ge:keywords tour route-order tsp travelling-salesman or-opt 2-opt precedence speed-run stops
// ge:keywords complete hundred-percent kill-order secrets-order campaign single-player
// ge:skill shooterbots bots
// ge:test tests/shooterbots_tour_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace ge::shooterbots {

constexpr double kTourFar = 1e6;  // a leg dearer than this has no way: counted at this, s
constexpr int kTourRounds = 64;   // rounds of moves at most
constexpr int kTourKicks = 24;    // runs moved at random and bettered again

// Stop `first` must come before stop `then` (both 1..n).
struct TourBefore {
  int32_t first = 0, then = 0;
};

struct TourStats {
  int64_t moves = 0;   // or-opt moves taken
  int64_t flips = 0;   // 2-opt flips taken
  int32_t rounds = 0;  // rounds run
  int64_t kicks = 0;   // kicks that found a quicker order
  double built = 0;    // the round's cost as first built, s
  double cost = 0;     // and when bettered, s
};

struct Tour {
  std::vector<int32_t> order;  // the stops (1..n) in the order taken
  TourStats stats;
  // Scratch, kept between calls.
  std::vector<int32_t> pos, trial;
  std::vector<uint8_t> placed;
  std::vector<int32_t> waiting;
};

// The order through stops 1..n of `cost` ((n + 1) * (n + 1), row-major) from stop 0, with
// `before` held and `last` (1..n, or 0 for none) at the end. Fills `t.order` and `t.stats`.
void tour_plan(Tour& t, std::span<const double> cost, int32_t n, std::span<const TourBefore> before,
               int32_t last = 0);

// The cost of taking `order` from stop 0 (each leg capped at kTourFar).
double tour_cost(std::span<const double> cost, int32_t n, std::span<const int32_t> order);

}  // namespace ge::shooterbots
