// THE QUALITY LADDER'S FIT: which stop of each picture row a machine draws at, so the frame holds
// its budget and keeps as much of the look as it can (docs/rendering.md § The quality ladder).
// Generic: a game states its rows and their stops (what each stop does to its passes is the
// game's), prices every stop with a lab, and hands the price table here.
//
// - EVERY STOP HAS A COST AND A WORTH. `cost_ms` is what the stop adds to a frame over its row's
//   cheapest, measured on the table's reference machine; `worth` is what it adds to the look over
//   the cheapest, on one scale for every row (a judgement the game writes down beside its row).
//   A row's stops run cheapest first, and the first costs and is worth 0.
// - ONE MACHINE IS THE REFERENCE SCALED. A machine is read as the reference made faster or slower:
//   its timed frame at the picture it drew, over what the table says that picture costs
//   (`machine_scale`), scales every cost. Fit, apply, time again and fit again: the second reading
//   is of the neighbourhood the answer lies in.
// - THE FIT IS EXACT: of every picture the rows can make (one stop a row), the one of most worth
//   whose scaled cost fits the budget over the floor, a knapsack with a choice in each row, solved
//   in hundredths of a millisecond. A greedy walk can hide a dear stop that matters behind a cheap
//   one that does not, and hand a slower machine more of the look than a faster one; the exact
//   answer only loses pictures as the budget shrinks. A stop that costs nothing is always kept.
// - ROWS GIVEN UP FIRST, BY RULE (`LadderRow::first`): such rows step down their ladders, in their
//   order, a stop at a time, while every other row stays at its top; only once they are all at
//   their cheapest does the search over the other rows begin.
//
// ge:summary The quality ladder's fit: the price table, the machine's scale, the exact knapsack
// ge:keywords render quality-ladder picture-fit preset price-table knapsack budget fit_ladder
// ge:keywords machine_scale ladder_cost ladder_worth automatic-preset
// ge:skill render perf
// ge:test tests/render_ladder_test.cpp
#pragma once

#include <span>
#include <string_view>
#include <vector>

namespace ge::render {

struct LadderStop {
  std::string_view name;
  double cost_ms = 0;  // over the row's cheapest stop, on the reference machine
  double worth = 0;    // over the row's cheapest stop, on the game's one scale
};

struct LadderRow {
  std::string_view name;
  std::span<const LadderStop> stops;  // cheapest first
  int first = 0;  // 0: an ordinary row; 1, 2, ...: given up first, in this order
};

// A picture: a stop for each row, by index.
using LadderPick = std::vector<int>;

// What a picture costs on the reference machine over the floor, and what it is worth.
double ladder_cost(std::span<const LadderRow> rows, const LadderPick& pick);
double ladder_worth(std::span<const LadderRow> rows, const LadderPick& pick);

// A machine's scale: its timed frame (ms) at `drawn` over what the table says that picture costs
// with the floor (the frame of every row at its cheapest). 1 on the reference machine.
double machine_scale(std::span<const LadderRow> rows, const LadderPick& drawn, double floor_ms,
                     double timed_ms);

struct LadderFit {
  LadderPick pick;     // a stop for each row
  double cost_ms = 0;  // the pick's cost over the floor, scaled
  double worth = 0;    // the pick's worth
  bool fits = false;   // false: even the cheapest picture is over the budget (pick is that one)
};

// The picture of most worth that fits `budget_ms` on a machine `scale` times the reference, with
// `floor_ms` the reference's frame at every row's cheapest.
LadderFit fit_ladder(std::span<const LadderRow> rows, double budget_ms, double floor_ms,
                     double scale = 1.0);

}  // namespace ge::render
