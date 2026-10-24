// THE BODY'S CATALOG: the twenty parts a skier's body is kept as, how much each gives when it
// meets the snow or a trunk, what the snow gives, and the ladder of injuries each part can take
// with the dose that does it. `body.hpp` reads it; every number of the body is here.
//
// EVERY NUMBER IS A MEASUREMENT OR SITS BETWEEN TWO, restated from the injury research:
// - HEAD DROPS ON SNOW: a 6 kg headform at 6.1 m/s peaks at about 51 g on soft snow, 106 g on
//   hard and 170 g on very hard. `kGive` is fitted to it: a stop of v^2 / 2s peaked as half a
//   sine (pi/2 the mean), over the head's own 6 mm and the snow's 5.0, 2.2 and 1.2 cm.
// - A CONCUSSION from 70 to 100 g at the head; a skull fracture from about 140 g bare; a
//   helmet's test passed at 250 g from 5.4 m/s.
// - THE CHEST: 60 g over 3 ms its tolerance (Mertz and Gadd), soft tissue hurt from about 46.
// - THE SPINE (Eiband 1959, the Dynamic Response Index): some 18 g headward borne without
//   injury, a compression fracture from 20 to 25 g, read off the landing's load.
// - THE LEGS UNDER AN AXIAL LOAD (cadaver drop and pendulum tests): the chain breaks from the
//   bottom: the heel at 6 to 8 kN, the tibia's end at the ankle (the pilon) a little past it,
//   its top in the knee (the plateau) at 8 to 10 kN, the femur's shaft at 10 to 15 kN and its
//   head into the hip socket between. One leg carries half the stop of a skier of 80 kg: some
//   0.39 kN a g.
// - THE KNEE: the anterior cruciate parts at 2.2 kN (Woo 1991); the twist of a caught edge, the
//   phantom foot and a landing in the back seat tear it; the medial ligament is the twist's.
// - THE BOOT-TOP FRACTURE: the shin levered over the boot's rim in a fall forward at speed.
// - THE HAND: the thumb's ligament on the pole in a fall on it, the wrist at 1.6 to 3.4 kN.
// - THE ORGANS: in ski falls the spleen is torn first and most (a fall on the left flank), the
//   kidney (a blow from behind) and the liver (the right) after it; in falls from a height the
//   lungs bruise with the ribs over them and the liver and spleen tear from the deceleration.
// - HOW A BREAK BREAKS (long bones struck by a pendulum, Cohen et al. 2016): simple at its
//   dose, comminuted at 2.3 times its energy; a hairline crack of the same bone at four fifths
//   of the break's dose and a rank under it.
// The SEVERITY of each injury is its Abbreviated Injury Scale rank (AIS): 1 minor, 2 moderate,
// 3 serious, 4 severe, 5 critical. Six, unsurvivable, is on no ladder.
//
// ge:summary The body's catalog: the parts, the bones, the organs, the injuries and their doses
// ge:keywords body anatomy injury injuries ais part parts bone bones organ organs fracture
// ge:keywords mechanism blunt load drawer twist bend pierce facing give ladder injury_def
// ge:skill skier
// ge:test tests/skier_body_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ge::skier {

// THE PARTS, in the order every per-part array keeps them; a paired part left then right.
enum class Part : uint8_t {
  head,
  neck,
  chest,
  back,
  abdomen,
  pelvis,
  shoulder_l,
  shoulder_r,
  arm_l,
  arm_r,
  hand_l,
  hand_r,
  thigh_l,
  thigh_r,
  knee_l,
  knee_r,
  shin_l,
  shin_r,
  foot_l,
  foot_r,
};
inline constexpr int32_t kParts = 20;

// A part as its ladder and its give name it: a paired part's either side.
enum class Base : uint8_t {
  head,
  neck,
  chest,
  back,
  abdomen,
  pelvis,
  shoulder,
  arm,
  hand,
  thigh,
  knee,
  shin,
  foot,
};
inline constexpr int32_t kBases = 13;

Base base_of(Part p);
// -1 his left, 1 his right, 0 a midline part.
int32_t side_of(Part p);
// The part of `base` on `side` (-1 left, 1 right); a midline base is itself.
Part sided(Base b, int32_t side);
const char* part_name(Part p);

