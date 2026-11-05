---
type: Added
title: Foam and the wet shore on the drawn water
---

The water's surface now breaks white: whitecaps where the waves' surface folds (the Jacobian of their motion), set so the whitecap law's cover at the sea's own wind holds, and surf where a wave stands over 0.78 of the depth; each patch is kept a few seconds as it drifts downwind (worked out again from the waves' own past, so it is right on the first frame and after a seek) and drawn as lace. A line of foam lies where the water laps the shore, and the ground the waves run up is darkened. `WaterDraw::foam` and `wet_band` turn them off; `water::wind_of` reads a sea's wind back off its height and period, and `water::whitecap_cover` is the cover at a wind.
