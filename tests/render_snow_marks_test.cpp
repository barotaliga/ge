// engine/render/snow_marks.hpp: the snow's fine marks.
//
//   * FROM THE RECORDS: a layer's imprint records become stamps of their outline (a capsule, a
//     disc, a box along its heading, an ellipse), their sink as the depth, a berm only where snow
//     was thrown, a round wall for a slide and a square one for a step; a record with no sink
//     leaves none.
//   * ON A DEVICE (skipped without one): a box lands square, its floor pressed to its corners and
//     nothing past its end; an ellipse lands round, pressed at its centre, not at the box's
//     corner; a ski pressed into a real layer leaves its groove in the fine map through
//     `marks_from_records`, deepest on its axis, with the berm beside it.
//
// ge:summary Tests for the snow's fine marks: stamps from records, a box and an ellipse drawn
// ge:keywords test render snow snow-marks fine-marks imprint-records box ellipse capsule readback
// ge:skill render snow testing
#include <cmath>
#include <numbers>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "render/frame.hpp"
#include "render/snow_marks.hpp"
#include "snow/layer.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"

namespace {

namespace r = ge::render;
namespace sn = ge::snow;

sn::ImprintRecord record(sn::Shape shape, sn::Motion motion, float sink, float thrown) {
  sn::ImprintRecord rec;
  rec.shape = shape;
  rec.motion = motion;
  rec.sink = sink;
  rec.thrown = thrown;
  return rec;
}

void frame_at(ge::gpu::Device& d, const ge::gpu::PipelineSet& pipelines, r::SnowMarks& m,
              r::FrameRecorder& rec, double x, double z) {
  rec.begin_frame(d);
  r::prepare_snow_marks(d, pipelines, m, x, z);
  wgpu::CommandEncoder encoder = d.device.CreateCommandEncoder();
  r::encode_snow_marks(m, pipelines, rec, encoder);
  rec.end_frame(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  d.queue.Submit(1, &commands);
  rec.after_submit(d);
}

int texel(const ge::gpu::Image& img, const r::MarkRect& rect, double x, double z, int k) {
  const auto c = static_cast<size_t>(std::floor((x - rect.x0) / rect.texel));
  const auto w = static_cast<size_t>(std::floor((z - rect.z0) / rect.texel));
  return img.rgba[(w * img.width + c) * 4 + static_cast<size_t>(k)];
}

}  // namespace

TEST(records_become_stamps_of_their_outline) {
  std::vector<sn::ImprintRecord> recs = {
      record(sn::capsule(1, 2, 1, 4, 0.04), sn::Motion::slide, 0.06f, 0.5f),
      record(sn::disc(5, 5, 0.1), sn::Motion::step, 0.1f, 0),
      record(sn::box(10, 10, std::numbers::pi / 2, 1.5, 0.2), sn::Motion::roll, 0.12f, 2),
      record(sn::ellipse(20, 20, 0, 0.9, 0.3), sn::Motion::dig, 0.3f, 4),
      record(sn::disc(30, 30, 0.1), sn::Motion::step, 0, 0),  // no sink: no mark
  };
  std::vector<r::MarkStamp> out;
  r::marks_from_records(recs, out);
  CHECK(out.size() == 4);
  CHECK(out[0].shape == r::MarkShape::capsule && out[0].ax == 1 && out[0].bz == 4);
  CHECK(out[0].wall == 2 && std::fabs(out[0].depth - 0.06f) < 1e-7f && out[0].berm > 0);
  CHECK(out[1].shape == r::MarkShape::capsule && out[1].ax == out[1].bx && out[1].az == out[1].bz);
  CHECK(out[1].wall == 10 && out[1].berm == 0);
  // The box heads along +x (a quarter turn clockwise from +z): its ends 1.5 m either side.
  CHECK(out[2].shape == r::MarkShape::box && std::fabs(out[2].ax - 8.5f) < 1e-5f &&
        std::fabs(out[2].bx - 11.5f) < 1e-5f && std::fabs(out[2].az - 10) < 1e-5f);
  CHECK(out[2].wall == 6);
  CHECK(out[3].shape == r::MarkShape::ellipse && std::fabs(out[3].az - 19.1f) < 1e-5f &&
        std::fabs(out[3].bz - 20.9f) < 1e-5f);
  for (const r::MarkStamp& m : out) CHECK(m.berm <= r::kMarkBerm);
}

TEST(a_box_lands_square_and_an_ellipse_round) {
  auto device = ge::test::test_device();
  if (!device) return;
  ge::gpu::PipelineSet pipelines;
  r::SnowMarks m;
  r::MarkOptions o;
  o.fine_size = 256;  // 16 m: a texel 6.25 cm
  o.fine_span = 16;
  o.coarse_size = 64;
  r::create_snow_marks(*device, pipelines, m, 0, 0, 64, o);
  CHECK(pipelines.finish(*device));
  r::FrameRecorder rec;
  rec.init(*device, "marks.test", 2);
  // A box 2 m by 0.8 m along +z centred at (6, 8); an ellipse the same size at (10, 8).
  const std::vector<r::MarkStamp> stamps = {
      {6, 7, 6, 9, 0.4f, 0.1f, 0.02f, 6, r::MarkShape::box},
      {10, 7, 10, 9, 0.4f, 0.1f, 0.02f, 6, r::MarkShape::ellipse},
  };
  r::add_mark_stamps(m, stamps);
  frame_at(*device, pipelines, m, rec, 8, 8);
  const ge::gpu::Image img = ge::gpu::read_now(*device, m.fine);
  const r::MarkRect rect = r::fine_rect(m);
  // Near the box's corner: pressed. Near the ellipse's: not.
  CHECK(texel(img, rect, 6.33, 7.07, 2) > 0);
  CHECK(texel(img, rect, 10.33, 7.07, 2) == 0);
  // Both pressed and deep at the centre.
  CHECK(texel(img, rect, 6.03, 8.03, 0) > 70 &&
        texel(img, rect, 10.03, 8.03, 0) > 70);  // sqrt(0.1)
  // Past the box's end: no floor, and the berm.
  CHECK(texel(img, rect, 6.03, 9.1, 2) == 0 && texel(img, rect, 6.03, 9.1, 1) > 0);
  // A metre off: nothing.
  CHECK(texel(img, rect, 6.03, 10.6, 0) == 0 && texel(img, rect, 6.03, 10.6, 1) == 0);
  CHECK(device->errors.load() == 0);
}

TEST(a_layers_records_leave_their_grooves) {
  auto device = ge::test::test_device();
  if (!device) return;
  sn::SnowWorld world;
  sn::LayerSpec spec;
  spec.region = {0, 0, 16, 16};
  spec.cover.depth = 0.4;
  sn::Layer& layer = sn::add_layer(world, spec);
  const sn::Imprint ski = sn::reference_contact(0).imprint;  // a carving ski, 7 cm wide
  // Its axis on a texel's centre (x = 8 + half a 3.125 cm texel).
  for (int i = 0; i < 20; i++) {
    sn::press(layer, sn::place(ski, 8.015625, 4 + 0.4 * i, 0), 0.05 * i);
  }
  std::vector<r::MarkStamp> stamps;
  r::marks_from_records(layer.records, stamps);
  CHECK(!stamps.empty());
  sn::clear_records(layer);

  ge::gpu::PipelineSet pipelines;
  r::SnowMarks m;
  r::MarkOptions o;
  o.fine_size = 512;  // 16 m: a texel 3.125 cm
  o.fine_span = 16;
  o.coarse_size = 64;
  r::create_snow_marks(*device, pipelines, m, 0, 0, 16, o);
  CHECK(pipelines.finish(*device));
  r::FrameRecorder rec;
  rec.init(*device, "marks.test", 2);
  r::add_mark_stamps(m, stamps);
  frame_at(*device, pipelines, m, rec, 8, 8);
  const ge::gpu::Image img = ge::gpu::read_now(*device, m.fine);
  const r::MarkRect rect = r::fine_rect(m);
  // Across the groove at z = 7: deepest on the axis, shallower a texel off (u = 0.89 of its 3.5 cm
  // half width), gone past the half width.
  const int axis = texel(img, rect, 8.0157, 7.01, 0);
  const int off = texel(img, rect, 8.0469, 7.01, 0);
  CHECK(axis > 0 && off > 0 && off < axis);
  CHECK(texel(img, rect, 8.0157, 7.01, 2) > 0);
  CHECK(texel(img, rect, 8.5, 7.01, 0) == 0);
  CHECK(device->errors.load() == 0);
}
