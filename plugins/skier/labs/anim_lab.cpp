// LAB `anim`: the skier's figure (app/figure.hpp) ridden through a scenario (scenarios.hpp) and
// recorded every step as a pose track (engine/lab/anim.hpp), then judged the ways an animation is
// judged: filmstrips from the side, behind and above (the pieces' wireframe under the bones), the
// frames laid over one another with the hands' and the tips' paths, the joints' angles and every
// joint's jolt over time, and the five faults (pop, snap, angle snap, stretch, clip) as findings.
//
// A COPY of the track then has a pop, a stretch and a clip PLANTED at known frames, and the lab
// fails if any of the three is not found where it was put: the measures are held to finding what
// they exist to find. The real track's findings are reported, never failed on: they are what a
// change to the figure is read against (`--tag=before`, then `--compare=before`).
//
// ge:summary Lab: the skier figure recorded as a pose track, filmstrips, onion skins, faults
// ge:keywords lab anim animation figure skier filmstrip onion-skin wireframe joints bones jolt
// ge:keywords pop snap stretch clip planted pose-track
// ge:skill debug skier labs
// ge:test tests/lab_anim_test.cpp
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "lab/anim.hpp"
#include "lab/lab.hpp"
#include "skier/app/figure.hpp"
#include "skier/scenarios.hpp"
#include "skier/skis.hpp"
#include "skier/trees.hpp"

