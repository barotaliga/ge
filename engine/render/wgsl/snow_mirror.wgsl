// THE SNOW LAYER, READ ON THE GPU (render/snow_mirror.hpp): the bindings of a mirrored layer
// (group 3, 0 to 4; a material's own bindings come after) and the reads over them.
//
// - `snow_raw(c, r)`: what is stored at sample (c, r), to the bit: a fine cell of the window's
//   atlas, else the coarse cell over it (its sums and how many samples they stand for), else the
//   cover (source 0). `snow_mirror_expect` is its CPU twin.
// - `snow_column(x, z)`: the column at a plan point as the physics' `snow::sample` reads it: the
//   four samples round it, the stored ones as stored and the rest as the cover, blended
//   bilinearly; the depth, the density, the packed share, how far the surface stands under the
//   untouched cover, whether a press touched it and how much of it is a pressed floor.
//
// ge:summary WGSL library: a mirrored snow layer's bindings, the raw cells and the column
// ge:keywords snow snow-mirror snow_raw snow_column atlas page-table coarse-view drop pressed
// ge:keywords storage-buffer wgsl library
// ge:skill render snow
// ge:test tests/render_snow_mirror_test.cpp

struct SnowMirrorInfo {
  region: vec4f,           // x0, z0, cell m, 0
  dims: vec4i,             // cols, rows, tile_cols, tile_rows
  window: vec4i,           // the window's first tile x and z, its side in tiles, 0
  cover: vec4f,            // depth m, density kg/m3, packed share, kind
  kinds: array<vec4f, 8>,  // each named snow's loose, packed and cap densities, kg/m3
}

@group(3) @binding(0) var<uniform> snow_mirror: SnowMirrorInfo;
@group(3) @binding(1) var<storage, read> snow_page: array<u32>;
@group(3) @binding(2) var<storage, read> snow_atlas: array<u32>;
@group(3) @binding(3) var<storage, read> snow_coarse_page: array<u32>;
@group(3) @binding(4) var<storage, read> snow_coarse: array<u32>;

const kSnowTile = 64;
const kSnowCoarse = 16;
const kSnowTouched = 1u;
const kSnowPressed = 2u;

// Sums over `samples` for a coarse cell; `bits` is kind | flags << 8 | minute << 16; `source` 0
// the cover, 1 a fine cell, 2 a coarse cell.
struct SnowRaw {
  depth: u32,
  mass: u32,
  bits: u32,
  source: u32,
  samples: u32,
}

fn snow_raw(c: i32, r: i32) -> SnowRaw {
  var out = SnowRaw(0u, 0u, 0u, 0u, 1u);
  let d = snow_mirror.dims;
  if (c < 0 || r < 0 || c >= d.x || r >= d.y) {
    return out;
  }
  let tx = c / kSnowTile;
  let tz = r / kSnowTile;
  let w = snow_mirror.window;
  let wi = tx - w.x;
  let wj = tz - w.y;
  if (wi >= 0 && wj >= 0 && wi < w.z && wj < w.z) {
    let slot = snow_page[u32(wj * w.z + wi)];
    if (slot != 0u) {
      let k = (slot - 1u) * 4096u + u32((r % kSnowTile) * kSnowTile + c % kSnowTile);
      let w0 = snow_atlas[2u * k];
      let w1 = snow_atlas[2u * k + 1u];
      if (((w1 >> 8u) & kSnowTouched) != 0u) {
        out = SnowRaw(w0 & 0xffffu, w0 >> 16u, w1, 1u, 1u);
      }
      return out;
    }
  }
  let block = snow_coarse_page[u32(tz * d.z + tx)];
  if (block == 0u) {
    return out;
  }
  let i = (c % kSnowTile) / kSnowCoarse;
  let j = (r % kSnowTile) / kSnowCoarse;
  let k = (block - 1u) * 16u + u32(j * 4 + i);
  let bits = snow_coarse[3u * k + 2u];
  if (((bits >> 8u) & kSnowTouched) == 0u) {
    return out;
  }
  let c0 = tx * kSnowTile + i * kSnowCoarse;
  let r0 = tz * kSnowTile + j * kSnowCoarse;
  let n = u32(min(kSnowCoarse, d.x - c0) * min(kSnowCoarse, d.y - r0));
  return SnowRaw(snow_coarse[3u * k + 1u], snow_coarse[3u * k], bits, 2u, n);
}

// The packed share snow of `kind` reads as at density `rho` (snow::packed_from_density).
fn snow_packed_share(kind: u32, rho: f32) -> f32 {
  let row = snow_mirror.kinds[min(kind, 7u)];
  if (row.y <= row.x) {
    return 1.0;
  }
  return clamp((rho - row.x) / (row.y - row.x), 0.0, 1.0);
}

struct SnowColumn {
  depth: f32,    // m of snow
  density: f32,  // kg/m3
  packed: f32,   // 0..1
  drop: f32,     // m the surface stands under the untouched cover (negative: a berm)
  touched: f32,  // 1 where a press changed any of the four samples
  pressed: f32,  // the share of the four that are a pressed floor
  kind: u32,
}

fn snow_column(x: f32, z: f32) -> SnowColumn {
  let cover = snow_mirror.cover;
  let fx = (x - snow_mirror.region.x) / snow_mirror.region.z;
  let fz = (z - snow_mirror.region.y) / snow_mirror.region.z;
  let d = snow_mirror.dims;
  let c0 = clamp(i32(floor(fx)), 0, d.x - 1);
  let r0 = clamp(i32(floor(fz)), 0, d.y - 1);
  let c1 = min(c0 + 1, d.x - 1);
  let r1 = min(r0 + 1, d.y - 1);
  let tx = clamp(fx - f32(c0), 0.0, 1.0);
  let tz = clamp(fz - f32(r0), 0.0, 1.0);
  let cs = array<i32, 4>(c0, c1, c0, c1);
  let rs = array<i32, 4>(r0, r0, r1, r1);
  let ws = array<f32, 4>((1.0 - tx) * (1.0 - tz), tx * (1.0 - tz), (1.0 - tx) * tz, tx * tz);
  var depth = 0.0;
  var mass = 0.0;
  var packed = 0.0;
  var pressed = 0.0;
  var any = false;
  var best = -1.0;
  var kind = u32(cover.w);
  for (var k = 0; k < 4; k++) {
    let raw = snow_raw(cs[k], rs[k]);
    var dk = cover.x;
    var mk = cover.x * cover.y;
    var pk = cover.z;
    var kk = u32(cover.w);
    if (raw.source != 0u) {
      any = true;
      let n = f32(raw.samples);
      dk = f32(raw.depth) * 0.001 / n;
      mk = f32(raw.mass) * 0.05 / n;
      kk = raw.bits & 0xffu;
      pk = select(1.0, snow_packed_share(kk, mk / dk), dk > 0.0);
      if (((raw.bits >> 8u) & kSnowPressed) != 0u) {
        pressed += 0.25;
      }
    }
    depth += ws[k] * dk;
    mass += ws[k] * mk;
    packed += ws[k] * pk;
    if (ws[k] > best) {
      best = ws[k];
      kind = kk;
    }
  }
  if (!any) {
    return SnowColumn(cover.x, cover.y, cover.z, 0.0, 0.0, 0.0, u32(cover.w));
  }
  let density = select(0.0, mass / depth, depth > 0.0);
  return SnowColumn(depth, density, packed, cover.x - depth, 1.0, pressed, kind);
}
