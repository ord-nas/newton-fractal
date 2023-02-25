#ifndef _CROW_FRACTAL_SERVER_DEVELOPMENT_UTILS_
#define _CROW_FRACTAL_SERVER_DEVELOPMENT_UTILS_

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <png++/png.hpp>

class ImageCycler {
public:
  ImageCycler() {
    for (int i = 0; i < 5; i++) {
      const char c = 'a' + i;
      const std::string filename = "images/" + std::string(1, c) + "_small.png";
      std::ifstream fin(filename,  std::ios::binary);
      std::ostringstream sstrm;
      sstrm << fin.rdbuf();
      images_.push_back(sstrm.str());
    }
  }

  const std::string& Get() {
    curr_ = (curr_ + 1) % images_.size();
    return images_[curr_];
  }

private:
  std::vector<std::string> images_;
  int curr_ = 0;
};

std::string GenerateSamplePng() {
  // Dynamically generate an in-memory PNG.
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>> image(1280, 720);
  for (png::uint_32 y = 0; y < image.get_height(); ++y) {
    for (png::uint_32 x = 0; x < image.get_width(); ++x) {
      image[y][x] = png::rgb_pixel(x, y, x + y);
    }
  }
  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  return png_sstrm.str();
}

uint64_t Now() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

#endif // _CROW_FRACTAL_SERVER_DEVELOPMENT_UTILS_
