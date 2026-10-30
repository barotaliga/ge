// The trained texture upscaler: its weights read and its run (upscaler.hpp).
//
// ge:summary The texture upscaler's load (shapes checked, biases worked out) and its forward run
// ge:keywords upscaler load forward window-attention channel-attention mask roll pad gate
// ge:skill classicenhance
// ge:test tests/classicenhance_upscaler_test.cpp
#include "classicenhance/upscaler.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#include "assets/sha256.hpp"
#include "core/json_read.hpp"
#include "core/parallel.hpp"
#include "core/prng.hpp"

namespace ge::classicenhance {

const UpscalerWeights kUpscalerWeights{
    "4x-PBRify_UpscalerV4.safetensors",
    "https://github.com/Kim2091/Kim2091-Models/releases/download/4x-PBRify_UpscalerV4/"
    "4x-PBRify_UpscalerV4.safetensors",
    "a00fa50116895a9b766aeb012edad0a14d66f3afda65539efc0f6fc28c991ba2"};

namespace {

struct Loader {
  const Weights& w;
  std::string& error;

  std::span<const float> get(const std::string& name, std::initializer_list<int64_t> shape) {
    if (!error.empty()) return {};
    const auto it = w.tensors.find(name);
    if (it == w.tensors.end() || it->second.dtype != "F32") {
      error = "the upscaler's weights have no float tensor '" + name +
              "'; they are not the network this runner reads (expected " +
              std::string(kUpscalerWeights.file) + ")";
      return {};
    }
    if (!std::equal(shape.begin(), shape.end(), it->second.shape.begin(), it->second.shape.end())) {
      std::string got;
      for (const int64_t d : it->second.shape) got += (got.empty() ? "" : "x") + std::to_string(d);
      error = "the upscaler's tensor '" + name + "' is " + got + ", not the shape this network has";
      return {};
    }
    return it->second.data;
  }
  bool has(const std::string& name) const { return w.tensors.count(name) > 0; }

