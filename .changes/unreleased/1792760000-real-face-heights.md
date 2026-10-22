---
type: Added
title: The real faces' heights (R25): 114 real mountainsides, their decoder and their bake
---

The mountain plugin now carries 114 REAL FACES: 4×4 km stretches of real ski mountains, each from its summit ridge down to its valley floor, turned so the fall line runs down the map. `real_face_crops.hpp` lists each face's region, place (range, area, part), middle on the globe and bearing; `real_face.hpp` decodes a face's heights (126×126 samples 32 m apart, kept to 0.5 m) and samples them bicubically (`face_height`), and turns a map point to the globe and back (`globe_at`, `map_at`). The heights are generated into `plugins/mountain/mapgen/real_faces/` by the new `real-faces` lab, which also measures and draws every face: with `--bake --fetch --write` it downloads the free Copernicus DEM GLO-30 tiles from their public bucket (no credentials), reads them with the labs' own GeoTIFF reader and inflater (`engine/lab/inflate.hpp`), smooths, quantises and encodes each face. The generator does not use the faces yet (M4d2). The elevation data is © DLR e.V. 2010–2014 and © Airbus Defence and Space GmbH 2014–2018, provided under COPERNICUS by the European Union and ESA.
