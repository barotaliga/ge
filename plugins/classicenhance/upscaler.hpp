// THE TRAINED TEXTURE UPSCALER: a dual aggregation transformer, which makes a picture four times
// larger with the grain, the cracks and the worn edges a material keeps. It is run on the
// player's machine with weights fetched at bake time (`kUpscalerWeights`), never committed.
//
// The network, from the picture in to the picture out (every map token-major, net.hpp):
//
// - the picture less a mean colour, then a 3 × 3 convolution to `embed` channels;
// - a layer norm, then residual GROUPS: each is `depth` blocks then a 3 × 3 convolution, added to
//   what came into the group; then a layer norm and a 3 × 3 convolution, added to the first
//   convolution's output;
// - a 3 × 3 convolution to 64 channels and a leaky ReLU, then per factor of 2 a 3 × 3 convolution
//   to 256 channels and a pixel shuffle, then a 3 × 3 convolution to RGB, plus the mean again.
//
// A BLOCK adds an attention of its normed input, then a feed-forward of its normed result. The
// attention of an even block (counted from 0 in its group) is SPATIAL: half the channels attend
// within windows `split[0]` tall and `split[1]` wide, the other half within windows the other way
// round, each head's scores biased by the tokens' offset in the window (a small MLP of the offset,
// worked out once at load); the map is zero-padded right and bottom to a whole number of the
// longer side, and in some blocks (`shifted`) both halves are rolled by half a window first, with
// the scores between tokens that wrapped from different sides pushed down by 100. The attention
// of an odd block is across CHANNELS: per head, every channel's values over the whole map,
// normalised, attend to every other channel's, scaled by a learned temperature. Either way a
// depthwise 3 × 3 convolution of the values runs beside the attention, and each gates the other:
// one through a channel map (the mean over the map, through two 1 × 1 layers), one through a
// spatial map (each token through two 1 × 1 layers). The FEED-FORWARD doubles the channels
// (GELU), then multiplies the first half by the second half layer-normed and run through a
// depthwise 3 × 3 convolution, and projects back.
//
// The channel attention reads the whole map, so a picture upscaled in tiles is not the picture
// upscaled whole; the bake upscales each texture whole, wrapped round a margin so its edges
// tile (texture stage).
//
// ge:summary The trained texture upscaler: a dual aggregation transformer, its weights and its run
// ge:keywords upscaler super-resolution transformer dual-aggregation attention window channel
// ge:keywords spatial-attention channel-attention position-bias shifted-window gated feed-forward
// ge:keywords pixel-shuffle weights load inference pbr texture
// ge:skill classicenhance
// ge:test tests/classicenhance_upscaler_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/net.hpp"
#include "classicenhance/safetensors.hpp"

namespace ge::classicenhance {

// Where the bake fetches the weights from, and the digest it holds them to (CC0).
struct UpscalerWeights {
  std::string_view file;    // the name in the cache
  std::string_view url;     // fetched once over HTTPS
  std::string_view sha256;  // lower-case hex
};
extern const UpscalerWeights kUpscalerWeights;

struct UpscalerBlock {
  net::Norm norm1, norm2;
  net::Linear qkv, proj;
  bool spatial = true;  // an even block's
  bool shifted = false;
  // Spatial: per branch, the score bias [head][query][key] of the window's tokens.
  std::array<std::vector<float>, 2> bias;
  std::vector<float> temperature;  // channel: per head
  net::Depthwise3 dwconv;          // its batch norm folded in, GELU after
  net::Linear channel1, channel2;  // the channel map's two 1 × 1 layers (the first's norm folded)
  net::Linear spatial1, spatial2;  // the spatial map's
  net::Linear fc1, fc2;
  net::Norm gate_norm;
  net::Depthwise3 gate_conv;
};

struct UpscalerGroup {
  std::vector<UpscalerBlock> blocks;
  net::Conv3 conv;
};

struct Upscaler {
  std::string digest;  // its weights file's SHA-256, set by whoever read the file: keys the cache
  int embed = 0, heads = 0, scale = 0;
  std::array<int, 2> split{};
  std::array<float, 3> mean{};
  net::Conv3 first;
  net::Norm before;
  std::vector<UpscalerGroup> groups;
  net::Norm norm;
  net::Conv3 after_body, before_up, last;
  std::vector<net::Conv3> up;
};

// The network in `weights`; false with `error` when a tensor it needs is missing or the wrong
// shape, or its metadata names a form this runner does not have.
bool load_upscaler(const Weights& weights, Upscaler& out, std::string& error);

// What runs the network when the CPU does not: the GPU's runner (app/upscaler_gpu.hpp).
class UpscaleDevice {
 public:
  virtual ~UpscaleDevice() = default;
  // `upscale`'s contract; false with `error` when this picture cannot run here (the caller then
  // runs it on the CPU).
  virtual bool run(const net::Map& rgb, net::Map& out, std::string& error) = 0;
  // A short name for the cache's key and the reports: "gpu".
  virtual std::string name() const = 0;
};

// The network in the weights file at `file`, its digest set from the file's bytes. A file that is
// not the published one (`kUpscalerWeights.sha256`) is still run when it loads; `known` says
// which it was.
bool open_upscaler(const std::filesystem::path& file, Upscaler& out, bool* known,
                   std::string& error);

// `rgb` (3 channels, 0..1, as stored: not linearised) made `scale` times larger. Not clamped.
net::Map upscale(const Upscaler& net, const net::Map& rgb);

// A small network of the same form with seeded random weights (32 channels, 2 heads, windows
// 2 × 4, two groups of two blocks, so a plain and a shifted window attention and both channel
// attentions run; 4×): what the tests and the labs run where the trained weights are not fetched.
Weights seeded_upscaler_weights(uint32_t seed);

}  // namespace ge::classicenhance
