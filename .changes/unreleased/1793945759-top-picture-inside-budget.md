---
type: Changed
title: The top picture inside 8.3 ms at 1440p on the reference GPU
---

At the top of the quality ladder the cloud marches every pixel's own ray spread over four frames (`Clouds::quarter` false with `spread`), and the light shafts through the cloud's shadow are worked out once a frame in a volume over the screen (`sky.shafts`, group 0's binding 16) instead of per pixel. On the M3 Max at 2560x1440 the top frame falls from 11.8 to 6.5 ms under a flurry and from 8.4 to 6.4 ms from the chase camera. `fit_ladder` no longer takes a picture a fraction of a hundredth over its budget, and Fall Line's price table is measured again.
