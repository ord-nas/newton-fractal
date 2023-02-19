#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>

#include <png++/png.hpp>
#include <crow.h>

#include "complex.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"

int i = 0;

std::string DrawToPng(int width, int height) {
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>
    image(width, height);

  const Complex origin(0.0, 0.0);
  const double r_rng = 5.0;
  const double i_rng = r_rng / width * height;
  const double r_min = origin.r - r_rng / 2;
  const double r_max = origin.r + r_rng / 2;
  const double i_min = origin.i - i_rng / 2;
  const double i_max = origin.i + i_rng / 2;

  const AnalyzedPolynomial p = AnalyzedPolynomial({Complex(1.0, 0.0),
						   Complex(-0.5, 0.86602540378),
						   Complex(-0.5, -0.86602540378)});
  std::cout << "Drawing: " << p << std::endl;

  const double i_delta = (i_max - i_min) / height;
  const double r_delta = (r_max - r_min) / width;
  double i = i_min;
  for (size_t y = 0; y < height; ++y) {
    double r = r_min;
    for (size_t x = 0; x < width; ++x) {
      const Complex result = Newton(p, Complex(r, i), 100);
      const size_t zero_index = ClosestZero(result, p.zeros);
      if (zero_index == 0) {
	image[y][x] = png::rgb_pixel(255, 0, 0);
      } else if (zero_index == 1) {
        image[y][x] = png::rgb_pixel(0, 255, 0);
      } else if (zero_index == 2) {
	image[y][x] = png::rgb_pixel(0, 0, 255);
      }
      r += r_delta;
    }
    i += i_delta;
  }

  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  return png_sstrm.str();
}

int main() {
  std::vector<std::string> images;
  for (int img = 0; img < 5; img++) {
    const char c = 'a' + img;
    const std::string filename = "images/" + std::string(1, c) + "_small.png";
    std::ifstream fin(filename,  std::ios::binary);
    std::ostringstream sstrm;
    sstrm << fin.rdbuf();
    images.push_back(sstrm.str());
  }

  // Dynamically generate an in-memory PNG.
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>> image(1280, 720);
  for (png::uint_32 y = 0; y < image.get_height(); ++y) {
    for (png::uint_32 x = 0; x < image.get_width(); ++x) {
      image[y][x] = png::rgb_pixel(x, y, x + y);
    }
  }
  std::ostringstream png_sstrm;
  image.write_stream(png_sstrm);
  images.push_back(png_sstrm.str());

  Polynomial p1({Complex(1, 0), Complex(-2, 0)});
  Polynomial p2({Complex(5, 0), Complex(3, 0)});
  std::cout << p1 << " * " << p2 << " = " << (p1 * p2) << std::endl;

  AnalyzedPolynomial ap({Complex(1, 2), Complex(3, 4), Complex(5, 6)});
  std::cout << ap << std::endl;

  crow::SimpleApp app; //define your crow application

  //define your endpoint at the root directory
  CROW_ROUTE(app, "/")([](){
    std::cout << "Main page okay!";
    auto page = crow::mustache::load_text("index.html");
    return page;
  });

  //define dynamic image endpoint
  CROW_ROUTE(app, "/dynamic_image")([&](){
    i++;
    i = i % (images.size() + 1);
    if (i >= images.size()) {
      // The real deal!
      return crow::response("png", DrawToPng(1280, 720));
    } else {
      return crow::response("png", images[i]);
    }
  });

  //set the port, set the app to run on multiple threads, and run the app
  app.port(18080).multithreaded().run();
}
