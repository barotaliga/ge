// engine/render/ui_pass.hpp: the 2D pass draws what the CPU raster (ui/raster.hpp) draws. Each
// list is drawn both ways and held within a level or two at almost every pixel: rounded boxes,
// borders, feathered shadows, every paint, opacity and clips; text at several sizes, clipped;
// images stretched and tinted; boxes turned about their centre or a pivot and nine-sliced;
// lines, arcs and convex polygons; a whole menu page over a backdrop. Then the bookkeeping: a list
// drawn again uploads no glyph again, a list with no image is one draw, each image change
// another, and an atlas too small for a frame is emptied and refilled.
//
// ge:summary Tests for the 2D pass: GPU pictures against the CPU raster, the atlas, the draws
// ge:keywords test render ui 2d-pass raster parity glyph atlas image draws tolerance
// ge:skill render ui testing
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"
#include "hud/instruments.hpp"
#include "render/frame.hpp"
#include "render/ui_pass.hpp"
#include "ui/background.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/menu_view.hpp"
#include "ui/raster.hpp"

namespace {

namespace r = ge::render;
namespace u = ge::ui;

// What a GPU picture may differ from the raster's by: `kLevels` a channel at all but `kShare`
// of the pixels (edges a rasteriser rounds its own way).
constexpr int kLevels = 3;
constexpr double kShare = 0.003;

struct Rig {
  std::unique_ptr<ge::gpu::Device> device;
  ge::gpu::PipelineSet pipelines;
  r::UiPass pass;
  r::FrameRecorder rec;
};

std::unique_ptr<Rig> make_rig() {
  auto rig = std::make_unique<Rig>();
  rig->device = ge::test::test_device();
  if (!rig->device) return nullptr;  // skipped (or failed under GE_GPU_REQUIRE)
  r::create_ui_pass(*rig->device, rig->pipelines, rig->pass);
  CHECK(rig->pipelines.finish(*rig->device));
  ge::gpu::end_load(*rig->device);
  rig->rec.init(*rig->device, "test.ui", 2);
  return rig;
}

ge::gpu::Image gpu_draw(Rig& rig, const u::DrawList& list, uint32_t w, uint32_t h) {
  auto& d = *rig.device;
  const ge::gpu::Target out = ge::gpu::create_target(d, "test.ui.out", w, h);
  rig.rec.begin_frame(d);
  r::prepare_ui(d, rig.pass, list, w, h);
  wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
  r::encode_ui(rig.pass, rig.pipelines, rig.rec, e, out.view);
  rig.rec.end_frame(e);
  wgpu::CommandBuffer c = e.Finish();
  d.queue.Submit(1, &c);
  rig.rec.after_submit(d);
  return ge::gpu::read_now(d, out);
}

ge::gpu::Image cpu_draw(const u::DrawList& list, int w, int h) {
  u::Image img = u::make_image(w, h, {0, 0, 0, 1});
  u::render(list, img);
  return {static_cast<uint32_t>(w), static_cast<uint32_t>(h), std::move(img.rgba)};
}

void check_same(Rig& rig, const char* name, const u::DrawList& list, int w, int h) {
  const ge::gpu::Image want = cpu_draw(list, w, h);
  const ge::gpu::Image got =
      gpu_draw(rig, list, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  CHECK(got.width == want.width && got.height == want.height);
  if (got.width != want.width || got.height != want.height) return;
  const ge::test::ImageDiff diff = ge::test::diff_images(got, want, kLevels);
  if (diff.over > kShare) {
    ge::test::write_ppm(std::string(name) + ".gpu.ppm", got);
    ge::test::write_ppm(std::string(name) + ".cpu.ppm", want);
    char m[256];
    std::snprintf(m, sizeof m,
                  "%s: %.3f%% of pixels differ from the raster by more than %d levels (worst %d); "
                  "see %s.gpu.ppm and %s.cpu.ppm beside the build; skill: render.",
                  name, diff.over * 100, kLevels, diff.max_delta, name, name);
    ge::test::fail(__FILE__, __LINE__, m);
  }
  CHECK(rig.device->errors.load() == 0);
}

// The first command of every list: an opaque backdrop over the whole target (the GPU target
// starts undefined), the frame the pass draws over.
void backdrop(u::DrawList& list, int w, int h) {
  const u::Stop s[3] = {{0, u::hex(0x0e2a47)}, {0.6f, u::hex(0x14385e)}, {1, u::hex(0x5d86b0)}};
  u::Box b;
  b.rect = {0, 0, static_cast<float>(w), static_cast<float>(h)};
  b.fill = u::linear_at_angle(b.rect.w, b.rect.h, 180, s, 3);
  list.box(b);
}

}  // namespace

TEST(boxes_paints_borders_and_clips_match_the_raster) {
  auto rig = make_rig();
  if (!rig) return;
  constexpr int w = 160, h = 120;
  u::DrawList list;
  list.begin(w, h);
  backdrop(list, w, h);
  u::Box shadow;
  shadow.rect = {14, 16, 60, 40};
  shadow.radius = 8;
  shadow.fill = u::solid(u::hex(0x000000, 0.5f));
  shadow.feather = 9;
  list.box(shadow);
  u::Box slab;
  slab.rect = {10.5f, 10.25f, 60, 40};
  slab.radius = 8;
  slab.fill = u::solid(u::hex(0xd8312b));
  slab.border = 2;
  slab.border_color = u::hex(0xffffff, 0.6f);
  list.box(slab);
  const u::Stop rainbow[8] = {{0, u::hex(0xff0000)},    {0.1f, u::hex(0xff8800)},
                              {0.3f, u::hex(0xffff00)}, {0.45f, u::hex(0x00ff00)},
                              {0.6f, u::hex(0x00ffff)}, {0.75f, u::hex(0x0000ff)},
                              {0.9f, u::hex(0x8800ff)}, {1, u::hex(0xff00ff, 0.5f)}};
  u::Box bar;
  bar.rect = {84, 12, 66, 30};
  bar.radius = 15;
  bar.fill = u::linear_at_angle(66, 30, 90, rainbow, 8);
  list.box(bar);
  const u::Stop glow[2] = {{0, u::hex(0xffc58a, 0.9f)}, {1, u::hex(0xffc58a, 0)}};
  u::Box spot;
  spot.rect = {80, 50, 70, 60};
  spot.fill = u::radial(35, 30, 30, glow, 2, 0.7f);
  spot.opacity = 0.8f;
  list.box(spot);
  list.push_clip({20, 64, 50, 40});
  u::Box clipped;
  clipped.rect = {8, 58, 80, 56};
  clipped.radius = 30;
  clipped.fill = u::solid(u::hex(0x4caf50, 0.75f));
  list.box(clipped);
  list.pop_clip();
  check_same(*rig, "ui_pass_boxes", list, w, h);
}

TEST(text_matches_the_raster_at_every_size_and_clip) {
  auto rig = make_rig();
  if (!rig) return;
  constexpr int w = 240, h = 140;
  u::DrawList list;
  list.begin(w, h);
  backdrop(list, w, h);
  const u::Font& semi = u::ui_font(u::Weight::semibold);
  const u::Font& med = u::ui_font(u::Weight::medium);
  list.text(semi, 40, 8.3f, 40, "FALL LINE", u::hex(0xffffff), u::Align::left, 1.5f);
  list.text(med, 19, 10, 66, "ENTER SELECT   ESC BACK", u::hex(0xffffff, 0.68f), u::Align::left, 0);
  list.text(semi, 13, 10.7f, 86, "Kerning AV To Wa 0123456789", u::hex(0xffd54f), u::Align::left,
            0.3f);
  list.push_clip({10, 96, 90, 30});
  list.text(semi, 30, 6, 124, "CLIPPED TEXT", u::hex(0x80d8ff), u::Align::left, 0);
  list.pop_clip();
  check_same(*rig, "ui_pass_text", list, w, h);
}

TEST(images_match_the_raster_stretched_and_tinted) {
  auto rig = make_rig();
  if (!rig) return;
  constexpr int w = 128, h = 96;
  u::Image img = u::make_image(16, 12, {0, 0, 0, 1});
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      uint8_t* p = img.rgba.data() + (static_cast<size_t>(y) * img.width + x) * 4;
      p[0] = static_cast<uint8_t>(x * 16), p[1] = static_cast<uint8_t>(y * 20);
      p[2] = static_cast<uint8_t>((x + y) % 2 ? 200 : 40), p[3] = 255;
    }
  }
  u::DrawList list;
  list.begin(w, h);
  backdrop(list, w, h);
  u::Box a;
  a.rect = {4, 4, 64, 48};
  a.image = &img;
  a.uv = {0, 0, 16, 12};
  list.box(a);
  u::Box b;
  b.rect = {72, 8, 50, 80};
  b.radius = 10;
  b.image = &img;
  b.uv = u::fit_uv(img, 50, 80, u::Fit::cover, 0.3f, 0.5f, 1.2f);
  b.fill = u::solid(u::hex(0xd6e2ef, 0.9f));
  list.box(b);
  check_same(*rig, "ui_pass_images", list, w, h);
}

