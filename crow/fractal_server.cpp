#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>

#include <png++/png.hpp>
#include <crow.h>

int i = 0;

int main()
{
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
    i = i % images.size();
    return crow::response("png", images[i]);
  });

  //set the port, set the app to run on multiple threads, and run the app
  app.port(18080).multithreaded().run();
}
