// LAVA AND SLIME (render/liquid.hpp; docs/liquids.md § Drawn): a body's flat surface drawn over
// the opaque scene by its look, a grid of quads over its box at its level (the slime's ripples
// lifting its vertices).
//
// LAVA: the surface's temperature is worked out per pixel and its light read from the glow
// table (a black body's radiance by temperature, cd/m^2, water/glow.hpp). The crust is a Voronoi
// tiling of plates, carried by the flow on two phases of a flow map (each phase's pattern drawn
// back along the flow for its share of the period and the two crossfaded, so it never
// stretches); a plate's age (hashed, running on at `age_rate`) cools its skin and widens its
// cracks, where the melt's heat shows, falling off exp(-d / w) from the crack. Open patches of
// melt where a slow noise passes `cover`. The crust's height (plates over cracks, the ropy folds
// across the flow) gives its normal; the crust and the melt are glasses of their roughness under
// the sun and the sky, and the glow is added. Detail under a pixel is filtered: a crack narrower
// than the pixel's footprint is widened to it with its heat spread over the width.
//
// SLIME: a turbid volume; its own colour the Kubelka-Munk reflectance of a thick layer from K
// and S, the scene behind seen through exp(-(K + S) d), a scum over it, ripples, bubbles that
// swell and burst into rings, an oily film's interference colours on its reflection, its gloss
// and its own glow.
//
// BOTH: the scene mirrored in the surface (`scene_mirror`: the reflected ray marched through the
// scene's depth copy and the colour copy read where it meets it, the sky's light where it meets
// nothing). LAVA: the heat shimmer (`bend`: the hot air's rising cells bending a ray, summed
// through a layer kShimmerHeight deep; the surface looked up where its bent ray lands, and what
// is seen across the lake drawn again through the bent rays by `fs_shimmer`, a box over the
// body), and the crust broken where something fell in (`broken_at`). FROM BELOW (`fs_under`, one
// triangle over the screen for the body the eye is under): the slime's fog, its surface's window
// and mirror and its scum's shadow; the lava's melt all round.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/liquid_body.wgsl"

override kGrid: u32 = 96u;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,
  @location(1) xz: vec2f,
}

