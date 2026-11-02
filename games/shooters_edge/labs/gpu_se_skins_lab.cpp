// LAB `se-skins`: A GAME'S PICTURE STANDING IN FOR A MODEL'S SKIN, alpha and all. The fixture
// halls (plugins/brush) under the baseline mod as a listen server, with an armour on the floor:
// the fixture model (plugins/morph) under the armour's name. Drawn twice as the window draws it
// (client/world_draw.hpp, client/world_models.hpp): once with the pack's own skin, once with a
// picture beside the model (`progs/armor.mdl_0.tga`, the first game's adapter's place for it) of
// orange bars with see-through gaps, so the armour is drawn as a cage the room shows through.
//
// The adapter's answer (the skins it found pictures for, the see-through texels) is the digest;
// the pictures are measured (each adapter rasterises its own way).
//
// ge:summary Lab: a model's skin from a PNG or TGA with alpha, its see-through texels cut
// ge:keywords lab se-skins replacement-skin skin-stand-in alpha hole cut-out open_model tga
// ge:keywords armour morph classic
// ge:skill shooter render labs
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "assets/tga.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "morph/write.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace ls = ge::lab::liquid_scene;
namespace lm = ge::core::libm;

constexpr int kSkin = 64;  // the picture's side, texels

br::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// Orange bars a texel wide every four, across and down; the gaps between them clear.
ge::assets::Image cage() {
  ge::assets::Image img;
  img.width = kSkin;
  img.height = kSkin;
  img.rgba.resize(size_t{kSkin} * kSkin * 4);
  for (int y = 0; y < kSkin; y++)
    for (int x = 0; x < kSkin; x++) {
      uint8_t* p = &img.rgba[(size_t(y) * kSkin + size_t(x)) * 4];
      const bool bar = (x % 4) == 0 || (y % 4) == 0;
      p[0] = 240, p[1] = uint8_t(120 + (y * 2)), p[2] = 30, p[3] = bar ? 255 : 0;
    }
  return img;
}

