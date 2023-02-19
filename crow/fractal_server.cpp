#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <optional>

#include <png++/png.hpp>
#include <crow.h>

#include "complex.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"
#include "development_utils.h"
#include "fractal_params.h"

crow::query_string GetBodyParams(const crow::request& req) {
  std::string fake_url = "?" + req.body;
  return crow::query_string(fake_url);
}

std::string DrawToPng(const FractalParams& params) {
  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>>
    image(params.width, params.height);

  const Complex origin(0.0, 0.0);
  const double r_rng = 5.0;
  const double i_rng = r_rng / params.width * params.height;
  const double r_min = origin.r - r_rng / 2;
  const double r_max = origin.r + r_rng / 2;
  const double i_min = origin.i - i_rng / 2;
  const double i_max = origin.i + i_rng / 2;

  std::random_device rd;
  std::mt19937 twister(rd());
  std::uniform_real_distribution<> dist(-1.0, 1.0);
  const AnalyzedPolynomial p = AnalyzedPolynomial({Complex(dist(twister), dist(twister)),
						   Complex(dist(twister), dist(twister)),
						   Complex(dist(twister), dist(twister))});
  // const AnalyzedPolynomial p = AnalyzedPolynomial({Complex(1.0, 0.0),
  // 						   Complex(-0.5, 0.86602540378),
  // 						   Complex(-0.5, -0.86602540378)});
  std::cout << "Drawing: " << p << std::endl;

  const double i_delta = (i_max - i_min) / params.height;
  const double r_delta = (r_max - r_min) / params.width;
  double i = i_min;
  for (size_t y = 0; y < params.height; ++y) {
    double r = r_min;
    for (size_t x = 0; x < params.width; ++x) {
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
  crow::SimpleApp app; //define your crow application

  // Main page.
  CROW_ROUTE(app, "/")([](){
    return crow::mustache::load_text("index.html");
  });

  // Fractal image for main page.
  CROW_ROUTE(app, "/fractal").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      std::cout << "Got the following params in request:" << std::endl;
      const auto& params = GetBodyParams(req);
      for (const std::string& key : params.keys()) {
	std::cout << "  " << key << " => " << std::string(params.get(key)) << std::endl;
      }
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      } else {
	return crow::response("png", DrawToPng(*fractal_params));
      }
    });

  // Test page - image cycler.
  CROW_ROUTE(app, "/image_cycler.html")([](){
    return crow::mustache::load_text("image_cycler.html");
  });

  // Actual image cycler endpoint.
  ImageCycler cycler;
  CROW_ROUTE(app, "/cyclic_image").methods(crow::HTTPMethod::POST)
    ([&](){
    return crow::response("png", cycler.Get());
  });

  // Test page - dynamic image generation.
  CROW_ROUTE(app, "/dynamic_image.html")([](){
    return crow::mustache::load_text("dynamic_image.html");
  });

  // Actual dynamic image endpoint.
  CROW_ROUTE(app, "/sample_dynamic_image.png")([](){
    return crow::response("png", GenerateSamplePng());
  });

  //set the port, set the app to run on multiple threads, and run the app
  app.port(18080).multithreaded().run();
}