namespace {

namespace gl = ge::lab;
namespace sk = ge::skier;
using ge::core::Quat;
using ge::core::Vec3;

constexpr double kHz = 120;

size_t joint_index(const gl::Rig& rig, const char* name) {
  for (size_t j = 0; j < rig.joints.size(); j++) {
    if (rig.joints[j] == name) return j;
  }
  return 0;
}

gl::Rig figure_rig() {
  gl::Rig rig;
  for (const sk::FigureJoint& j : sk::figure_joints()) {
    rig.joints.emplace_back(j.name);
    rig.parent.push_back(j.parent);
    rig.side.push_back(j.side);
  }
  for (const char* end : {"hand_l", "hand_r", "pole_tip_l", "pole_tip_r", "ski_tip_l"}) {
    rig.ends.push_back(static_cast<int>(joint_index(rig, end)));
  }
  return rig;
}

// The scenario ridden at 120 Hz, the figure posed and recorded every step.
gl::PoseTrack record(const sk::Scenario& sc, double seconds) {
  const ge::mountain::mapgen::Level level = sk::scenario_level(sc);
  const sk::TreeIndex trees = sk::build_tree_index(level);
  const sk::World world{&level, &trees};
  const sk::SkiSpec spec = sk::kReferenceSkis;
  gl::PoseTrack track;
  track.rig = figure_rig();
  track.dt = 1 / kHz;
  track.has_floor = true;  // the skis' base: the snow, in the figure's frame
  sk::SkierState s = sk::stand_skier(world, sc.x, sc.z, sc.heading, sc.speed, sc.lift);
  sk::SkierEvents ev;
  std::array<sk::PiecePose, sk::kFigurePieces> pieces{};
  std::array<Vec3, sk::kFigureJoints> joints{};
  std::vector<gl::WireSeg> wire;
  const auto steps = static_cast<int64_t>(ge::core::num::round(seconds * kHz));
  for (int64_t i = 0; i < steps; i++) {
    sk::pose_figure(s, world, pieces);
    const sk::FigureFrame frame = sk::figure_frame(s, world);
    sk::figure_joint_positions(pieces, frame, joints);
    const Quat back{-frame.rot.x, -frame.rot.y, -frame.rot.z, frame.rot.w};
    wire.clear();
    for (const sk::PiecePose& p : pieces) {
      const Vec3 rel{p.centre.x - frame.at.x, p.centre.y - frame.at.y, p.centre.z - frame.at.z};
      gl::add_box_wire(wire, ge::core::unrotate(frame.rot, rel),
                       ge::core::multiply(back, p.rotation), p.size);
    }
    track.add_frame(joints, wire);
    sk::step_skier(s, sc.input(static_cast<double>(i) / kHz, s), world, spec, track.dt, ev);
  }
  return track;
}

// A smooth bump 0..1..0 over `n` frames from `f0`, 0 outside.
double bump(size_t f, size_t f0, size_t n) {
  if (f < f0 || f >= f0 + n) return 0;
  const double k =
      ge::core::libm::sin(ge::core::kPi * static_cast<double>(f - f0) / static_cast<double>(n));
  return k * k;
}

struct Planted {
  const char* kind;
  const char* joint;
  size_t first, last;
};

void shoot(gl::Lab& lab, const std::string& id, const std::string& title, const gl::Picture& pic,
           const std::string& caption, const std::string& look_for) {
  lab.shot({id,
            title,
            caption,
            look_for,
            {{"scenario", lab.arg("scenario")}, {"seconds", lab.arg("seconds")}}},
           pic);
}

void shoot_chart(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                 const std::string& caption, const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  shoot(lab, id, chart.title, gl::to_picture(chart.plot),
        caption + " The SVG beside it (" + id + ".svg) carries every label as text.", look_for);
}

void run(gl::Lab& lab) {
  const sk::Scenario* sc = sk::find_scenario(lab.arg("scenario"));
  if (!sc) {
    lab.fail("no ride scenario \"" + lab.arg("scenario") + "\"",
             "Pick one of plugins/skier/scenarios.cpp's ids (`make lab LAB=ride` lists them).");
    return;
  }
  const double seconds = lab.arg_num("seconds") > 0 ? lab.arg_num("seconds") : sc->seconds;
  gl::PoseTrack track = record(*sc, seconds);
  const size_t nf = track.frames();
  if (nf < 64) {
    lab.fail("the track has " + std::to_string(nf) + " frames, the views need 64",
             "Pass a longer --seconds (at least 0.6).");
    return;
  }
  const gl::Motion motion = gl::measure_motion(track);
  const std::vector<gl::AnimFinding> found = gl::anim_findings(track, motion);
  lab.metric("frames", static_cast<double>(nf), "count");
  std::map<std::string, int> by_kind{
      {"pop", 0}, {"snap", 0}, {"angle-snap", 0}, {"stretch", 0}, {"clip", 0}};
  for (const gl::AnimFinding& a : found) by_kind[a.kind]++;
  for (const auto& [kind, n] : by_kind) lab.metric("findings." + kind, n, "count");
  gl::LabTable table{
      "the figure's faults, by frame (the first 16)", {"kind", "joint", "frame", "what"}, {}};
  for (size_t i = 0; i < found.size() && i < 16; i++) {
    const gl::AnimFinding& a = found[i];
    table.rows.push_back({a.kind, track.rig.joints[a.joint], std::to_string(a.frame), a.note});
  }
  lab.table(table);
  lab.note(std::to_string(found.size()) + " faults on the figure over " + std::to_string(nf) +
           " frames" + (found.empty() ? "" : "; the first: " + found.front().note));

  const std::vector<size_t> strip = gl::spread_frames(track, 16);
  for (const gl::AnimView view : {gl::AnimView::side, gl::AnimView::behind, gl::AnimView::above}) {
    const std::string name = gl::view_name(view);
    gl::StripSpec spec;
    spec.view = view;
    spec.frames = strip;
    spec.rings = &found;
    shoot(lab, "strip-" + name, "FIGURE FROM " + name + ", 16 FRAMES", gl::filmstrip(track, spec),
          "The skier's figure at 16 frames spread over the run, seen from " + name +
              ", the pieces' boxes in grey under the bones (left blue, right orange, the spine "
              "ink), one scale for every panel; the green line is the snow.",
          "Limbs that keep their length, the poles above the snow, no joint ringed red.");
  }
  // The onion skins: the last two seconds (or the run), a frame every tenth of a second.
  std::vector<size_t> onion;
  const size_t from = nf > 240 ? nf - 240 : 0;
  for (size_t f = from; f < nf; f += 12) onion.push_back(f);
  gl::Picture onions = gl::create_picture(960, 334, gl::Rgba{250, 250, 252});
  int x = 0;
  for (const gl::AnimView view : {gl::AnimView::side, gl::AnimView::behind, gl::AnimView::above}) {
    gl::blit(onions, gl::onion_skin(track, view, onion), x, 0);
    x += 320;
  }
  shoot(lab, "onion", "ONION SKIN, LAST 2 S", onions,
        "The figure's frames every 0.1 s over the run's last two seconds laid over one another "
        "(fainter earlier), from the side, behind and above, with the hands', the pole tips' and "
        "the left ski tip's paths over every step.",
        "Smooth paths with no kinks or jumps; the poles' paths staying over the snow.");
  const gl::Rig& rig = track.rig;
  std::vector<size_t> angled;
  for (const char* j : {"knee_l", "knee_r", "hip_l", "elbow_l", "elbow_r"}) {
    angled.push_back(joint_index(rig, j));
  }
  const gl::Chart angles = gl::angle_chart(
      track, motion, angled,
      {"the figure's joint angles", sc->id, "t (s)", "angle (deg, 180 straight)", 960, 420});
  shoot_chart(lab, "angles", angles,
              "The knees', the left hip's and the elbows' angles over the run; a ring is a spike.",
              "Angles that move with the turns, no ring.");
  lab.note(gl::chart_text(angles, 4));
  const gl::Chart jolt = gl::jolt_heatmap(
      track, motion, {"every joint's jolt", sc->id, "t (s)", "log10(1 + jolt m/s^3)", 960, 0});
  shoot_chart(lab, "jolt", jolt,
              "Every joint's jolt (the change of its acceleration) over the run, one row a joint, "
              "darker harder, on a log scale.",
              "An even shade; a dark cell is a moment a joint jerks.");
  const std::string tsv = gl::write_series(gl::motion_series(track, motion));
  lab.attach("motion.tsv", std::vector<uint8_t>(tsv.begin(), tsv.end()));

  // The planted copy: a pop on the right hand, the left thigh stretched, the left pole tip
  // pushed through the snow.
  gl::PoseTrack bad = track;
  const size_t pop = nf / 4, stretch = nf / 2, clip = 3 * nf / 4;
  const size_t hand = joint_index(rig, "hand_r"), hip = joint_index(rig, "hip_l"),
               knee = joint_index(rig, "knee_l"), tip = joint_index(rig, "pole_tip_l");
  bad.at(pop, hand).x += 0.06;
  for (size_t f = stretch; f < stretch + 30 && f < nf; f++) {
    const Vec3 h = bad.at(f, hip);
    Vec3& k = bad.at(f, knee);
    const double grow = 1 + 0.12 * bump(f, stretch, 30);
    k = Vec3{h.x + (k.x - h.x) * grow, h.y + (k.y - h.y) * grow, h.z + (k.z - h.z) * grow};
  }
  for (size_t f = clip; f < clip + 40 && f < nf; f++) bad.at(f, tip).y -= 0.15 * bump(f, clip, 40);
  const std::vector<gl::AnimFinding> bad_found = gl::anim_findings(bad, gl::measure_motion(bad));
  const Planted planted[] = {{"pop", "hand_r", pop, pop + 1},
                             {"stretch", "knee_l", stretch, stretch + 30},
                             {"clip", "pole_tip_l", clip, clip + 40}};
  std::vector<gl::AnimFinding> hits;
  for (const Planted& p : planted) {
    const size_t j = joint_index(rig, p.joint);
    const gl::AnimFinding* hit = nullptr;
    for (const gl::AnimFinding& a : bad_found) {
      if (a.kind == p.kind && a.joint == j && a.frame >= p.first && a.frame <= p.last) hit = &a;
    }
    lab.metric(std::string("planted.") + p.kind, hit ? 1 : 0, "count");
    if (hit) {
      hits.push_back(*hit);
      lab.note("planted " + std::string(p.kind) + " found: " + hit->note);
    } else {
      lab.fail("the " + std::string(p.kind) + " planted on " + p.joint + " at frames " +
                   std::to_string(p.first) + ".." + std::to_string(p.last) + " was not found",
               "Read engine/lab/anim.cpp's anim_findings and its AnimLimits, then rerun `make "
               "lab LAB=anim` (skill: debug).");
    }
  }
  gl::StripSpec spec;
  spec.view = gl::AnimView::behind;
  spec.frames = {pop - 1, pop, pop + 1};
  for (size_t i = 1; i < hits.size(); i++) spec.frames.push_back(hits[i].frame);
  spec.rings = &hits;
  spec.panel = 180;
  shoot(lab, "planted", "PLANTED FAULTS FROM BEHIND", gl::filmstrip(bad, spec),
        "A copy of the track with three faults planted: the right hand jumps 6 cm for one frame "
        "(panels 1-3), the left thigh grows 12% (panel 4) and the left pole tip sinks 15 cm into "
        "the snow (panel 5), each later panel at the frame its fault was found worst.",
        "A red ring on the right hand in panel 2, on the left knee in panel 4 and on the left "
        "pole tip in panel 5.");
}

const gl::LabSpec kSpec{
    "anim",
    gl::LabKind::scenario,
    "plugin:skier",
    "engine/lab/anim.hpp, plugins/skier/app/figure.hpp",
    "debug",
    "Does the skier's figure move without pops, snaps, stretched bones or poles through the "
    "snow, and do the measures find each fault planted in a copy?",
    {
        {"scenario", "turns", "the ride scenario the figure is recorded through (scenarios.cpp)"},
        {"seconds", "0", "how much of it, s (0: the scenario's own length)"},
    },
    "--seconds=4",
    run,
};

GE_LAB(kSpec);

}  // namespace
