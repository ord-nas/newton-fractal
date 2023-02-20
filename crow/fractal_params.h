#ifndef _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_
#define _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_

#include <string>
#include <optional>

#include <png++/png.hpp>
#include <crow.h>

#include "complex.h"

bool ToInt(const char* c_str,
	   int* output,
	   std::optional<int> min_value = std::nullopt,
	   std::optional<int> max_value = std::nullopt) {
  std::string s(c_str);

  int num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stoi(s, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != s.size()) {
    return false;
  }

  if (min_value.has_value() && num < *min_value) {
    return false;
  }
  if (max_value.has_value() && num > *max_value) {
    return false;
  }

  *output = num;
  return true;
}

bool ParsePositiveInt(const crow::query_string& url_params,
		      const std::string& key,
		      int* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  return ToInt(value, output, /*min_value=*/1);
}

bool ToFiniteDouble(const char* c_str, double* output) {
  const std::string s(c_str);

  double num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stod(s, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != s.size()) {
    return false;
  }

  if (!std::isfinite(num)) {
    return false;
  }

  *output = num;
  return true;
}

bool ParseFiniteDouble(const crow::query_string& url_params,
		       const std::string& key,
		       double* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  return ToFiniteDouble(value, output);
}

bool ParseComplexList(const crow::query_string& url_params,
		      const std::string& real_key,
		      const std::string& img_key,
		      std::vector<ComplexD>* output) {
  const std::vector<char*> rs = url_params.get_list(real_key, /*use_brackets=*/false);
  const std::vector<char*> is = url_params.get_list(img_key, /*use_brackets=*/false);
  if (rs.empty() || is.empty() || rs.size() != is.size()) {
    return false;
  }

  const size_t N = rs.size();
  std::vector<ComplexD> cs(N);
  for (size_t i = 0; i < N; ++i) {
    if (!ToFiniteDouble(rs[i], &cs[i].r) ||
	!ToFiniteDouble(is[i], &cs[i].i)) {
      return false;
    }
  }

  *output = cs;
  return true;
}

bool ParseColorList(const crow::query_string& url_params,
		    const std::string& red_key,
		    const std::string& green_key,
		    const std::string& blue_key,
		    std::vector<png::rgb_pixel>* output) {
  const std::vector<char*> rs = url_params.get_list(red_key, /*use_brackets=*/false);
  const std::vector<char*> gs = url_params.get_list(green_key, /*use_brackets=*/false);
  const std::vector<char*> bs = url_params.get_list(blue_key, /*use_brackets=*/false);
  if (rs.empty() || gs.empty() || bs.empty() || rs.size() != gs.size() || rs.size() != bs.size()) {
    return false;
  }

  const size_t N = rs.size();
  std::vector<png::rgb_pixel> colors;
  for (size_t i = 0; i < N; ++i) {
    int red, green, blue;
    if (!ToInt(rs[i], &red, 0, 255) ||
	!ToInt(gs[i], &green, 0, 255) ||
	!ToInt(bs[i], &blue, 0, 255)) {
      return false;
    }
    colors.emplace_back(static_cast<char>(red),
			static_cast<char>(green),
			static_cast<char>(blue));
  }

  *output = colors;
  return true;
}

struct FractalParams {
  static std::optional<FractalParams> Parse(const crow::query_string& url_params) {
    FractalParams fractal_params;
    if (ParseFiniteDouble(url_params, "i_min", &fractal_params.i_min) &&
	ParseFiniteDouble(url_params, "i_max", &fractal_params.i_max) &&
	ParseFiniteDouble(url_params, "r_min", &fractal_params.r_min) &&
	ParseFiniteDouble(url_params, "r_max", &fractal_params.r_max) &&
	ParsePositiveInt(url_params, "width", &fractal_params.width) &&
	ParsePositiveInt(url_params, "height", &fractal_params.height) &&
	ParseComplexList(url_params, "zero_rs", "zero_is", &fractal_params.zeros) &&
	ParseColorList(url_params, "zero_reds", "zero_greens", "zero_blues", &fractal_params.colors) &&
	fractal_params.zeros.size() == fractal_params.colors.size()) {
      return fractal_params;
    }
    return std::nullopt;
  }

  double i_min;
  double i_max;
  double r_min;
  double r_max;
  int width;
  int height;
  std::vector<ComplexD> zeros;
  std::vector<png::rgb_pixel> colors;
};

#endif // _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_