fn ripple_height(xz: vec2f, t: f32) -> f32 {
  var h = 0.0;
  let n = u32(body.info.z);
  for (var i = 0u; i < n; i++) {
    let r = ripples[i];
    h += r.dir.z * sin(dot(r.dir.xy, xz) - r.dir.w * t + r.more.x);
  }
  return h;
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VertexOut {
  let n = kGrid + 1u;
  let g = vec2f(f32(vid % n), f32(vid / n)) / f32(kGrid);
  let xz = mix(body.box.xy, body.box.zw, g);
  let y = body.info.x + ripple_height(xz, frame.time.x);
  var out: VertexOut;
  out.rel = vec3f(xz.x, y, xz.y) - frame.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.xz = xz;
  return out;
}

// A grimy pool's dirt: silt and rot, a dark brown.
const kGrimeAlbedo = vec3f(0.05, 0.04, 0.025);

// A Voronoi cell at `p` (in cells): x the distance to the border (half of F2 - F1), y the cell's
// hash, zw the offset to its point.
fn voronoi(p: vec2f) -> vec4f {
  let i = vec2i(floor(p));
  let f = fract(p);
  var f1 = 8.0;
  var f2 = 8.0;
  var id = 0.0;
  var off = vec2f(0.0);
  for (var y = -1; y <= 1; y++) {
    for (var x = -1; x <= 1; x++) {
      let c = vec2i(x, y);
      let h = hash2(i + c);
      let d = vec2f(c) + 0.15 + 0.7 * h - f;
      let l = length(d);
      if (l < f1) {
        f2 = f1;
        f1 = l;
        id = fract(h.x * 7.31 + h.y * 3.17);
        off = d;
      } else if (l < f2) {
        f2 = l;
      }
    }
  }
  return vec4f(0.5 * (f2 - f1), id, off);
}

// THE LAVA'S SURFACE at plan point `q` (the flow taken out), its pixel footprint `foot` (m):
// x the temperature (K), y the height (m) for the normal, z the share that is open melt.
fn lava_at(q: vec2f, t: f32, foot: f32) -> vec3f {
  let core = body.lava0.x;
  let hot = body.lava0.y;
  let cold = body.lava0.z;
  let plate = body.lava1.x;
  // The open melt: where a slow noise (spread to about even over 0..1) passes the cover.
  let field = smoothstep(0.28, 0.72, fbm(q / body.lava2.w + vec2f(t * 0.004, -t * 0.003)));
  let cover = body.lava0.w;
  let open = smoothstep(cover - 0.03, cover + 0.03, field);
  // The plates, warped so no two are alike: an age per plate, running on and renewed; each
  // plate broken again into smaller ones along fainter, narrower cracks.
  let warp = vec2f(fbm(q / plate * 0.7), fbm(q / plate * 0.7 + vec2f(5.2, 1.3))) - 0.5;
  let pq = q / plate + 0.6 * warp;
  let v = voronoi(pq);
  let vs = voronoi(pq * 2.7 + vec2f(3.1, 7.7));
  let age = fract(v.y + t * body.lava1.w);
  // Plates near the open melt are young.
  let young = smoothstep(cover - 0.2, cover, field);
  let a = age * (1.0 - 0.85 * young);
  let skin = mix(hot, cold, sqrt(a));
  // The crack's heat off its line, its width growing with the plate's age, never under the
  // pixel (its heat then spread over the pixel's width).
  let w = body.lava1.y * (0.4 + 1.2 * a);
  let wf = max(w, foot);
  let edge = v.x * plate;
  let ws = 0.4 * w;
  let wsf = max(ws, foot);
  let crack = max(exp(-edge / wf) * (w / wf),
                  0.8 * exp(-vs.x * plate / 2.7 / wsf) * (ws / wsf) * (1.0 - a));
  var temp = skin + (core - skin) * crack;
  // The melt's own skin: cooler swirls where a film has begun to form, wrinkled into threads
  // drawn out along the flow, and hotter streaks where fresh melt wells up between them.
  let swirl = fbm(q * 0.9 + 0.8 * warp + vec2f(0.0, t * 0.05));
  let threads = abs(sin(9.0 * fbm(q * 2.3 + 1.5 * warp) + 3.0 * swirl));
  let thread_seen = smoothstep(0.02, 0.08, 0.25 / max(foot, 1e-3) * 0.01);
  let skin_on = smoothstep(0.42, 0.78, swirl);
  let melt = core + 45.0 * (1.0 - smoothstep(0.2, 0.5, swirl)) - 260.0 * skin_on -
             90.0 * (1.0 - threads) * skin_on * thread_seen;
  temp = mix(temp, max(melt, temp), open);
  // The height: plates stand over their cracks, the ropy folds across the flow on the crust.
  let flow = body.flow.xy;
  let along = select(vec2f(0.0, 1.0), normalize(flow), length(flow) > 1e-4);
  let rope_len = 0.12 * plate;
  let ropes = body.lava1.z * 0.02 * sin(6.2831853 * (dot(q, along) / rope_len +
              1.5 * fbm(q * 0.8))) * smoothstep(rope_len, 2.0 * rope_len, 3.0 * foot);
  let stand = 0.05 * plate * smoothstep(0.0, 1.5 * w, edge) * (0.5 + 0.5 * a);
  // The rind's own roughness: a fine knobbly texture where the pixel can hold it.
  let knobs = 0.006 * fbm(q * 14.0) * smoothstep(0.08, 0.02, foot);
  let h = (1.0 - open) * (stand + ropes + knobs);
  return vec3f(temp, h, open);
}

// The surface on the flow map's two phases, crossfaded.
fn lava_flowing(p: vec2f, t: f32, foot: f32) -> vec3f {
  let period = 8.0;
  let flow = body.flow.xy;
  let ph0 = fract(t / period);
  let ph1 = fract(t / period + 0.5);
  let s0 = lava_at(p - flow * ph0 * period + vec2f(31.0, 17.0) * floor(t / period), t, foot);
  let s1 = lava_at(p - flow * ph1 * period + vec2f(11.0, 43.0) * floor(t / period + 0.5) +
                   vec2f(5.3, 9.1), t, foot);
  let k = abs(1.0 - 2.0 * ph0);
  return mix(s1, s0, k);
}

// THE CRUST BROKEN at plan point `p` at time `t`: x the share of open melt, y the heat of the
// shards round the hole (0..1), z the crest's height (m). A break opens at once to its radius,
// its edge torn, and skins over from the edge over 8 + 6 r seconds, carried by the flow; the
// crest runs out from it in the first second.
fn broken_at(p: vec2f, t: f32) -> vec3f {
  var out = vec3f(0.0);
  let n = u32(body.fx.z);
  for (var i = 0u; i < n; i++) {
    let b = body.breaks[i];
    let age = t - b.w;
    let r0 = b.z;
    let heal = 8.0 + 6.0 * r0;
    if (age < 0.0 || age > 4.0 * heal || r0 <= 0.0) {
      continue;
    }
    let off = p - b.xy - body.flow.xy * age;
    // A torn edge, not a circle: the distance roughened by a noise over the break's own scale.
    let torn = fbm(off * 5.0 / r0 + vec2f(f32(i) * 7.3, 1.1));
    let d = length(off) * (0.8 + 0.4 * torn);
    let fade = exp(-age / heal);
    let r = r0 * (1.0 - exp(-age * 6.0)) * fade;
    let e = max(0.15 * r, 0.02);
    let hole = select(0.0, 1.0 - smoothstep(r - e, r, d), r > 0.02);
    // The plates round it broken into shards a third of its radius across, hot through, and
    // hotter in the cracks between them where the melt shows.
    let shards = 1.0 - smoothstep(0.0, 0.2, voronoi(off * 3.0 / r0 + vec2f(f32(i) * 3.1)).x);
    let ring = 1.0 - smoothstep(0.5 * r0, 1.1 * r0, d - r);
    let rim = ring * sqrt(fade) * (1.0 - hole) * (0.6 + 0.4 * shards);
    let u = (d - r0 * (0.8 + 3.0 * age)) / (0.3 * r0);
    let crest = 0.15 * r0 * exp(-age * 2.5) * exp(-u * u);
    out = vec3f(max(out.x, hole), max(out.y, rim), out.z + crest);
  }
  return out;
}

// The lava's surface at plan point `p`, the breaks over the flowing crust.
fn lava_here(p: vec2f, t: f32, foot: f32) -> vec3f {
  let s = lava_flowing(p, t, foot);
  if (body.fx.z < 0.5) {
    return s;
  }
  let b = broken_at(p, t);
  let core = body.lava0.x;
  let cracked = core - 120.0;
  var temp = mix(s.x, max(s.x, core - 40.0), b.x);
  // The glow climbs steeply with the temperature, so the shards' heat is laid on in kelvin under
  // the cracks' (700 K cooler where b.y is 0, under any crust's), never a blend toward it.
  temp = max(temp, cracked - 700.0 * (1.0 - b.y));
  return vec3f(temp, (s.y + b.z) * (1.0 - b.x), max(s.z, b.x));
}

// THE HEAT SHIMMER: the hot air's rising cells at world point `w`, two components in -1..1.
fn shimmer_cell(w: vec3f, t: f32) -> vec2f {
  let a = vec2f(w.x * 2.1 + w.z * 1.3, w.y * 2.6 - t * 3.0);
  let b = vec2f(w.z * 2.1 - w.x * 1.1 + 17.3, w.y * 2.6 - t * 3.0 + 4.1);
  let x = value_noise(a) + 0.5 * value_noise(a * 2.3 + 7.7);
  let y = value_noise(b) + 0.5 * value_noise(b * 2.3 + 3.1);
  return vec2f(x, y) / 1.5 * 2.0 - 1.0;
}

// The bend (rad, across and up the view) of the camera-relative ray `dir` over [t0, t1] through
// the hot layer: six samples, each weighted by its height over the melt (e-folding over 1 m).
fn bend(dir: vec3f, t0: f32, t1: f32, t: f32) -> vec2f {
  var sum = vec2f(0.0);
  let dl = (t1 - t0) / 6.0;
  for (var i = 0; i < 6; i++) {
    let w = frame.eye.xyz + dir * (t0 + (f32(i) + 0.5) * dl);
    let over = w.y - body.info.x;
    if (over < 0.0 || over > kShimmerHeight) {
      continue;
    }
    sum += shimmer_cell(w, t) * exp(-over) * dl;
  }
  return sum * body.fx.y;
}

// `dir` turned by the bend `th` (rad) across and up the view.
fn bent(dir: vec3f, th: vec2f) -> vec3f {
  let side = cross(dir, vec3f(0.0, 1.0, 0.0));
  let right = select(vec3f(1.0, 0.0, 0.0), normalize(side), length(side) > 1e-4);
  let up = cross(right, dir);
  return normalize(dir + th.x * right + th.y * up);
}

// The glow of a surface at `kelvin`, in the frame's light.
// GGX's specular for a roughness `r` with Schlick's Fresnel from `f0`.
fn ggx(n: vec3f, v: vec3f, l: vec3f, r: f32, f0: f32) -> f32 {
  let h = normalize(v + l);
  let nh = max(dot(n, h), 0.0);
  let nv = max(dot(n, v), 1e-3);
  let nl = max(dot(n, l), 0.0);
  let a2 = max(r * r * r * r, 1e-5);
  let d = a2 / (PI * pow(nh * nh * (a2 - 1.0) + 1.0, 2.0));
  let k = r * r * 0.5;
  let g = nv / (nv * (1.0 - k) + k) * nl / (nl * (1.0 - k) + k);
  let f = f0 + (1.0 - f0) * pow(1.0 - max(dot(v, h), 0.0), 5.0);
  return d * g * f / (4.0 * nv);
}

// THE SCENE IN THE SURFACE: the camera-relative ray from `rel` along `r` marched through the
// scene's depth copy, 24 steps growing with the distance out to 80 m, then halved five times down
// onto where it meets the scene: rgb the colour copy there, a the share it counts for (faded
// toward the screen's edges and the march's end); none where it meets nothing, leaves the
// screen or meets the scene under the body's level (which the liquid hides). A point behind the
// scene by more than its own thickness (a metre, more far off) passes behind it.
fn scene_mirror(rel: vec3f, r: vec3f) -> vec4f {
  if (body.fx.x < 0.5) {
    return vec4f(0.0);
  }
  let reach = 80.0;
  var last = 0.0;
  for (var i = 1; i <= 24; i++) {
    let f = f32(i) / 24.0;
    let s = reach * f * f;
    let p = rel + r * s;
    let clip = frame.view_proj * vec4f(p, 1.0);
    if (clip.w <= 1e-4) {
      return vec4f(0.0);
    }
    let ndc = clip.xyz / clip.w;
    let uv = vec2f(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < vec2f(0.0)) || any(uv > vec2f(1.0))) {
      return vec4f(0.0);
    }
    let px = vec2i(uv * frame.size.xy);
    let zs = textureLoad(scene_depth, px, 0).r;
    if (zs > 0.0 && ndc.z < zs) {
      let gap = length(p) - behind_distance(px, uv);
      if (gap < max(1.0, 0.15 * s)) {
        // Halve down onto the surface between the last step in front and this one.
        var a = last;
        var b = s;
        for (var k = 0; k < 5; k++) {
          let m = 0.5 * (a + b);
          let cm = frame.view_proj * vec4f(rel + r * m, 1.0);
          let um = vec2f(cm.x / cm.w * 0.5 + 0.5, 0.5 - cm.y / cm.w * 0.5);
          let zm = textureLoad(scene_depth, vec2i(clamp(um, vec2f(0.0), vec2f(0.9999)) *
                                                  frame.size.xy), 0).r;
          if (cm.z / cm.w < zm) {
            b = m;
          } else {
            a = m;
          }
        }
        let ch = frame.view_proj * vec4f(rel + r * b, 1.0);
        let uh = clamp(vec2f(ch.x / ch.w * 0.5 + 0.5, 0.5 - ch.y / ch.w * 0.5), vec2f(0.0),
                       vec2f(0.9999));
        // What the copy shows under a liquid's level the liquid hides in the picture (the bed
        // under it): no reflection of it.
        let zh = textureLoad(scene_depth, vec2i(uh * frame.size.xy), 0).r;
        let wh = frame.inv_view_proj * vec4f(uh.x * 2.0 - 1.0, 1.0 - uh.y * 2.0, zh, 1.0);
        if (frame.eye.y + wh.y / wh.w < body.info.x - 0.05) {
          return vec4f(0.0);
        }
        let edge = min(min(uh.x, 1.0 - uh.x), min(uh.y, 1.0 - uh.y));
        let share = smoothstep(0.0, 0.08, edge) * (1.0 - smoothstep(0.6, 1.0, b / reach));
        return vec4f(textureLoad(scene_colour, vec2i(uh * frame.size.xy), 0).rgb, share);
      }
    }
    last = s;
  }
  return vec4f(0.0);
}

