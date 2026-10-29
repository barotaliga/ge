# Other worlds' moons' maps

The faces `render/moon_maps.hpp` draws: one 1024 x 512 luminance PNG a moon (east longitude
-180..180 across, 90 N at the top, its mean over the sphere scaled to 100), and for the three
mosaics in colour a 256 x 128 PNG of the colour over the luminance (128 a byte of 1). All are
public domain: spacecraft images from NASA missions, mosaicked by the USGS Astrogeology Science
Center and NASA/JPL-Caltech.

| Map | Source mosaic | Publisher |
| --- | --- | --- |
| `phobos.png` | Phobos Viking global mosaic, 40 pixels a degree (DLR control) | USGS Astrogeology |
| `deimos.png` | Deimos Viking global map, 1440 x 720 | NASA/JPL-Caltech/USGS |
| `io.png`, `io_colour.png` | Io Galileo SSI and Voyager global colour-merge mosaic, 1 km | USGS Astrogeology |
| `europa.png` | Europa Voyager and Galileo SSI global mosaic, 500 m | USGS Astrogeology |
| `ganymede.png`, `ganymede_colour.png` | Ganymede Voyager and Galileo SSI global colour mosaic, 1.4 km | USGS Astrogeology |
| `callisto.png` | Callisto Voyager and Galileo SSI global mosaic, 1 km | USGS Astrogeology |
| `titan.png` | Titan Cassini ISS global mosaic (near infrared), 4 km | USGS Astrogeology |
| `enceladus.png` | Enceladus Cassini ISS global mosaic, 110 m | USGS Astrogeology |
| `miranda.png` | Miranda Voyager 2 global map, 1440 x 720 | NASA/JPL-Caltech/USGS |
| `triton.png`, `triton_colour.png` | Triton Voyager 2 global colour mosaic, 600 m | USGS Astrogeology |

How each was cut from its mosaic:

- **Box-filtered** to 1024 x 512 (and the colour to 256 x 128), the mosaic's no-data texels left
  out of every average.
- **Gaps filled**: where no spacecraft saw the ground (Miranda's and Triton's northern
  hemispheres, the south polar caps of Europa, Ganymede and Callisto), each column's missing end is
  the ground beside it reflected about the edge of the data (the edge smoothed along the
  longitudes), what is still missing comes from the coarser levels, and the fill is feathered
  three texels into the data.
- **Scaled** so the mean over the sphere (each row weighted by the cosine of its latitude) is 100:
  a byte over 100 is the albedo against the moon's mean, which `render/moons.hpp`'s geometric
  albedo sets.
- **Colour**: the colour over the luminance, its own luminance held at 1, so the map only colours.
  Triton's is kept at 40% of its saturation (Voyager's colour is enhanced).

`tests/render_moons_test.cpp` checks that every map decodes at its size with its mean at 100 and
that every colour map's luminance is 1.