  net::Norm norm(const std::string& p, int c) {
    const auto wt = get(p + ".weight", {c}), b = get(p + ".bias", {c});
    return {{wt.begin(), wt.end()}, {b.begin(), b.end()}};
  }
  net::Linear linear(const std::string& p, int out, int in) {
    const auto wt = get(p + ".weight", {out, in}), b = get(p + ".bias", {out});
    return error.empty() ? net::to_linear(wt, b, out, in) : net::Linear{};
  }
  net::Linear pointwise(const std::string& p, int out, int in) {  // a 1 × 1 convolution
    const auto wt = get(p + ".weight", {out, in, 1, 1}), b = get(p + ".bias", {out});
    return error.empty() ? net::to_linear(wt, b, out, in) : net::Linear{};
  }
  net::Conv3 conv3(const std::string& p, int out, int in) {
    const auto wt = get(p + ".weight", {out, in, 3, 3}), b = get(p + ".bias", {out});
    return error.empty() ? net::to_conv3(wt, b, out, in) : net::Conv3{};
  }
  net::Depthwise3 depthwise(const std::string& p, int c) {
    const auto wt = get(p + ".weight", {c, 1, 3, 3}), b = get(p + ".bias", {c});
    return error.empty() ? net::to_depthwise3(wt, b, c) : net::Depthwise3{};
  }
  net::BatchNorm batch_norm(const std::string& p, int c) {
    const auto wt = get(p + ".weight", {c}), b = get(p + ".bias", {c});
    const auto m = get(p + ".running_mean", {c}), v = get(p + ".running_var", {c});
    return {
        {wt.begin(), wt.end()}, {b.begin(), b.end()}, {m.begin(), m.end()}, {v.begin(), v.end()}};
  }
};

// One of a small MLP's layer norms (eps 1e-5) then ReLU, in place.
void norm_relu(std::vector<float>& x, std::span<const float> w, std::span<const float> b) {
  const size_t n = x.size();
  double sum = 0, var = 0;
  for (const float v : x) sum += v;
  const double mean = sum / double(n);
  for (const float v : x) var += (v - mean) * (v - mean);
  const double inv = 1.0 / std::sqrt(var / double(n) + 1e-5);
  for (size_t i = 0; i < n; i++) x[i] = std::max(0.0f, float((x[i] - mean) * inv) * w[i] + b[i]);
}

std::vector<float> dense(const std::vector<float>& x, std::span<const float> w,
                         std::span<const float> b, size_t out) {
  std::vector<float> y(out);
  for (size_t o = 0; o < out; o++) {
    float s = b[o];
    for (size_t i = 0; i < x.size(); i++) s += w[o * x.size() + i] * x[i];
    y[o] = s;
  }
  return y;
}

// The score bias of a window `hs` tall and `ws` wide: an MLP of each offset between two of its
// tokens gives one bias a head; [head][query][key].
std::vector<float> position_bias(Loader& l, const std::string& p, int hs, int ws, int heads) {
  const auto proj_w = l.w.tensors.find(p + ".pos_proj.weight");
  if (proj_w == l.w.tensors.end() || proj_w->second.shape.size() != 2) {
    l.get(p + ".pos_proj.weight", {0, 2});
    return {};
  }
  const int dim = int(proj_w->second.shape[0]);
  const auto pw = l.get(p + ".pos_proj.weight", {dim, 2}), pb = l.get(p + ".pos_proj.bias", {dim});
  std::array<std::span<const float>, 3> nw, nb, lw, lb;
  for (int k = 0; k < 3; k++) {
    const std::string s = p + ".pos" + std::to_string(k + 1);
    const int out = k == 2 ? heads : dim;
    nw[size_t(k)] = l.get(s + ".0.weight", {dim});
    nb[size_t(k)] = l.get(s + ".0.bias", {dim});
    lw[size_t(k)] = l.get(s + ".2.weight", {out, dim});
    lb[size_t(k)] = l.get(s + ".2.bias", {out});
  }
  if (!l.error.empty()) return {};
  const int oh = 2 * hs - 1, ow = 2 * ws - 1;
  std::vector<float> table(size_t(oh) * size_t(ow) * size_t(heads));
  for (int a = 0; a < oh; a++) {
    for (int c = 0; c < ow; c++) {
      std::vector<float> x = dense({float(a - (hs - 1)), float(c - (ws - 1))}, pw, pb, size_t(dim));
      for (int k = 0; k < 3; k++) {
        norm_relu(x, nw[size_t(k)], nb[size_t(k)]);
        x = dense(x, lw[size_t(k)], lb[size_t(k)], size_t(k == 2 ? heads : dim));
      }
      std::copy(x.begin(), x.end(),
                table.begin() + (size_t(a) * size_t(ow) + size_t(c)) * size_t(heads));
    }
  }
  const int n = hs * ws;
  std::vector<float> bias(size_t(heads) * size_t(n) * size_t(n));
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      const int r = (i / ws - j / ws + hs - 1) * ow + (i % ws - j % ws + ws - 1);
      for (int h = 0; h < heads; h++) {
        bias[(size_t(h) * size_t(n) + size_t(i)) * size_t(n) + size_t(j)] =
            table[size_t(r) * size_t(heads) + size_t(h)];
      }
    }
  }
  return bias;
}

bool meta_int(const core::FlatJson& m, const std::string& key, int& out) {
  const auto it = m.find(key);
  if (it == m.end() || it->second.kind != core::JsonValue::Kind::number) return false;
  out = int(it->second.number);
  return true;
}

}  // namespace

