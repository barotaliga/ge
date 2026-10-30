Status: done
Milestone: **The ground's own shadow**: the horizon map (two bearings blended, rebaked one at a time as the sun turns), read by the terrain's draw
Exit: The `world` views show ridge shadows that move with `--hour`; the maps rebake only when the sun crosses a bearing step (a counter in the lab); the GPU's bake agrees with the CPU reference
Labs: `world` (`--hour` sweeps), `shadows` (the maps themselves, the bake counter)
