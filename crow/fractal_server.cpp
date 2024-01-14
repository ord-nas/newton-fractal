#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <set>
#include <mutex>
#include <chrono>

#include <png++/png.hpp>
#include <crow.h>
#include <boost/version.hpp>

#include "fpng/fpng.h"

#include "rgb_image.h"
#include "complex.h"
#include "complex_array.h"
#include "polynomial.h"
#include "analyzed_polynomial.h"
#include "development_utils.h"
#include "fractal_params.h"
#include "thread_pool.h"
#include "image_regions.h"
#include "pixel_iterator.h"
#include "fractal_drawing.h"
#include "png_encoding.h"
#include "handler_group.h"

crow::query_string GetBodyParams(const crow::request& req) {
  std::string fake_url = "?" + req.body;
  // std::string fake_url = "?width=1280&height=720&zero_rs=0.28996322542741826&zero_rs=0.5620779489112948&zero_rs=0.512216828436002&zero_rs=-0.5068977187436685&zero_rs=0.920913298409193&zero_rs=-0.160837351103718&zero_is=0.5078938895349396&zero_is=-0.7409446143156879&zero_is=0.702083372851301&zero_is=-0.5798783869152273&zero_is=-0.7920066322468213&zero_is=0.356435466743505&zero_reds=177&zero_reds=185&zero_reds=182&zero_reds=187&zero_reds=70&zero_reds=73&zero_greens=224&zero_greens=214&zero_greens=39&zero_greens=36&zero_greens=88&zero_greens=103&zero_blues=235&zero_blues=1&zero_blues=201&zero_blues=132&zero_blues=114&zero_blues=119&i_min=-1.6796875&i_max=1.1328125&r_min=-2.296875&r_max=2.703125";
  return crow::query_string(fake_url);
}

std::string ParamsToString(const crow::query_string& params) {
  std::ostringstream ss;
  const auto keys = params.keys();
  const std::set<std::string> unique_keys(keys.begin(), keys.end());
  for (const std::string& key : unique_keys) {
    const std::vector<char*> values = params.get_list(key, /*use_brackets=*/false);
    if (values.size() == 1) {
      ss << "  " << key << " => " << std::string(values[0]) << std::endl;
    } else {
      ss << "  " << key << " => [" << std::endl;
      for (const char* value : values) {
	ss << "    " << std::string(value) << "," << std::endl;
      }
      ss << "  ]" << std::endl;
    }
  }
  return ss.str();
}

int main() {
  std::cout << "Using Boost "
	    << BOOST_VERSION / 100000     << "."  // major version
	    << BOOST_VERSION / 100 % 1000 << "."  // minor version
	    << BOOST_VERSION % 100                // patch level
	    << std::endl;

  fpng::fpng_init();

  // Using 8-1 threads (since we have 8 logical CPUs) even though there are only
  // 4 physical cores. Experiments seem to show that 8 is slightly faster
  // (although not 2x faster) than 4. We subtract 1 because this leaves us on
  // average one core to handle non-thread-pool tasks. This seems to be
  // especially important when running in bash-on-windows - in that setting,
  // having 8 threads in the pool can cause noticeable hiccups in other tasks,
  // which can make everything jerky.
  ThreadPool thread_pool(/*num_threads=*/7);
  HandlerGroup handlers(&thread_pool);

  crow::SimpleApp app; //define your crow application

  // Main page.
  CROW_ROUTE(app, "/")([](){
    return crow::mustache::load_text("index.html");
  });

  // Params endpoint.
  CROW_ROUTE(app, "/params").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      // std::cout << "Got the following params in request:" << std::endl;
      // std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      }
      return handlers.HandleParamsRequest(*fractal_params);
    });

  // Fractal image for main page.
  CROW_ROUTE(app, "/fractal").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      // std::cout << "Got the following params in request:" << std::endl;
      // std::cout << ParamsToString(params);
      std::optional<FractalParams> fractal_params = FractalParams::Parse(params);
      if (!fractal_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	return crow::response(400);
      }
      return handlers.HandleFractalRequest(*fractal_params);
    });

  // Save image with metadata.
  CROW_ROUTE(app, "/save").methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req){
      const auto params = GetBodyParams(req);
      // std::cout << "Got the following params in request:" << std::endl;
      // std::cout << ParamsToString(params);
      std::optional<SaveParams> save_params = SaveParams::Parse(params);
      if (!save_params.has_value()) {
	std::cout << "Malformed params :(" << std::endl;
	crow::json::wvalue json({{"success", false},
				 {"error_message", "Malformed params"}});
	return crow::response(json);
      }
      return handlers.HandleSaveRequest(*save_params);
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
  //app.port(18080).run();
}