bool load_upscaler(const Weights& weights, Upscaler& out, std::string& error) {
  out = {};
  error.clear();
  const auto meta = weights.metadata.find("metadata");
  core::FlatJson m;
  std::string why;
  if (meta == weights.metadata.end() || !core::read_flat(meta->second, m, why, true)) {
    error = "the upscaler's weights carry no readable network metadata (" + why +
            "); they are not the network this runner reads";
    return false;
  }
  auto text = [&](const std::string& k) { return m.count(k) ? m.at(k).text : std::string(); };
  int in_chans = 0, range = 0;
  if (!meta_int(m, "embed_dim", out.embed) || !meta_int(m, "upscale", out.scale) ||
      !meta_int(m, "split_size.0", out.split[0]) || !meta_int(m, "split_size.1", out.split[1]) ||
      !meta_int(m, "in_chans", in_chans) || !meta_int(m, "img_range", range) || in_chans != 3 ||
      range != 1 || text("upsampler") != "pixelshuffle" || text("resi_connection") != "1conv" ||
      out.scale < 2 || (out.scale & (out.scale - 1)) != 0) {
    error =
        "the upscaler's metadata names a form this runner does not have (it runs 3 channels, "
        "range 1, a 1conv residual and pixel-shuffle upsampling by a power of 2)";
    return false;
  }
  const int c = out.embed;
  out.mean = {0.4488f, 0.4371f, 0.4040f};
  Loader l{weights, error};
  out.first = l.conv3("conv_first", c, 3);
  out.before = l.norm("before_RG.1", c);
  const auto expansion = m.find("expansion_factor");
  const int hidden = expansion == m.end() ? 0 : int(double(c) * expansion->second.number);
  if (hidden < 2 || hidden % 2) {
    error = "the upscaler's metadata gives no expansion factor that makes an even hidden width";
    return false;
  }
  for (int g = 0;; g++) {
    int depth = 0, heads = 0;
    if (!meta_int(m, "depth." + std::to_string(g), depth)) break;
    if (!meta_int(m, "num_heads." + std::to_string(g), heads) || heads % 2 || c % heads) {
      error = "the upscaler's metadata gives group " + std::to_string(g) +
              " a head count that does not divide its channels in two halves";
      return false;
    }
    out.heads = heads;
    UpscalerGroup& group = out.groups.emplace_back();
    for (int b = 0; b < depth; b++) {
      const std::string p = "layers." + std::to_string(g) + ".blocks." + std::to_string(b);
      UpscalerBlock& k = group.blocks.emplace_back();
      k.spatial = b % 2 == 0;
      k.shifted =
          k.spatial && ((g % 2 == 0 && b > 0 && (b - 2) % 4 == 0) || (g % 2 != 0 && b % 4 == 0));
      k.norm1 = l.norm(p + ".norm1", c);
      k.norm2 = l.norm(p + ".norm2", c);
      k.qkv = l.linear(p + ".attn.qkv", 3 * c, c);
      k.proj = l.linear(p + ".attn.proj", c, c);
      if (k.spatial) {
        for (int br = 0; br < 2; br++) {
          const int hs = br == 0 ? out.split[0] : out.split[1];
          const int ws = br == 0 ? out.split[1] : out.split[0];
          k.bias[size_t(br)] =
              position_bias(l, p + ".attn.attns." + std::to_string(br) + ".pos", hs, ws, heads / 2);
        }
      } else {
        const auto t = l.get(p + ".attn.temperature", {heads, 1, 1});
        k.temperature.assign(t.begin(), t.end());
      }
      k.dwconv = l.depthwise(p + ".attn.dwconv.0", c);
      const auto bn = l.batch_norm(p + ".attn.dwconv.1", c);
      k.channel1 = l.pointwise(p + ".attn.channel_interaction.1", c / 8, c);
      const auto cbn = l.batch_norm(p + ".attn.channel_interaction.2", c / 8);
      k.channel2 = l.pointwise(p + ".attn.channel_interaction.4", c, c / 8);
      k.spatial1 = l.pointwise(p + ".attn.spatial_interaction.0", c / 16, c);
      const auto sbn = l.batch_norm(p + ".attn.spatial_interaction.1", c / 16);
      k.spatial2 = l.pointwise(p + ".attn.spatial_interaction.3", 1, c / 16);
      k.fc1 = l.linear(p + ".ffn.fc1", hidden, c);
      k.gate_norm = l.norm(p + ".ffn.sg.norm", hidden / 2);
      k.gate_conv = l.depthwise(p + ".ffn.sg.conv", hidden / 2);
      k.fc2 = l.linear(p + ".ffn.fc2", c, hidden / 2);
      if (!error.empty()) return false;
      net::fold(k.dwconv, bn);
      net::fold(k.channel1, cbn);
      net::fold(k.spatial1, sbn);
    }
    group.conv = l.conv3("layers." + std::to_string(g) + ".conv", c, c);
  }
  if (out.groups.empty() && error.empty()) error = "the upscaler's metadata lists no groups";
  out.norm = l.norm("norm", c);
  out.after_body = l.conv3("conv_after_body", c, c);
  out.before_up = l.conv3("conv_before_upsample.0", 64, c);
  for (int s = out.scale, i = 0; s > 1; s /= 2, i += 2) {
    out.up.push_back(l.conv3("upsample." + std::to_string(i), 256, 64));
  }
  out.last = l.conv3("conv_last", 3, 64);
  return error.empty();
}

