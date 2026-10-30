// THE LAYERS A TRAINED UPSCALER IS BUILT FROM, on the CPU: a feature map is H × W tokens of C
// float channels, token-major (y, then x, then the channel), and every layer reads one and
// writes another. Each layer keeps its weights in the order its inner loop walks (a linear's and
// a convolution's output channel innermost, so the loop over them vectorises), turned round once
// from the trained network's order when the weights are loaded (`to_linear`, `to_conv3`, ...).
// A batch norm is folded into the convolution before it.
//
// The sums run in a fixed order and the rows are split in bands whose outputs never overlap
// (core::parallel_bands), so a map's bits are the same for any number of threads. The
// transcendentals are ge::core::libm's, so they are the same on every platform.
//
// ge:summary An upscaler's layers on the CPU: linear, 3x3 and depthwise convolutions, norms, GELU
// ge:keywords neural-network inference layer linear convolution conv3x3 depthwise layer-norm
// ge:keywords batch-norm fold gelu sigmoid softmax pixel-shuffle feature-map token
// ge:skill classicenhance
// ge:test tests/classicenhance_upscaler_test.cpp
#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace ge::classicenhance::net {

struct Map {
  int h = 0, w = 0, c = 0;
  std::vector<float> v;  // h * w * c

  Map() = default;
  Map(int h_, int w_, int c_) : h(h_), w(w_), c(c_), v(size_t(h_) * size_t(w_) * size_t(c_)) {}
  size_t tokens() const { return size_t(h) * size_t(w); }
  float* at(int y, int x) { return v.data() + (size_t(y) * size_t(w) + size_t(x)) * size_t(c); }
  const float* at(int y, int x) const {
    return v.data() + (size_t(y) * size_t(w) + size_t(x)) * size_t(c);
  }
};

struct Linear {
  int in = 0, out = 0;
  std::vector<float> wt;  // [in][out]
  std::vector<float> b;   // [out]
};

struct Conv3 {  // 3 × 3, stride 1, zero padding 1
  int in = 0, out = 0;
  std::vector<float> wt;  // [tap = ky * 3 + kx][in][out]
  std::vector<float> b;   // [out]
};

struct Depthwise3 {  // one 3 × 3 kernel a channel, zero padding 1
  int c = 0;
  std::vector<float> w;  // [tap][c]
  std::vector<float> b;  // [c]
};

struct Norm {  // a layer norm's scale and shift
  std::vector<float> w, b;
};

// A batch norm in inference: y = (x - mean) / sqrt(var + 1e-5) * w + b.
struct BatchNorm {
  std::vector<float> w, b, mean, var;
};

// The layers from the trained network's order: a linear's weight [out][in]; a convolution's
// [out][in][3][3] (a 1 × 1 one's [out][in][1][1] is a Linear); a depthwise one's [c][1][3][3].
Linear to_linear(std::span<const float> w, std::span<const float> b, int out, int in);
Conv3 to_conv3(std::span<const float> w, std::span<const float> b, int out, int in);
Depthwise3 to_depthwise3(std::span<const float> w, std::span<const float> b, int c);
// The batch norm after a layer folded into it.
void fold(Linear& l, const BatchNorm& bn);
void fold(Depthwise3& d, const BatchNorm& bn);

// y[t] = x[t] · W + b for `tokens` rows of x (row stride xs) into y (row stride ys).
void linear(const Linear& l, const float* x, size_t xs, float* y, size_t ys, size_t tokens);
Map linear(const Linear& l, const Map& x);
Map conv3(const Conv3& k, const Map& x);
Map depthwise3(const Depthwise3& k, const Map& x);
// Every token normalised over its channels (eps 1e-5), into `out` (may be `x`).
void layer_norm(const Norm& n, const Map& x, Map& out);
// Each output pixel (2y + i, 2x + j) of channel c is input channel c * 4 + i * 2 + j.
Map pixel_shuffle2(const Map& x);

float gelu(float x);  // the exact one, x Φ(x)
float sigmoid(float x);
float exponential(float x);  // e^x, by ge::core::libm
void gelu(Map& x);
// A row of logits turned into probabilities in place.
void softmax(float* row, size_t n);

}  // namespace ge::classicenhance::net
