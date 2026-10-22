// THE ANIMATION VIEWS: an animated figure recorded frame by frame as a POSE TRACK (its joints, and
// optionally the wireframe of its pieces, in the figure's own frame), MEASURED for the faults an
// animation shows when it is wrong, and DRAWN the ways those faults are seen. Any figure records
// into it: a skier, a rider, an animal, a machine with moving parts.
//
// THE TRACK. A `Rig` names the joints, each joint's parent (a bone runs parent to joint; -1 is a
// root with no bone), its side (left, centre, right: the colour it is drawn in) and the END joints
// whose paths an onion skin traces (the hands, the tips). A frame is every joint's position, in
// metres, in the figure's frame: x its right, y up, z forward (engine/core/quat.hpp), with the
// ground or the snow at y = `floor` when the figure stands on something. `add_box_wire` adds a
// piece's twelve edges to a frame's wireframe.
//
// THE MEASURES (`measure_motion`), every joint every frame: the speed, the acceleration and the
// jolt (m/s, m/s^2, m/s^3, finite differences over `dt`), each bone's STRETCH against its median
// length, and the ANGLE at every joint that has a parent and a child (180 is straight) and its
// rate (deg/s). `anim_findings` reads them for the five faults:
//   pop        a joint jumps in one frame (a speed spike past `pop`, m/s over its neighbours),
//              found at the frame it jumped to
//   snap       a joint's acceleration spikes past `snap` (m/s^2) with no pop beside it
//   angle-snap a joint's angle turns faster than its neighbours by `angle_snap` (deg/s)
//   stretch    a bone is longer or shorter than its median by more than `stretch` (a share)
//   clip       a joint is under the floor by more than `clip` (m)
// A spike is stats.hpp's (`spikes`: the robust z over a rolling median) AND past the absolute
// floor, so a smooth 120 Hz motion flags nothing. Neighbouring frames of one fault on one joint
// are one finding, at its worst frame.
//
// THE VIEWS. `filmstrip`: chosen frames side by side from the side, behind or above, the
// wireframe grey under the bones (left blue, right orange, centre ink), one scale for every panel
// so a size change is a real one, a finding's joint ringed red in its frame. `onion_skin`: the
// frames laid over one another, the earlier fainter, and every end joint's path over the whole
// span. `angle_chart` and `jolt_heatmap` hand the measures to chart.hpp (rings, findings, PNG and
// SVG); `motion_series` writes them as a series file `ge graph` reads (series.hpp).
//
// No transcendental beyond core/libm, integer pixels: a lab's animation pictures are digested.
//
// ge:summary Animation debug views: the pose track, its faults measured, filmstrips and onion skins
// ge:keywords anim animation pose track rig joint bone wireframe filmstrip onion-skin jolt jerk
// ge:keywords pop snap angle-snap stretch clip joint-angle side behind above motion tip-path
// ge:skill debug labs
// ge:test tests/lab_anim_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/quat.hpp"
#include "lab/chart.hpp"
#include "lab/raster.hpp"
#include "lab/series.hpp"

namespace ge::lab {

struct Rig {
  std::vector<std::string> joints;  // names, kebab or snake case ("knee_l")
  std::vector<int> parent;          // per joint: the joint its bone starts at, -1 for none
  std::vector<int> side;            // per joint: -1 left, 0 centre, 1 right
  std::vector<int> ends;            // the joints whose paths an onion skin traces
};

struct WireSeg {
  core::Vec3 a, b;
};

struct PoseTrack {
  Rig rig;
  double dt = 1.0 / 120;  // s between frames
  bool has_floor = false;
  double floor = 0;                // the ground's height in the figure's frame, m
  std::vector<core::Vec3> joints;  // frames x joints
  std::vector<WireSeg> wire;       // frames x wire_per_frame
  size_t wire_per_frame = 0;

  size_t frames() const { return rig.joints.empty() ? 0 : joints.size() / rig.joints.size(); }
  const core::Vec3& at(size_t frame, size_t joint) const {
    return joints[frame * rig.joints.size() + joint];
  }
  core::Vec3& at(size_t frame, size_t joint) { return joints[frame * rig.joints.size() + joint]; }
  // One frame: every joint, and the same number of wire segments every frame (or none).
  void add_frame(std::span<const core::Vec3> pose, std::span<const WireSeg> segs = {});
};

// A box's twelve edges: `size` its extent along its own x, y and z, turned by `rot`.
void add_box_wire(std::vector<WireSeg>& out, core::Vec3 centre, core::Quat rot,
                  const double size[3]);

struct Motion {                            // each frames x joints, row-major like the track
  std::vector<double> speed, accel, jolt;  // m/s, m/s^2, m/s^3
  std::vector<double> stretch;             // the bone's length over its median, less 1; 0 rootless
  std::vector<double> angle;  // deg at the joint between its parent and its first child; NaN none
  std::vector<double> angle_rate;  // deg/s
};

Motion measure_motion(const PoseTrack& track);

struct AnimLimits {
  double pop = 1.0;         // m/s over the joint's neighbours
  double snap = 100;        // m/s^2 over the joint's neighbours
  double angle_snap = 360;  // deg/s over the joint's neighbours
  double stretch = 0.05;    // a share of the bone's median length
  double clip = 0.02;       // m under the floor
  double z = kOutlierZ;
};

struct AnimFinding {
  std::string kind;  // pop, snap, angle-snap, stretch, clip
  size_t joint = 0;
  size_t frame = 0;            // the worst frame
  size_t first = 0, last = 0;  // the frames it spans
  double value = 0;            // the measure there (m/s, m/s^2, deg/s, share, m under)
  double usual = 0;            // what the neighbours or the median say
  std::string note;            // a sentence: what, where, against what
};

// The faults, by frame then joint.
std::vector<AnimFinding> anim_findings(const PoseTrack& track, const Motion& motion,
                                       const AnimLimits& limits = {});

enum class AnimView : uint8_t { side, behind, above };
const char* view_name(AnimView view);

// `count` frames spread evenly over the track, the first and the last included.
std::vector<size_t> spread_frames(const PoseTrack& track, size_t count);

struct StripSpec {
  AnimView view = AnimView::side;
  std::vector<size_t> frames;
  bool wire = true;
  bool bones = true;
  const std::vector<AnimFinding>* rings = nullptr;  // ringed in the panel of their frame
  int panel = 150;                                  // each panel's width and height, px
  int columns = 8;
};
Picture filmstrip(const PoseTrack& track, const StripSpec& spec);

// `frames` laid over one another from `view`, and every end joint's path from the first to the
// last of them.
Picture onion_skin(const PoseTrack& track, AnimView view, const std::vector<size_t>& frames,
                   int size = 320);

// The angle at `joints` over time, with the findings' rings (chart.hpp).
Chart angle_chart(const PoseTrack& track, const Motion& motion, const std::vector<size_t>& joints,
                  const ChartFrame& frame);
// Every joint's jolt over time as a heatmap, one row a joint.
Chart jolt_heatmap(const PoseTrack& track, const Motion& motion, const ChartFrame& frame);

// The measures as a series (t_s, then <joint>.accel, <joint>.jolt and <joint>.angle).
Series motion_series(const PoseTrack& track, const Motion& motion);

}  // namespace ge::lab
