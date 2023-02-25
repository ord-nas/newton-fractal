#ifndef _CROW_FRACTAL_SERVER_PNG_ENCODING_
#define _CROW_FRACTAL_SERVER_PNG_ENCODING_

#include <vector>
#include <string>
#include <optional>
#include <sstream>

#include <png++/png.hpp>

#include "fpng/fpng.h"

#include "rgb_image.h"
#include "fractal_params.h"

std::string EncodeWithPngPlusPlus(RGBImage& image) {
  std::ostringstream ss;
  image.write_stream(ss);
  return ss.str();
}

std::string EncodeWithFPng(RGBImage& image) {
  const void* image_bytes = image.get_pixbuf().get_bytes().data();
  std::vector<uint8_t> encoded;
  fpng::fpng_encode_image_to_memory(image_bytes, image.get_width(), image.get_height(), /*num_channels=*/3, encoded);
  return std::string(encoded.begin(), encoded.end());
}

std::string EncodePng(const FractalParams& params, RGBImage& image) {
  std::string png;
  switch (params.png_encoder.value_or(PngEncoder::FPNG)) {
  case PngEncoder::PNGPP:
    png = EncodeWithPngPlusPlus(image);
    break;
  case PngEncoder::FPNG:
    png = EncodeWithFPng(image);
    break;
  }
  return png;
}

#endif // _CROW_FRACTAL_SERVER_PNG_ENCODING_
