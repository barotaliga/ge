// THE MAP'S LAKES AS DRAWN: every lake a game laid on the map (mountain/mapgen/types.hpp's
// `Level::lakes`, the free ride's tarn: mountain/tarn/tarn.hpp) added to the frame's water
// (`add_water`, its shore and the dug bed under it) and frozen by its ice on the map's day
// (`freeze_water`: mountain/app/ice_look.hpp's sheet, the water hidden under it). A tarn frozen
// to bear a skier lies on ground filled flat at its surface; the sheet hides the snow under it.
// Every lake has its own sheet. THE STREAMS a game laid (`Level::streams`, a real face's: mountain/
// tarn/brook.hpp) run as channels where they are open (render/water_stream.hpp's ribbons, built
// once); a frozen one lies under the map's snow, its gully the ground's own, and is not drawn.
//
// ge:summary The map's laid lakes drawn: the water over the dug bed, the ice by the map's day
// ge:keywords lakes tarn show_lakes add_water freeze_water lake-ice map-lake map-stream
// ge:skill render water
#pragma once

#include "fall_line/app/frame.hpp"

namespace fall_line::app {

// Add `frame`'s map's lakes to its water and freeze them by their ice (create_frame calls it).
void show_lakes(ge::gpu::Device& device, GameFrame& frame);

}  // namespace fall_line::app