// A thin film's reflectance at three wavelengths (650, 550, 450 nm) for a thickness `d` nm of
// index 1.45 seen at `cos_v`: the two surfaces' reflections interfering, normalised to 1 mean.
fn film_tint(d: f32, cos_v: f32) -> vec3f {
  let cos_t = sqrt(max(1.0 - (1.0 - cos_v * cos_v) / (1.45 * 1.45), 0.0));
  let path = 4.0 * PI * 1.45 * d * cos_t;
  let r = 1.0 - cos(vec3f(path / 650.0, path / 550.0, path / 450.0));
  return r;
}

// One layer of bubbles at plan `p` in cells of `cell` m: the dome's height and slope (xyz: h,
// dh/dx, dh/dz) and how near its rim the point is (w, 1 at the rim); the rings of burst bubbles
// add their slope to yz. Each cell blows one bubble every 1 / (rate cell^2) s, at a point of its
// own each time, which swells over the first part of its period and bursts into a ring.
fn bubbles_at(p: vec2f, t: f32, cell: f32, size: f32, rate: f32) -> vec4f {
  let q = p / cell;
  let i = vec2i(floor(q));
  var out = vec4f(0.0);
  for (var y = -1; y <= 1; y++) {
    for (var x = -1; x <= 1; x++) {
      let c = i + vec2i(x, y);
      let h = hash2(c);
      let h2 = hash2(c + vec2i(57, 113));
      let period = 1.0 / max(rate * cell * cell, 1e-4);
      let s = t / period + h.x;
      let k = floor(s);
      let ph = fract(s);
      let at = (vec2f(c) + 0.2 + 0.6 * hash2(c + vec2i(i32(k), 7))) * cell;
      let d = p - at;
      let dist = max(length(d), 1e-5);
      let life = 0.25 + 0.5 * h2.x;
      if (ph < life) {
        let r = size * (0.4 + 0.6 * h2.y) * sqrt(ph / life);
        if (dist < r) {
          let hh = sqrt(r * r - dist * dist);
          let lift = 0.7;
          out.x = max(out.x, lift * hh);
          out.y += -lift * d.x / max(hh, 0.08 * r);
          out.z += -lift * d.y / max(hh, 0.08 * r);
          out.w = max(out.w, smoothstep(0.6 * r, r, dist));
        }
      } else {
        // The ring: a raised crest running out from the burst and dying away.
        let since = (ph - life) * period;
        let radius = size * (1.0 + 6.0 * since);
        let width = 0.35 * size;
        let u = (dist - radius) / width;
        let a = 0.25 * size * exp(-since * 2.5);
        let slope = a * exp(-u * u) * (-2.0 * u / width);
        out.y += slope * d.x / dist;
        out.z += slope * d.y / dist;
      }
    }
  }
  return out;
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  if (any(in.xz < body.box.xy) || any(in.xz > body.box.zw)) {
    discard;
  }
  let t = frame.time.x;
  let dist = length(in.rel);
  let v = -in.rel / dist;
  let world_y = in.rel.y + frame.eye.y;
  // A body under a roof sees the sky through `open` of its dome and the room's light, a share of
  // a white face's, over the rest (flow.z, flow.w).
  let open_sky = body.flow.z;
  let room = vec3f(white_luminance() * body.flow.w);
  let sun = sun_light(world_y, in.xz) * step(0.0, frame.sun.y) * open_sky;
  let pixel = vec2i(in.position.xy);
  let uv = in.position.xy * frame.size.zw;
  let depth_view = max(behind_distance(pixel, uv) - dist, 0.0);
  let feather = smoothstep(0.0, 0.04, depth_view);
  var c = vec3f(0.0);

  if (body.info.y < 0.5) {
    // LAVA.
    let e = max(foot, 0.01);
    // The shimmer: the surface looked up where the ray, bent by the hot air it crossed, lands
    // (across the view by the bend times the distance, along it the more the flatter the ray).
    var at = in.xz;
    if (body.fx.y > 0.0) {
      let dir = in.rel / dist;
      let top = body.info.x + kShimmerHeight;
      let enter = select(0.0, (top - frame.eye.y) / min(dir.y, -1e-4), frame.eye.y > top);
      let th = bend(dir, max(enter, 0.0), dist, t);
      let across = normalize(vec2f(-dir.z, dir.x) + vec2f(1e-6, 0.0));
      let along = vec2f(dir.x, dir.z) / max(length(dir.xz), 1e-4);
      let shift = across * th.x * dist + along * th.y * dist / max(abs(dir.y), 0.1);
      at += shift * min(1.0, 0.15 / max(length(shift), 1e-6));
    }
    let s = lava_here(at, t, foot);
    let sx = lava_here(at + vec2f(e, 0.0), t, foot);
    let sz = lava_here(at + vec2f(0.0, e), t, foot);
    let n = normalize(vec3f(-(sx.y - s.y) / e, 1.0, -(sz.y - s.y) / e));
    let open = s.z;
    let rough = mix(body.lava2.y, body.lava2.z, open);
    let cos_v = max(dot(n, v), 1e-3);
    let sky_d = sky_irradiance(n) * open_sky + room * PI;
    let lit = body.crust.rgb * (sun * max(dot(n, frame.sun.xyz), 0.0) + sky_d) / PI;
    let r = reflect(-v, n);
    let sky_mirror = mix(room, sky_luminance(normalize(vec3f(r.x, max(r.y, 0.01), r.z))), open_sky);
    // The scene's mirror is sharp, so only the smoother melt takes it; on the rough crust a
    // sharp ray's hits scatter into specks.
    let seen_in = scene_mirror(in.rel, r);
    let sharp = seen_in.a * (1.0 - smoothstep(0.1, 0.3, rough));
    let mirror = mix(sky_mirror, seen_in.rgb, sharp) * fresnel(cos_v, 0.04) * (1.0 - rough) *
                 (1.0 - rough);
    let spec = sun * ggx(n, v, frame.sun.xyz, rough, 0.04);
    c = lit * (1.0 - open) + mirror + spec + glow_at(s.x) * body.lava2.x;
  } else {
    // SLIME.
    var slope = vec2f(0.0);
    let nr = u32(body.info.z);
    for (var i = 0u; i < nr; i++) {
      let w = ripples[i];
      let k = w.more.y;
      let keep = smoothstep(2.0 * foot, 4.0 * foot, 6.2831853 / k);
      let phi = dot(w.dir.xy, in.xz) - w.dir.w * t + w.more.x;
      slope += keep * w.dir.z * cos(phi) * w.dir.xy;
    }
    // The bubbles: two sizes, faded where they are under a few pixels.
    let size = body.slime1.x;
    let rate = body.slime0.w;
    var dome = 0.0;
    var rim = 0.0;
    if (rate > 0.0) {
      let b1 = bubbles_at(in.xz, t, size * 6.0, size, rate * 0.5);
      let b2 = bubbles_at(in.xz + vec2f(3.7, 1.9), t * 1.13, size * 3.5, size * 0.45, rate * 0.5);
      let seen1 = smoothstep(2.0 * foot, 4.0 * foot, size);
      let seen2 = smoothstep(2.0 * foot, 4.0 * foot, 0.45 * size);
      slope += b1.yz * seen1 + b2.yz * seen2;
      dome = max(step(1e-6, b1.x) * seen1, step(1e-6, b2.x) * seen2);
      rim = max(b1.w * seen1, b2.w * seen2);
    }
    let n0 = vec3f(0.0, 1.0, 0.0);
    let n = normalize(vec3f(-slope.x, 1.0, -slope.y));
    let cos_v = max(dot(n, v), 1e-3);
    // The marbling: the stuff's strength varies in slow swirls carried on the flow.
    let drift = in.xz - body.flow.xy * t;
    let swirl = fbm(drift * 0.15 + vec2f(0.0, t * 0.004));
    let marble = 0.7 + 0.6 * fbm(drift * 0.45 + 2.5 * vec2f(swirl, -swirl));
    // The volume's own colour (Kubelka-Munk) and what is seen through it.
    let K = body.absorb.rgb * marble;
    let S = max(body.scatter.rgb, vec3f(1e-4));
    let ks = K / S;
    let own = 1.0 + ks - sqrt(ks * ks + 2.0 * ks);
    let through = exp(-(K + S) * depth_view);
    let seen = textureLoad(scene_colour, pixel, 0).rgb;
    let light = sun * max(frame.sun.y, 0.0) + sky_irradiance(vec3f(0.0, 1.0, 0.0)) * open_sky +
                room * PI;
    let body_colour = own * (1.0 - through) * light / PI;
    var into = seen * through + body_colour;
    // A bubble's skin is thinner than the pool, so its dome is a little lighter, and darker where
    // it meets the surface.
    into *= 1.0 + dome * (0.5 - 1.1 * rim);
    // The scum: patches drifting with the surface's flow.
    let scum_field = fbm(drift * 0.9) * 0.7 + fbm(drift * 4.0) * 0.3;
    let scum = body.slime0.z * smoothstep(1.0 - body.slime0.z - 0.05, 1.0 - body.slime0.z + 0.1,
                                          scum_field) / max(body.slime0.z, 1e-3);
    let scum_share = clamp(scum, 0.0, 1.0) * step(1e-3, body.slime0.z);
    let scum_lit = body.scum.rgb *
                   (sun * max(dot(n0, frame.sun.xyz), 0.0) + sky_irradiance(n0) * open_sky + room * PI) /
                   PI;
    into = mix(into, scum_lit, scum_share);
    // The grime: silt and rot mottled over the whole pool, darker than the scum and matte.
    var cover = scum_share;
    let light_flat = (sun * max(dot(n0, frame.sun.xyz), 0.0) + sky_irradiance(n0) * open_sky +
                      room * PI) / PI;
    let grime = body.slime1.y;
    if (grime > 0.0) {
      let g = fbm(drift * 2.3 + vec2f(5.1, 7.7)) * 0.6 + fbm(drift * 11.0) * 0.4;
      let dirt = grime * smoothstep(0.55 - 0.5 * grime, 0.85 - 0.3 * grime, g);
      let grime_albedo = mix(kGrimeAlbedo, body.scum.rgb, 0.3 * g);
      into = mix(into, grime_albedo * light_flat, dirt * 0.95);
      cover = max(cover, dirt * 0.8);
    }
    // The debris: twigs, leaves and clumps afloat, drifting with the surface; `slime1.z` a square
    // metre's, each in its own cell, turned and stretched by its hash, faded under a pixel.
    let debris = body.slime1.z;
    if (debris > 0.0) {
      let cell = inverseSqrt(debris);
      let q = drift / cell;
      let k = vec2i(floor(q));
      var fleck = 0.0;
      var fleck_shade = 0.0;
      for (var dy = -1; dy <= 1; dy++) {
        for (var dx = -1; dx <= 1; dx++) {
          let kk = k + vec2i(dx, dy);
          let h = hash2(kk);
          let h2 = hash2(kk + vec2i(71, 13));
          let centre = (vec2f(kk) + h) * cell;
          let ang = h2.x * 6.2831853;
          let axis = vec2f(cos(ang), sin(ang));
          let rel = drift - centre;
          let size = cell * (0.05 + 0.12 * h2.y);
          let along = dot(rel, axis) / size;
          let across = dot(rel, vec2f(-axis.y, axis.x)) / (size * (0.18 + 0.6 * h.y));
          let r2 = along * along + across * across;
          let inside = 1.0 - smoothstep(0.7, 1.0, r2);
          if (inside > fleck) {
            fleck = inside;
            fleck_shade = h2.y;
          }
        }
      }
      let debris_albedo = mix(vec3f(0.035, 0.03, 0.02), vec3f(0.16, 0.12, 0.06), fleck_shade);
      into = mix(into, debris_albedo * light_flat, fleck);
      cover = max(cover, fleck);
    }
    // The glow: its own light, pulsing slowly, brighter in the bubbles.
    let pulse = 1.0 + body.glow.w * (sin(t * 0.7 + fbm(in.xz * 0.3) * 6.0) * 0.5 + 0.6 * dome);
    into += body.glow.rgb * pulse * (1.0 - cover) * frame.world.y / kSunLuxCd;
    // The reflection: the sky's, tinted by the film's interference where an oily film lies.
    let rough = mix(body.slime0.x, 0.8, cover);
    let r = reflect(-v, n);
    var mirror = mix(room, sky_luminance(normalize(vec3f(r.x, max(r.y, 0.01), r.z))), open_sky);
    let seen_in = scene_mirror(in.rel, r);
    mirror = mix(mirror, seen_in.rgb, seen_in.a * (1.0 - smoothstep(0.1, 0.3, rough)));
    var tint = vec3f(1.0);
    var tint_spec = vec3f(1.0);
    if (body.slime0.y > 0.0) {
      // The oil lies in patches, a few hundred nm thick, its colours a sheen over the reflection.
      let d = body.slime0.y * (0.6 + 0.8 * fbm(drift * 0.35 + vec2f(t * 0.01, 0.0)));
      let oily = smoothstep(0.42, 0.62, fbm(drift * 0.12 + vec2f(9.1, 3.3))) * (1.0 - cover);
      let film = film_tint(d, cos_v);
      tint = mix(vec3f(1.0), film, 0.55 * oily);
      tint_spec = mix(vec3f(1.0), film, 0.3 * oily);
    }
    let f = fresnel(cos_v, 0.03) * (1.0 - 0.8 * rough);
    let spec = sun * ggx(n, v, frame.sun.xyz, max(rough, 0.03), 0.03) * (1.0 - cover);
    // A bubble's wet skin is smoother than the pool's surface: a small sharp highlight.
    let sparkle = sun * ggx(n, v, frame.sun.xyz, 0.05, 0.03) * dome * (1.0 - cover);
    c = mix(into, mirror * tint, f * (1.0 - cover)) + (spec + sparkle) * tint_spec;
  }
  c = mix(textureLoad(scene_colour, pixel, 0).rgb, c, feather);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, uv, dist), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}

