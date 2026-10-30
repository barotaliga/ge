// plugins/classicenhance/upscaler.hpp, net.hpp and safetensors.hpp: a weights file written and
// read back whole, a cut or damaged one refused with what was wrong; a network missing a tensor
// or with one the wrong shape refused by name; the small seeded network upscaled 4× to the values
// the network's reference implementation gives for the same weights and input (checked once
// against it, within 1e-6), and to the same bits every run; and the texture enhancer using it
// in place of the kernel.
//
// ge:summary Test: the trained upscaler's weights file, its load, its run against reference values
// ge:keywords test classicenhance upscaler safetensors weights network transformer reference
// ge:skill classicenhance testing
#include <cmath>
#include <cstring>
#include <string>

#include "check.hpp"
#include "classicenhance/safetensors.hpp"
#include "classicenhance/texture.hpp"
#include "classicenhance/upscaler.hpp"

namespace ce = ge::classicenhance;

namespace {

// The input every run here upscales: 13 wide, 10 tall, a fixed pattern in 0..1.
ce::net::Map pattern() {
  ce::net::Map x(10, 13, 3);
  for (size_t i = 0; i < x.v.size(); i++) x.v[i] = float(double((i * 37) % 101) / 100.0);
  return x;
}

uint64_t fnv(const std::vector<float>& v) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (const float f : v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, 4);
    for (int i = 0; i < 4; i++) h = (h ^ ((bits >> (8 * i)) & 0xff)) * 0x100000001b3ull;
  }
  return h;
}

ce::Upscaler small() {
  ce::Upscaler u;
  std::string error;
  CHECK_MSG(ce::load_upscaler(ce::seeded_upscaler_weights(7), u, error), error.c_str());
  return u;
}

}  // namespace

TEST(weights_file_round_trips) {
  const ce::Weights w = ce::seeded_upscaler_weights(7);
  const std::vector<uint8_t> bytes = ce::write_safetensors(w);
  ce::Weights back;
  std::string error;
  CHECK_MSG(ce::read_safetensors(bytes, back, error), error.c_str());
  CHECK(back.metadata == w.metadata);
  CHECK(back.tensors.size() == w.tensors.size());
  for (const auto& [name, t] : w.tensors) {
    const auto it = back.tensors.find(name);
    CHECK_MSG(it != back.tensors.end(), name.c_str());
    if (it == back.tensors.end()) continue;
    CHECK(it->second.shape == t.shape);
    CHECK(it->second.data == t.data);
  }
}

TEST(weights_file_cut_or_damaged_is_refused) {
  const std::vector<uint8_t> bytes = ce::write_safetensors(ce::seeded_upscaler_weights(7));
  ce::Weights w;
  std::string error;
  const std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + std::ptrdiff_t(bytes.size() / 2));
  CHECK(!ce::read_safetensors(cut, w, error));
  CHECK_MSG(error.find("cut short") != std::string::npos, error.c_str());
  std::vector<uint8_t> garbled = bytes;
  garbled[8] = 'x';
  CHECK(!ce::read_safetensors(garbled, w, error));
  CHECK_MSG(error.find("not JSON") != std::string::npos, error.c_str());
  CHECK(!ce::read_safetensors(std::vector<uint8_t>(5, 0), w, error));
}

TEST(network_missing_a_tensor_or_misshapen_is_refused_by_name) {
  ce::Weights w = ce::seeded_upscaler_weights(7);
  w.tensors.erase("layers.1.blocks.0.attn.proj.weight");
  ce::Upscaler u;
  std::string error;
  CHECK(!ce::load_upscaler(w, u, error));
  CHECK_MSG(error.find("layers.1.blocks.0.attn.proj.weight") != std::string::npos, error.c_str());
  w = ce::seeded_upscaler_weights(7);
  w.tensors["conv_last.bias"].shape = {4};
  w.tensors["conv_last.bias"].data.push_back(0);
  CHECK(!ce::load_upscaler(w, u, error));
  CHECK_MSG(error.find("conv_last.bias") != std::string::npos, error.c_str());
  w = ce::seeded_upscaler_weights(7);
  w.metadata["metadata"] = "{\"upsampler\": \"pixelshuffledirect\"}";
  CHECK(!ce::load_upscaler(w, u, error));
}

TEST(small_network_matches_the_reference_values_and_its_bits) {
  const ce::Upscaler u = small();
  CHECK(u.scale == 4 && u.groups.size() == 2 && u.groups[1].blocks[0].shifted);
  const ce::net::Map y = ce::upscale(u, pattern());
  CHECK(y.w == 52 && y.h == 40 && y.c == 3);
  // The reference implementation's output for the same weights and input.
  struct Want {
    int y, x, c;
    float v;
  };
  for (const Want& w :
       {Want{0, 0, 0, 0.4395880f}, Want{5, 17, 1, 0.4092107f}, Want{39, 51, 2, 0.4191881f},
        Want{20, 26, 0, 0.4251678f}, Want{13, 40, 2, 0.4019156f}}) {
    const float got = y.at(w.y, w.x)[w.c];
    CHECK_MSG(std::fabs(got - w.v) < 1e-6f,
              (std::to_string(got) + " vs " + std::to_string(w.v)).c_str());
  }
  // The same bits on every run (the bands never overlap, the sums run in a fixed order).
  CHECK(fnv(ce::upscale(u, pattern()).v) == fnv(y.v));
}

TEST(texture_enhancer_uses_the_network_in_place_of_the_kernel) {
  const ce::Upscaler u = small();
  ce::Palette palette;
  for (int i = 0; i < 256; i++) {
    palette.rgb[size_t(i) * 3] = uint8_t(i);
    palette.rgb[size_t(i) * 3 + 1] = uint8_t(255 - i);
    palette.rgb[size_t(i) * 3 + 2] = uint8_t((i * 7) & 255);
  }
  std::vector<uint8_t> indexed(size_t{16} * 8);
  for (size_t i = 0; i < indexed.size(); i++) indexed[i] = uint8_t((i * 13) % 200);
  ce::EnhanceOptions kernel;
  ce::EnhanceOptions net = kernel;
  net.network = &u;
  const ce::Enhanced a = ce::enhance_texture("wall", indexed, 16, 8, palette, kernel);
  const ce::Enhanced b = ce::enhance_texture("wall", indexed, 16, 8, palette, net);
  CHECK(b.albedo.width == 64 && b.albedo.height == 32 && b.normal.width == 64);
  CHECK(a.albedo.rgba != b.albedo.rgba);
  // Another factor than the network's falls back to the kernel.
  net.scale = 2;
  kernel.scale = 2;
  CHECK(ce::enhance_texture("wall", indexed, 16, 8, palette, net).albedo.rgba ==
        ce::enhance_texture("wall", indexed, 16, 8, palette, kernel).albedo.rgba);
}
