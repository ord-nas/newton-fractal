#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>

#include "crow.h"

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
    i = i % 5;
    return crow::response("png", images[i]);
  });

  //set the port, set the app to run on multiple threads, and run the app
  app.port(18080).multithreaded().run();
}