bool open_upscaler(const std::filesystem::path& file, Upscaler& out, bool* known,
                   std::string& error) {
  std::ifstream f(file, std::ios::binary);
  if (!f) {
    error = "cannot open the upscaler's weights '" + file.string() +
            "'; fetch them again (the classicenhance tool does on a run)";
    return false;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
  Weights w;
  if (!read_safetensors(bytes, w, error)) return false;
  if (!load_upscaler(w, out, error)) return false;
  out.digest = assets::sha256_hex(bytes);
  if (known != nullptr) *known = out.digest == kUpscalerWeights.sha256;
  return true;
}

namespace {

net::Map channels(const net::Map& x, int from, int count) {
  net::Map y(x.h, x.w, count);
  for (size_t t = 0; t < x.tokens(); t++) {
    std::memcpy(y.v.data() + t * size_t(count), x.v.data() + t * size_t(x.c) + size_t(from),
                sizeof(float) * size_t(count));
  }
  return y;
}

void add(net::Map& x, const net::Map& d) {
  for (size_t i = 0; i < x.v.size(); i++) x.v[i] += d.v[i];
}

// The channel map: the mean of `x` over the map through two 1 × 1 layers, as a sigmoid gate.
std::vector<float> channel_gate(const UpscalerBlock& k, const net::Map& x) {
  std::vector<double> sum(size_t(x.c), 0.0);
  for (size_t t = 0; t < x.tokens(); t++) {
    for (int i = 0; i < x.c; i++) sum[size_t(i)] += x.v[t * size_t(x.c) + size_t(i)];
  }
  std::vector<float> mean(size_t(x.c));
  for (int i = 0; i < x.c; i++) mean[size_t(i)] = float(sum[size_t(i)] / double(x.tokens()));
  std::vector<float> mid(size_t(k.channel1.out)), gate(size_t(x.c));
  net::linear(k.channel1, mean.data(), 0, mid.data(), 0, 1);
  for (float& v : mid) v = net::gelu(v);
  net::linear(k.channel2, mid.data(), 0, gate.data(), 0, 1);
  for (float& v : gate) v = net::sigmoid(v);
  return gate;
}

// The spatial map: each token of `x` through two 1 × 1 layers, as a sigmoid gate.
std::vector<float> spatial_gate(const UpscalerBlock& k, const net::Map& x) {
  net::Map mid = net::linear(k.spatial1, x);
  net::gelu(mid);
  const net::Map g = net::linear(k.spatial2, mid);
  std::vector<float> gate(g.v.size());
  for (size_t i = 0; i < gate.size(); i++) gate[i] = net::sigmoid(g.v[i]);
  return gate;
}

// One half's window attention, written into `out`'s channels [half * c/2, (half + 1) * c/2).
void window_attention(const Upscaler& u, const UpscalerBlock& k, const net::Map& qkv, int half,
                      net::Map& out) {
  const int c = u.embed, heads = u.heads / 2, hd = c / 2 / heads;
  const int hs = half == 0 ? u.split[0] : u.split[1];
  const int ws = half == 0 ? u.split[1] : u.split[0];
  const int longest = std::max(u.split[0], u.split[1]);
  const int ph = (qkv.h + longest - 1) / longest * longest;
  const int pw = (qkv.w + longest - 1) / longest * longest;
  // The roll, and the regions a shifted window's tokens come from (rows and columns each split
  // at a window and at half a window from the far side).
  const int sy = k.shifted ? hs / 2 : 0, sx = k.shifted ? ws / 2 : 0;
  auto region = [&](int y, int x) {
    const int ry = y < ph - hs ? 0 : (y < ph - sy ? 1 : 2);
    const int rx = x < pw - ws ? 0 : (x < pw - sx ? 1 : 2);
    return ry * 3 + rx;
  };
  const int n = hs * ws, wx = pw / ws, windows = (ph / hs) * wx;
  const float scale = float(1.0 / std::sqrt(double(hd)));
  core::parallel_bands(size_t(windows) * size_t(heads), [&](size_t lo, size_t hi) {
    std::vector<float> q(size_t(n) * size_t(hd)), kt(size_t(hd) * size_t(n)),
        v(size_t(n) * size_t(hd));
    std::vector<float> row(static_cast<size_t>(n)), acc(static_cast<size_t>(hd));
    std::vector<int> src(static_cast<size_t>(n)), rid(static_cast<size_t>(n));
    for (size_t task = lo; task < hi; task++) {
      const int win = int(task) / heads, h = int(task) % heads;
      const int y0 = win / wx * hs, x0 = win % wx * ws;
      const int ch = half * c / 2 + h * hd;
      for (int i = 0; i < n; i++) {
        const int y = y0 + i / ws, x = x0 + i % ws;
        const int oy = (y + sy) % ph, ox = (x + sx) % pw;
        src[size_t(i)] = oy < qkv.h && ox < qkv.w ? oy * qkv.w + ox : -1;
        rid[size_t(i)] = k.shifted ? region(y, x) : 0;
        const float* t =
            src[size_t(i)] < 0 ? nullptr : qkv.v.data() + size_t(src[size_t(i)]) * size_t(qkv.c);
        for (int d = 0; d < hd; d++) {
          q[size_t(i) * size_t(hd) + size_t(d)] = t ? t[ch + d] * scale : 0.0f;
          kt[size_t(d) * size_t(n) + size_t(i)] = t ? t[c + ch + d] : 0.0f;
          v[size_t(i) * size_t(hd) + size_t(d)] = t ? t[2 * c + ch + d] : 0.0f;
        }
      }
      const float* bias = k.bias[size_t(half)].data() + size_t(h) * size_t(n) * size_t(n);
      for (int i = 0; i < n; i++) {
        if (src[size_t(i)] < 0) continue;  // a padding token: cropped away
        std::memcpy(row.data(), bias + size_t(i) * size_t(n), sizeof(float) * size_t(n));
        float* __restrict r = row.data();
        for (int d = 0; d < hd; d++) {
          const float a = q[size_t(i) * size_t(hd) + size_t(d)];
          const float* __restrict kr = kt.data() + size_t(d) * size_t(n);
          for (int j = 0; j < n; j++) r[j] += a * kr[j];
        }
        if (k.shifted) {
          for (int j = 0; j < n; j++) {
            if (rid[size_t(j)] != rid[size_t(i)]) r[j] -= 100.0f;
          }
        }
        net::softmax(r, size_t(n));
        std::fill(acc.begin(), acc.end(), 0.0f);
        for (int j = 0; j < n; j++) {
          const float p = r[j];
          const float* __restrict vr = v.data() + size_t(j) * size_t(hd);
          for (int d = 0; d < hd; d++) acc[size_t(d)] += p * vr[d];
        }
        std::memcpy(out.v.data() + size_t(src[size_t(i)]) * size_t(c) + size_t(ch), acc.data(),
                    sizeof(float) * size_t(hd));
      }
    }
  });
}

// Every head's channels attending to each other over the whole map.
net::Map channel_attention(const Upscaler& u, const UpscalerBlock& k, const net::Map& qkv) {
  const int c = u.embed, heads = u.heads, hd = c / heads;
  const size_t tokens = qkv.tokens();
  std::vector<double> nq(size_t(c), 0.0), nk(size_t(c), 0.0);
  for (size_t t = 0; t < tokens; t++) {
    const float* r = qkv.v.data() + t * size_t(qkv.c);
    for (int i = 0; i < c; i++) {
      nq[size_t(i)] += double(r[i]) * r[i];
      nk[size_t(i)] += double(r[c + i]) * r[c + i];
    }
  }
  for (int i = 0; i < c; i++) {
    nq[size_t(i)] = 1.0 / std::max(std::sqrt(nq[size_t(i)]), 1e-12);
    nk[size_t(i)] = 1.0 / std::max(std::sqrt(nk[size_t(i)]), 1e-12);
  }
  // attn[h][d1][d2], then softmax along d2.
  std::vector<float> attn(size_t(heads) * size_t(hd) * size_t(hd));
  core::parallel_bands(size_t(heads) * size_t(hd), [&](size_t lo, size_t hi) {
    std::vector<double> s(static_cast<size_t>(hd));
    for (size_t task = lo; task < hi; task++) {
      const int h = int(task) / hd, d1 = int(task) % hd;
      const int a = h * hd + d1;
      std::fill(s.begin(), s.end(), 0.0);
      for (size_t t = 0; t < tokens; t++) {
        const float* r = qkv.v.data() + t * size_t(qkv.c);
        const double qa = r[a] * nq[size_t(a)];
        for (int d2 = 0; d2 < hd; d2++)
          s[size_t(d2)] += qa * (r[c + h * hd + d2] * nk[size_t(h) * size_t(hd) + size_t(d2)]);
      }
      float* row = attn.data() + task * size_t(hd);
      for (int d2 = 0; d2 < hd; d2++) row[d2] = float(s[size_t(d2)]) * k.temperature[size_t(h)];
      net::softmax(row, size_t(hd));
    }
  });
  net::Map out(qkv.h, qkv.w, c);
  core::parallel_bands(tokens, [&](size_t lo, size_t hi) {
    for (size_t t = lo; t < hi; t++) {
      const float* r = qkv.v.data() + t * size_t(qkv.c) + 2 * size_t(c);
      float* o = out.v.data() + t * size_t(c);
      for (int h = 0; h < heads; h++) {
        for (int d1 = 0; d1 < hd; d1++) {
          const float* row = attn.data() + (size_t(h) * size_t(hd) + size_t(d1)) * size_t(hd);
          float s = 0;
          for (int d2 = 0; d2 < hd; d2++) s += row[d2] * r[h * hd + d2];
          o[h * hd + d1] = s;
        }
      }
    }
  });
  return out;
}

net::Map attention(const Upscaler& u, const UpscalerBlock& k, const net::Map& x) {
  const int c = u.embed;
  const net::Map qkv = net::linear(k.qkv, x);
  net::Map conv = net::depthwise3(k.dwconv, channels(qkv, 2 * c, c));
  net::gelu(conv);
  net::Map att;
  std::vector<float> cgate, sgate;
  if (k.spatial) {
    att = net::Map(x.h, x.w, c);
    window_attention(u, k, qkv, 0, att);
    window_attention(u, k, qkv, 1, att);
    cgate = channel_gate(k, conv);  // gates the attention
    sgate = spatial_gate(k, att);   // gates the convolution
  } else {
    att = channel_attention(u, k, qkv);
    sgate = spatial_gate(k, conv);  // gates the attention
    cgate = channel_gate(k, att);   // gates the convolution
  }
  net::Map mixed(x.h, x.w, c);
  for (size_t t = 0; t < x.tokens(); t++) {
    const float* a = att.v.data() + t * size_t(c);
    const float* v = conv.v.data() + t * size_t(c);
    float* o = mixed.v.data() + t * size_t(c);
    for (int i = 0; i < c; i++) {
      o[i] = k.spatial ? a[i] * cgate[size_t(i)] + sgate[t] * v[i]
                       : a[i] * sgate[t] + v[i] * cgate[size_t(i)];
    }
  }
  return net::linear(k.proj, mixed);
}

net::Map feed_forward(const UpscalerBlock& k, const net::Map& x) {
  net::Map h = net::linear(k.fc1, x);
  net::gelu(h);
  const int half = h.c / 2;
  net::Map second = channels(h, half, half);
  net::layer_norm(k.gate_norm, second, second);
  const net::Map gate = net::depthwise3(k.gate_conv, second);
  net::Map g(x.h, x.w, half);
  for (size_t t = 0; t < x.tokens(); t++) {
    for (int i = 0; i < half; i++) {
      g.v[t * size_t(half) + size_t(i)] =
          h.v[t * size_t(h.c) + size_t(i)] * gate.v[t * size_t(half) + size_t(i)];
    }
  }
  return net::linear(k.fc2, g);
}

}  // namespace

net::Map upscale(const Upscaler& u, const net::Map& rgb) {
  net::Map x = rgb;
  for (size_t t = 0; t < x.tokens(); t++) {
    for (int i = 0; i < 3; i++) x.v[t * 3 + size_t(i)] -= u.mean[size_t(i)];
  }
  const net::Map first = net::conv3(u.first, x);
  net::Map body;
  net::layer_norm(u.before, first, body);
  net::Map normed;
  for (const UpscalerGroup& g : u.groups) {
    const net::Map in = body;
    for (const UpscalerBlock& k : g.blocks) {
      net::layer_norm(k.norm1, body, normed);
      add(body, attention(u, k, normed));
      net::layer_norm(k.norm2, body, normed);
      add(body, feed_forward(k, normed));
    }
    body = net::conv3(g.conv, body);
    add(body, in);
  }
  net::layer_norm(u.norm, body, body);
  net::Map y = net::conv3(u.after_body, body);
  add(y, first);
  y = net::conv3(u.before_up, y);
  for (float& v : y.v) v = v < 0 ? v * 0.01f : v;
  for (const net::Conv3& up : u.up) y = net::pixel_shuffle2(net::conv3(up, y));
  y = net::conv3(u.last, y);
  for (size_t t = 0; t < y.tokens(); t++) {
    for (int i = 0; i < 3; i++) y.v[t * 3 + size_t(i)] += u.mean[size_t(i)];
  }
  return y;
}

Weights seeded_upscaler_weights(uint32_t seed) {
  constexpr int c = 32, heads = 2, hidden = 64, groups = 2, depth = 2;
  Weights w;
  w.metadata["metadata"] =
      "{\"img_size\": 64, \"in_chans\": 3, \"embed_dim\": 32, \"split_size\": [2, 4], "
      "\"depth\": [2, 2], \"num_heads\": [2, 2], \"expansion_factor\": 2, \"upscale\": 4, "
      "\"img_range\": 1.0, \"resi_connection\": \"1conv\", \"upsampler\": \"pixelshuffle\"}";
  core::Rng rng = core::make_rng(seed);
  // Weights uniform in ±1/√fan-in, so every layer passes on a signal of about its input's size.
  auto put = [&](const std::string& name, std::vector<int64_t> shape, double fan_in,
                 double centre) {
    Tensor& t = w.tensors[name];
    t.dtype = "F32";
    t.shape = std::move(shape);
    t.data.resize(size_t(t.count()));
    const double a = 1.0 / std::sqrt(fan_in);
    for (float& v : t.data) v = float(centre + core::range(rng, -a, a));
  };
  auto norm = [&](const std::string& p, int n) {
    put(p + ".weight", {n}, 100, 1.0);
    put(p + ".bias", {n}, 100, 0.0);
  };
  auto linear = [&](const std::string& p, int out, int in) {
    put(p + ".weight", {out, in}, in, 0.0);
    put(p + ".bias", {out}, in, 0.0);
  };
  auto conv = [&](const std::string& p, int out, int in, int k) {
    put(p + ".weight",
        k == 0 ? std::vector<int64_t>{out, 1, 3, 3} : std::vector<int64_t>{out, in, k, k},
        k == 0 ? 9.0 : double(in * k * k), 0.0);
    put(p + ".bias", {out}, k == 0 ? 9.0 : double(in * k * k), 0.0);
  };
  auto batch_norm = [&](const std::string& p, int n) {
    norm(p, n);
    put(p + ".running_mean", {n}, 100, 0.0);
    put(p + ".running_var", {n}, 4, 1.0);
  };
  conv("conv_first", c, 3, 3);
  norm("before_RG.1", c);
  for (int g = 0; g < groups; g++) {
    for (int b = 0; b < depth; b++) {
      const std::string p = "layers." + std::to_string(g) + ".blocks." + std::to_string(b);
      norm(p + ".norm1", c);
      norm(p + ".norm2", c);
      linear(p + ".attn.qkv", 3 * c, c);
      linear(p + ".attn.proj", c, c);
      if (b % 2 == 0) {
        for (int br = 0; br < 2; br++) {
          const std::string q = p + ".attn.attns." + std::to_string(br) + ".pos";
          const int dim = c / 2 / 4 / 4;
          linear(q + ".pos_proj", dim, 2);
          for (int k = 1; k <= 3; k++) {
            norm(q + ".pos" + std::to_string(k) + ".0", dim);
            linear(q + ".pos" + std::to_string(k) + ".2", k == 3 ? heads / 2 : dim, dim);
          }
        }
      } else {
        put(p + ".attn.temperature", {heads, 1, 1}, 4, 1.0);
      }
      conv(p + ".attn.dwconv.0", c, c, 0);
      batch_norm(p + ".attn.dwconv.1", c);
      conv(p + ".attn.channel_interaction.1", c / 8, c, 1);
      batch_norm(p + ".attn.channel_interaction.2", c / 8);
      conv(p + ".attn.channel_interaction.4", c, c / 8, 1);
      conv(p + ".attn.spatial_interaction.0", c / 16, c, 1);
      batch_norm(p + ".attn.spatial_interaction.1", c / 16);
      conv(p + ".attn.spatial_interaction.3", 1, c / 16, 1);
      linear(p + ".ffn.fc1", hidden, c);
      norm(p + ".ffn.sg.norm", hidden / 2);
      conv(p + ".ffn.sg.conv", hidden / 2, hidden / 2, 0);
      linear(p + ".ffn.fc2", c, hidden / 2);
    }
    conv("layers." + std::to_string(g) + ".conv", c, c, 3);
  }
  norm("norm", c);
  conv("conv_after_body", c, c, 3);
  conv("conv_before_upsample.0", 64, c, 3);
  conv("upsample.0", 256, 64, 3);
  conv("upsample.2", 256, 64, 3);
  conv("conv_last", 3, 64, 3);
  return w;
}

}  // namespace ge::classicenhance
