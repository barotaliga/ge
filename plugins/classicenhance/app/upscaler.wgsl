// THE TRAINED TEXTURE UPSCALER ON THE GPU (app/upscaler_gpu.hpp): the kernels its run is made
// of, one entry point each, all on one bind group layout. Every map is a flat f32 array,
// token-major (row r's channel c at r * stride + offset + c), as on the CPU (net.hpp).
//
//   gemm         y = x · W + b over rows, tiled 64 × 64 in workgroup memory; or, with CONV, the
//                3 × 3 convolution as the same product over each token's nine neighbours (zero
//                outside the map); the epilogue applies GELU or a leaky ReLU and writes or adds.
//   depthwise    one 3 × 3 kernel a channel.
//   layer_norm   each row normalised over its channels.
//   window       one window's attention for one head: every query token a thread, the keys in
//                chunks of 32 through workgroup memory, softmax online (running max and sum).
//   column_sum   per column, the sum (or sum of squares) over the rows, scaled.
//   channel_scores, softmax_rows, channel_apply
//                the channel attention: every channel's values over the map against every
//                other's of its head, the scores' softmax, and the values mixed by them.
//   gate_mix     the attention and the convolution gating each other through sigmoids.
//   multiply     the feed-forward's two halves multiplied.
//   shuffle      the pixel shuffle by 2.
//
// The sums run in another order than the CPU's, so the bits differ from upscale()'s: within
// 1e-4 of it on the tests' network, far below the 8-bit step.

struct Params {
  rows: u32,   // rows (tokens) the kernel covers
  k: u32,      // a product's inner length; a head's width
  n: u32,      // outputs a row; heads
  flags: u32,  // GELU 1, ADD 2, LEAKY 4, CONV 8; window: SHIFTED 1; column_sum: SQUARE 1
  xs: u32,     // the input's row stride
  xo: u32,     // the input's first channel
  ys: u32,     // the output's row stride
  yo: u32,     // the output's first channel
  h: u32,      // the map's height and width
  w: u32,
  cin: u32,    // a convolution's input channels
  a0: u32,     // window: the padded height
  a1: u32,     // window: the padded width
  a2: u32,     // window: the window's height | width << 16
  a3: u32,     // window: the roll down | across << 16
  a4: u32,
  f0: f32,     // window: the query scale; column_sum: the scale
  f1: f32,
  f2: f32,
  f3: f32,
}

@group(0) @binding(0) var<uniform> p: Params;
@group(0) @binding(1) var<storage, read> src: array<f32>;
@group(0) @binding(2) var<storage, read> wts: array<f32>;
@group(0) @binding(3) var<storage, read> bias: array<f32>;
@group(0) @binding(4) var<storage, read_write> dst: array<f32>;
@group(0) @binding(5) var<storage, read> aux: array<f32>;

const GELU = 1u;
const ADD = 2u;
const LEAKY = 4u;
const CONV = 8u;

// x Φ(x), erf by the same rational fit as the CPU's (within 1.5e-7).
fn gelu(x: f32) -> f32 {
  let z = abs(x) * 0.70710678;
  let t = 1.0 / (1.0 + 0.3275911 * z);
  let poly = t * (0.254829592 + t * (-0.284496736 + t * (1.421413741 + t * (-1.453152027 + t * 1.061405429))));
  let e = 1.0 - poly * exp(-z * z);
  return 0.5 * x * (1.0 + select(e, -e, x < 0.0));
}

fn sigmoid(x: f32) -> f32 {
  return 1.0 / (1.0 + exp(-x));
}

// A flat index over a 1-D job dispatched as a 2-D grid of 256-wide workgroups.
fn flat(wg: vec3u, nw: vec3u, li: vec3u) -> u32 {
  return (wg.y * nw.x + wg.x) * 256u + li.x;
}

// ---- gemm ----

var<workgroup> tile_a: array<f32, 1024>;  // [64 rows][16 k]
var<workgroup> tile_b: array<f32, 1024>;  // [16 k][64 columns]

fn a_at(r: u32, kk: u32) -> f32 {
  if (r >= p.rows || kk >= p.k) {
    return 0.0;
  }
  if ((p.flags & CONV) == 0u) {
    return src[r * p.xs + p.xo + kk];
  }
  let tap = kk / p.cin;
  let ci = kk - tap * p.cin;
  let y = i32(r / p.w) + i32(tap / 3u) - 1;
  let x = i32(r % p.w) + i32(tap % 3u) - 1;
  if (y < 0 || x < 0 || y >= i32(p.h) || x >= i32(p.w)) {
    return 0.0;
  }
  return src[(u32(y) * p.w + u32(x)) * p.xs + p.xo + ci];
}