TEST(turned_and_sliced_boxes_lines_arcs_and_polygons_match_the_raster) {
  auto rig = make_rig();
  if (!rig) return;
  constexpr int w = 200, h = 140;
  // A 12x12 frame for the nine-slice: light corners, mid edges, a dark middle.
  u::Image frame = u::make_image(12, 12, u::hex(0x203040));
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 12; ++x) {
      const bool ex = x < 4 || x >= 8, ey = y < 4 || y >= 8;
      if (!ex && !ey) continue;
      uint8_t* p = frame.rgba.data() + (static_cast<size_t>(y) * 12 + x) * 4;
      p[0] = ex && ey ? 240 : 120, p[1] = ex && ey ? 220 : 160, p[2] = 90, p[3] = 255;
    }
  }
  u::DrawList list;
  list.begin(w, h);
  backdrop(list, w, h);
  const u::Stop warm[2] = {{0, u::hex(0xffd54f)}, {1, u::hex(0xd8312b)}};
  u::Box tilted;
  tilted.rect = {20, 20, 50, 24};
  tilted.radius = 6;
  tilted.fill = u::linear(0, 0, 50, 0, warm[0].color, warm[1].color);
  tilted.border = 2;
  tilted.border_color = u::hex(0xffffff, 0.8f);
  tilted.rotation = 0.4f;
  list.box(tilted);
  u::Box needle;  // a needle standing on (150, 70), turned past three o'clock
  needle.rect = {148, 30, 4, 40};
  needle.radius = 2;
  needle.fill = u::solid(u::hex(0xff5252));
  needle.pivot = u::Vec2{2, 40};
  needle.rotation = 2.2f;
  list.box(needle);
  u::Box panel;
  panel.rect = {14, 86, 80, 44};
  panel.image = &frame;
  panel.uv = {0, 0, 12, 12};
  panel.slice = {4, 4, 4, 4};
  panel.slice_to = {8, 8, 8, 8};
  list.box(panel);
  list.arc({150, 70}, 30, 6, -2.2f, 2.2f, u::linear(0, 0, 72, 0, warm[0].color, warm[1].color));
  list.arc({150, 70}, 22, 3, 0, 1.3f, u::solid(u::hex(0x80d8ff)), true);
  list.arc({110, 110}, 12, 4, 0, 7, u::solid(u::hex(0xffffff, 0.7f)));
  list.line({100, 20}, {190, 40.5f}, 3, u::hex(0xffffff));
  const u::Vec2 zig[4] = {{100, 50}, {112, 34}, {124, 50}, {136, 34}};
  list.polyline(zig, 2, u::hex(0x4caf50));
  const u::Vec2 chevron[4] = {{150, 112}, {170, 100}, {190, 112}, {170, 106}};
  list.polygon(std::span<const u::Vec2>(chevron, 3), u::solid(u::hex(0xffd54f)));
  const u::Vec2 hexagon[6] = {{140, 120}, {148, 116}, {156, 120},
                              {156, 130}, {148, 134}, {140, 130}};
  const u::Stop cool[2] = {{0, u::hex(0x80d8ff)}, {1, u::hex(0x0e2a47, 0.6f)}};
  list.polygon(hexagon, u::radial(8, 9, 10, cool, 2));
  list.push_clip({60, 60, 40, 30});
  list.arc({80, 75}, 16, 8, 0, 6.3f, u::solid(u::hex(0xd8312b, 0.8f)));
  list.pop_clip();
  check_same(*rig, "ui_pass_shapes", list, w, h);
}

