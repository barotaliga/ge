// THE BODY: what a skier's body takes when he meets a trunk or comes down from a flight, part by
// part: the g of every blow, the injuries it does, the whole body's severity, and whether it
// kills him. Every number is `anatomy.hpp`'s.
//
// A BLOW IS A STOP. A part met at `v` m/s is brought to rest over a distance: its own give and
// that of what it met (a trunk's, and the helmet's liner on the head). Its mean deceleration is
// v^2 / 2s and its peak half a sine's: that peak, in g, is the blow (`blow_of`).
//
// WHERE THE DOSES COME FROM, each off something the skier's step has already measured
// (`SkierEvents`), nothing re-measured:
// - A TRUNK met at its closing speed: square in front of the skis the tips meet it, the shins
//   are levered over the boots and the knees twisted, and when it wipes him out his front goes
//   on into it; beside him it takes the shoulder, the arm, the ribs, the flank, the hip and a
//   glancing share of the head on that side.
// - A LANDING of load `g`: up the legs' bones (the heel, the pilon, the plateau, the femur, the
//   hip socket) into the spine, onto the knees as a drawer as far as he came down crooked, the
//   neck whipped, and the spine handed more when the legs folded under it: that share owed until
//   his trunk meets the snow (`Owed`).
// - A LANDING ACROSS his way: the ski on the side he slides toward catches, and the knee over it
//   is twisted at the slide's speed.
// - THROWN (`Ragdoll`): every point of his body arriving into the snow (`Ragdoll::impact`, the
//   snow's give by the packed share there) or onto a trunk (`Ragdoll::struck`): the head (the
//   neck whipped, and loaded down the spine in a dive), the shoulders and the hips (the trunk on
//   the side he lies on; sat upright on the hips, the spine loaded), the knees and the feet (a
//   sole met along a straight leg loads the leg as a landing does), the elbows and the hands (the
//   arm, and the shoulder up it).
//
// A DOSE IS A CHANCE, NOT A LINE (`risk_of`): every injury on a part's ladder has the dose at
// which it is an even chance, and the chance at any other comes off the log-logistic of the
// injury-risk curves, drawn not from the run's Rng but off a hash of the step, the part and the
// seed, so a run replays injury for injury and stepping the body moves no other digest. The
// ladder is read worst first, a part takes at most one new injury a step and never a lesser one
// than it carries, and a part already hurt is hurt more easily.
//
// THE WHOLE BODY is summed as a trauma ward sums it, the INJURY SEVERITY SCORE (`severity_of`):
// the squares of the worst AIS in each of the three worst-hurt regions, 0 to 75. MORTAL: an
// injury of AIS 5, a score of 50, or the run's frailty drawn against the score; mortally hurt on
// his skis he goes down, and he dies once he has lain still or a few seconds on.
//
// A game decides what the body means: a readout mended at a reset (`mend_body`), or carried.
// Other doses (a round's energy, `Mech::pierce`) go through `Doses` and `judge_body`.
//
// ge:summary The skier's body: blows by part, the injury ladders, the severity score, death
// ge:keywords body injury injuries blow blow_of risk_of energy_over severity iss ais dose doses
// ge:keywords judge_body step_body mend_body body_hash mortal death dies_of g-meter impact
// ge:keywords trunk landing drawer twist bend thrown ragdoll point_blow snow_give owed dive seat
// ge:skill skier
// ge:test tests/skier_body_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "skier/anatomy.hpp"
#include "skier/skier.hpp"

