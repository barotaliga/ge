// THE PARAMOTOR'S ACRO: the wing pitched round the pilot on its risers and what goes wrong past it,
// the parts `step_para` (para.hpp) calls, stated beside each other so they read as one thing.
// Every number is `spec::acro`'s.
//
// - THE WING'S WAY is KEPT (`kept_way`): it noses into the air square to its lines as ever, but a
//   wing met by the air from behind does not turn round to it: it is FLYING BACKWARDS, which a soft
//   wing cannot do, and it collapses whole.
// - THE BAR FORWARD PITCHES IT (`riser_rig`): the fronts pulled down, the wing driven ahead of him
//   and down, a DIVE; the air through it grows, so a dive feeds itself.
// - THE BAR BACK at a dive's speed (`bite_rears`, `riser_lift`, `held_share`, `hold_pitch`): the
//   rears bend the pair's path up hard and hold the wing over him on the inside of the curve, so
//   held from a big enough dive the wing goes on round over him and down behind: a LOOP. At a
//   crawl the rears do no more than the trimmers.
// - THE SWING counted (`count_swing`): the lines' turn about the level pitch axis, unwrapped, a
//   whole turn round him a loop; carried round a spiral (turning about the plumb) it counts
//   nothing.
// - FALLING INTO THE WING (`fallen_in`): the lines slack with the wing level with him or below, or
//   the two fallen together, out of a swing he threw on the rears: he drops into his own canopy.
//   What rough air does to the wing is its folds'.
// - TANGLED (`step_tangled`): the cloth and the lines round him, the motor stopped, the bundle
//   dragging him against the air and turning him round as he falls.
//
// Pure over the state and the clock: nothing here draws a random number or allocates.
//
// ge:summary The paramotor's acro: the dive, the swing-up, the loop, the slack lines, the tangle
// ge:keywords paramotor para acro dive swing-up loop tumble risers rears fronts backflown slack
// ge:keywords tangle tangled kept_way riser_rig riser_lift bite_rears hold_pitch count_swing
// ge:keywords fallen_in read_load step_tangled
// ge:skill paramotor
// ge:test tests/paramotor_scenarios_test.cpp
#pragma once

#include "core/quat.hpp"
#include "paramotor/para.hpp"
#include "paramotor/spec.hpp"
#include "skier/skier.hpp"

namespace ge::paramotor {

// THE WING'S WAY into `out`: the air through it `a` (of which `down` runs along the lines `u`)
// square to the lines when it meets the wing from ahead, else the way it last had (`last`) turned
// onto the lines' plane. True when the air meets it from behind: flying backwards.
bool kept_way(core::Vec3 a, double down, core::Vec3 u, core::Vec3 last, core::Vec3& out);

// THE BAR FORWARD on the risers as trim, rad, added to the wing's once he flies: a DIVE.
double riser_rig(const ParaState& p, double bar);
// THE BAR BACK as lift, a share of the lift coefficient, as hard as the rears bite.
double riser_lift(const ParaState& p, double bar);
// THE REARS' BITE, kept in `p.rears`: as hard as the speed `v` m/s he carries has given them at
// most, held until he lets the bar go.
void bite_rears(ParaState& p, double bar, double v);
// How far under its folding angle his hands on the risers hold the leading edge, rad.
double reflex_of(const ParaState& p, double bar);
// How far past its stall the rears hold a wing they haul round, rad.
double held_of(const ParaState& p, double bar);
// HOW HARD THE REARS HOLD THE WING'S PITCH, 0..1, as his speed `v` m/s is past `swing::kFrom`:
// slowed at the top of a loop the hold lets go.
double held_share(const ParaState& p, double bar, double v);

// THE WING HELD IN ITS PITCH on the rears: a spring and a damper between the wing and a spot on
// the inside of the pilot's path (his air `c`, the lines `u`), `live` of their full strength. The
// force on the wing returned, its opposite taken off the pilot (at `at`, moving `vel`, of mass
// `m`) now.
core::Vec3 hold_pitch(ParaState& p, core::Vec3 at, core::Vec3& vel, double live, core::Vec3 c,
                      core::Vec3 u, double m, double dt);

// THE SWING: the lines `u` pitched off plumb about the level pitch axis, unwrapped into
// `p.swing`; true when that made a whole turn round him (`p.loops` counts them).
bool count_swing(ParaState& p, core::Vec3 u);

// WHETHER HE HAS FALLEN INTO THE WING this step: the lines' pull `tension`, N, on a pilot of mass
// `m` under `kSlack` of his weight with the wing level with him or below for `kSlackFor` s, or the
// two `apart` m within `kClose` of the lines, out of a swing he threw on the rears.
bool fallen_in(ParaState& p, double tension, double m, core::Vec3 u, double apart, double dt);

// How far past `kLoaded` g the lines are pulled: a wing held full by the pull on it.
inline double loaded(const ParaState& p) { return p.g - spec::acro::kLoaded; }

// The pull on the lines, N, read as his load, g, eased.
void read_load(ParaState& p, double tension, double m, double dt);

// TANGLED IN THE WING for one step, after the skier's own: the bundle's drag on him against the
// air, the bundle turning him round, the cloth streaming over him.
void step_tangled(ParaState& p, skier::SkierState& s, const ParaWorld& world, double m, double dt);

}  // namespace ge::paramotor