TEST(a_menu_page_over_its_backdrop_matches_the_raster) {
  auto rig = make_rig();
  if (!rig) return;
  constexpr int w = 640, h = 360;
  u::Menu menu;
  u::Page& root = menu.add_page("root");
  root.items = {{.id = "play", .text = "PLAY", .hint = "Ride the mountain", .prominent = true},
                {.id = "opt", .text = "OPTIONS"},
                {.id = "off", .text = "REPLAYS", .disabled = true},
                {.id = "quit", .text = "QUIT"}};
  u::MenuStyle style;
  style.logo = "FALL LINE";
  style.help = "ENTER SELECT    ESC BACK";
  style.footer = "test";
  const u::MenuLayout layout = u::layout_menu(menu, w, h, style);
  u::MenuView view;
  view.settle(menu, layout);
  u::Background bg = u::Background::gradient(160, u::hex(0x0b1524), u::hex(0x1d3550));
  bg.glow(0.72f, 0.3f, 0.55f, u::hex(0xffc58a, 0.55f)).vignette(0.7f);
  u::DrawList list;
  list.begin(w, h);
  u::draw_background(list, bg, {0, 0, float{w}, float{h}}, 0, nullptr);
  view.draw(list, menu, layout, style);
  check_same(*rig, "ui_pass_menu", list, w, h);
}