@compute @workgroup_size(16, 16)
fn gemm(@builtin(workgroup_id) wg: vec3u, @builtin(local_invocation_id) li: vec3u) {
  let row0 = wg.y * 64u;
  let col0 = wg.x * 64u;
  let tid = li.y * 16u + li.x;
  var acc: array<f32, 16>;
  for (var k0 = 0u; k0 < p.k; k0 += 16u) {
    for (var i = 0u; i < 4u; i++) {
      let e = tid + i * 256u;
      tile_a[e] = a_at(row0 + e / 16u, k0 + e % 16u);
      let kb = k0 + e / 64u;
      let c = col0 + e % 64u;
      var b = 0.0;
      if (kb < p.k && c < p.n) {
        b = wts[kb * p.n + c];
      }
      tile_b[e] = b;
    }
    workgroupBarrier();
    for (var kk = 0u; kk < 16u; kk++) {
      var av: array<f32, 4>;
      var bv: array<f32, 4>;
      for (var i = 0u; i < 4u; i++) {
        av[i] = tile_a[(li.y + 16u * i) * 16u + kk];
        bv[i] = tile_b[kk * 64u + li.x + 16u * i];
      }
      for (var i = 0u; i < 4u; i++) {
        for (var j = 0u; j < 4u; j++) {
          acc[i * 4u + j] += av[i] * bv[j];
        }
      }
    }
    workgroupBarrier();
  }
  for (var i = 0u; i < 4u; i++) {
    let r = row0 + li.y + 16u * i;
    if (r >= p.rows) {
      continue;
    }
    for (var j = 0u; j < 4u; j++) {
      let c = col0 + li.x + 16u * j;
      if (c >= p.n) {
        continue;
      }
      var v = acc[i * 4u + j] + bias[c];
      if ((p.flags & GELU) != 0u) {
        v = gelu(v);
      }
      if ((p.flags & LEAKY) != 0u && v < 0.0) {
        v = v * 0.01;
      }
      let at = r * p.ys + p.yo + c;
      if ((p.flags & ADD) != 0u) {
        dst[at] = dst[at] + v;
      } else {
        dst[at] = v;
      }
    }
  }
}

// ---- depthwise: rows = tokens, n = channels; wts [tap][n] ----

@compute @workgroup_size(256)
fn depthwise(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
             @builtin(local_invocation_id) li: vec3u) {
  let e = flat(wg, nw, li);
  if (e >= p.rows * p.n) {
    return;
  }
  let r = e / p.n;
  let c = e % p.n;
  let y0 = i32(r / p.w);
  let x0 = i32(r % p.w);
  var v = bias[c];
  for (var t = 0u; t < 9u; t++) {
    let y = y0 + i32(t / 3u) - 1;
    let x = x0 + i32(t % 3u) - 1;
    if (y >= 0 && x >= 0 && y < i32(p.h) && x < i32(p.w)) {
      v += src[(u32(y) * p.w + u32(x)) * p.xs + p.xo + c] * wts[t * p.n + c];
    }
  }
  if ((p.flags & GELU) != 0u) {
    v = gelu(v);
  }
  dst[r * p.ys + p.yo + c] = v;
}

// ---- layer_norm: rows = tokens, n = channels; wts the scale, bias the shift ----

@compute @workgroup_size(256)
fn layer_norm(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
              @builtin(local_invocation_id) li: vec3u) {
  let r = flat(wg, nw, li);
  if (r >= p.rows) {
    return;
  }
  let base = r * p.xs + p.xo;
  var sum = 0.0;
  for (var c = 0u; c < p.n; c++) {
    sum += src[base + c];
  }
  let mean = sum / f32(p.n);
  var var_sum = 0.0;
  for (var c = 0u; c < p.n; c++) {
    let d = src[base + c] - mean;
    var_sum += d * d;
  }
  let inv = 1.0 / sqrt(var_sum / f32(p.n) + 1e-5);
  for (var c = 0u; c < p.n; c++) {
    dst[r * p.ys + p.yo + c] = (src[base + c] - mean) * inv * wts[c] + bias[c];
  }
}

// ---- window: workgroup (window, head); src the qkv map (q at xo, k at xo + k-offset ...) ----
// rows: tokens a window; k: a head's width; n: unused; xs: qkv's stride; xo: this half's first
// channel within q; cin: the embed width (k = q + cin, v = q + 2 cin); ys: the output's stride;
// wts: this half's bias [head][query][key].

var<workgroup> keys: array<f32, 1024>;  // [32 keys][32]
var<workgroup> vals: array<f32, 1024>;
var<workgroup> key_region: array<u32, 32>;

