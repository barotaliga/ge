// The upscaler's layers on the CPU (net.hpp).
//
// ge:summary The upscaler's CPU layers: blocked linear and convolution kernels, norms, activations
// ge:keywords neural-network kernel blocked matmul convolution im2col layer-norm gelu erf softmax
// ge:skill classicenhance
// ge:test tests/classicenhance_upscaler_test.cpp
#include "classicenhance/net.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "core/parallel.hpp"

namespace ge::classicenhance::net {

namespace {

// Rows of `count` handed out in bands of at least `grain`, so a small map is not split thin.
template <class Body>
void bands(size_t count, size_t grain, Body&& body) {
  const size_t workers = std::min(core::default_workers(), std::max<size_t>(1, count / grain));
  core::parallel_bands(count, workers, body);
}

// out[0..n) += a * w[0..n), four rows at once so each weight row is loaded once for the four.
inline void axpy4(float* __restrict y0, float* __restrict y1, float* __restrict y2,
                  float* __restrict y3, const float* __restrict w, float a0, float a1, float a2,
                  float a3, int n) {
  for (int j = 0; j < n; j++) {
    const float wj = w[j];
    y0[j] += a0 * wj;
    y1[j] += a1 * wj;
    y2[j] += a2 * wj;
    y3[j] += a3 * wj;
  }
}

inline void axpy(float* __restrict y, const float* __restrict w, float a, int n) {
  for (int j = 0; j < n; j++) y[j] += a * w[j];
}

// The input with a one-token border of zeros round it.
Map bordered(const Map& x) {
  Map p(x.h + 2, x.w + 2, x.c);
  for (int y = 0; y < x.h; y++) {
    std::memcpy(p.at(y + 1, 1), x.at(y, 0), sizeof(float) * size_t(x.w) * size_t(x.c));
  }
  return p;
}

}  // namespace

Linear to_linear(std::span<const float> w, std::span<const float> b, int out, int in) {
  Linear l{in, out, std::vector<float>(size_t(in) * size_t(out)),
           std::vector<float>(b.begin(), b.end())};
  if (l.b.empty()) l.b.assign(size_t(out), 0.0f);
  for (int o = 0; o < out; o++) {
    for (int i = 0; i < in; i++)
      l.wt[size_t(i) * size_t(out) + size_t(o)] = w[size_t(o) * size_t(in) + size_t(i)];
  }
  return l;
}

Conv3 to_conv3(std::span<const float> w, std::span<const float> b, int out, int in) {
  Conv3 k{in, out, std::vector<float>(9 * size_t(in) * size_t(out)),
          std::vector<float>(b.begin(), b.end())};
  if (k.b.empty()) k.b.assign(size_t(out), 0.0f);
  for (int o = 0; o < out; o++) {
    for (int i = 0; i < in; i++) {
      for (int t = 0; t < 9; t++) {
        k.wt[(size_t(t) * size_t(in) + size_t(i)) * size_t(out) + size_t(o)] =
            w[(size_t(o) * size_t(in) + size_t(i)) * 9 + size_t(t)];
      }
    }
  }
  return k;
}

Depthwise3 to_depthwise3(std::span<const float> w, std::span<const float> b, int c) {
  Depthwise3 d{c, std::vector<float>(9 * size_t(c)), std::vector<float>(b.begin(), b.end())};
  if (d.b.empty()) d.b.assign(size_t(c), 0.0f);
  for (int i = 0; i < c; i++) {
    for (int t = 0; t < 9; t++)
      d.w[size_t(t) * size_t(c) + size_t(i)] = w[size_t(i) * 9 + size_t(t)];
  }
  return d;
}

namespace {

float bn_scale(const BatchNorm& bn, size_t o) {
  return float(double(bn.w[o]) / std::sqrt(double(bn.var[o]) + 1e-5));
}

}  // namespace

void fold(Linear& l, const BatchNorm& bn) {
  for (int o = 0; o < l.out; o++) {
    const float s = bn_scale(bn, size_t(o));
    for (int i = 0; i < l.in; i++) l.wt[size_t(i) * size_t(l.out) + size_t(o)] *= s;
    l.b[size_t(o)] = (l.b[size_t(o)] - bn.mean[size_t(o)]) * s + bn.b[size_t(o)];
  }
}

void fold(Depthwise3& d, const BatchNorm& bn) {
  for (int o = 0; o < d.c; o++) {
    const float s = bn_scale(bn, size_t(o));
    for (int t = 0; t < 9; t++) d.w[size_t(t) * size_t(d.c) + size_t(o)] *= s;
    d.b[size_t(o)] = (d.b[size_t(o)] - bn.mean[size_t(o)]) * s + bn.b[size_t(o)];
  }
}

void linear(const Linear& l, const float* x, size_t xs, float* y, size_t ys, size_t tokens) {
  const int n = l.out;
  bands((tokens + 3) / 4, 8, [&](size_t lo, size_t hi) {
    for (size_t q = lo; q < hi; q++) {
      const size_t t = q * 4;
      if (t + 4 <= tokens) {
        float* y0 = y + t * ys;
        float* y1 = y0 + ys;
        float* y2 = y1 + ys;
        float* y3 = y2 + ys;
        const float* x0 = x + t * xs;
        for (float* r : {y0, y1, y2, y3}) std::memcpy(r, l.b.data(), sizeof(float) * size_t(n));
        for (int k = 0; k < l.in; k++) {
          axpy4(y0, y1, y2, y3, l.wt.data() + size_t(k) * size_t(n), x0[k], x0[xs + size_t(k)],
                x0[2 * xs + size_t(k)], x0[3 * xs + size_t(k)], n);
        }
      } else {
        for (size_t r = t; r < tokens; r++) {
          float* yr = y + r * ys;
          const float* xr = x + r * xs;
          std::memcpy(yr, l.b.data(), sizeof(float) * size_t(n));
          for (int k = 0; k < l.in; k++) axpy(yr, l.wt.data() + size_t(k) * size_t(n), xr[k], n);
        }
      }
    }
  });
}

Map linear(const Linear& l, const Map& x) {
  Map y(x.h, x.w, l.out);
  linear(l, x.v.data(), size_t(x.c), y.v.data(), size_t(l.out), x.tokens());
  return y;
}

Map conv3(const Conv3& k, const Map& x) {
  const Map p = bordered(x);
  Map y(x.h, x.w, k.out);
  const int n = k.out, in = k.in;
  bands(size_t(x.h), 1, [&](size_t lo, size_t hi) {
    for (int yy = int(lo); yy < int(hi); yy++) {
      int xx = 0;
      for (; xx + 4 <= x.w; xx += 4) {
        float* y0 = y.at(yy, xx);
        float* y1 = y0 + n;
        float* y2 = y1 + n;
        float* y3 = y2 + n;
        for (float* r : {y0, y1, y2, y3}) std::memcpy(r, k.b.data(), sizeof(float) * size_t(n));
        for (int t = 0; t < 9; t++) {
          const float* s = p.at(yy + t / 3, xx + t % 3);
          const float* wt = k.wt.data() + size_t(t) * size_t(in) * size_t(n);
          for (int i = 0; i < in; i++) {
            axpy4(y0, y1, y2, y3, wt + size_t(i) * size_t(n), s[i], s[in + i], s[2 * in + i],
                  s[3 * in + i], n);
          }
        }
      }
      for (; xx < x.w; xx++) {
        float* yr = y.at(yy, xx);
        std::memcpy(yr, k.b.data(), sizeof(float) * size_t(n));
        for (int t = 0; t < 9; t++) {
          const float* s = p.at(yy + t / 3, xx + t % 3);
          const float* wt = k.wt.data() + size_t(t) * size_t(in) * size_t(n);
          for (int i = 0; i < in; i++) axpy(yr, wt + size_t(i) * size_t(n), s[i], n);
        }
      }
    }
  });
  return y;
}

Map depthwise3(const Depthwise3& k, const Map& x) {
  const Map p = bordered(x);
  Map y(x.h, x.w, k.c);
  const int c = k.c;
  bands(size_t(x.h), 4, [&](size_t lo, size_t hi) {
    for (int yy = int(lo); yy < int(hi); yy++) {
      for (int xx = 0; xx < x.w; xx++) {
        float* __restrict yr = y.at(yy, xx);
        std::memcpy(yr, k.b.data(), sizeof(float) * size_t(c));
        for (int t = 0; t < 9; t++) {
          const float* __restrict s = p.at(yy + t / 3, xx + t % 3);
          const float* __restrict w = k.w.data() + size_t(t) * size_t(c);
          for (int i = 0; i < c; i++) yr[i] += s[i] * w[i];
        }
      }
    }
  });
  return y;
}

void layer_norm(const Norm& n, const Map& x, Map& out) {
  if (&out != &x) out = Map(x.h, x.w, x.c);
  const int c = x.c;
  bands(x.tokens(), 64, [&](size_t lo, size_t hi) {
    for (size_t t = lo; t < hi; t++) {
      const float* xr = x.v.data() + t * size_t(c);
      float* yr = out.v.data() + t * size_t(c);
      double sum = 0;
      for (int i = 0; i < c; i++) sum += xr[i];
      const double mean = sum / c;
      double var = 0;
      for (int i = 0; i < c; i++) var += (xr[i] - mean) * (xr[i] - mean);
      const float inv = float(1.0 / std::sqrt(var / c + 1e-5));
      const float m = float(mean);
      for (int i = 0; i < c; i++) yr[i] = (xr[i] - m) * inv * n.w[size_t(i)] + n.b[size_t(i)];
    }
  });
}

Map pixel_shuffle2(const Map& x) {
  const int c = x.c / 4;
  Map y(x.h * 2, x.w * 2, c);
  for (int yy = 0; yy < x.h; yy++) {
    for (int xx = 0; xx < x.w; xx++) {
      const float* s = x.at(yy, xx);
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
          float* d = y.at(2 * yy + i, 2 * xx + j);
          for (int ch = 0; ch < c; ch++) d[ch] = s[ch * 4 + i * 2 + j];
        }
      }
    }
  }
  return y;
}