TEST(the_hud_instruments_match_the_raster) {
  auto rig = make_rig();
  if (!rig) return;
  constexpr int w = 640, h = 360;
  namespace gh = ge::hud;
  const gh::Style st;
  const gh::Frame f = gh::frame_for(w, h, st);
  u::DrawList list;
  list.begin(w, h);
  backdrop(list, w, h);
  gh::Dial d;
  d.max = 140, d.knee = 30, d.red = 110;
  gh::draw(list, f, st, d, {87, "87", "KM/H", st.ink}, {20, 20, 150, 150});
  gh::WindReading wr;
  wr.heading = 0.6, wr.felt_x = -9, wr.felt_z = -12, wr.weather = true, wr.weather_x = 6;
  wr.figure = "15", wr.unit = "M/S";
  gh::draw(list, f, st, gh::WindGauge{}, wr, {220, 20, 120, 150});
  const std::vector<gh::CompassMark> marks{{0.5, st.good}};
  gh::draw(list, f, st, gh::Compass{}, {0.8, marks, st.ink}, {200, 250, 360, 44});
  check_same(*rig, "ui_pass_hud_instruments", list, w, h);
}

TEST(a_list_drawn_again_uploads_nothing_and_counts_its_draws) {
  auto rig = make_rig();
  if (!rig) return;
  auto& d = *rig->device;
  const u::Font& semi = u::ui_font(u::Weight::semibold);
  u::Image img = u::make_image(4, 4, u::hex(0x336699));
  u::DrawList list;
  list.begin(64, 32);
  backdrop(list, 64, 32);
  list.text(semi, 16, 2, 20, "AGAIN", u::hex(0xffffff), u::Align::left, 0);
  r::prepare_ui(d, rig->pass, list, 64, 32);
  const uint32_t first = rig->pass.glyphs_uploaded;
  CHECK(first > 0);
  r::prepare_ui(d, rig->pass, list, 64, 32);
  CHECK(rig->pass.glyphs_uploaded == first);
  uint32_t draws = 0;
  for (const auto& range : rig->pass.ranges) draws += range.count > 0 ? 1 : 0;
  CHECK(draws == 1);  // boxes and glyphs alike, one draw

  u::Box pic;
  pic.rect = {0, 0, 8, 8};
  pic.image = &img;
  pic.uv = {0, 0, 4, 4};
  list.box(pic);
  list.text(semi, 16, 30, 20, "AFTER", u::hex(0xffffff), u::Align::left, 0);
  r::prepare_ui(d, rig->pass, list, 64, 32);
  draws = 0;
  for (const auto& range : rig->pass.ranges) draws += range.count > 0 ? 1 : 0;
  CHECK(draws == 2);  // the image's box opens a draw; the text after it rides along
  CHECK(rig->pass.pictures.size() == 1);
}