fn window_token(win: u32, i: u32, shifted: bool) -> vec2i {
  let hs = p.a2 & 0xffffu;
  let ws = p.a2 >> 16u;
  let per_row = p.a1 / ws;
  let y = (win / per_row) * hs + i / ws;
  let x = (win % per_row) * ws + i % ws;
  var sy = 0u;
  var sx = 0u;
  if (shifted) {
    sy = p.a3 & 0xffffu;
    sx = p.a3 >> 16u;
  }
  return vec2i(i32((y + sy) % p.a0), i32((x + sx) % p.a1));
}

fn window_region(win: u32, i: u32) -> u32 {
  let hs = p.a2 & 0xffffu;
  let ws = p.a2 >> 16u;
  let per_row = p.a1 / ws;
  let y = (win / per_row) * hs + i / ws;
  let x = (win % per_row) * ws + i % ws;
  let sy = p.a3 & 0xffffu;
  let sx = p.a3 >> 16u;
  var ry = 2u;
  if (y < p.a0 - hs) {
    ry = 0u;
  } else if (y < p.a0 - sy) {
    ry = 1u;
  }
  var rx = 2u;
  if (x < p.a1 - ws) {
    rx = 0u;
  } else if (x < p.a1 - sx) {
    rx = 1u;
  }
  return ry * 3u + rx;
}

fn source_row(t: vec2i) -> i32 {
  if (t.x >= i32(p.h) || t.y >= i32(p.w)) {
    return -1;
  }
  return t.x * i32(p.w) + t.y;
}

@compute @workgroup_size(256)
fn window(@builtin(workgroup_id) wg: vec3u, @builtin(local_invocation_id) li: vec3u) {
  let win = wg.x;
  let head = wg.y;
  let n = p.rows;
  let hd = p.k;
  let shifted = (p.flags & 1u) != 0u;
  let i = li.x;
  let ch = p.xo + head * hd;
  var q: array<f32, 32>;
  var acc: array<f32, 32>;
  var row = -1;
  var region = 0u;
  if (i < n) {
    row = source_row(window_token(win, i, shifted));
    if (shifted) {
      region = window_region(win, i);
    }
    if (row >= 0) {
      for (var d = 0u; d < hd; d++) {
        q[d] = src[u32(row) * p.xs + ch + d] * p.f0;
      }
    }
  }
  var m = -1e30;
  var l = 0.0;
  for (var j0 = 0u; j0 < n; j0 += 32u) {
    for (var e = li.x; e < 32u * hd; e += 256u) {
      let jj = e / hd;
      let d = e % hd;
      var kv = 0.0;
      var vv = 0.0;
      if (j0 + jj < n) {
        let kr = source_row(window_token(win, j0 + jj, shifted));
        if (kr >= 0) {
          kv = src[u32(kr) * p.xs + p.cin + ch + d];
          vv = src[u32(kr) * p.xs + 2u * p.cin + ch + d];
        }
      }
      keys[jj * 32u + d] = kv;
      vals[jj * 32u + d] = vv;
    }
    if (li.x < 32u && shifted && j0 + li.x < n) {
      key_region[li.x] = window_region(win, j0 + li.x);
    }
    workgroupBarrier();
    if (i < n) {
      let count = min(32u, n - j0);
      for (var jj = 0u; jj < count; jj++) {
        var s = wts[(head * n + i) * n + j0 + jj];
        for (var d = 0u; d < hd; d++) {
          s += q[d] * keys[jj * 32u + d];
        }
        if (shifted && key_region[jj] != region) {
          s -= 100.0;
        }
        if (s > m) {
          let scale = exp(m - s);
          l *= scale;
          for (var d = 0u; d < hd; d++) {
            acc[d] *= scale;
          }
          m = s;
        }
        let e = exp(s - m);
        l += e;
        for (var d = 0u; d < hd; d++) {
          acc[d] += e * vals[jj * 32u + d];
        }
      }
    }
    workgroupBarrier();
  }
  if (row >= 0) {
    for (var d = 0u; d < hd; d++) {
      dst[u32(row) * p.ys + ch + d] = acc[d] / l;
    }
  }
}

// ---- column_sum: workgroup a column; rows: tokens; n: columns ----

var<workgroup> partial: array<f32, 256>;

@compute @workgroup_size(256)
fn column_sum(@builtin(workgroup_id) wg: vec3u, @builtin(local_invocation_id) li: vec3u) {
  let c = wg.x;
  var s = 0.0;
  for (var r = li.x; r < p.rows; r += 256u) {
    let v = src[r * p.xs + p.xo + c];
    s += select(v, v * v, (p.flags & 1u) != 0u);
  }
  partial[li.x] = s;
  workgroupBarrier();
  for (var half = 128u; half > 0u; half >>= 1u) {
    if (li.x < half) {
      partial[li.x] += partial[li.x + half];
    }
    workgroupBarrier();
  }
  if (li.x == 0u) {
    dst[p.yo + c] = partial[0] * p.f0;
  }
}

