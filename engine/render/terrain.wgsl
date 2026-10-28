// THE GROUND'S DEFAULT MATERIAL (render/terrain.hpp): the game's albedo lit by the sun and the
// sky, the shape every material starts from. A game with a material of its own hands
// `TerrainSource::shader` instead (Fall Line's snow: plugins/mountain/app/snow.wgsl).
#include "render/wgsl/terrain_draw.wgsl"

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let g = ground_at(in);
  let albedo = wet_albedo(g.albedo, 1.0);
  var c = albedo / PI * (g.sun * max(dot(g.n, frame.sun.xyz), 0.0) + sky_irradiance(g.n));
  c += wet_sheen(g.n, -g.rel / max(g.dist, 1e-3), g.sun, g.n.y);
  return ground_out(in, c, 0.0);
}