// THE SHIMMER'S BOX: the hot layer over the body, 36 corners of its six faces.
struct ShimmerOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,
}

@vertex
fn vs_shimmer(@builtin(vertex_index) vid: u32) -> ShimmerOut {
  let quads = array<vec4u, 6>(vec4u(0u, 2u, 6u, 4u), vec4u(1u, 3u, 7u, 5u), vec4u(0u, 1u, 5u, 4u),
                              vec4u(2u, 3u, 7u, 6u), vec4u(0u, 1u, 3u, 2u), vec4u(4u, 5u, 7u, 6u));
  let q = quads[vid / 6u];
  let k = array<u32, 6>(q.x, q.y, q.z, q.x, q.z, q.w)[vid % 6u];
  let bit = vec3f(f32(k & 1u), f32((k >> 1u) & 1u), f32((k >> 2u) & 1u));
  let w = vec3f(mix(body.box.x, body.box.z, bit.x), body.info.x + kShimmerHeight * bit.y,
                mix(body.box.y, body.box.w, bit.z));
  var out: ShimmerOut;
  out.rel = w - frame.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  return out;
}

// Whether the camera-relative ray `dir` meets the melt's surface inside the box before `d`.
fn meets_melt(dir: vec3f, lo: vec3f, hi: vec3f, d: f32) -> bool {
  if (dir.y >= 0.0) {
    return false;
  }
  let tl = lo.y / dir.y;
  let q = dir * tl;
  return tl < d && q.x > lo.x && q.x < hi.x && q.z > lo.z && q.z < hi.z;
}

