---
name: render
description: "Use for a draw pass, the frame's pass order, a shader's look, or extracting the FrameView from GameState in gameengine2 (engine/render and games/fall_line/app). What game5 draws, pass by pass, and how a pass is built and checked against game5's pictures."
---

# Render

The renderer reads `GameState` through one extraction per frame (`FrameView`, plain
data, interpolated by `alpha`) and never writes it.

## The passes game5 needs (port in roughly this order; docs/goals.md M7–M8)

1. Sky + haze (game5 `sky.ts`, `haze.ts`, baked `sky-table.ts`): one `SkyLook` per
   frame. Every world material fades into the haze along its own view direction.
2. Terrain: a geometry clipmap over the level's heightfield (`terrain.ts`), with the
   snow shader (`snow-glsl.ts`: packed share, glitter, corduroy) and the trail map's
   grooves (`trail-map.ts`, `trail-stamp.ts`).
3. Sun shadows: a shadow map plus the riders' shadow box (`shadow-box.ts`).
4. Forest: instanced procedural trees, ten variants a kind at three LODs, in bands
   (`tree-shapes.ts`, `tree-bands.ts`).
5. Figures: skinned skier and crowd (`skier-*.ts`, `skier-dress.ts`), and the glTF skis
   (`pwa/models/`, loaded with cgltf).
6. Particles: spray, snow cloud and snowfall.
7. Post: the colour grade.

## A pass

```cpp
struct TerrainPass { /* GPU buffers, pipelines, CPU staging */ };
void prepare(TerrainPass&, const FrameView&);       // CPU: cull, fill staging; testable headless
void encode(TerrainPass&, gpu::RenderPass&);        // GPU: bind + draw, no decisions
```

A pass is generic (`engine/render/`) when it carries no Fall Line word, and goes in
`games/fall_line/app/` otherwise. The pass order lives in one function.

## Checking a pass

- Compare against game5's own pictures: run game5's `make world SEED=38` (in
  `../game5`, read-only) and the port's screenshot tool on the same views, and put
  both in the PR.
- Count draw calls, triangles and GPU ms per pass (timestamp queries), and keep the
  frame inside the `perf` skill's budget.