// The halls with an armour on the floor and a palette of our own ramps, in a pack; with the cage
// beside the model when `stand_in`.
bool fixture_pack(bool stand_in, std::vector<uint8_t>& out, std::string& error) {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "64 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "item_armor1"}, {"origin", "160 192 24"}}));
  std::vector<uint8_t> bsp;
  if (!br::write_bsp(f, br::Family::v29, &bsp, &error)) return false;
  std::vector<uint8_t> palette(768);
  for (int i = 0; i < 256; ++i)
    for (int k = 0; k < 3; ++k)
      palette[size_t(i) * 3 + size_t(k)] = uint8_t((i % 16) * 16 + 8 - k * 2 * (i / 16 % 3));
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
      {"maps/halls.bsp", bsp},
      {"gfx/palette.lmp", palette},
      {"progs/armor.mdl", ge::morph::write_model(ge::morph::sample_model())}};
  if (stand_in) files.push_back({"progs/armor.mdl_0.tga", ge::assets::encode_tga(cage(), true)});
  return ge::vfs::write_pak(files, &out, &error);
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  std::string error;
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail(error, "The baseline mod should compile. Skill: progs.");
    return;
  }
  auto device = ge::gpu::create_device({.label = "se-skins", .validation = 1});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  if (!device) lab.note("No GPU adapter on this machine: no pictures.");
  shooters_edge::look::Picture look;
  look.look = "modern";  // the classic look holds the picture to the pack's palette
  std::vector<gl::Picture> pictures;
  const cl::GameAdapter& game = cl::classic_adapter();
  for (const bool stand_in : {false, true}) {
    cl::SearchPath files;
    std::vector<uint8_t> pak;
    if (!fixture_pack(stand_in, pak, error) ||
        !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "Read the fixture writers' refusal above. Skill: shooter.");
      return;
    }
    // What the adapter hands for the armour: the skins it found a picture for, the clear texels.
    cl::OpenedModel opened;
    if (!game.open_model(game, files, "progs/armor.mdl", opened, error)) {
      lab.fail("progs/armor.mdl did not open: " + error,
               "Read the reason; the fixture model should read. Skill: shooter.");
      return;
    }
    double found = 0, clear = 0;
    for (const ge::assets::Image& img : opened.skins) {
      if (img.width == 0) continue;
      found++;
      for (size_t i = 3; i < img.rgba.size(); i += 4) clear += img.rgba[i] < 128 ? 1 : 0;
    }
    const std::string tag = stand_in ? "picture" : "own";
    lab.metric("skins." + tag + ".stood-in", found, "count");
    lab.metric("skins." + tag + ".see-through-texels", clear, "count");
    cl::Listen l;
    std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
    if (!map || !cl::listen_start(l, progs, std::move(map), {}, error)) {
      lab.fail(error, "The fixture halls should load under the baseline mod. Skill: shooter.");
      return;
    }
    constexpr double kTic = 0.013;
    double t = 0;
    for (int i = 0; i < 120; i++) {  // the armour drops to the floor
      cl::listen_frame(l, {});
      t += kTic;
    }
    std::vector<cl::Placed> placed;
    cl::place_models(l, placed);
    const cl::Placed* armour = nullptr;
    for (const cl::Placed& p : placed)
      if (l.server->models[size_t(p.modelindex)] == "progs/armor.mdl") armour = &p;
    lab.metric("placed." + tag + ".armour", armour ? 1 : 0, "count");
    if (!device || !armour) continue;
    const cl::ModelLoad load{l.server->models, {}};
    cl::WorldDrawPtr frame = cl::create_world_draw(
        *device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {}, error, load, game);
    if (!frame) {
      lab.fail(error, "Read the gpu: lines above. Skill: render.");
      return;
    }
    ge::gpu::Target out = ge::gpu::create_target(*device, "se-skins.final", w, h);
    cl::ViewState shown;
    cl::FxState fx_state;
    cl::FxFrame fx;
    cl::frame_view(l, shown, 1, t, kTic);
    cl::step_fx(l, shown, fx_state, t, kTic, fx);
    ge::render::FrameView v = cl::frame_view(l, shown, 0, t, 0);
    // From a little way off toward the spawn and above, looking at the armour's middle.
    const float e[3] = {armour->origin.x - 56, armour->origin.y - 8, armour->origin.z + 28};
    const float at_f[3] = {armour->origin.x, armour->origin.y, armour->origin.z + 10};
    double eye[3], at[3];
    br::to_engine(e, eye);
    br::to_engine(at_f, at);
    for (int c = 0; c < 3; c++) v.eye[c] = eye[c];
    const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
    v.heading = lm::atan2(dx, dz);
    v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
    cl::draw_world(*device, *frame, v, fx, nullptr, 0, {}, out.view);
    const ge::gpu::Image img = ge::gpu::read_now(*device, out);
    pictures.push_back({int(img.width), int(img.height), img.rgba});
  }
  if (pictures.size() == 2) {
    lab.shot({"armour-own",
              "THE ARMOUR, ITS OWN SKIN",
              "The armour on the floor of the halls, drawn with the skin its model file carries.",
              "a solid figure standing on the floor",
              {{"size", lab.arg("size")}}},
             pictures[0], gl::Det::measured);
    lab.shot({"armour-picture",
              "THE ARMOUR, A PICTURE WITH ALPHA AS ITS SKIN",
              "The same armour with progs/armor.mdl_0.tga beside it in the pack: orange bars with "
              "see-through gaps. The gaps are not drawn.",
              "an orange cage where the figure stood, the wall and floor seen through its gaps",
              {{"size", lab.arg("size")}}},
             pictures[1], gl::Det::measured);
  } else if (!device) {
    ls::placeholders(lab, {"armour-own", "armour-picture"});
  }
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-skins",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/game_adapter (open_model: a picture with alpha for a model's skin)",
    "shooter",
    "Does a PNG or TGA beside a model stand in for its skin, its see-through texels not drawn?",
    {{"size", "960,540", "the frame's width and height, px"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
