// THE ARMY ON FOOT: the soldiers the wanted level sends after a skier, on skis. Its numbers are
// army_rules.hpp, each man's way over the snow army_troop.hpp, the state `GameState::army`
// (army_state.hpp).
//
// - UP THE LIFTS: from the star whose roster asks for them (`WantedRoster::lifted`), a squad of
//   `army::kSquad` is put down `kRespondS` after the call below the load line of the lift whose
//   top is nearest him and well above him (`lift_for`), skates up to it, is taken by the carriers
//   that come round (`kSeats` a carrier), rides up and is let go where a rider is, after him.
//   Another every `kEveryS` while the roster is short of them.
// - THE GROUND WAVES (`ground_wave`, counted with `waves_sent`): each wave's men on foot are put
//   down out of his sight `kDismountM` down THE WAY from him (`GameState::ways`: the run nearest
//   him and every run it flows into, to its foot and straight on past it), in a column of files
//   as wide as the run, and climb it at him; a man leaves the column for the fight near where he
//   was last seen or seeing him.
// - THE CHASE: after him while he is seen, else to where he was last seen, standing there once
//   it is searched. Where the army shoots to kill a man who sees him within `kStandOffM` HOLDS
//   there with his rifle on him (their fire is a later row); where it does not, they run him down
//   and the first to reach him while he is slow or down ARRESTS him (`arrest_quarry`).
// - Every man who can look is written into the wanted level's pursuers each step (not one in a
//   gondola's cabin), so the army's own eyes keep the stars up.
// - SENT HOME: a wave's man left `kBehindM` behind while the hunt runs; once it is over, every
//   man further than `kHomeM` from him. The rest stand where they are.
//
// `step` calls `step_army` after skier against skier and before the wanted level (game.hpp has
// the order). Nothing here draws a random number or allocates in a step, and a run whose hunt
// never reached a star leaves the army inactive and unhashed.
//
// ge:summary The army on foot: lift squads, ground waves in a column, the chase, hold and arrest
// ge:keywords army soldier troops step_army lift_for army_ways lift-squad ground-wave column
// ge:keywords chase hold arrest stand-off sent-home army_hash pursuers way-down army_way_at
// ge:keywords enlist_soldier
// ge:skill sim
// ge:test tests/fall_line_army_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "fall_line/sim/army_state.hpp"
#include "fall_line/sim/state.hpp"
#include "fall_line/sim/wanted.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

// THE WAYS a wave climbs, read downhill: from the head of every run that is no link lane, that
// run and every run it flows into to its foot (`run_chain`); on a map with no ski area the piste.
// Each with its nearest-point index built.
std::shared_ptr<const std::vector<ge::mountain::mapgen::Track>> army_ways(
    const ge::mountain::mapgen::Level& level);

// A POINT `s` m DOWN A WAY, and straight on past its foot along its last heading.
ge::mountain::mapgen::TrackPoint army_way_at(const ge::mountain::mapgen::Track& way, double s);

// THE LIFT a squad takes to come down at a man standing at `q`: the one whose top is nearest him
// and `army::kAboveM` over him, or with none so high the one whose top is highest; -1 with none.
// A choice over the lifts (ai/choose.hpp), recorded on `why` when it is not null.
int32_t lift_for(const GameState& state, Spot q, ge::ai::Why* why = nullptr);

// A SOLDIER ENTERS `next` from his mode (ai/choose.hpp): recorded as his decision (agent
// `army::kPursuerBase` + his id) while a trace is on, his time in the mode back to 0.
void enter_mode(GameState& state, Soldier& s, SoldierMode next, const char* because);

// A SOLDIER PUT ON THE MAP at (x, z), facing `heading`, in `mode`: his id the army's next. The
// caller keeps within `army::kMostSoldiers`.
Soldier& enlist_soldier(GameState& state, SoldierFrom from, int32_t squad, SoldierMode mode,
                        double x, double z, double heading);

// THE ARMY'S STEP (the header says what it does), and the snowmobiles' after the men
// (army_sled.hpp).
void step_army(GameState& state);

// FNV-1a 64 over the army, folded into `h`; nothing while it is inactive.
uint64_t army_hash(const ArmyState& a, uint64_t h);

// What an army event says (`GameEvent::gate`); its `value` is how many men.
ArmyChange army_change(const GameEvent& e);
const char* soldier_mode_name(SoldierMode m);
const char* army_change_name(ArmyChange c);

}  // namespace fall_line::sim