float exponential(float x) { return float(core::libm::exp(double(x))); }

float sigmoid(float x) { return float(1.0 / (1.0 + core::libm::exp(-double(x)))); }

float gelu(float x) {
  // Φ(x) = (1 + erf(x / √2)) / 2, erf by a rational fit in t = 1 / (1 + p z) times e^(-z²),
  // within 1.5e-7 of erf everywhere: below a float's step at the values a network passes.
  const double z = std::fabs(double(x)) * 0.70710678118654752440;
  const double t = 1.0 / (1.0 + 0.3275911 * z);
  const double poly =
      t *
      (0.254829592 + t * (-0.284496736 + t * (1.421413741 + t * (-1.453152027 + t * 1.061405429))));
  const double erf = 1.0 - poly * core::libm::exp(-z * z);
  return float(0.5 * double(x) * (1.0 + (x < 0 ? -erf : erf)));
}

void gelu(Map& x) {
  bands(x.v.size(), 4096, [&](size_t lo, size_t hi) {
    for (size_t i = lo; i < hi; i++) x.v[i] = gelu(x.v[i]);
  });
}

void softmax(float* row, size_t n) {
  float hi = row[0];
  for (size_t i = 1; i < n; i++) hi = std::max(hi, row[i]);
  double sum = 0;
  for (size_t i = 0; i < n; i++) {
    row[i] = exponential(row[i] - hi);
    sum += row[i];
  }
  const float inv = float(1.0 / sum);
  for (size_t i = 0; i < n; i++) row[i] *= inv;
}

}  // namespace ge::classicenhance::net
