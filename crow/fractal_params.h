#ifndef _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_
#define _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_

#include <string>
#include <optional>
#include <crow.h>

bool ParsePositiveInt(const crow::query_string& url_params,
		      const std::string& key,
		      int* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  std::string value_str(value);

  int num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stoi(value_str, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != value_str.size()) {
    return false;
  }

  if (num <= 0) {
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
  std::string value_str(value);

  double num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stod(value_str, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != value_str.size()) {
    return false;
  }

  if (!std::isfinite(num)) {
    return false;
  }

  *output = num;
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
	ParsePositiveInt(url_params, "height", &fractal_params.height)) {
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
};

#endif // _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_