// HOW A DOSE MEETS A PART: `blunt` a blow on it (g at the part), `load` a landing's load up the
// legs and the spine (g), `drawer` the same landing taken crooked onto the knees (g), `twist` a
// caught edge (m/s of slide when the edge bit), `bend` the shin levered over the boot (m/s),
// `pierce` a round's or a fragment's energy at the part (J; the catalog of wounds is the
// gunfire's).
enum class Mech : uint8_t { blunt, load, drawer, twist, bend, pierce };
inline constexpr int32_t kMechs = 6;

// WHICH SIDE OF THE BODY a blow came on, for the injuries that care.
enum class Facing : int8_t { none = -1, front, back, left, right };

// The sides a bone breaks from (`InjuryDef::on`), a mask: the facings, and `outside` the part's
// own outer side (his left of a left part).
inline constexpr uint8_t kOnFront = 1, kOnBack = 2, kOnLeft = 4, kOnRight = 8, kOnOutside = 16;

enum class Bone : uint8_t {
  skull,
  mandible,
  cervical,
  clavicle,
  scapula,
  sternum,
  ribs,
  thoracic,
  lumbar,
  pelvis,
  humerus,
  radius,
  ulna,
  hand,
  femur,
  patella,
  tibia,
  fibula,
  foot,
};
enum class Organ : uint8_t { brain, heart, lung, liver, spleen, stomach, bowel, kidney, bladder };
// The organs there are two of, one a side: the side an injury hurts is chosen by the blow.
inline constexpr uint16_t kPairedOrgans = (1u << 2) | (1u << 7);  // the lungs, the kidneys

enum class Fracture : uint8_t { none, hairline, broken };

// ONE INJURY a part can take: its name, the part (by its base), its severity, the mechanism and
// the dose with an even chance of it (`at`); for an organ the side the blow must come from
// (`face`); for a bone broken only from some sides, those (`on`); the bones it cracks or breaks
// and how (`bones`, a mask by `Bone`), the organs it hurts (a mask by `Organ`), and whether a
// fracture is more than the bone and is said (`said`: the spinal cord).
struct InjuryDef {
  const char* name;
  Base base;
  uint8_t ais;
  Mech mech;
  double at;
  Facing face;
  uint8_t on;
  Fracture fracture;
  uint32_t bones;
  uint16_t organs;
  bool said;
};

// Every injury, in a fixed order: an injury's kind is its index here.
std::span<const InjuryDef> injuries_catalog();
inline const InjuryDef& injury_def(uint8_t kind) { return injuries_catalog()[kind]; }
// The kind named `name`, or -1.
int32_t find_injury(std::string_view name);

