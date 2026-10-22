// THE REAL FACES' CROPS (R25): where each real face sits on the globe, what the place is called,
// and how the map's square is laid on it. A real face is a 4×4 km stretch of a real ski
// mountain, from its summit ridge down to its valley floor, turned so its fall line runs down the
// map (+z) and laid 1:1: a map metre is a real metre. The heights baked off it are in
// `real_faces/` (real_face.hpp reads them); this table is what the bake crops them by and what a
// game names a face by.
//
// A face's id is its region and a number, a key that never moves. Its PLACE is three words: the
// RANGE it lies in (a country's ISO 3166-1 alpha-2 code where the range's ski areas lie in one
// country, a key of its own across borders), the AREA (the town or mountain its ski area is known
// by) and the PART of that area (the area itself where it is one). `lat` and `lon` are the middle
// the crop was searched round (°); `east` and `north` the window's middle from it (m); `bearing`
// the compass bearing the map's +z runs to (° clockwise from north). A face marked `bare` stands
// wholly above its trees (a high, dry range): its real map drawing no forest is read as treeless
// rather than unmapped.
//
// Where each crop sits was searched once, offline: every bearing 10° apart and every offset 450 m
// apart round the middle, scored on how much of the playable face falls down the map and how far
// the ridge stands over the valley floor. A row moved is a new face: its heights are baked again.
//
// ge:summary R25 the real faces' crops: each face's place, its middle and the turn of its window
// ge:keywords real-face face-crop range area part latitude longitude bearing bare r25
// ge:skill mountain
// ge:test tests/mountain_real_face_test.cpp
#pragma once

#include <array>

#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

struct FaceCrop {
  const char* id;
  RegionId region;
  const char* range;
  const char* area;
  const char* part;
  double lat, lon;     // the middle searched round, °
  double east, north;  // the window's middle from it, m
  double bearing;      // the map's +z, ° clockwise from north
  bool bare;           // wholly above its trees
};

// The map's side a face is cropped to, m.
inline constexpr double kFaceSize = 4000;

