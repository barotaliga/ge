// The `skier` data set: what the skier model's builder (build.py) is made from. The rig's joints
// (app/rig.hpp: every bind position, parent and limb radius) and the reference pair's numbers (the
// length, the waist, the sidecut, the mount, the rocker), so the bones, the skis' outline and the
// poles are the code's own and a change to any of them makes skier.glb stale.
//
// ge:summary The skier model's data: the rig's joints and the reference skis, as JSON
// ge:keywords skier asset data rig joints skis builder json ge_asset_data
// ge:skill skier assets
// ge:test tests/skier_rig_test.cpp
#include <string>

#include "assets/data.hpp"
#include "skier/app/rig.hpp"
#include "skier/skis.hpp"

namespace {

namespace sk = ge::skier;

std::string make() {
  ge::assets::JsonOut j;
  j.begin_object();
  j.field("frame", "figure: x right, y up, z forward, m; the skis' base at y = 0");
  j.key("joints").begin_array();
  for (const sk::RigJoint& r : sk::rig_joints()) {
    j.begin_object().field("name", r.name).field("parent", r.parent);
    j.key("at").begin_array().num(r.at.x).num(r.at.y).num(r.at.z).end_array();
    j.field("radius", r.radius).end_object();
  }
  j.end_array();
  const sk::SkiSpec& s = sk::kReferenceSkis;
  j.key("skis").begin_object();
  j.field("length", s.length).field("waist", s.waist).field("sidecut", s.sidecut);
  j.field("mount_back", sk::kMountBack).field("thickness", sk::kSkiThick);
  j.field("rocker_length", sk::kRockerLength).field("rocker_rise", sk::kRockerRise);
  j.end_object();
  j.key("poles").begin_object().field("length", sk::kPoleLength).end_object();
  j.end_object();
  return j.text();
}

const ge::assets::DataSpec kSpec{
    "skier", "the skier rig's joints and the reference skis (app/rig.hpp)", make};
GE_ASSET_DATA(kSpec);

}  // namespace
