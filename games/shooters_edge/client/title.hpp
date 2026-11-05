// THE TITLE: what the window shows behind the main menu when the game opens, and behind the
// loading spinner while a map loads: a night sky over no map, the full moon low over the horizon
// in a total eclipse (the renderer's own model of the Earth's shadow: the umbra lit only by the
// air's refracted red light, so the disc reads blood red, its lower limb brighter, the axis set a
// little off its middle; drawn nearer than it is, so it is a disc and not a dot), the air tinted
// red, and two decks of cloud carried across it on a strong wind, played several times faster than
// life so the motion reads in a glance. The eye looks at the moon from a
// little under it and sways a degree, slowly. It is a function of the clock alone.
//
// The title's world is a map-less world (world_draw.hpp: the sky, its clouds and the post pass,
// no weapon) with its sky owned by the caller (`own_sky`), so a frame is `title_view` and
// `draw_world`.
//
// ge:summary The title: a night sky, the moon in total eclipse, clouds passing it, behind the menu
// ge:keywords title main-menu backdrop blood-moon lunar-eclipse umbra night sky clouds loading
// ge:keywords title_view title_sky make_title_world
// ge:skill render shooter
#pragma once

#include <cstdint>

#include "brush/app/bsp_sky.hpp"
#include "gpu/device.hpp"
#include "render/view.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/world_draw.hpp"

namespace shooters_edge::client {

// The title's sky: the night's hour, the moon low and full, the wind and the cloud.
ge::brush::app::SkyPick title_sky();

// The title's frame at `t_s` (the window's clock).
ge::render::FrameView title_view(double t_s);

// The title's world: the map-less world in `picture`'s look, its sky the title's. Null with the
// reason in `error`.
WorldDrawPtr make_title_world(ge::gpu::Device& device, uint32_t width, uint32_t height,
                              wgpu::TextureFormat format, const look::Picture& picture,
                              std::string& error);

}  // namespace shooters_edge::client