TEST(a_full_atlas_is_emptied_and_refilled) {
  auto rig = make_rig();
  if (!rig) return;
  auto& d = *rig->device;
  const u::Font& semi = u::ui_font(u::Weight::semibold);
  // Every capital at many sizes, one list at a time: more glyphs than one atlas holds.
  u::DrawList list;
  for (int size = 40; size <= 200; size += 8) {
    list.begin(64, 64);
    list.text(semi, static_cast<float>(size), 0, static_cast<float>(size),
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ", u::hex(0xffffff), u::Align::left, 0);
    r::prepare_ui(d, rig->pass, list, 64, 64);
  }
  CHECK(rig->pass.atlas_resets > 0);
  // The last list's glyphs are all in the atlas, each drawn.
  CHECK(rig->pass.staged_items.size() == 26);
  CHECK(d.errors.load() == 0);
}

TEST(a_texture_bound_to_a_key_is_what_its_boxes_draw) {
  auto rig = make_rig();
  if (!rig) return;
  auto& d = *rig->device;
  // The "live frame": a target the GPU filled, never on the CPU.
  const ge::gpu::Target frame = ge::gpu::create_target(d, "test.ui.frame", 8, 8);
  {
    u::DrawList fill;
    fill.begin(8, 8);
    fill.rect({0, 0, 8, 8}, u::hex(0x2080c0));
    r::prepare_ui(d, rig->pass, fill, 8, 8);
    wgpu::CommandEncoder e = d.device.CreateCommandEncoder();
    rig->rec.begin_frame(d);
    r::encode_ui(rig->pass, rig->pipelines, rig->rec, e, frame.view);
    rig->rec.end_frame(e);
    wgpu::CommandBuffer c = e.Finish();
    d.queue.Submit(1, &c);
    rig->rec.after_submit(d);
  }
  const u::Image key;  // stands for the frame; its own pixels are never read
  r::ui_texture(d, rig->pass, &key, frame.view, 8, 8);
  u::DrawList list;
  list.begin(16, 16);
  list.rect({0, 0, 16, 16}, u::hex(0x000000));
  u::Box b;
  b.rect = {0, 0, 16, 16};
  b.image = &key;
  b.uv = {0, 0, 8, 8};
  list.box(b);
  const ge::gpu::Image got = gpu_draw(*rig, list, 16, 16);
  const uint8_t* centre = &got.rgba[(size_t{8} * 16 + 8) * 4];
  CHECK(std::abs(int{centre[0]} - 0x20) <= 1 && std::abs(int{centre[1]} - 0x80) <= 1 &&
        std::abs(int{centre[2]} - 0xc0) <= 1);
  CHECK(d.errors.load() == 0);
}