// What is seen across the lake, again, through the ray bent by the hot air it crossed. A pixel
// where the ray meets the melt is the surface's own (fs_main bends its own ray); a bent ray that
// lands on the melt or on something nearer than the layer keeps the pixel as it was.
@fragment
fn fs_shimmer(in: ShimmerOut) -> FragmentOut {
  let pixel = vec2i(in.position.xy);
  let uv = in.position.xy * frame.size.zw;
  let dir = normalize(in.rel);
  let lo = vec3f(body.box.x, body.info.x, body.box.y) - frame.eye.xyz;
  let hi = vec3f(body.box.z, body.info.x + kShimmerHeight, body.box.w) - frame.eye.xyz;
  let inv = 1.0 / select(dir, vec3f(1e-6), abs(dir) < vec3f(1e-6));
  let ta = lo * inv;
  let tb = hi * inv;
  let near = min(ta, tb);
  let far = max(ta, tb);
  let t0 = max(max(max(near.x, near.y), near.z), 0.0);
  let d = behind_distance(pixel, uv);
  let t1 = min(min(min(far.x, far.y), far.z), d);
  if (t1 <= t0 || meets_melt(dir, lo, hi, d)) {
    discard;
  }
  let dir2 = bent(dir, bend(dir, t0, t1, frame.time.x));
  let clip = frame.view_proj * vec4f(dir2 * min(d, 1e4), 1.0);
  var pick = pixel;
  if (clip.w > 1e-4) {
    let uv2 = vec2f(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
    if (all(uv2 >= vec2f(0.0)) && all(uv2 < vec2f(1.0))) {
      let p2 = vec2i(uv2 * frame.size.xy);
      let d2 = behind_distance(p2, uv2);
      if (d2 > t0 && !meets_melt(dir2, lo, hi, d2)) {
        pick = p2;
      }
    }
  }
  var out: FragmentOut;
  out.colour = vec4f(textureLoad(scene_colour, pick, 0).rgb, 1.0);
  out.motion = vec4f(0.0);
  return out;
}
