// THE SURFACE'S RESPONSE TO LIGHT (render/surface.hpp, docs/rendering.md § Materials): one
// physically based model for every material, so stone, metal, wood, slime and a wet floor each
// answer the sun, the sky and the lamps in their own way and by the same rules.
//
// - DIFFUSE: Lambert, albedo / pi, weighed by what the specular lobe did not take (1 - F) and by
//   how little metal the surface is (a metal has no diffuse; its colour is in its reflection).
// - SPECULAR: GGX (Trowbridge-Reitz) microfacets with the height-correlated Smith visibility and
//   Schlick's Fresnel; the reflectance head on (f0) is 0.08 x `spec` for a dielectric (0.5: 4 %,
//   water's 2 % at 0.25) and the albedo for a metal. A surface with no reflectance (`spec` and
//   `metal` both 0: every legacy matte material) has no lobe at all, not even at grazing angles
//   (the lobe fades out under 2 % of f0), so it shades exactly as Lambert did.
// - THE SKY IN IT: the sky's radiance along the reflected ray, blurred toward the sky's irradiance
//   as the surface roughens, times the split-sum's analytic environment term (no table to bake).
//
// Roughness is perceptual (`rough`, 0 a mirror .. 1 chalk); alpha = rough^2, floored so a
// mirror's highlight stays on the screen's pixels.
//
// ge:summary The physically based surface: GGX specular, Smith visibility, Fresnel, Lambert
// ge:keywords render surface material pbr physically-based ggx trowbridge-reitz smith fresnel
// ge:keywords schlick lambert roughness metalness metal specular f0 env-brdf split-sum
// ge:keywords surface_brdf surface_sky surface_f0 emissive
// ge:skill render

struct Surface {
  albedo: vec3f,   // linear; a metal's reflectance
  rough: f32,      // perceptual roughness, 0..1
  metal: f32,      // 0 a dielectric .. 1 a metal
  spec: f32,       // a dielectric's reflectance, 0.5 = 4 % head on; 0 (and metal 0): matte
  emissive: f32,   // its own light: luminance in the frame's units (albedo-tinted)
  porous: f32,     // how much a soaking darkens it (frame.wgsl's wet_albedo)
}

fn matte(albedo: vec3f) -> Surface {
  return Surface(albedo, 1.0, 0.0, 0.0, 0.0, 0.6);
}

fn surface_f0(s: Surface) -> vec3f {
  return mix(vec3f(0.08 * s.spec), s.albedo, s.metal);
}

// Schlick's Fresnel, with the lobe faded out under 2 % reflectance (none at all at f0 = 0).
fn surface_fresnel(f0: vec3f, cos_h: f32) -> vec3f {
  let k = pow(1.0 - clamp(cos_h, 0.0, 1.0), 5.0);
  return f0 + (clamp(50.0 * f0.g, 0.0, 1.0) - f0) * k;
}

fn ggx_alpha(rough: f32) -> f32 {
  let r = clamp(rough, 0.045, 1.0);
  return r * r;
}

// The specular lobe's value times n.l for light from `l` (unit), seen from `v` (unit): what a
// unit of irradiance perpendicular to `l` reflects toward the eye.
fn surface_specular(s: Surface, n: vec3f, v: vec3f, l: vec3f) -> vec3f {
  let f0 = surface_f0(s);
  if (max(f0.r, max(f0.g, f0.b)) <= 0.0) {
    return vec3f(0.0);
  }
  let nl = dot(n, l);
  if (nl <= 0.0) {
    return vec3f(0.0);
  }
  let nv = max(dot(n, v), 1e-4);
  let h = normalize(l + v);
  let nh = max(dot(n, h), 0.0);
  let a = ggx_alpha(s.rough);
  let a2 = a * a;
  let dd = nh * nh * (a2 - 1.0) + 1.0;
  let d = a2 / (PI * dd * dd);
  let vis = 0.5 / (nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2));
  return d * vis * surface_fresnel(f0, dot(v, h)) * nl;
}

// The diffuse share of a surface lit from `l`: (1 - F) of a dielectric, none of a metal.
fn surface_diffuse_weight(s: Surface, v: vec3f, l: vec3f) -> vec3f {
  let f0 = surface_f0(s);
  if (max(f0.r, max(f0.g, f0.b)) <= 0.0) {
    return vec3f(1.0 - s.metal);
  }
  return (1.0 - surface_fresnel(f0, dot(v, normalize(l + v)))) * (1.0 - s.metal);
}

// THE WHOLE RESPONSE to light from `l` (unit): the radiance toward `v` per unit of irradiance
// perpendicular to `l`, n.l included.
fn surface_brdf(s: Surface, n: vec3f, v: vec3f, l: vec3f) -> vec3f {
  let nl = max(dot(n, l), 0.0);
  return s.albedo / PI * surface_diffuse_weight(s, v, l) * nl + surface_specular(s, n, v, l);
}

// The split sum's environment term, fitted analytically (no lookup table): the share of the
// sky's radiance a lobe of this roughness reflects at this angle, scale and bias on f0.
fn env_brdf(f0: vec3f, rough: f32, nv: f32) -> vec3f {
  let c0 = vec4f(-1.0, -0.0275, -0.572, 0.022);
  let c1 = vec4f(1.0, 0.0425, 1.04, -0.04);
  let r = rough * c0 + c1;
  let a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
  let ab = vec2f(-1.04, 1.04) * a004 + r.zw;
  return f0 * ab.x + clamp(50.0 * f0.g, 0.0, 1.0) * ab.y;
}

// THE SUN AND THE SKY on a surface: `sun` is the key light's irradiance perpendicular to it at
// this point (its colour here, past every shadow; before the cosine); `seen` the share of the sky
// the point sees (the ambient occlusion's `sky_seen`, 1 for open sky), which dims the sky's light
// and its reflection alike. A matte surface keeps the plain Lambert sum.
fn surface_lit(s: Surface, n: vec3f, v: vec3f, sun: vec3f, seen: f32) -> vec3f {
  let l = frame.sun.xyz;
  let f0 = surface_f0(s);
  if (max(f0.r, max(f0.g, f0.b)) <= 0.0) {
    return s.albedo / PI * (1.0 - s.metal) *
           (sun * max(dot(n, l), 0.0) + sky_irradiance(n) * seen);
  }
  let nv = max(dot(n, v), 1e-4);
  let env = env_brdf(f0, s.rough, nv);
  let r = reflect(-v, n);
  let up = normalize(vec3f(r.x, max(r.y, 0.02), r.z));
  // A rough lobe sees the sky's light spread over its half of the dome, a smooth one its ray.
  let blur = smoothstep(0.2, 0.75, s.rough);
  let sky_r = mix(sky_luminance(up), sky_irradiance(up) / PI, blur) * seen;
  let diffuse = s.albedo / PI * (1.0 - s.metal) *
                (sun * max(dot(n, l), 0.0) * surface_diffuse_weight(s, v, l) +
                 sky_irradiance(n) * seen * (1.0 - env));
  return diffuse + sun * surface_specular(s, n, v, l) + sky_r * env;
}
