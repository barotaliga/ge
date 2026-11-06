// CAPSULE SHADOWS (docs/rendering.md § Shadows of moving models): the share of a light that reaches
// a point past a capsule standing in for a figure (a model's box as a vertical capsule). The point
// on the capsule's axis nearest the ray from the point to the light, and how far the ray passes
// from it against the capsule's radius, softened over the penumbra a light of radius `lr` casts
// past an occluder that far along the ray (its angular size seen from the point, scaled out to
// the occluder: sharp where the figure stands on the floor, softer further off). Several
// capsules multiply; a pass loops over its own list. For the lights a figure's real shadow map does
// not cover, so a figure in a room of many lights still darkens the floor round him.

// The nearest points of the ray p + t d (0 < t < len) and the segment a..b: (t, s on the segment).
fn capsule_nearest(p: vec3f, d: vec3f, len: f32, a: vec3f, b: vec3f) -> vec2f {
  let ab = b - a;
  let ap = a - p;
  let aa = dot(ab, ab);
  let ad = dot(ab, d);
  let denom = aa - ad * ad;  // |d| = 1
  var s = 0.0;
  if (denom > 1e-8) {
    s = clamp((ad * dot(ap, d) - dot(ap, ab)) / denom, 0.0, 1.0);
  }
  let t = clamp(dot(a + ab * s - p, d), 0.0, len);
  // Back onto the segment from the ray's point, once more.
  if (aa > 1e-8) {
    s = clamp(dot(p + d * t - a, ab) / aa, 0.0, 1.0);
  }
  return vec2f(t, s);
}

// One capsule's share of the light (radius `lr`, `len` m off along unit `d` from `p`).
fn capsule_one(p: vec3f, d: vec3f, len: f32, lr: f32, a: vec3f, b: vec3f, r: f32) -> f32 {
  let ts = capsule_nearest(p, d, len, a, b);
  let t = ts.x;
  if (t <= 1e-3 || t >= len - 1e-3) {
    return 1.0;  // beyond the light, or the point itself inside
  }
  let q = a + (b - a) * ts.y;
  let miss = length(p + d * t - q) - r;
  // The penumbra at the capsule: the light's size seen from the point, scaled out to it.
  let w = max(lr * t / len, 0.02) + 0.02 * t;
  return smoothstep(-w, w, miss);
}