namespace ge::skier {

// ONE INJURY TAKEN: the part, its kind (an index into `injuries_catalog()`), its AIS, the side
// of a paired organ it hurt (-1 left, 1 right, 0 none), when, s, and the energy of its dose
// over its even chance's (a fracture struck again keeps the harder).
struct Injury {
  Part part = Part::head;
  uint8_t kind = 0;
  uint8_t ais = 0;
  int8_t side = 0;
  double t = 0;
  double energy = 0;
  template <class Io>
  friend void describe(Io& io, Injury& v) {
    io(v.part);
    io(v.kind);
    io(v.ais);
    io(v.side);
    io(v.t);
    io(v.energy);
  }
};
inline constexpr int32_t kMostInjuries = 64;

enum class BlowFrom : uint8_t { none, landing, snow, tree };
const char* blow_from_name(BlowFrom f);

// ONE BLOW ON THE G METER: its peak, the part that took it, what it came from, how long ago,
// s, its number (`BodyState::blows`), and whether he went down on it.
struct Impact {
  double g = 0;
  Part part = Part::pelvis;
  BlowFrom from = BlowFrom::none;
  double age = 0;
  int32_t id = 0;  // 0: none yet
  bool fall = false;
  template <class Io>
  friend void describe(Io& io, Impact& v) {
    io(v.g);
    io(v.part);
    io(v.from);
    io(v.age);
    io(v.id);
    io(v.fall);
  }
};

// A LANDING'S LOAD OWED TO THE TRUNK: a landing the legs folded under hands the spine, the organs
// and the neck their share the step his trunk meets the snow (`injury::kOwedMost` s on at the
// latest), not the step the skis did.
struct Owed {
  bool owed = false;
  double t = 0;                             // s, when the skis landed
  double back = 0, chest = 0, abdomen = 0;  // g, a load each
  double neck = 0;                          // g, a blow
  template <class Io>
  friend void describe(Io& io, Owed& v) {
    io(v.owed);
    io(v.t);
    io(v.back);
    io(v.chest);
    io(v.abdomen);
    io(v.neck);
  }
};

struct BodyState {
  std::array<uint8_t, kParts> worst{};  // each part's worst AIS
  std::array<Injury, kMostInjuries> injuries{};
  int32_t count = 0;  // injuries taken; past `kMostInjuries` the rest are counted in `worst` only
  Impact impact;
  double peak = 0;       // the run's hardest blow, g
  double fall_peak = 0;  // ...of those he went down on
  int32_t blows = 0;
  double mortal = -1;  // s he was mortally hurt, -1 not
  double dead = -1;    // s he died, -1 alive
  Owed owed;
  template <class Io>
  friend void describe(Io& io, BodyState& v) {
    io(v.worst);
    io(v.injuries);
    io(v.count);
    io(v.impact);
    io(v.peak);
    io(v.fall_peak);
    io(v.blows);
    io(v.mortal);
    io(v.dead);
    io(v.owed);
  }
};

// What one step of the body did: the injuries taken are `injuries[first .. first + taken)`.
struct BodyEvents {
  int32_t first = 0;
  int32_t taken = 0;
  bool mortal = false;
  bool died = false;
};

// The run's clock and seed, which the risk draws hash.
struct BodyClock {
  double t = 0;   // s
  double dt = 0;  // s, the step
  int64_t tick = 0;
  uint32_t seed = 0;
};

// THIS STEP'S DOSES, per part and mechanism, the side each part's hardest blow came on, the
// blow billed to the g meter and how many injuries the step may take.
struct Doses {
  std::array<double, kParts * kMechs> dose{};
  std::array<Facing, kParts> faced{};
  double bill_g = 0;
  Part bill_part = Part::pelvis;
  BlowFrom bill_from = BlowFrom::none;
  int32_t cap = injury::kPerBlow;

  Doses() { faced.fill(Facing::none); }
  double at(Part p, Mech m) const {
    return dose[static_cast<size_t>(p) * kMechs + static_cast<size_t>(m)];
  }
  // A blow of `g` on `part` (blunt), from `face` for the injuries that care.
  void strike(Part part, double g, Facing face = Facing::none);
  // A dose `d` of `mech` (not a blow) on `part`.
  void charge(Part part, Mech mech, double d);
  // Offer the g meter a blow: the hardest this step is the one billed.
  void offer(double g, Part part, BlowFrom from);
};

// THE BLOW, g: the peak deceleration of a part met at `v` m/s and stopped over `give` m.
double blow_of(double v, double give);
// The chance of an injury of even-chance dose `at` at a dose `d`.
double risk_of(double d, double at);
// The energy of a dose `d` over that of the even-chance dose `at`: a blow's or a load's g goes
// as its energy, a twist's or a bend's (a speed) as its square.
double energy_over(Mech mech, double d, double at);
// THE INJURY SEVERITY SCORE, 0..75.
int32_t severity_of(const BodyState& b);
// Whether a body at severity `iss` dies of it on the run of `seed`: the trauma ward's chance at
// the score against the run's frailty, drawn once off a hash of the seed.
bool dies_of(uint32_t seed, int32_t iss);

// HEALED: every injury and the death gone, as a reset stands him back up. The run's hardest
// blows and the meter's count stay.
void mend_body(BodyState& b);

// READ `d` against every part's ladder: the injuries taken onto the body (at most `d.cap`), the
// hardest blow onto the meter (`down`: he is down after it), then whether it is mortal.
void judge_body(BodyState& b, const Doses& d, bool down, const BodyClock& clock, BodyEvents& out);

// ONE STEP OF THE BODY, after the skier's own (`step_skier`) wrote `ev`: the step's blows
// judged, and death's course run. Mortally hurt on his skis, `s` is put down. Allocates nothing.
void step_body(BodyState& b, SkierState& s, const SkierEvents& ev, const World& world,
               const SkiSpec& spec, const BodyClock& clock, BodyEvents& out);

// FNV-1a 64 over the body, folded into `h`.
uint64_t body_hash(const BodyState& b, uint64_t h);

}  // namespace ge::skier
