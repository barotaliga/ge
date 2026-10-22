// THE SNOWMOBILE'S DRIVE: the engine, the CVT and the belt, from the thumb throttle to the force
// the belt offers the snow. What the snow TAKES of it is the grip's (`body.cpp` sums it probe by
// probe); this module owns the belt between the two and the engine that turns it.
//
// - THE ENGINE is a power curve over rpm (`power_share`), a two-stroke's: little down low and a
//   rush onto the pipe, to its peak at `kPeakRpm` and falling past it to the limiter at
//   `kMaxRpm`, which cuts the fuel. Shut, it freewheels.
// - THE CVT is modelled by what it does rather than by its sheaves: with the throttle open it
//   holds the engine at the rpm the thumb asks for (from the clutch's engagement up to the peak
//   at full throttle), shifting up as the belt gains speed until it runs out of ratio at
//   `kGearTop`; past that the engine is locked to the belt and climbs with it to the limiter.
//   So the drive's force is POWER over belt speed (flat power, falling force), floored at a
//   launch speed and capped at the peak torque through the lowest ratio.
// - THE BELT is a mass of its own: the engine pushes it, the snow pushes back through the grip,
//   its rails and idlers drag it, the brake clamps it. It spins faster than the sled goes in
//   powder or off the line (the slip that digs the trench and throws the roost), spins up free in
//   the air, and never runs backwards: the drive is one-way.
//
// ge:summary The snowmobile's engine, CVT and belt: power curve, drive force, belt and rpm steps
// ge:keywords snowmobile drive engine power-curve two-stroke cvt clutch belt tread rpm throttle
// ge:keywords brake limiter drive_force step_belt step_rpm
// ge:skill snowmobile
// ge:test tests/snowmobile_scenarios_test.cpp
#pragma once

namespace ge::snowmobile {

// The engine's power at `rpm`, as a share of its peak.
double power_share(double rpm);

// The engine speed the belt drives it at in the CVT's top ratio, rpm.
double rpm_at_top(double tread_speed);

// The most the drive can ever push the belt with, N: the peak torque through the lowest ratio.
double max_drive_force();

// The force the drive offers the belt, N, at `rpm`, `throttle` 0..1 and the belt at
// `tread_speed` m/s; negative with the throttle shut (the little a two-stroke brakes).
double drive_force(double rpm, double throttle, double tread_speed);

// Where the CVT holds the engine: the rpm the thumb asks for, or what the belt drives it at in
// top ratio if that is higher.
double rpm_goal(double throttle, double tread_speed);

// THE BELT'S STEP: its speed advanced by the drive, the snow's reaction (`ground` N, what the
// belt's grip put INTO the snow, positive pushing the sled forward), its losses and the brake
// 0..1, which never clamps it below `floor` m/s. Returns the new speed, m/s.
double step_belt(double tread_speed, double rpm, double throttle, double brake, double ground,
                 double dt, double floor);

// The engine's step toward where the CVT holds it, rpm.
double step_rpm(double rpm, double throttle, double tread_speed, double dt);

}  // namespace ge::snowmobile
