---
type: Fixed
title: Rain, snow and the sky's light stay out of a classic map's closed rooms
---

The BSP renderer's maps of what lies open to the sky (the sun's, the zenith's and the fall's) counted any place nothing was drawn above as open sky: inside every wall, along the seams where faces meet, through the cracks between them. With the weather on, a closed map such as dm4 got rain falling in its corners, wet streaks down its walls and the sky's light in lines. The maps now see the sky only through the map's sky faces, so the weather falls only under a sky opening and a map with no sky has none indoors.
