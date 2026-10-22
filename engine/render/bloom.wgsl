// BLOOM (render/bloom.hpp): the chain's three kernels. `first` and `down` read the level above
// (the post pass's input, or the last down level) and write the next; `up` reads the level below
// it (an up level, or the smallest down level) and the down level it sits on. Alpha carries
// the number of levels a texel sums: 1 on a down level, one more on each up level.

@group(0) @binding(0) var src: texture_2d<f32>;
@group(0) @binding(1) var lin: sampler;
@group(0) @binding(2) var dst: texture_storage_2d<rgba16float, write>;
@group(0) @binding(3) var base: texture_2d<f32>;  // up: the down level this one adds to

fn tap(uv: vec2f) -> vec3f {
  return textureSampleLevel(src, lin, uv, 0.0).rgb;
}

fn tap4(uv: vec2f) -> vec4f {
  return textureSampleLevel(src, lin, uv, 0.0);
}

fn luma(c: vec3f) -> f32 {
  return dot(c, vec3f(0.2126, 0.7152, 0.0722));
}

// The 13-tap downsample at `uv` (a texel of the source is `t`): the centre 2x2 box at half the
// weight and the four corner boxes at an eighth each. `karis`: each box weighed by 1 / (1 +
// its brightness), so one bright texel cannot dominate.
fn down13(uv: vec2f, t: vec2f, karis: bool) -> vec3f {
  let a = tap(uv + t * vec2f(-2.0, -2.0));
  let b = tap(uv + t * vec2f(0.0, -2.0));
  let c = tap(uv + t * vec2f(2.0, -2.0));
  let d = tap(uv + t * vec2f(-2.0, 0.0));
  let e = tap(uv);
  let f = tap(uv + t * vec2f(2.0, 0.0));
  let g = tap(uv + t * vec2f(-2.0, 2.0));
  let h = tap(uv + t * vec2f(0.0, 2.0));
  let i = tap(uv + t * vec2f(2.0, 2.0));
  let j = tap(uv + t * vec2f(-1.0, -1.0));
  let k = tap(uv + t * vec2f(1.0, -1.0));
  let l = tap(uv + t * vec2f(-1.0, 1.0));
  let m = tap(uv + t * vec2f(1.0, 1.0));
  var boxes = array<vec3f, 5>((j + k + l + m) * 0.25, (a + b + d + e) * 0.25,
                              (b + c + e + f) * 0.25, (d + e + g + h) * 0.25,
                              (e + f + h + i) * 0.25);
  let weights = array<f32, 5>(0.5, 0.125, 0.125, 0.125, 0.125);
  var sum = vec3f(0.0);
  var total = 0.0;
  for (var n = 0; n < 5; n++) {
    var w = weights[n];
    if (karis) {
      w /= 1.0 + luma(boxes[n]);
    }
    sum += boxes[n] * w;
    total += w;
  }
  return sum / total;
}

fn downsample(id: vec2u, karis: bool) {
  let size = textureDimensions(dst);
  if (id.x >= size.x || id.y >= size.y) {
    return;
  }
  let uv = (vec2f(id) + 0.5) / vec2f(size);
  let t = 1.0 / vec2f(textureDimensions(src));
  textureStore(dst, id, vec4f(down13(uv, t, karis), 1.0));
}

@compute @workgroup_size(8, 8)
fn first(@builtin(global_invocation_id) id: vec3u) {
  downsample(id.xy, true);
}

@compute @workgroup_size(8, 8)
fn down(@builtin(global_invocation_id) id: vec3u) {
  downsample(id.xy, false);
}

// The level below through a 3x3 tent (one of its texels apart), plus the down level here.
@compute @workgroup_size(8, 8)
fn up(@builtin(global_invocation_id) id: vec3u) {
  let size = textureDimensions(dst);
  if (id.x >= size.x || id.y >= size.y) {
    return;
  }
  let uv = (vec2f(id.xy) + 0.5) / vec2f(size);
  let t = 1.0 / vec2f(textureDimensions(src));
  var sum = tap4(uv) * 4.0;
  sum += (tap4(uv + vec2f(t.x, 0.0)) + tap4(uv - vec2f(t.x, 0.0)) + tap4(uv + vec2f(0.0, t.y)) +
          tap4(uv - vec2f(0.0, t.y))) * 2.0;
  sum += tap4(uv + t) + tap4(uv - t) + tap4(uv + vec2f(t.x, -t.y)) + tap4(uv + vec2f(-t.x, t.y));
  // Alpha counts the levels summed (each down level writes 1), so the post pass can average.
  let here = textureLoad(base, vec2i(id.xy), 0);
  textureStore(dst, id.xy, here + sum / 16.0);
}