// Every face, in the order a card lists them.
inline constexpr std::array<FaceCrop, 114> kFaceCrops{{
    {"alpine-1", RegionId::alpine, "ALPS", "Val Thorens", "Val Thorens", 45.297, 6.585, -1200,
     -1800, -4, false},
    {"alpine-2", RegionId::alpine, "ALPS", "Courchevel", "Courchevel", 45.4, 6.618, 450, -300, 42,
     false},
    {"alpine-3", RegionId::alpine, "ALPS", "Tignes", "Val-Claret", 45.452, 6.9, -450, -1500, 28,
     false},
    {"alpine-4", RegionId::alpine, "ALPS", "Val d'Isère", "La Daille", 45.442, 6.965, -1800, 1800,
     68, false},
    {"alpine-5", RegionId::alpine, "ALPS", "La Plagne", "Plagne Centre", 45.5, 6.68, -1500, 1050, 6,
     false},
    {"alpine-6", RegionId::alpine, "ALPS", "L'Alpe d'Huez", "L'Alpe d'Huez", 45.105, 6.085, 450,
     -1050, 226, false},
    {"alpine-7", RegionId::alpine, "ALPS", "Zermatt", "Zermatt", 46.015, 7.77, 750, 1800, 288,
     false},
    {"alpine-8", RegionId::alpine, "ALPS", "Verbier", "La Chaux", 46.093, 7.245, 0, -1800, 226,
     false},
    {"alpine-9", RegionId::alpine, "ALPS", "St. Anton am Arlberg", "St. Anton am Arlberg", 47.14,
     10.24, 750, 0, 148, false},
    {"alpine-10", RegionId::alpine, "ALPS", "Ischgl", "Ischgl", 46.995, 10.305, 1500, 1800, 270,
     false},
    {"alpine-11", RegionId::alpine, "ALPS", "Sölden", "Sölden", 46.958, 10.985, 0, -300, 52, false},
    {"alpine-12", RegionId::alpine, "ALPS", "Breuil-Cervinia", "Breuil-Cervinia", 45.94, 7.65, 450,
     -450, -70, false},
    {"alpine-13", RegionId::alpine, "ALPS", "Courmayeur", "Courmayeur", 45.795, 6.935, -900, -900,
     0, false},
    {"alpine-14", RegionId::alpine, "ALPS", "Livigno", "Livigno", 46.535, 10.14, -1350, 0, 80,
     false},
    {"alpine-15", RegionId::alpine, "ALPS", "Bormio", "Bormio", 46.455, 10.37, 1350, -1350, -30,
     false},
    {"alpine-16", RegionId::alpine, "ALPS", "Val Gardena", "Selva", 46.555, 11.76, -900, -1350, 10,
     false},
    {"alpine-17", RegionId::alpine, "ALPS", "Alta Badia", "Corvara", 46.535, 11.84, 1350, 0, 20,
     false},
    {"alpine-18", RegionId::alpine, "ALPS", "Cortina d'Ampezzo", "Cortina d'Ampezzo", 46.54, 12.12,
     -900, 450, 120, false},
    {"alpine-19", RegionId::alpine, "ALPS", "Madonna di Campiglio", "Madonna di Campiglio", 46.23,
     10.83, -1350, 900, 120, false},
    {"alpine-20", RegionId::alpine, "ALPS", "Sestriere", "Sestriere", 44.955, 6.88, 1350, -450, -50,
     false},
    {"alpine-21", RegionId::alpine, "ALPS", "Chamonix", "Argentière", 45.97, 6.95, -900, -900, -60,
     false},
    {"alpine-22", RegionId::alpine, "ALPS", "Chamonix", "Le Brévent", 45.93, 6.85, 900, 900, 120,
     false},
    {"alpine-23", RegionId::alpine, "ALPS", "Les Arcs", "Arc 1800", 45.57, 6.8, -450, -900, -40,
     false},
    {"alpine-24", RegionId::alpine, "ALPS", "Méribel", "Méribel", 45.395, 6.565, 1350, -900, -60,
     false},
    {"alpine-25", RegionId::alpine, "ALPS", "Les Deux Alpes", "Les Deux Alpes", 45.005, 6.12, 1350,
     -450, -80, false},
    {"alpine-26", RegionId::alpine, "ALPS", "Serre Chevalier", "Chantemerle", 44.94, 6.56, 0, -1350,
     30, false},
    {"alpine-27", RegionId::alpine, "ALPS", "Avoriaz", "Avoriaz", 46.19, 6.775, 0, -450, -40,
     false},
    {"alpine-28", RegionId::alpine, "ALPS", "Les Menuires", "Les Menuires", 45.32, 6.54, 1350, 0,
     -110, false},
    {"alpine-29", RegionId::alpine, "ALPS", "La Clusaz", "La Clusaz", 45.905, 6.42, 1350, -1350,
     -40, false},
    {"continental-1", RegionId::continental, "US", "Vail", "Vail", 39.62, -106.365, -300, 750, -6,
     false},
    {"continental-2", RegionId::continental, "US", "Breckenridge", "Breckenridge", 39.475, -106.075,
     -1050, 1500, 70, false},
    {"continental-3", RegionId::continental, "US", "Park City", "Park City", 40.64, -111.53, 450,
     1050, 28, false},
    {"continental-4", RegionId::continental, "US", "Aspen", "Aspen", 39.17, -106.82, 1050, -750, 32,
     false},
    {"continental-5", RegionId::continental, "US", "Teton Village", "Teton Village", 43.587,
     -110.84, -900, 450, 120, false},
    {"continental-6", RegionId::continental, "US", "Big Sky", "Big Sky", 45.275, -111.43, 0, 0, 100,
     false},
    {"continental-7", RegionId::continental, "US", "Telluride", "Telluride", 37.925, -107.83, -450,
     0, -30, false},
    {"continental-8", RegionId::continental, "US", "Steamboat Springs", "Steamboat Springs", 40.46,
     -106.79, 900, 450, -60, false},
    {"continental-9", RegionId::continental, "US", "Alta", "Alta", 40.585, -111.645, 450, -1350,
     -30, false},
    {"continental-10", RegionId::continental, "US", "Sun Valley", "Bald Mountain", 43.665, -114.39,
     0, -450, 90, false},
    {"continental-11", RegionId::continental, "CA", "Lake Louise", "Lake Louise", 51.45, -116.15,
     450, 0, -150, false},
    {"continental-12", RegionId::continental, "CA", "Kicking Horse", "Kicking Horse", 51.29,
     -117.07, 450, -450, 30, false},
    {"continental-13", RegionId::continental, "CA", "Fernie", "Fernie", 49.46, -115.1, 0, -450, 60,
     false},
    {"continental-14", RegionId::continental, "CA", "Sunshine Village", "Sunshine Village", 51.08,
     -115.78, 1350, 900, -50, false},
    {"continental-15", RegionId::continental, "CA", "Big White", "Big White", 49.72, -118.94, 1350,
     1350, 150, false},
    {"continental-16", RegionId::continental, "CA", "Sun Peaks", "Sun Peaks", 50.89, -119.9, 0, 450,
     170, false},
    {"continental-17", RegionId::continental, "CA", "Rossland", "Red Mountain", 49.105, -117.85,
     900, 0, 80, false},
    {"continental-18", RegionId::continental, "CA", "Panorama", "Panorama", 50.455, -116.26, 1350,
     -1350, -20, false},
    {"continental-19", RegionId::continental, "CL", "Valle Nevado", "Valle Nevado", -33.343, -70.24,
     -450, 450, 170, true},
    {"continental-20", RegionId::continental, "CL", "Farellones", "Farellones", -33.34, -70.3, 900,
     -450, -100, true},
    {"continental-21", RegionId::continental, "CL", "Portillo", "Portillo", -32.836, -70.129, -1350,
     0, 130, true},
    {"continental-22", RegionId::continental, "AR", "Las Leñas", "Las Leñas", -35.142, -70.093,
     -450, 900, 110, true},
    {"maritime-1", RegionId::maritime, "CA", "Whistler", "Whistler Creekside", 50.085, -122.95,
     -1800, 450, 316, false},
    {"maritime-2", RegionId::maritime, "JP", "Niseko", "Hirafu", 42.865, 140.68, 0, -1500, 148,
     false},
    {"maritime-3", RegionId::maritime, "JP", "Hakuba", "Happo-one", 36.7, 137.815, 1800, -300, 94,
     false},
    {"fell-1", RegionId::fell, "SE", "Åre", "Åre by", 63.418, 13.085, -450, -750, 192, false},
    {"fell-2", RegionId::fell, "NO", "Hemsedal", "Hemsedal", 60.86, 8.45, 1500, 300, 60, false},
    {"fell-3", RegionId::fell, "SE", "Åre", "Duved", 63.401, 12.938, 450, 450, -180, false},
    {"fell-4", RegionId::fell, "SE", "Vemdalsskalet", "Vemdalsskalet", 62.483, 13.961, -1350, -1350,
     -50, false},
    {"fell-5", RegionId::fell, "SE", "Björnrike", "Björnrike", 62.418, 13.964, 0, -450, -110,
     false},
    {"fell-6", RegionId::fell, "SE", "Klövsjö", "Klövsjö", 62.501, 14.171, -900, 900, 30, false},
    {"fell-7", RegionId::fell, "SE", "Kittelfjäll", "Kittelfjäll", 65.255, 15.489, 600, 150, 135,
     false},
    {"fell-8", RegionId::fell, "SE", "Ramundberget", "Ramundberget", 62.696, 12.384, 150, -1350, 25,
     false},
    {"fell-9", RegionId::fell, "SE", "Björkliden", "Björkliden", 68.397, 18.664, 150, -300, 65,
     false},
    {"fell-10", RegionId::fell, "SE", "Sälen", "Lindvallen", 61.156, 13.186, 1350, 0, 60, false},
    {"fell-11", RegionId::fell, "SE", "Sälen", "Tandådalen", 61.174, 13.002, -900, -950, -10,
     false},
    {"fell-12", RegionId::fell, "NO", "Geilo", "Geilo", 60.53, 8.2, -1500, 1200, 165, false},
    {"fell-13", RegionId::fell, "SE", "Branäs", "Branäs", 60.652, 12.951, 900, -1350, 70, false},
    {"fell-14", RegionId::fell, "SE", "Hemavan", "Hemavan", 65.828, 15.098, 0, 900, -120, false},
    {"fell-15", RegionId::fell, "SE", "Tärnaby", "Tärnaby", 65.719, 15.274, 600, -300, 145, false},
    {"fell-16", RegionId::fell, "SE", "Riksgränsen", "Riksgränsen", 68.418, 18.116, 450, 450, 60,
     false},
    {"fell-17", RegionId::fell, "NO", "Trysil", "Trysil", 61.313, 12.209, 450, 450, 80, false},
    {"fell-18", RegionId::fell, "NO", "Hafjell", "Hafjell", 61.236, 10.485, -300, -300, -85, false},
    {"fell-19", RegionId::fell, "NO", "Kvitfjell", "Kvitfjell", 61.461, 10.125, 1350, 900, 60,
     false},
    {"fell-20", RegionId::fell, "NO", "Oppdal", "Vangslia", 62.612, 9.645, -750, 300, -175, false},
    {"fell-21", RegionId::fell, "FI", "Ylläs", "Ylläsjärvi", 67.565, 24.226, 750, -750, 140, false},
    {"maritime-4", RegionId::maritime, "NO", "Narvik", "Narvik", 68.428, 17.454, 300, 600, -45,
     false},
    {"fell-22", RegionId::fell, "SE", "Idre", "Idre", 61.8897, 12.8347, 0, -1350, -140, false},
    {"fell-23", RegionId::fell, "SE", "Lofsdalen", "Lofsdalen", 62.1216, 13.3057, -450, 0, -160,
     false},
    {"fell-24", RegionId::fell, "SE", "Tänndalen", "Tänndalen", 62.5623, 12.2361, -1350, -900, 50,
     false},
    {"fell-25", RegionId::fell, "SE", "Storlien", "Storlien", 63.3228, 12.0906, -1350, 1350, -90,
     false},
    {"fell-26", RegionId::fell, "SE", "Sälen", "Stöten", 61.2631, 12.8876, 0, 450, 0, false},
    {"fell-27", RegionId::fell, "SE", "Hovfjället", "Hovfjället", 60.2942, 12.9718, 1350, 0, 80,
     false},
    {"fell-28", RegionId::fell, "NO", "Norefjell", "Norefjell", 60.2235, 9.5648, 450, 450, 150,
     false},
    {"fell-29", RegionId::fell, "NO", "Beitostølen", "Beitostølen", 61.2538, 8.9058, -1350, 0, -170,
     false},
    {"fell-30", RegionId::fell, "NO", "Skeikampen", "Skeikampen", 61.3497, 10.0797, 450, -450, -170,
     false},
    {"fell-31", RegionId::fell, "NO", "Hovden", "Hovden", 59.5739, 7.3322, 0, 900, 140, false},
    {"fell-32", RegionId::fell, "NO", "Vassfjellet", "Vassfjellet", 63.2658, 10.3859, 450, 450, 90,
     false},
    {"maritime-5", RegionId::maritime, "NO", "Myrkdalen", "Myrkdalen", 60.859, 6.4707, -450, 450,
     140, false},
    {"maritime-6", RegionId::maritime, "NO", "Voss", "Voss", 60.6491, 6.4024, 450, 1350, 130,
     false},
    {"maritime-7", RegionId::maritime, "NO", "Røldal", "Røldal", 59.8255, 6.7342, 900, 450, 160,
     false},
    {"maritime-8", RegionId::maritime, "NO", "Sirdal", "Sirdal", 58.9165, 6.8421, -900, -900, 110,
     false},
    {"maritime-9", RegionId::maritime, "NO", "Sogndal", "Hodlekve", 61.2933, 6.9717, -450, 0, 100,
     false},
    {"maritime-10", RegionId::maritime, "US", "Mammoth Lakes", "Mammoth Lakes", 37.64, -119.03, 450,
     450, 30, false},
    {"maritime-11", RegionId::maritime, "CA", "Revelstoke", "Revelstoke", 50.96, -118.15, 1350, 450,
     -60, false},
    {"maritime-12", RegionId::maritime, "CA", "Blackcomb", "Blackcomb", 50.108, -122.935, 450, -450,
     -30, false},
    {"maritime-13", RegionId::maritime, "JP", "Rusutsu", "Rusutsu", 42.728, 140.92, 450, -450, 20,
     false},
    {"maritime-14", RegionId::maritime, "JP", "Furano", "Furano", 43.33, 142.343, 0, 0, 100, false},
    {"maritime-15", RegionId::maritime, "JP", "Kiroro", "Kiroro", 43.063, 141.005, 450, -450, -90,
     false},
    {"maritime-16", RegionId::maritime, "JP", "Myoko", "Akakura", 36.88, 138.165, -450, 450, 90,
     false},
    {"maritime-17", RegionId::maritime, "JP", "Nozawa Onsen", "Nozawa Onsen", 36.925, 138.46, 450,
     -1350, -60, false},
    {"maritime-18", RegionId::maritime, "JP", "Shiga Kogen", "Yakebitai", 36.783, 138.515, 450,
     -900, 40, false},
    {"maritime-19", RegionId::maritime, "JP", "Goryu", "Goryu", 36.665, 137.83, 0, 0, 50, false},
    {"maritime-20", RegionId::maritime, "JP", "Zao", "Zao Onsen", 38.16, 140.42, -900, -450, -60,
     false},
    {"maritime-21", RegionId::maritime, "JP", "Appi", "Appi", 39.988, 140.97, 450, -450, 50, false},
    {"maritime-22", RegionId::maritime, "CL", "Nevados de Chillán", "Nevados de Chillán", -36.901,
     -71.414, 450, 0, -120, false},
    {"maritime-23", RegionId::maritime, "AR", "Cerro Catedral", "Cerro Catedral", -41.166, -71.463,
     0, -450, 70, false},
    {"alpine-30", RegionId::alpine, "ALPS", "Engelberg", "Engelberg", 46.808, 8.405, 0, -450, -30,
     false},
    {"alpine-31", RegionId::alpine, "ALPS", "Saas-Fee", "Saas-Fee", 46.09, 7.92, 0, 0, 20, false},
    {"alpine-32", RegionId::alpine, "ALPS", "Mürren", "Mürren", 46.56, 7.86, 1350, -450, 100,
     false},
    {"alpine-33", RegionId::alpine, "GE", "Gudauri", "Gudauri", 42.486, 44.495, 0, 0, -160, false},
    {"maritime-24", RegionId::maritime, "RU", "Krasnaya Polyana", "Krasnaya Polyana", 43.649,
     40.295, 900, 0, 20, false},
    {"continental-23", RegionId::continental, "NZ", "The Remarkables", "The Remarkables", -45.055,
     168.815, -450, 0, -10, true},
    {"continental-24", RegionId::continental, "NZ", "Treble Cone", "Treble Cone", -44.63, 168.885,
     900, -450, 80, true},
    {"maritime-25", RegionId::maritime, "NZ", "Mount Hutt", "Mount Hutt", -43.49, 171.535, 900, 0,
     100, true},
}};

}  // namespace ge::mountain::mapgen
