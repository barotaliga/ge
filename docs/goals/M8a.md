Status: done
Milestone: **The snow's material**: the terrain's material made the game's (a shader over the engine's shared terrain draw, with a bind group of its own), and the snow's in `mountain/app`: the ground map (the packed share, rock by slope, the piste's direction), the micro-relief, the groomer's corduroy and lanes, the wrap's blue terminator, the glitter (reactive for TAA), the glare
Exit: The `snow` lab's close views of seed 38 in the sun, plain against the material, reviewed in the PR; the `world` views with the material; the terrain pass's GPU ms before and after in `profile`
Labs: `snow` (the close views, plain and snow, at a sweep of hours), `world` (`corduroy`, `powder`, `glitter`)

seed 38 at 640x360, the nearest snow at 10, 12 and 14 h: the comb and the lanes raise the piste's detail from 0.44–0.52 to 0.53–1.36 levels a pixel, the loose snow's from 0.34–0.61 to 0.48–0.70; the glitter 2–12 points a frame where the plain snow has none; the snow lit 23–66 levels brighter by the wrap and the glare; the terrain pass's GPU ms in `profile` at 1280x720 on the software adapter 51.7 plain and 60.5 with the material (+17 %), the CPU half's median unmoved at 0.25 ms