// ---- channel attention: src qkv (stride xs, embed cin); k: a head's width ----
// channel_scores: workgroup (channel a = head * k + d1, d2); aux the sums of squares [q | k];
// wts the temperatures; dst [channel][k].

@compute @workgroup_size(256)
fn channel_scores(@builtin(workgroup_id) wg: vec3u, @builtin(local_invocation_id) li: vec3u) {
  let a = wg.x;
  let d2 = wg.y;
  let head = a / p.k;
  let b = head * p.k + d2;
  var s = 0.0;
  for (var r = li.x; r < p.rows; r += 256u) {
    s += src[r * p.xs + a] * src[r * p.xs + p.cin + b];
  }
  partial[li.x] = s;
  workgroupBarrier();
  for (var half = 128u; half > 0u; half >>= 1u) {
    if (li.x < half) {
      partial[li.x] += partial[li.x + half];
    }
    workgroupBarrier();
  }
  if (li.x == 0u) {
    let nq = max(sqrt(aux[a]), 1e-12);
    let nk = max(sqrt(aux[p.cin + b]), 1e-12);
    dst[a * p.k + d2] = partial[0] / (nq * nk) * wts[head];
  }
}

// softmax_rows: rows of length k, in place.
@compute @workgroup_size(256)
fn softmax_rows(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
                @builtin(local_invocation_id) li: vec3u) {
  let r = flat(wg, nw, li);
  if (r >= p.rows) {
    return;
  }
  var m = dst[r * p.k];
  for (var j = 1u; j < p.k; j++) {
    m = max(m, dst[r * p.k + j]);
  }
  var sum = 0.0;
  for (var j = 0u; j < p.k; j++) {
    let e = exp(dst[r * p.k + j] - m);
    dst[r * p.k + j] = e;
    sum += e;
  }
  for (var j = 0u; j < p.k; j++) {
    dst[r * p.k + j] = dst[r * p.k + j] / sum;
  }
}

// channel_apply: rows tokens, n the embed width; aux the scores [channel][k].
@compute @workgroup_size(256)
fn channel_apply(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
                 @builtin(local_invocation_id) li: vec3u) {
  let e = flat(wg, nw, li);
  if (e >= p.rows * p.n) {
    return;
  }
  let r = e / p.n;
  let c = e % p.n;
  let head = c / p.k;
  var s = 0.0;
  for (var d2 = 0u; d2 < p.k; d2++) {
    s += aux[c * p.k + d2] * src[r * p.xs + 2u * p.n + head * p.k + d2];
  }
  dst[r * p.ys + c] = s;
}

// ---- gate_mix: src the attention, aux the convolution, wts the channel gate's logits [n], bias
// the spatial gate's [rows]; flags 1: spatial (the channel gate gates the attention) ----

@compute @workgroup_size(256)
fn gate_mix(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
            @builtin(local_invocation_id) li: vec3u) {
  let e = flat(wg, nw, li);
  if (e >= p.rows * p.n) {
    return;
  }
  let r = e / p.n;
  let c = e % p.n;
  let a = src[e];
  let v = aux[e];
  let cg = sigmoid(wts[c]);
  let sg = sigmoid(bias[r]);
  if ((p.flags & 1u) != 0u) {
    dst[e] = a * cg + sg * v;
  } else {
    dst[e] = a * sg + v * cg;
  }
}

// ---- multiply: dst[r][c] = src[r * xs + c] * aux[r * n + c] ----

@compute @workgroup_size(256)
fn multiply(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
            @builtin(local_invocation_id) li: vec3u) {
  let e = flat(wg, nw, li);
  if (e >= p.rows * p.n) {
    return;
  }
  let r = e / p.n;
  let c = e % p.n;
  dst[e] = src[r * p.xs + c] * aux[e];
}

// ---- shuffle: h × w of 4n channels into 2h × 2w of n ----

@compute @workgroup_size(256)
fn shuffle(@builtin(workgroup_id) wg: vec3u, @builtin(num_workgroups) nw: vec3u,
           @builtin(local_invocation_id) li: vec3u) {
  let e = flat(wg, nw, li);
  let w2 = p.w * 2u;
  if (e >= p.h * 2u * w2 * p.n) {
    return;
  }
  let c = e % p.n;
  let pix = e / p.n;
  let y = pix / w2;
  let x = pix % w2;
  dst[e] = src[((y / 2u) * p.w + x / 2u) * 4u * p.n + c * 4u + (y % 2u) * 2u + x % 2u];
}
