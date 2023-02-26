#ifndef _CROW_FRACTAL_SERVER_RGB_IMAGE_
#define _CROW_FRACTAL_SERVER_RGB_IMAGE_

#include <png++/png.hpp>

using RGBImage = png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>;

#endif // _CROW_FRACTAL_SERVER_RGB_IMAGE_