// THE BODY'S NUMBERS (`body.hpp`).
namespace injury {

// THE STOP each part is brought to against a hard surface, m, by `Base`: the flesh, the bone's
// flex and the clothes; a limb's is its joints folding too, so several times the trunk's.
inline constexpr double kGive[kBases] = {0.006, 0.006, 0.04, 0.03, 0.06, 0.04, 0.03,
                                         0.05,  0.06,  0.05, 0.05, 0.04, 0.08};
// A trunk's give, m: its bark, its branches and its sway.
inline constexpr double kTrunk = 0.01;
// The helmet's liner crushing, m, against a trunk (against snow the snow gives first).
inline constexpr double kHelmet = 0.012;
// A blow's peak over its mean: a half sine's.
inline constexpr double kPeak = 1.5707963267948966;
// THE RISK CURVE: the chance at a dose d of an injury of even-chance dose `at` is
// 1 / (1 + (at / d)^kSteep), the log-logistic of the injury-risk curves; under kFloor of `at`,
// none, so a square landing called clean hurts nothing.
inline constexpr double kSteep = 8;
inline constexpr double kFloor = 0.5;
// A part already hurt is hurt again more easily: each AIS rank it carries lowers its every
// even-chance dose by this share.
inline constexpr double kWeaken = 0.06;
// What reaches a part beside the one struck: the head's blow whips the neck.
inline constexpr double kNeckShare = 0.25;
// THE SNOW'S GIVE where a part meets it, m: the groomer's and loose snow's, blended by the packed
// share there (a body dropped into powder is stopped over five centimetres, on the groomer over
// two).
inline constexpr double kSnowPacked = 0.022;
inline constexpr double kSnowLoose = 0.05;
// A thrown body's point arriving into the snow slower than this is lying on it, not meeting it,
// m/s; and the same for its closing speed on a trunk.
inline constexpr double kTouch = 0.8;
// WHAT REACHES A PART BESIDE THE POINT OF THE THROWN BODY THAT MET THE SNOW OR A TRUNK: a hand's
// blow runs up the arm to the shoulder (the fall on an outstretched hand), an elbow's into the
// point of the shoulder, a knee's into the thigh and the shin, a foot's into the shin, and a
// shoulder's or a hip's into the trunk on the side it faces down (`kFlank` met flat on a side:
// the lower ribs and the spleen or the liver under them).
namespace share {
inline constexpr double kHandArm = 0.5, kHandShoulder = 0.5, kElbowShoulder = 0.4, kKneeThigh = 0.5,
                        kKneeShin = 0.4, kFootShin = 0.5, kTrunk = 0.6, kFlank = 0.9;
// A head met first down the spine (a dive) loads the neck along it on top of the whip: the
// diving injury's cervical fracture.
inline constexpr double kNeckAxial = 0.35;
// A sole met first along a straight leg loads the leg along it, in a landing's g: feet first into
// a trunk at 40 km/h, the femur's even chance.
inline constexpr double kLegAxial = 0.7;
}  // namespace share
// A SEAT met with the trunk upright over it: this share of the hips' blow runs up the spine as a
// load, g (the seated fall's compression fracture); nothing from a fall on the side or the back.
inline constexpr double kSeat = 0.18;
// A landing the legs folded under owes the spine, the organs and the neck its load until his
// trunk meets the snow, or this many s on at the latest.
inline constexpr double kOwedMost = 0.2;
// A TRUNK MET ON THE SKIS: square in front of them, within `kLane` m of his line past its
// radius, the tips meet it and the skis stop over `kStop` m (the bindings' release and the
// legs' fold): the shins levered, the knees twisted (`kNoseTwist` of the speed). Beside him it
// takes his side, each part this share of the blow.
inline constexpr double kStop = 0.6;
inline constexpr double kLane = 0.25;
inline constexpr double kNoseTwist = 0.45;
namespace side {
inline constexpr double kShoulder = 1, kArm = 0.7, kChest = 0.6, kAbdomen = 0.5, kPelvis = 0.4,
                        kThigh = 0.4, kHead = 0.25;
}  // namespace side
// THE BODY CARRIED ON INTO A TRUNK the skis stopped at, at the closing speed: a trunk is
// narrower than he is, so it meets the shoulder on its side of his line in full, the chest
// behind it at the trunk's share (the ribs and what is under them), the head in its helmet at a
// glancing share, and the hip under it after him, the belly behind it.
namespace front {
inline constexpr double kShoulder = 1, kChest = 0.6, kHead = 0.4, kPelvis = 0.5, kAbdomen = 0.3;
}  // namespace front
// THE LANDING: the knees' drawer load is `kSquare` of the landing's g landed square, more by
// `kCrooked` at `kRolled` rad off his way; the neck takes `kNeck` of the load less one as a
// blow; a landing the legs folded under hands the spine `kFolded` times its load.
inline constexpr double kSquare = 0.6;
inline constexpr double kCrooked = 0.6;
inline constexpr double kRolled = 0.6;
inline constexpr double kNeck = 1.6;
inline constexpr double kFolded = 1.3;
// A landing is billed on the g meter from this load, and only after a flight this long, s.
inline constexpr double kLandingShown = 2;
inline constexpr double kLandingAir = 0.4;
// Any other blow is billed from this many g, held `kHold` s: a harder one in that time takes
// its place.
inline constexpr double kShown = 3;
inline constexpr double kHold = 1.6;
// The most new injuries one step's blows do: the worst of what was drawn.
inline constexpr int32_t kPerBlow = 5;
// HOW A BREAK BREAKS by its energy over its even chance's: simple under `kWedge`, a wedge
// under `kShatter`, past it shattered.
inline constexpr double kWedge = 1.6;
inline constexpr double kShatter = 2.3;

// MORTAL: an injury of this AIS, a severity score of this, or by the whole body as often as a
// trauma ward's patients die of the score: a logistic with its even chance at `kHalf` and its
// odds e-fold every `kSpread` points, none under `kLeast` (one in ten at 33, nine in ten at 59).
inline constexpr int32_t kMortalAis = 5;
inline constexpr int32_t kMortalIss = 50;
inline constexpr double kHalf = 46;
inline constexpr double kSpread = 6;
inline constexpr double kLeast = 16;
// Mortally hurt, he dies once he has lain still `kStill` s, or `kLast` s on at the latest.
inline constexpr double kStill = 1.2;
inline constexpr double kLast = 7;

}  // namespace injury

}  // namespace ge::skier
