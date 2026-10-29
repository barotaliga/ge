// MOTION BLUR'S TILES (render/motion_blur.hpp): the fastest motion in each 16 x 16 tile of the
// motion target, then the fastest of each tile's 3 x 3 neighbours; beside each (b) the slowest
// motion's length in the motion target's pixels, so post can tell a tile moving as one (the
// camera turning over open snow) from one with an edge between speeds. A motion is compared by its
// length in pixels (the uv motion times the target's size), so a sideways streak and an upward one
// of the same length weigh the same on a wide picture.

@group(0) @binding(0) var src: texture_2d<f32>;
@group(0) @binding(1) var dst: texture_storage_2d<rgba16float, write>;

const kTile: u32 = 16u;

// One lane a tile, reading its 256 texels in turn: no shared memory and no barrier, which a
// software rasterizer pays dearly for, and on a GPU the target's few thousand tiles are still a
// sliver of a millisecond.
@compute @workgroup_size(8, 8)
fn tiles(@builtin(global_invocation_id) id: vec3u) {
  let size = textureDimensions(src);
  let grid = (size + vec2u(kTile - 1u)) / kTile;
  if (any(id.xy >= grid)) {
    return;
  }
  let scale = vec2f(size);
  let lo = id.xy * kTile;
  let hi = min(lo + vec2u(kTile), size);
  var fastest = vec2f(0.0);
  var most = 0.0;
  var least = 1e30;
  for (var y = lo.y; y < hi.y; y++) {
    for (var x = lo.x; x < hi.x; x++) {
      let v = textureLoad(src, vec2i(i32(x), i32(y)), 0).xy;
      let px = v * scale;
      let l = dot(px, px);
      if (l > most) {
        most = l;
        fastest = v;
      }
      least = min(least, l);
    }
  }
  textureStore(dst, vec2i(id.xy), vec4f(fastest, sqrt(least), 1.0));
}

// The fastest of a tile and its eight neighbours.
@compute @workgroup_size(8, 8)
fn near(@builtin(global_invocation_id) id: vec3u) {
  let size = vec2i(textureDimensions(src));
  let t = vec2i(id.xy);
  if (any(t >= size)) {
    return;
  }
  var v = vec2f(0.0);
  var most = -1.0;
  var slowest = 1e30;
  for (var y = -1; y <= 1; y++) {
    for (var x = -1; x <= 1; x++) {
      let q = t + vec2i(x, y);
      if (any(q < vec2i(0)) || any(q >= size)) {
        continue;
      }
      let t4 = textureLoad(src, q, 0);
      let m = t4.xy;
      slowest = min(slowest, t4.z);
      let px = m * vec2f(size);  // in tiles: in proportion to pixels on both axes
      let l = dot(px, px);
      if (l > most) {
        most = l;
        v = m;
      }
    }
  }
  textureStore(dst, t, vec4f(v, slowest, 1.0));
}
